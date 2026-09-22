#include "ThistleStateTreeFire.h"

EStateTreeRunStatus FFireTurret::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UBarrageDispatch* AreWeBarraging = UBarrageDispatch::Get(Context.GetWorld());
	auto ArtilleryDispatch = UArtilleryDispatch::Get(Context.GetWorld());
	if (AreWeBarraging != nullptr)
	{
		const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		bool found = true;
		UArtilleryLibrary::GetLocation(ArtilleryDispatch, InstanceData.KeyOf, found); // used as an existence check, sue me.
		if (found)
		{
			UThistleBehavioralist::AttemptAttackFromKey(UThistleBehavioralist::Get(Context.GetWorld()), InstanceData.KeyOf);
			return EStateTreeRunStatus::Succeeded;
		}
	}
	return EStateTreeRunStatus::Failed;
}
