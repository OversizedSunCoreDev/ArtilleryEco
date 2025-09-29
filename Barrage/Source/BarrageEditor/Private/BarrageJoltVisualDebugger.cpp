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

// simple draw command structure
// Draw commands let us gather then execute, allowing flexible ordering and batching if needed
// and we can extend them into the UE recording system if needed
struct FDrawShapeCommand
{
	static const FColor JoltDebugColor;
	FText DeubgText;
	FDrawShapeCommand(FText InDebugText)
		: DeubgText(InDebugText)
	{
	}

	virtual ~FDrawShapeCommand() = default;
	virtual void Draw(FPrimitiveDrawInterface* PDI) const = 0;
};

const FColor FDrawShapeCommand::JoltDebugColor = FColor::Green;

struct DrawPointCommand : public FDrawShapeCommand
{
	FVector Position;
	float Size;
	FColor Color;
	DrawPointCommand(const JPH::Body& Body)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "point", "Empty"))
		, Position(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition())))
		, Size(5.f)
		, Color(JoltDebugColor)
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		PDI->DrawPoint(Position, Color, Size, SDPG_World);
	}
};

struct DrawSphereCommand : public FDrawShapeCommand
{
	FVector Position;
	FRotator Orientation;
	float Radius;
	FColor Color;

	DrawSphereCommand(const JPH::Body& Body, const JPH::SphereShape* SphereShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "sphere", "Sphere"))
		, Position(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition())))
		, Orientation(FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation())))
		, Radius(CoordinateUtils::JoltToRadius(SphereShape->GetRadius()))
		, Color(JoltDebugColor)
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		DrawWireSphere(PDI, Position, FLinearColor::Green, Radius, 2, SDPG_World, 1.f);
	}
};

struct DrawBoxCommand : public FDrawShapeCommand
{
	FVector Position;
	FRotator Orientation;
	FVector Extents;
	FColor Color;
	DrawBoxCommand(const JPH::Body& Body, const JPH::BoxShape* BoxShape)
		: FDrawShapeCommand(NSLOCTEXT("joltbarrage", "box", "Box"))
		, Position(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition())))
		, Orientation(FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation())))
		, Extents(FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(BoxShape->GetHalfExtent())) * 2.f) // Jolt boxes are half extents
		, Color(JoltDebugColor)
	{
	}
	virtual void Draw(FPrimitiveDrawInterface* PDI) const override
	{
		FQuat BodyRotation = Orientation.Quaternion();
		DrawOrientedWireBox(PDI, Position, BodyRotation.GetRightVector(), BodyRotation.GetForwardVector(), BodyRotation.GetUpVector(), Extents, FLinearColor::Green, SDPG_World, 1.f);
	}
};

// This macro function will log the debug message of the shape type and sub shape type
#ifndef LOG_UNHANDLED_SHAPE_SUB_SHAPE
#define LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType) \
	UE_LOG(LogTemp, Warning, TEXT("BarrageJoltVisualDebugger: Encountered unknown shape type %d and sub shape type %s, cannot visualize"), static_cast<int32>(ShapeType), *FString(JPH::sSubShapeTypeNames[static_cast<int32>(ShapeSubType)]));
#endif
// Helper recursive function to decompose BodyShapes to a core set of "scalar" shapes that we can draw
void GatherScalarShapes(const JPH::Body& Body, const JPH::Shape* BodyShape, TArray<const FDrawShapeCommand*>& CollectedScalarShapes)
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
				CollectedScalarShapes.Add(new DrawSphereCommand(Body, SphereShape));
			}
			break;
		}
		case JPH::EShapeSubType::Box:
		{
			const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
			if (BoxShape != nullptr)
			{
				CollectedScalarShapes.Add(new DrawBoxCommand(Body, BoxShape));
			}
			break;
		}
		case JPH::EShapeSubType::Triangle:
		case JPH::EShapeSubType::Capsule:
		case JPH::EShapeSubType::TaperedCapsule:
		case JPH::EShapeSubType::Cylinder:
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
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
			break;
		default:
			// Unknown convex shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
			break;
		}
		break;
	case JPH::EShapeType::Compound:
		switch (ShapeSubType)
		{
		case JPH::EShapeSubType::StaticCompound:
		case JPH::EShapeSubType::MutableCompound:
		default:
			// Unknown convex shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
				GatherScalarShapes(Body, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
			}
			break;
		}
		case JPH::EShapeSubType::Scaled:
		{
			const JPH::ScaledShape* DecoratedShape = reinterpret_cast<const JPH::ScaledShape*>(BodyShape);
			if (DecoratedShape != nullptr)
			{
				GatherScalarShapes(Body, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
			}
			break;
		}
		case JPH::EShapeSubType::OffsetCenterOfMass:
		{
			const JPH::OffsetCenterOfMassShape* DecoratedShape = reinterpret_cast<const JPH::OffsetCenterOfMassShape*>(BodyShape);
			if (DecoratedShape != nullptr)
			{
				GatherScalarShapes(Body, DecoratedShape->GetInnerShape(), CollectedScalarShapes);
			}
			break;
		}
		default:
			// Unknown decorator shape subtype
			LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
			CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
			CollectedScalarShapes.Add(new DrawPointCommand(Body));

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
		CollectedScalarShapes.Add(new DrawPointCommand(Body));
		break;
	default:
		// Unknown shape type
		LOG_UNHANDLED_SHAPE_SUB_SHAPE(ShapeType, ShapeSubType);
		CollectedScalarShapes.Add(new DrawPointCommand(Body));
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
			PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, RigidBodies);
			for (const JPH::BodyID& BodyID : RigidBodies)
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];

						FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
						JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
						if (BodyReadLock.Succeeded())
						{
							const JPH::Body& Body = BodyReadLock.GetBody();
							TArray<const FDrawShapeCommand*> CollectedScalarShapes;
							GatherScalarShapes(Body, Body.GetShape(), CollectedScalarShapes);
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
