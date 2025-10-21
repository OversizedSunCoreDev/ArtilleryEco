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

// SOOOOOOOO
// I really want to use the Jolt debug renderer, but it requires 
// pre-processor macros to enable and that implies a lot of other
// things. It also does a FULL body interface lock for body access via the 
// body interface. This means we have a few bockers to using it:
// - Greedy locking of the body interface, this may make the "Draw" step to be fast, but interferes with the simulation step
// - We need to turn the feature on at compile time, but we really need it in runtime builds too
// - We *could* use the Shape polymorphic draw interface, but that requires the compile time flag, same boat
// So what we do instead matches the current patterns we see for Vis Debug and Gameplay Debugger within the UE5
// ecosystem. We create a Scene Proxy that draws the shapes we want to visualize, querying the Jolt simulation for
// the bodies and their shapes. This means we need to decompose the Jolt shapes ourselves, but that is not too hard
// and we can do it recursively. We only support a core set of shapes for now, but we can expand as needed.
// This means we can:
// - Enable in any build configuration for UE5
// - Extend into UE5 recorders if desired
// - Use a "sharing is caring" locking model for the body interface, locking only individual bodies as we need them
//    - This may make the debugger "slower", but it allows for interleaved simulation and debug drawing
// sincerely, martin hollstein

//TODO:
// - Add the following flags:
//   - Draw if enabled
//   - Change Debug Color
//   - Show/Hide Text Labels

// simple draw command structure
// Draw commands let us gather then execute, allowing flexible ordering and batching if needed
// and we can extend them into the UE recording system if needed
struct FDrawShapeCommand
{
	FText DeubgText;
	FTransform Transform;
	FDrawShapeCommand(FText InDebugText, FTransform Transform)
		: DeubgText(InDebugText)
		, Transform(Transform)
	{
	}

	virtual ~FDrawShapeCommand() = default;

	/**
	* Occurs in render thread
	**/
	virtual void Draw(FPrimitiveDrawInterface* PDI) const {}

	/**
	* Use this if you want the culling for these shape types and just make Draw Empty.
	* True means it was added and should not be drawn again.
	**/
	virtual bool AddToDebugRenderProxy(FDebugRenderSceneProxy* Proxy) const { return false; }

	FDrawShapeCommand(const FDrawShapeCommand&) = delete;
	FDrawShapeCommand& operator=(const FDrawShapeCommand&) = delete;
};

struct DrawPointCommand : public FDrawShapeCommand
{
	DrawPointCommand(FTransform Transform)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "point", "Empty"), Transform)
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetPointColor();
		const float Size = UBarrageJoltVisualDebuggerSettings::Get().GetPointSize();
		PDI->DrawPoint(Transform.GetLocation(), Color, Size, SDPG_World);
	}
};

struct DrawSphereCommand : public FDrawShapeCommand
{
	FVector Position;
	float Radius;

	DrawSphereCommand(FTransform Transform, const JPH::SphereShape* SphereShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "sphere", "Sphere"), Transform)
		, Radius(CoordinateUtils::JoltToRadius(SphereShape->GetRadius()))
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetSphereColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetSphereColliderLineThickness();
		DrawWireSphere(PDI, Transform.GetLocation(), Color, Radius, 2, SDPG_World, Thickness);
	}
};

struct DrawBoxCommand : public FDrawShapeCommand
{
	FVector Extents;
	DrawBoxCommand(FTransform Transform, const JPH::BoxShape* BoxShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "box", "Box"), Transform)
		, Extents(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(BoxShape->GetHalfExtent()))) // Jolt boxes are half extents
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderLineThickness();
		DrawOrientedWireBox(PDI, Transform.GetLocation(), Transform.GetScaledAxis(EAxis::X), Transform.GetScaledAxis(EAxis::Y), Transform.GetScaledAxis(EAxis::Z), Extents, Color, SDPG_World, Thickness);
	}
};

struct DrawCapsuleCommand : public FDrawShapeCommand
{
	float Radius;
	float HalfHeight;
	DrawCapsuleCommand(FTransform Transform, const JPH::CapsuleShape* CapsuleShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "capsule", "Capsule"), Transform)
		, Radius(CoordinateUtils::JoltToRadius(CapsuleShape->GetRadius()))
		, HalfHeight(CapsuleShape->GetHalfHeightOfCylinder()) //TODO: What is the conversion to UE?
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderLineThickness();
		FQuat BodyRotation = Transform.GetRotation();
		DrawWireCapsule(PDI, Transform.GetLocation(), BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), Color, HalfHeight, Radius, 2, SDPG_World, Thickness);
	}
};

