#include "Debug/BarrageDebugComponent.h"
#include "BarrageDispatch.h"
#include "FWorldSimOwner.h"
#include "Debug/BarrageDebugRenderProxy.h"

UBarrageDebugComponent::UBarrageDebugComponent()
	: Super()
	, bDrawOnlyIfSelected(true)
{
}

FBoxSphereBounds UBarrageDebugComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	static const FVector MinExtent(1.f);
	static const auto Invalid = FBoxSphereBounds(FBox(-MinExtent, MinExtent)).TransformBy(LocalToWorld);

	if(!FBarragePrimitive::IsNotNull(TargetBodyToVisualize))
	{
		return Invalid;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return Invalid;
	}

	if (!GetWorld()->HasSubsystem<UBarrageDispatch>())
	{
		return Invalid;
	}

	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	if (!IsValid(BarrageDispatch))
	{
		return Invalid;
	}

	TSharedPtr<FWorldSimOwner> Simulation = BarrageDispatch->JoltGameSim;
	if (!Simulation.IsValid())
	{
		return Invalid;
	}

	// Get the Jolt implementation
	TSharedPtr<JPH::PhysicsSystem> PhysicsSystem = Simulation->physics_system;
	if (!PhysicsSystem.IsValid())
	{
		return Invalid;
	}

	JPH::BodyID BodyID;
	if (BarrageDispatch->JoltGameSim->GetBodyIDOrDefault(TargetBodyToVisualize->KeyIntoBarrage, BodyID))
	{
		JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
		if (BodyReadLock.Succeeded())
		{
			const JPH::Body& Body = BodyReadLock.GetBody();
			FBox B = FBox::BuildAABB(
				FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition())),
				FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetShape()->GetLocalBounds().GetExtent()))
			);
			return FBoxSphereBounds(B); // should already be in world space
		}
	}

	return Invalid;
}

#if UE_ENABLE_DEBUG_DRAWING
FDebugRenderSceneProxy* UBarrageDebugComponent::CreateDebugSceneProxy()
{
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

	struct FEveryBarrageDebugRenderProxy : public FBarrageDebugRenderProxy
	{
		FEveryBarrageDebugRenderProxy(const UPrimitiveComponent* Component, UBarrageDispatch* BarrageDispatch, TSharedPtr<JPH::PhysicsSystem> Simulation, FBLet Lease, bool bDebugOnlyIfSelected)
			: FBarrageDebugRenderProxy(Component, Simulation)
		{
			JPH::BodyID BodyID;
			if (BarrageDispatch->JoltGameSim->GetBodyIDOrDefault(Lease->KeyIntoBarrage, BodyID))
			{
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_GetSingleBody);
					JPH::BodyLockRead BodyReadLock(Simulation->GetBodyLockInterface(), BodyID);
				}
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_BuildSingleCommands);
					GatherBodyShapeCommands(BodyID);
				}
			}
			bDrawOnlyIfSelected = bDebugOnlyIfSelected;
		}

	private:
		JPH::BodyID TargetBodyID;
	};

	if(FBarragePrimitive::IsNotNull(TargetBodyToVisualize))
	{
		return new FEveryBarrageDebugRenderProxy(this, BarrageDispatch, PhysicsSystem, TargetBodyToVisualize, bDrawOnlyIfSelected);
	}
	return nullptr;
}
#endif