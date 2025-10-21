#include "BarrageJoltVisualDebugger.h"
#include "PrimitiveSceneProxy.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxyDesc.h"
#include "Misc/ScopeLock.h"
#include "DynamicMeshBuilder.h"
#include "SceneView.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "BarrageJoltVisualDebuggerSettings.h"

// include all the shapes
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/EmptyShape.h"
#include "Jolt/Physics/Collision/Shape/CompoundShape.h"
#include "Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/Shape/ScaledShape.h"
#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"
#include "Jolt/Physics/Collision/Shape/MeshShape.h"
#include "Jolt/Physics/Collision/Shape/TriangleShape.h"
#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"
#include "Jolt/Physics/Collision/Shape/MutableCompoundShape.h"
#include "Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h"


// This macro function will log the debug message of the shape type and sub shape type
#ifndef LOG_UNHANDLED_SHAPE_SUB_SHAPE
#define LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType) \
	UE_LOG(LogTemp, Warning, TEXT("BarrageJoltVisualDebugger: Encountered unknown shape type %d and sub shape type %s, cannot visualize"), static_cast<int32>(ShapeType), *FString(JPH::sSubShapeTypeNames[static_cast<int32>(ShapeSubType)]));
#endif

static TAutoConsoleVariable<bool> CVarDebugRenderBarrage(
	TEXT("r.DebugRender.Barrage"), false,
	TEXT("Depends on Engine Show Flag for Collision. Use this to view all Barrage bodies regardless of UE side components."),
	ECVF_RenderThreadSafe);

UBarrageJoltVisualDebugger::UBarrageJoltVisualDebugger()
{
	PrimaryComponentTick.bCanEverTick = true; // Always mark render state dirty for simulation updates.
}

void UBarrageJoltVisualDebugger::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CVarDebugRenderBarrage.GetValueOnGameThread()) return;
	MarkRenderStateDirty(); // force redraw every frame
}


FBoxSphereBounds UBarrageJoltVisualDebugger::CalcBounds(const FTransform& LocalToWorld) const
{
	// This component is global, so just return a huge bounds
	const FVector BoxExtent(HALF_WORLD_MAX);
	return FBoxSphereBounds(FVector::ZeroVector, BoxExtent, BoxExtent.Size());
}

// TODO:
// Nice to haves: editor customized colors, ability to toggle specific shapes, ability to toggle active vs inactive bodies, ability to toggle constraints