struct DrawTaperedCapsuleCommand : public FDrawShapeCommand
{
	float TopRadius;
	float BottomRadius;
	float HalfHeight;
	DrawTaperedCapsuleCommand(FTransform Transform, const JPH::TaperedCapsuleShape* CapsuleShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "taperedcapsule", "Tapered Capsule"), Transform)
		, TopRadius(CoordinateUtils::JoltToRadius(CapsuleShape->GetTopRadius()))
		, BottomRadius(CoordinateUtils::JoltToRadius(CapsuleShape->GetBottomRadius()))
		, HalfHeight(CapsuleShape->GetHalfHeight()) //TODO: What is the conversion to UE?
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor();
		FQuat BodyRotation = Transform.GetRotation();
		DrawWireChoppedCone(PDI, Transform.GetLocation(), BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), Color, HalfHeight, TopRadius, BottomRadius, 8, SDPG_World);
	}
};

struct DrawCylinderCommand : public FDrawShapeCommand
{
	float Radius;
	float HalfHeight;
	DrawCylinderCommand(FTransform Transform, const JPH::CylinderShape* CylinderShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "cylinder", "Cylinder"), Transform)
		, Radius(CoordinateUtils::JoltToRadius(CylinderShape->GetRadius()))
		, HalfHeight(CylinderShape->GetHalfHeight()) //TODO: What is the conversion to UE?
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderLineThickness();
		FQuat BodyRotation = Transform.GetRotation();
		DrawWireCylinder(PDI, Transform.GetLocation(), BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), Color, HalfHeight, Radius, 2, SDPG_World, Thickness);
	}
};

struct DrawTriangleCommand : public FDrawShapeCommand
{
	FVector Vertex0;
	FVector Vertex1;
	FVector Vertex2;
	DrawTriangleCommand(FTransform Transform, const JPH::TriangleShape* TriangleShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "triangle", "Triangle"), Transform)
		// TODO: Verify winding order
		, Vertex0(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex1())))
		, Vertex1(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex2())))
		, Vertex2(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(TriangleShape->GetVertex3())))
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderLineThickness();
		PDI->DrawLine(Vertex0, Vertex1, Color, SDPG_World, Thickness);
		PDI->DrawLine(Vertex1, Vertex2, Color, SDPG_World, Thickness);
		PDI->DrawLine(Vertex2, Vertex0, Color, SDPG_World, Thickness);
	}
};

struct DrawConvexHullCommand : public FDrawShapeCommand
{
	TArray<TStaticArray<FVector, 3>> Triangles;
	DrawConvexHullCommand(FTransform Transform, const JPH::ConvexHullShape* ConvexHullShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "convexhull", "Convex Hull"), Transform)
	{
		const auto Faces = ConvexHullShape->GetNumFaces();
		Triangles.Reserve(Faces);
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
				TStaticArray<FVector, 3> Triangle = {
					FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex0)),
					FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex1)),
					FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex2))
				};
				Triangles.Add(MoveTemp(Triangle));
			}
		}
	}

	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetConvexColliderColor();
		const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetConvexColliderLineThickness();
		for (const auto& Triangle : Triangles)
		{
			PDI->DrawLine(Triangle[0], Triangle[1], Color, SDPG_World, Thickness);
			PDI->DrawLine(Triangle[1], Triangle[2], Color, SDPG_World, Thickness);
			PDI->DrawLine(Triangle[2], Triangle[0], Color, SDPG_World, Thickness);
		}
	}
};

