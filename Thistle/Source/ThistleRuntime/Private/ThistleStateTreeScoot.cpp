#include "ThistleStateTreeScoot.h"

#include "Public/GameplayTags.h"

EStateTreeRunStatus FScoot::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool bShuckSuccess = false;
	FVector TargetLocation = InstanceData.ShuckPoi(bShuckSuccess);
	if (!bShuckSuccess)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	// This now runs continuously while active rather than on a cadence.
	// The State Tree transition to this state should be evaluated periodically.

	bool bFoundSelf = false;
	FVector HereIAm = UArtilleryLibrary::implK2_GetLocation(InstanceData.KeyOf, bFoundSelf);
	if (bFoundSelf && (HereIAm - TargetLocation).Length() < Tolerance)
	{
		// We are too close, so we need to scoot.
		return AttemptScootPath(Context, TargetLocation, HereIAm);
	}

	// If we are no longer too close succeeded
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FScoot::AttemptScootPath(FStateTreeExecutionContext& Context, FVector location, FVector HereIAm) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Calculate a destination point that is away from the target
	FVector DirectionAwayFromTarget = (HereIAm - location).GetSafeNormal();

	// If we are right on top of the target, pick a random direction to move away
	if (DirectionAwayFromTarget.IsNearlyZero())
	{
		DirectionAwayFromTarget = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	}

	// The new destination is a point further away from the target that is 'Tolerance' distance away
	FVector Destination = HereIAm + (DirectionAwayFromTarget * (Tolerance * 1.5f));

	if (UThistleBehavioralist::AttemptInvokePathingOnKey(InstanceData.KeyOf, Destination))
	{
		// Pathing was successfully invoked. We are running until we reach the destination or the condition changes
		return EStateTreeRunStatus::Running;
	}

	// Pathing failed for some reason.
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FBreakOff::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool Shuck = false;
	FVector location = InstanceData.ShuckPoi(Shuck);
	if (!Shuck)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	//run on cadence.
	if (UArtilleryLibrary::GetTotalsTickCount() % 4 == 0)
	{
		bool found = false;
		FVector HereIAm = UArtilleryLibrary::implK2_GetLocation(InstanceData.KeyOf, found);

		if (found && (HereIAm - location).Length()  < Tolerance * 2)
		{
			UBarrageDispatch* AreWeBarraging = UBarrageDispatch::SelfPtr;
			UThistleBehavioralist* Behavioralist = UThistleBehavioralist::SelfPtr;
			if (AreWeBarraging != nullptr && UThistleBehavioralist::SelfPtr)
			{
				FConservedTags tagc = UArtilleryLibrary::InternalTagsByKey(InstanceData.KeyOf, found);
				FVector destination = ( HereIAm - location).GetSafeNormal();
				if (destination != FVector::ZeroVector && (HereIAm - location).Length()  < Tolerance * 1.1)
				{
					if (found)
					{
						Behavioralist->BounceTag(InstanceData.KeyOf, TAG_Orders_Move_Needed,UThistleBehavioralist::DelayBetweenMoveOrders);
						Behavioralist->ExpireTag(InstanceData.KeyOf, TAG_Orders_Move_Break, UThistleBehavioralist::DelayBetweenMoveOrders); 
					}           
					return EStateTreeRunStatus::Succeeded;
				}
				return EStateTreeRunStatus::Running;
			}
		}
		else
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Succeeded;
}
