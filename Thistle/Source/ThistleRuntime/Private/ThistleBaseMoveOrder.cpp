#include "ThistleBaseMoveOrder.h"

#include "Public/GameplayTags.h"

////////////////////////MOVE TO

EStateTreeRunStatus FMoveOrder::AttemptMovePath(
	FStateTreeExecutionContext& Context,
	FVector location,
	FVector HereIAm) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UBarrageDispatch* AreWeBarraging = UBarrageDispatch::Get(Context.GetWorld());
	UThistleBehavioralist* Behavioralist = UThistleBehavioralist::Get(Context.GetWorld());
	if (AreWeBarraging != nullptr && Behavioralist)
	{
		bool found = false;
		FConservedTags tagc = UArtilleryLibrary::InternalTagsByKey(UArtilleryDispatch::Get(Context.GetWorld()), InstanceData.KeyOf, found);
		if (found)
		{
			tagc->Remove(TAG_Orders_Move_Needed);
		}
		Behavioralist->BounceTag(InstanceData.KeyOf,TAG_Orders_Move_Needed,UThistleBehavioralist::DelayBetweenMoveOrders);

		if ((HereIAm - location).Length() < FMath::Max(0.01f, Tolerance))
		{
			if (found)
			{
				tagc->Add(TAG_Orders_Move_Needed);
			}
			return EStateTreeRunStatus::Succeeded;
		}

		if (UThistleBehavioralist::AttemptInvokePathingOnKey(UThistleBehavioralist::Get(Context.GetWorld()), InstanceData.KeyOf, location))
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FMoveOrder::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool Shuck = false;
	FVector location = InstanceData.ShuckPoi(Context,Shuck);
	if (!Shuck)
	{
		return EStateTreeRunStatus::Failed;
	}
	//run on cadence.

	bool found = false;
	FVector HereIAm = UArtilleryLibrary::GetLocation(UArtilleryDispatch::Get(Context.GetWorld()), InstanceData.KeyOf, found);
	if (found && (HereIAm - location).Length() <= Tolerance)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return AttemptMovePath(Context, location, HereIAm);
}