struct DrawMeshCommand : public FDrawShapeCommand
{
	FDebugRenderSceneProxy::FMesh DebugMesh;
	DrawMeshCommand(FTransform Transform, const JPH::MeshShape* MeshShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "mesh", "Mesh"), Transform)
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
		DebugMesh.Box = FBox::BuildAABB(Transform.GetLocation(), FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Bounds.GetExtent())));
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
					FVector3f(Transform.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex0))))),
					FVector2f::ZeroVector,
					UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
				));
				DebugMesh.Vertices.Add(FDynamicMeshVertex(
					FVector3f(Transform.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex1))))),
					FVector2f::ZeroVector,
					UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
				));
				DebugMesh.Vertices.Add(FDynamicMeshVertex(
					FVector3f(Transform.TransformPositionNoScale(FVector(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Vertex2))))),
					FVector2f::ZeroVector,
					UBarrageJoltVisualDebuggerSettings::Get().GetTriangleMeshColliderColor()
				));

				DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
				DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
				DebugMesh.Indices.Add(CurrentTriangleVertexIndex++);
			}
		}
	}

	virtual bool AddToDebugRenderProxy(FDebugRenderSceneProxy* Proxy) const override
	{
		Proxy->Meshes.Add(DebugMesh);
		return true;
	}
};

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
				TArray<TArray<const FDrawShapeCommand*>> ThreadShapeArrays;
				ThreadShapeArrays.SetNum(RigidBodies.size());
				ParallelFor(RigidBodies.size(), [this, &RigidBodies, &ThreadShapeArrays](int32 BodyIndex)
					{
						GatherBodyShapeCommands(RigidBodies[BodyIndex], ThreadShapeArrays[BodyIndex]);
					});
				for (const TArray<const FDrawShapeCommand*>& LocalArray : ThreadShapeArrays)
				{
					for (const FDrawShapeCommand* ShapeCommand : LocalArray)
					{
						if (ShapeCommand != nullptr)
						{
							if (ShapeCommand->AddToDebugRenderProxy(this))
							{
								// added to proxy, do not add to collection
								continue;
							}
							_CollectedScalarShapes.Add(ShapeCommand);
						}
					}
				}
			}
		}

		~FProxy()
		{
			for (int32 Idx = 0; Idx < _CollectedScalarShapes.Num(); ++Idx)
			{
				delete _CollectedScalarShapes[Idx];
				_CollectedScalarShapes[Idx] = nullptr;
			}
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
			{
				//TODO: we can probably replace all of this with the refactor to just add shapes that are a part of the 
				//FDebugRenderSceneProxy members... that I missed, because UE docs SUCK! However.... those do not support
				//recording, which *may* be needed... perhaps a blend of both?
				QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_DrawViews);
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
						for (const FDrawShapeCommand* DrawCommand : _CollectedScalarShapes)
						{
							if (DrawCommand != nullptr)
							{
								DrawCommand->Draw(PDI);
							}
						}
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
		TArray<const FDrawShapeCommand*> _CollectedScalarShapes;
		FCriticalSection _MeshLocker; // used for the parallel builder, specific only to when a mesh is rendered.

		void GatherBodyShapeCommands(const JPH::BodyID& BodyID, TArray<const FDrawShapeCommand*>& OutShapeCommands)
		{
			FTransform LocalToWorld = FTransform::Identity;
			JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
			if (BodyReadLock.Succeeded())
			{
				const JPH::Body& Body = BodyReadLock.GetBody();
				FVector BodyPosition = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition()));
				FQuat BodyRotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation()));
				LocalToWorld = FTransform(BodyRotation, BodyPosition);
				GatherScalarShapes(LocalToWorld, Body.GetShape(), OutShapeCommands);
			}
		}

		// Helper recursive function to decompose BodyShapes to a core set of "scalar" shapes that we can draw
		void GatherScalarShapes(const FTransform& JoltLocalToWorld, const JPH::Shape* BodyShape, TArray<const FDrawShapeCommand*>& CollectedScalarShapes)
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
						CollectedScalarShapes.Add(new DrawSphereCommand(JoltLocalToWorld, SphereShape));
					}
					break;
				}
				case JPH::EShapeSubType::Box:
				{
					const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
					if (BoxShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawBoxCommand(JoltLocalToWorld, BoxShape));
					}
					break;
				}
				case JPH::EShapeSubType::Triangle:
				{
					const JPH::TriangleShape* TriangleShape = reinterpret_cast<const JPH::TriangleShape*>(BodyShape);
					if (TriangleShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawTriangleCommand(JoltLocalToWorld, TriangleShape));
					}
					break;
				}
				case JPH::EShapeSubType::Capsule:
				{
					const JPH::CapsuleShape* CapsuleShape = reinterpret_cast<const JPH::CapsuleShape*>(BodyShape);
					if (CapsuleShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawCapsuleCommand(JoltLocalToWorld, CapsuleShape));
					}
					break;
				}
				case JPH::EShapeSubType::TaperedCapsule:
				{
					const JPH::TaperedCapsuleShape* CapsuleShape = reinterpret_cast<const JPH::TaperedCapsuleShape*>(BodyShape);
					if (CapsuleShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawTaperedCapsuleCommand(JoltLocalToWorld, CapsuleShape));
					}
					break;
				}
				case JPH::EShapeSubType::Cylinder:
				{
					const JPH::CylinderShape* CylinderShape = reinterpret_cast<const JPH::CylinderShape*>(BodyShape);
					if (CylinderShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawCylinderCommand(JoltLocalToWorld, CylinderShape));
					}
					break;
				}
				case JPH::EShapeSubType::ConvexHull:
				{
					const JPH::ConvexHullShape* ConvexHullShape = reinterpret_cast<const JPH::ConvexHullShape*>(BodyShape);
					if (ConvexHullShape != nullptr)
					{
						CollectedScalarShapes.Add(new DrawConvexHullCommand(JoltLocalToWorld, ConvexHullShape));
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
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
					break;
				default:
					// Unknown convex shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
							GatherScalarShapes(NewLocalToWorld, Sub.mShape, CollectedScalarShapes);
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
							GatherScalarShapes(NewLocalToWorld, Sub.mShape, CollectedScalarShapes);
						}
					}
					break;
				}
				default:
					// Unknown convex shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
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
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
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
						GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
					}
					break;
				}
				default:
					// Unknown decorator shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
						CollectedScalarShapes.Add(new DrawMeshCommand(JoltLocalToWorld, MeshShape));
					}
					break;
				}
				default:
					// Unknown mesh shape subtype
					LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
					CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
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
				CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
				break;
			default:
				// Unknown shape type
				LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
				CollectedScalarShapes.Add(new DrawPointCommand(JoltLocalToWorld));
				break;
			}
		}

	};

	return new FProxy(this, PhysicsSystem);
}