FDebugRenderSceneProxy* UBarrageJoltVisualDebugger::CreateDebugSceneProxy()
{
	if (!CVarDebugRenderBarrage.GetValueOnGameThread()) return nullptr;
	// This subsystem requires a UBarrageDispatch Subsystem to be present and valid.
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	if (!GetWorld()->HasSubsystem<UBarrageDispatch>())
	{
		return nullptr;
	}

	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	if (!IsValid(BarrageDispatch))
	{
		return nullptr;
	}

	TSharedPtr<FWorldSimOwner> Simulation = BarrageDispatch->JoltGameSim;
	if (!Simulation.IsValid())
	{
		return nullptr;
	}

	// Get the Jolt implementation
	TSharedPtr<JPH::PhysicsSystem> PhysicsSystem = Simulation->physics_system;
	if (!PhysicsSystem.IsValid())
	{
		return nullptr;
	}

	struct FProxy final : public FDebugRenderSceneProxy
	{
		FProxy(const UBarrageJoltVisualDebugger* Component, TSharedPtr<JPH::PhysicsSystem> Simulation)
			: FDebugRenderSceneProxy(Component)
			, PhysicsSystem(Simulation)
		{
			bWillEverBeLit = false;
			bIsAlwaysVisible = true;
			DrawType = EDrawType::SolidAndWireMeshes;
			ViewFlagName = TEXT("BarrageJolt");
			// idk spawn a thread that updates stuff

			JPH::BodyIDVector RigidBodies;
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_GetBodies);
				PhysicsSystem->GetBodies(RigidBodies);
			}
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_BuildCommands);
				ParallelFor(RigidBodies.size(), [this, &RigidBodies](int32 BodyIndex)
					{
						GatherBodyShapeCommands(RigidBodies[BodyIndex]);
					});
			}
		}

		~FProxy()
		{
			//noop
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			// Assume Collision is always enabled
			bool bShowForCollision = View->Family->EngineShowFlags.Collision && CVarDebugRenderBarrage.GetValueOnRenderThread();

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) && bShowForCollision;
			Result.bDynamicRelevance = true;
			Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View);
			return Result;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

			// Draw new shapes
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					for (const DebugChoppedCone& ChoppedCone : ChoppedCones)
					{
						ChoppedCone.Draw(PDI);
					}

					for (const DebugAAB& AAB : AABs)
					{
						AAB.Draw(PDI);
					}
				}
			}
		}

		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

	private:
		TSharedPtr<JPH::PhysicsSystem> PhysicsSystem;

		FCriticalSection LineLocker;
		FCriticalSection DashedLocker;
		FCriticalSection ArrowLocker;
		FCriticalSection CircleLocker;
		FCriticalSection CylinderLocker;
		FCriticalSection StarLocker;
		FCriticalSection BoxLocker;
		FCriticalSection SphereLocker;
		FCriticalSection TextLocker;
		FCriticalSection ConeLocker;
		FCriticalSection CapsulLocker;
		FCriticalSection CoordinateSystemLocker;
		FCriticalSection MeshLocker;
		FCriticalSection ChoppedConeLocker;
		FCriticalSection AABLocker;


		// New shapes
		struct DebugChoppedCone
		{
			float TopRadius;
			float BottomRadius;
			float HalfHeight;
			FTransform Transform;

			void Draw(FPrimitiveDrawInterface* PDI) const
			{
				const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor();
				DrawWireChoppedCone(PDI, Transform.GetLocation(), Transform.GetScaledAxis(EAxis::X), Transform.GetScaledAxis(EAxis::Y), Transform.GetScaledAxis(EAxis::Z), Color, HalfHeight, TopRadius, BottomRadius, 8, SDPG_World);
			}
		};

		struct DebugAAB
		{
			FVector Extents;
			FTransform Transform;
			void Draw(FPrimitiveDrawInterface* PDI) const
			{
				const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderColor();
				const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderLineThickness();
				DrawOrientedWireBox(PDI, Transform.GetLocation(), Transform.GetScaledAxis(EAxis::X), Transform.GetScaledAxis(EAxis::Y), Transform.GetScaledAxis(EAxis::Z), Extents, Color, SDPG_World, Thickness);
			}
		};

		TArray<DebugChoppedCone> ChoppedCones;
		TArray<DebugAAB> AABs;

		void AddInvalidShapePointStar(FTransform Transform)
		{
			FScopeLock _(&StarLocker);
			Stars.Add(
				FDebugRenderSceneProxy::FWireStar(
					Transform.GetLocation(),
					UBarrageJoltVisualDebuggerSettings::Get().GetPointColor(),
					UBarrageJoltVisualDebuggerSettings::Get().GetPointSize()
				)
			);
		}

		void GatherBodyShapeCommands(const JPH::BodyID& BodyID)
		{
			FTransform LocalToWorld = FTransform(GetLocalToWorld());
			JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
			if (BodyReadLock.Succeeded())
			{
				const JPH::Body& Body = BodyReadLock.GetBody();
				FVector BodyPosition = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition()));
				FQuat BodyRotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation()));
				LocalToWorld = FTransform(BodyRotation, BodyPosition);
				GatherScalarShapes(LocalToWorld, Body.GetShape());
			}
		}

		// Helper recursive function to decompose BodyShapes to a core set of "scalar" shapes that we can draw
		void GatherScalarShapes(const FTransform& JoltLocalToWorld, const JPH::Shape* BodyShape)
		{
			if (BodyShape == nullptr)
			{
				return;
			}

			const JPH::EShapeType ShapeType = BodyShape->GetType();
			const JPH::EShapeSubType ShapeSubType = BodyShape->GetSubType();

			switch (ShapeType)
			{
			case JPH::EShapeType::Convex:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::Sphere:
				{
					const JPH::SphereShape* SphereShape = reinterpret_cast<const JPH::SphereShape*>(BodyShape);
					if (SphereShape != nullptr)
					{
						FScopeLock _(&SphereLocker);
						Spheres.Add(
							FDebugRenderSceneProxy::FSphere(
								FMath::TruncToFloat(CoordinateUtils::JoltToRadius(SphereShape->GetRadius())),
								JoltLocalToWorld.GetLocation(),
								UBarrageJoltVisualDebuggerSettings::Get().GetSphereColliderColor()
							)
						);
					}
					break;
				}
				case JPH::EShapeSubType::Box:
				{
					const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
					if (BoxShape != nullptr)
					{
						FScopeLock _(&AABLocker);
						AABs.Add(
							DebugAAB{
								.Extents = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(BoxShape->GetHalfExtent())),
								.Transform = JoltLocalToWorld
							}
						);
					}
					break;
				}
				case JPH::EShapeSubType::Triangle:
				{
					const JPH::TriangleShape* TriangleShape = reinterpret_cast<const JPH::TriangleShape*>(BodyShape);
					if (TriangleShape != nullptr)
					{
						const auto V0 = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex1()));
						const auto V1 = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex2()));
						const auto V2 = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex3()));
						const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor();

						FScopeLock _(&LineLocker);
						Lines.Add(FDebugRenderSceneProxy::FDebugLine(
							JoltLocalToWorld.TransformPosition(V0),
							JoltLocalToWorld.TransformPosition(V1),
							Color
						));
						Lines.Add(FDebugRenderSceneProxy::FDebugLine(
							JoltLocalToWorld.TransformPosition(V1),
							JoltLocalToWorld.TransformPosition(V2),
							Color
						));
						Lines.Add(FDebugRenderSceneProxy::FDebugLine(
							JoltLocalToWorld.TransformPosition(V2),
							JoltLocalToWorld.TransformPosition(V0),
							Color
						));
					}
					break;
				}
				case JPH::EShapeSubType::Capsule:
				{
					const JPH::CapsuleShape* CapsuleShape = reinterpret_cast<const JPH::CapsuleShape*>(BodyShape);
					if (CapsuleShape != nullptr)
					{
						const auto Radius = CoordinateUtils::JoltToRadius(CapsuleShape->GetRadius());
						const auto HalfHeight = CapsuleShape->GetHalfHeightOfCylinder();

						FScopeLock _(&CapsulLocker);
						Capsules.Add(FDebugRenderSceneProxy::FCapsule(
							JoltLocalToWorld.GetLocation(),
							Radius,
							JoltLocalToWorld.GetUnitAxis(EAxis::X),
							JoltLocalToWorld.GetUnitAxis(EAxis::Y),
							JoltLocalToWorld.GetUnitAxis(EAxis::Z),
							HalfHeight,
							UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor()
						));
					}
					break;
				}
				case JPH::EShapeSubType::TaperedCapsule:
				{
					const JPH::TaperedCapsuleShape* CapsuleShape = reinterpret_cast<const JPH::TaperedCapsuleShape*>(BodyShape);
					if (CapsuleShape != nullptr)
					{
						const float TopRadius(FMath::TruncToFloat(CoordinateUtils::JoltToRadius(CapsuleShape->GetTopRadius())));
						const float BottomRadius(FMath::TruncToFloat(CoordinateUtils::JoltToRadius(CapsuleShape->GetBottomRadius())));
						const float HalfHeight(FMath::TruncToFloat(CapsuleShape->GetHalfHeight()));
						FScopeLock _(&ChoppedConeLocker);
						ChoppedCones.Add(
							DebugChoppedCone{
								.TopRadius = TopRadius,
								.BottomRadius = BottomRadius,
								.HalfHeight = HalfHeight,
								.Transform = JoltLocalToWorld
							}
						);
					}
					break;
				}
				case JPH::EShapeSubType::Cylinder:
				{
					const JPH::CylinderShape* CylinderShape = reinterpret_cast<const JPH::CylinderShape*>(BodyShape);
					if (CylinderShape != nullptr)
					{
						const auto Radius(CoordinateUtils::JoltToRadius(CylinderShape->GetRadius()));
						const auto HalfHeight(CylinderShape->GetHalfHeight());
						
						FScopeLock _(&CylinderLocker);
						Cylinders.Add(
							FDebugRenderSceneProxy::FWireCylinder(
								JoltLocalToWorld.GetLocation(),
								JoltLocalToWorld.GetUnitAxis(EAxis::Z),
								Radius,
								HalfHeight,
								UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor()
							)
						);
					}
					break;
				}
				case JPH::EShapeSubType::ConvexHull:
				{
					const JPH::ConvexHullShape* ConvexHullShape = reinterpret_cast<const JPH::ConvexHullShape*>(BodyShape);
					if (ConvexHullShape != nullptr)
					{
						const auto Faces = ConvexHullShape->GetNumFaces();
						FDebugRenderSceneProxy::FMesh DebugMesh;
						DebugMesh.Color = UBarrageJoltVisualDebuggerSettings::Get().GetConvexColliderColor();
						DebugMesh.Box = FBox(UE::Math::TSphere<double>(JoltLocalToWorld.GetLocation(), ConvexHullShape->GetConvexRadius()));
						for (uint32 FaceIndex = 0; FaceIndex < Faces; ++FaceIndex)
						{
							const auto Vertices = ConvexHullShape->GetNumVerticesInFace(FaceIndex);
							TArray<JPH::uint> FaceVertices;
							FaceVertices.SetNum(Vertices);
							const auto VerticesCollected = ConvexHullShape->GetFaceVertices(FaceIndex, Vertices, FaceVertices.GetData());
							check(VerticesCollected == Vertices);
							for (uint32 VertexIndex = 2U; VertexIndex < VerticesCollected; ++VertexIndex)
							{
								const auto Vertex0 = ConvexHullShape->GetPoint(FaceVertices[VertexIndex - 2]);
								const auto Vertex1 = ConvexHullShape->GetPoint(FaceVertices[VertexIndex - 1]);
								const auto Vertex2 = ConvexHullShape->GetPoint(FaceVertices[VertexIndex]);

								const auto V0 = FVector3f(JoltLocalToWorld.TransformPosition(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex0))));
								const auto V1 = FVector3f(JoltLocalToWorld.TransformPosition(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex1))));
								const auto V2 = FVector3f(JoltLocalToWorld.TransformPosition(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex2))));
								
								DebugMesh.Vertices.Append({
									FDynamicMeshVertex(V0, FVector2f(0.f, 1.f), FColor::White),
									FDynamicMeshVertex(V1, FVector2f(0.f, 1.f), FColor::White),
									FDynamicMeshVertex(V2, FVector2f(0.f, 1.f), FColor::White)
								});
								DebugMesh.Indices.Append({
									VertexIndex - 2,
									VertexIndex - 1,
									VertexIndex
								});
							}
						}
						FScopeLock _(&MeshLocker);
						Meshes.Add(MoveTemp(DebugMesh));
					}
					break;
				}
				// TODO: Implement other convex shapes as needed
				case JPH::EShapeSubType::UserConvex1:
				case JPH::EShapeSubType::UserConvex2:
				case JPH::EShapeSubType::UserConvex3:
				case JPH::EShapeSubType::UserConvex4:
				case JPH::EShapeSubType::UserConvex5:
				case JPH::EShapeSubType::UserConvex6:
				case JPH::EShapeSubType::UserConvex7:
				case JPH::EShapeSubType::UserConvex8:
					// These are convex shapes we don't yet support drawing
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				default:
					// Unknown convex shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::Compound:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::StaticCompound:
				{
					const JPH::StaticCompoundShape* CompoundShape = reinterpret_cast<const JPH::StaticCompoundShape*>(BodyShape);
					if (CompoundShape != nullptr)
					{
						const int32 ChildCount = CompoundShape->GetNumSubShapes();
						JPH::Vec3 Scale3D = CoordinateUtils::ToJoltCoordinates(JoltLocalToWorld.GetScale3D());
						for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
						{
							const auto& Sub = CompoundShape->GetSubShape(ChildIndex);
							const auto T = Sub.GetLocalTransformNoScale(Scale3D);
							FVector Position = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(T.GetTranslation()));
							FQuat Rotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(T.GetQuaternion()));
							FTransform NewLocalToWorld = FTransform(Rotation, Position) * JoltLocalToWorld;
							GatherScalarShapes(NewLocalToWorld, Sub.mShape);
						}
					}
					break;
				}
				case JPH::EShapeSubType::MutableCompound:
				{
					const JPH::MutableCompoundShape* CompoundShape = reinterpret_cast<const JPH::MutableCompoundShape*>(BodyShape);
					if (CompoundShape != nullptr)
					{
						const int32 ChildCount = CompoundShape->GetNumSubShapes();
						JPH::Vec3 Scale3D = CoordinateUtils::ToJoltCoordinates(JoltLocalToWorld.GetScale3D());
						for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
						{
							const auto& Sub = CompoundShape->GetSubShape(ChildIndex);
							const auto T = Sub.GetLocalTransformNoScale(Scale3D);
							FVector Position = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(T.GetTranslation()));
							FQuat Rotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(T.GetQuaternion()));
							FTransform NewLocalToWorld = FTransform(Rotation, Position) * JoltLocalToWorld;
							GatherScalarShapes(NewLocalToWorld, Sub.mShape);
						}
					}
					break;
				}
				default:
					// Unknown convex shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::Decorated:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::RotatedTranslated:
				{
					const JPH::RotatedTranslatedShape* DecoratedShape = reinterpret_cast<const JPH::RotatedTranslatedShape*>(BodyShape);
					if (DecoratedShape != nullptr)
					{
						FVector Position = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(DecoratedShape->GetPosition()));
						FQuat Rotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(DecoratedShape->GetRotation()));
						FTransform NewLocalToWorld = FTransform(Rotation, Position) * JoltLocalToWorld;
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape());
					}
					break;
				}
				case JPH::EShapeSubType::Scaled:
				{
					const JPH::ScaledShape* DecoratedShape = reinterpret_cast<const JPH::ScaledShape*>(BodyShape);
					if (DecoratedShape != nullptr)
					{
						FVector Scale = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(DecoratedShape->GetScale()));
						FTransform NewLocalToWorld = FTransform(FQuat::Identity, FVector::ZeroVector, Scale) * JoltLocalToWorld;
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape());
					}
					break;
				}
				case JPH::EShapeSubType::OffsetCenterOfMass:
				{
					const JPH::OffsetCenterOfMassShape* DecoratedShape = reinterpret_cast<const JPH::OffsetCenterOfMassShape*>(BodyShape);
					if (DecoratedShape != nullptr)
					{
						const FVector PositionOffset = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(DecoratedShape->GetOffset()));
						FTransform NewLocalToWorld = FTransform(PositionOffset) * JoltLocalToWorld;
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape());
					}
					break;
				}
				default:
					// Unknown decorator shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::Mesh:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::Mesh:
				{
					const JPH::MeshShape* MeshShape = reinterpret_cast<const JPH::MeshShape*>(BodyShape);
					if (MeshShape != nullptr)
					{
						JPH::Shape::GetTrianglesContext TriContext;
						MeshShape->GetTrianglesStart(
							TriContext,
							JPH::AABox::sBiggest(), // we want all triangles
							JPH::Vec3::sZero(),   // position COM
							JPH::Quat::sIdentity(), // rotation
							JPH::Vec3::sReplicate(1.0f) // scale
						);

						// grab like 256 triangles at a time
						uint32 CurrentTriangleVertexIndex = 0U;
						TStaticArray<JPH::Float3, 3 * 256> RawTriangles;

						const auto Bounds = MeshShape->GetLocalBounds();
						FDebugRenderSceneProxy::FMesh DebugMesh;
						DebugMesh.Box = FBox::BuildAABB(JoltLocalToWorld.GetLocation(), FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Bounds.GetExtent())));
						DebugMesh.Color = UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor();

						while (true)
						{
							const int32 TrianglesFetched = MeshShape->GetTrianglesNext(TriContext, 256, RawTriangles.GetData());
							if (TrianglesFetched <= 0)
							{
								break;
							}
							for (int32 TriangleIndex = 0; TriangleIndex < TrianglesFetched; ++TriangleIndex)
							{
								const int32 BaseIndex = TriangleIndex * 3;

								const auto Vertex0 = JPH::Vec3(RawTriangles[BaseIndex + 0]);
								const auto Vertex1 = JPH::Vec3(RawTriangles[BaseIndex + 1]);
								const auto Vertex2 = JPH::Vec3(RawTriangles[BaseIndex + 2]);

								DebugMesh.Vertices.Add(FDynamicMeshVertex(
									FVector3f(JoltLocalToWorld.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex0))))),
									FVector2f::ZeroVector,
									UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
								));
								DebugMesh.Vertices.Add(FDynamicMeshVertex(
									FVector3f(JoltLocalToWorld.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex1))))),
									FVector2f::ZeroVector,
									UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
								));
								DebugMesh.Vertices.Add(FDynamicMeshVertex(
									FVector3f(JoltLocalToWorld.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex2))))),
									FVector2f::ZeroVector,
									UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
								));

								DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
								DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
								DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
							}
						}

						FScopeLock _(&MeshLocker);
						Meshes.Add(MoveTemp(DebugMesh));
					}
					break;
				}
				default:
					// Unknown mesh shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::HeightField:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::HeightField:
				default:
					// Unknown heightfield shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::SoftBody:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::SoftBody:
				default:
					// Unknown softbody shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				}
				break;
			case JPH::EShapeType::Plane:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::Plane:
				default:
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					break;
				}
				break;
			case JPH::EShapeType::Empty:
				switch (ShapeSubType)
				{
				case JPH::EShapeSubType::Empty:
					AddInvalidShapePointStar(JoltLocalToWorld);
					break;
				default:
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					break;
				}
				break;
			case JPH::EShapeType::User1:
			case JPH::EShapeType::User2:
			case JPH::EShapeType::User3:
			case JPH::EShapeType::User4:
				// These are user defined shapes, we don't know how to draw them
				LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
				AddInvalidShapePointStar(JoltLocalToWorld);
				break;
			default:
				// Unknown shape type
				LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
				AddInvalidShapePointStar(JoltLocalToWorld);
				break;
			}
		}

	};

	return new FProxy(this, PhysicsSystem);
}
