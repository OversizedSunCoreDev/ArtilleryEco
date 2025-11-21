#include "ThistleBaseMoveOrder.h"

#include "Public/GameplayTags.h"

////////////////////////MOVE TO

EStateTreeRunStatus FMoveOrder::AttemptMovePath(
	FStateTreeExecutionContext& Context,
	FVector location,
	FVector HereIAm) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto AreWeBarraging = UBarrageDispatch::SelfPtr;

	if (AreWeBarraging != nullptr && UThistleBehavioralist::SelfPtr)
	{
		bool found = false;
		FConservedTags tagc = UArtilleryLibrary::InternalTagsByKey(InstanceData.KeyOf, found);
		if (found)
		{
			tagc->Remove(TAG_Orders_Move_Needed);
		}
		UThistleBehavioralist::SelfPtr->BounceTag(InstanceData.KeyOf,TAG_Orders_Move_Needed,UThistleBehavioralist::DelayBetweenMoveOrders);

		if ((HereIAm - location).Length() < FMath::Max(0.01f, Tolerance))
		{
			if (found)
			{
				tagc->Add(TAG_Orders_Move_Needed);
			}
			return EStateTreeRunStatus::Succeeded;
		}

		if (UThistleBehavioralist::AttemptInvokePathingOnKey(InstanceData.KeyOf, location))
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
	FVector location = InstanceData.ShuckPoi(Shuck);
	if (!Shuck)
	{
		return EStateTreeRunStatus::Failed;
	}
	//run on cadence.

	bool found = false;
	FVector HereIAm = UArtilleryLibrary::implK2_GetLocation(InstanceData.KeyOf, found);
	if (found && (HereIAm - location).Length() <= Tolerance)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return AttemptMovePath(Context, location, HereIAm);
}
