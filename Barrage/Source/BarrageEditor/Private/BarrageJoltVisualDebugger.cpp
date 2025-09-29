#include "BarrageJoltVisualDebugger.h"
#include "PrimitiveSceneProxy.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxyDesc.h"

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
	static const FColor JoltDebugColor;
	FText DeubgText;
	FColor Color;
	FTransform Transform;
	FDrawShapeCommand(FText InDebugText, FTransform Transform)
		: DeubgText(InDebugText)
		, Color(JoltDebugColor)
		, Transform(Transform)
	{
	}

	virtual ~FDrawShapeCommand() = default;
	virtual void Draw(FPrimitiveDrawInterface* PDI) const = 0;
};

const FColor FDrawShapeCommand::JoltDebugColor = FColor::Orange;

struct DrawPointCommand : public FDrawShapeCommand
{
	float Size;
	FColor Color;
	DrawPointCommand(FTransform Transform)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "point", "Empty"), Transform)
		, Size(5.f)
		, Color(JoltDebugColor)
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
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
		DrawWireSphere(PDI, Transform.GetLocation(), Color, Radius, 2, SDPG_World, 1.f);
	}
};

struct DrawBoxCommand : public FDrawShapeCommand
{
	FVector Extents;
	DrawBoxCommand(FTransform Transform, const JPH::BoxShape* BoxShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "box", "Box"), Transform)
		, Extents(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(BoxShape->GetHalfExtent())) * 2.f) // Jolt boxes are half extents
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		FQuat BodyRotation = Transform.GetRotation();
		DrawOrientedWireBox(PDI, Transform.GetLocation(), BodyRotation.GetRightVector(), BodyRotation.GetForwardVector(), BodyRotation.GetUpVector(), Extents, Color, SDPG_World, 1.f);
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
		FQuat BodyRotation = Transform.GetRotation();
		DrawWireCapsule(PDI, Transform.GetLocation(), BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), Color, HalfHeight, Radius, 2, SDPG_World, 1.f);
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
		FQuat BodyRotation = Transform.GetRotation();
		DrawWireCylinder(PDI, Transform.GetLocation(), BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), Color, HalfHeight, Radius, 2, SDPG_World, 1.f);
	}
};


// This macro function will log the debug message of the shape type and sub shape type
#ifndef LOG_UNHANDLED_SHAPE_SUB_SHAPE
#define LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType) \
	UE_LOG(LogTemp, Warning, TEXT("BarrageJoltVisualDebugger: Encountered unknown shape type %d and sub shape type %s, cannot visualize"), static_cast<int32>(ShapeType), *FString(JPH::sSubShapeTypeNames[static_cast<int32>(ShapeSubType)]));
#endif
// Helper recursive function to decompose BodyShapes to a core set of "scalar" shapes that we can draw
void GatherScalarShapes(const FTransform& LocalToWorld, const JPH::Shape* BodyShape, TArray<const FDrawShapeCommand*>& CollectedScalarShapes)
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
				CollectedScalarShapes.Add(new DrawSphereCommand(LocalToWorld, SphereShape));
			}
			break;
		}
		case JPH::EShapeSubType::Box:
		{
			const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
			if (BoxShape != nullptr)
			{
				CollectedScalarShapes.Add(new DrawBoxCommand(LocalToWorld, BoxShape));
			}
			break;
		}
		case JPH::EShapeSubType::Triangle:
		case JPH::EShapeSubType::Capsule:
		{
			const JPH::CapsuleShape* CapsuleShape = reinterpret_cast<const JPH::CapsuleShape*>(BodyShape);
			if (CapsuleShape != nullptr)
			{
				CollectedScalarShapes.Add(new DrawCapsuleCommand(LocalToWorld, CapsuleShape));
			}
			break;
		}
		case JPH::EShapeSubType::TaperedCapsule:
			break;
		case JPH::EShapeSubType::Cylinder:
		{
			const JPH::CylinderShape* CylinderShape = reinterpret_cast<const JPH::CylinderShape*>(BodyShape);
			if (CylinderShape != nullptr)
			{
				CollectedScalarShapes.Add(new DrawCylinderCommand(LocalToWorld, CylinderShape));
			}
			break;
		}
		case JPH::EShapeSubType::ConvexHull:
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
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
			break;
		default:
			// Unknown convex shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
			}
			break;
		}
		case JPH::EShapeSubType::MutableCompound:
		{
			const JPH::MutableCompoundShape* CompoundShape = reinterpret_cast<const JPH::MutableCompoundShape*>(BodyShape);
			if (CompoundShape != nullptr)
			{
			}
			break;
		}
		default:
			// Unknown convex shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
				FTransform NewLocalToWorld = FTransform(Rotation, Position) * LocalToWorld;
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
				FTransform NewLocalToWorld = FTransform(FQuat::Identity, FVector::ZeroVector, Scale) * LocalToWorld;
				GatherScalarShapes(NewLocalToWorld, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
			}
			break;
		}
		case JPH::EShapeSubType::OffsetCenterOfMass:
		{
			const JPH::OffsetCenterOfMassShape* DecoratedShape = reinterpret_cast<const JPH::OffsetCenterOfMassShape*>(BodyShape);
			if (DecoratedShape != nullptr)
			{
				// TODO: Add visualization for Center of Mass rendering.
				GatherScalarShapes(LocalToWorld, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
			}
			break;
		}
		default:
			// Unknown decorator shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
			break;
		}
		break;
	case JPH::EShapeType::Mesh:
		switch (ShapeSubType)
		{
		case JPH::EShapeSubType::Mesh:
		default:
			// Unknown mesh shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
			CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
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
		CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
		break;
	default:
		// Unknown shape type
		LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
		CollectedScalarShapes.Add(new DrawPointCommand(LocalToWorld));
		break;
	}
}



UBarrageJoltVisualDebugger::UBarrageJoltVisualDebugger()
{
}


FBoxSphereBounds UBarrageJoltVisualDebugger::CalcBounds(const FTransform& LocalToWorld) const
{
	// This component is global, so just return a huge bounds
	return FBoxSphereBounds(FVector::ZeroVector, FVector(1000000.f), 1000000.f);
}

// TODO:
// Nice to haves: editor customized colors, ability to toggle specific shapes, ability to toggle active vs inactive bodies, ability to toggle constraints

FDebugRenderSceneProxy* UBarrageJoltVisualDebugger::CreateDebugSceneProxy()
{
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
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
			Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View);
			return Result;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_GetDynamicMeshElements);

			FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

			// Get a lock to get a list of all the bodies in the JOLT simulation
			JPH::BodyIDVector RigidBodies;
			TArray<const FDrawShapeCommand*> CollectedScalarShapes;

			PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, RigidBodies);
			for (const JPH::BodyID& BodyID : RigidBodies)
			{

				FTransform LocalToWorld = FTransform::Identity;
				{
					JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
					if (BodyReadLock.Succeeded())
					{
						const JPH::Body& Body = BodyReadLock.GetBody();
						FVector BodyPosition = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition()));
						FQuat BodyRotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation()));

						LocalToWorld = FTransform(BodyRotation, BodyPosition);
						GatherScalarShapes(LocalToWorld, Body.GetShape(), CollectedScalarShapes);

					}
				}
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					for (const FDrawShapeCommand* DrawCommand : CollectedScalarShapes)
					{
						if (DrawCommand != nullptr)
						{
							DrawCommand->Draw(PDI);
							delete DrawCommand;
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
	};

	return new FProxy(this, PhysicsSystem);
}
