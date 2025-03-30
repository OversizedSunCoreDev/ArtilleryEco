﻿#include "ThistleBaseMoveOrder.h"

#include "Public/GameplayTags.h"

////////////////////////MOVE TO


EStateTreeRunStatus FMoveOrder::AttemptMovePath(FStateTreeExecutionContext& Context, FVector location,
                                                FVector HereIAm) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto AreWeBarraging = UBarrageDispatch::SelfPtr;

	if (AreWeBarraging != nullptr && UThistleBehavioralist::SelfPtr)
	{
		bool found = false;
		auto tagc = UArtilleryLibrary::InternalTagsByKey(InstanceData.KeyOf, found);
		if (found)
		{
			tagc->RemoveTag(TAG_Orders_Move_Needed);
		}

		UThistleBehavioralist::SelfPtr->BounceTag(InstanceData.KeyOf, TAG_Orders_Move_Needed,
		                                          UThistleBehavioralist::DelayBetweenMoveOrders);

		if ((HereIAm - location).Length() < FMath::Max(0.01f, Tolerance))
		{
			if (found)
			{
				tagc->AddTag(TAG_Orders_Move_Needed);
			}
			return EStateTreeRunStatus::Succeeded;
		}

		if (!UThistleBehavioralist::AttemptInvokePathingOnKey(InstanceData.KeyOf, location))
		{
			if (found)
			{
				tagc->AddTag(TAG_Orders_Move_Needed);
			}
			return EStateTreeRunStatus::Failed;
		}
		// ReSharper disable once CppRedundantElseKeywordInsideCompoundStatement
		else
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
	auto location = InstanceData.ShuckPoi(Shuck);
	if (!Shuck)
	{
		return EStateTreeRunStatus::Failed;
	}
	//run on cadence.

	bool found = false;
	auto HereIAm = UArtilleryLibrary::implK2_GetLocation(InstanceData.KeyOf, found);
	if (found && (HereIAm - location).Length() > Tolerance)
	{
		return AttemptMovePath(Context, location, HereIAm);
	}
	return EStateTreeRunStatus::Succeeded;
}
