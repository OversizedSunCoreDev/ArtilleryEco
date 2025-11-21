#include "ThistleEvaluators.h"
#include "StateTreeExecutionContext.h"

static const FastExcludeBroadphaseLayerFilter BroadPhaseFilter(ExclusionFilters);
static const FastExcludeObjectLayerFilter ObjectLayerFilter(ExclusionFilters);

bool FThistleGetPlayerKey::Link(FStateTreeLinker& Linker)
{
	return true; // artillery is... always ready, weirdly.
}

void FThistleGetPlayerKey::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	//TODO: be great to have a version that doesn't go boom.
	InstanceData.OutputKey = UArtilleryLibrary::GetLocalPlayerKey_LOW_SAFETY();
}

void FThistleKeyToRelationship::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool found = false;
	InstanceData.OutputKey = UArtilleryLibrary::implK2_GetIdentity(InstanceData.InputKey, InstanceData.Relationship,found);
	InstanceData.Found = found;
}

void FThistleSphereCast::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if ((UArtilleryLibrary::GetTotalsTickCount() % InstanceData.TicksBetweenCastRefresh) == 0 &&
	UBarrageDispatch::SelfPtr)
	{
	bool Shucked = false;
	FVector Source = InstanceData.Source.ShuckPoi(Shucked);
	if (!Shucked) { return; }
	
	FVector Target = InstanceData.Target.ShuckPoi(Shucked);
	// ReSharper disable once CppRedundantControlFlowJump
	if (!Shucked) { return; }
	
	FVector ToFrom = (Target - Source);
	const double Length = ToFrom.Length() + 1;

		if (InstanceData.SourceBodyKey_SetOrRegret.IsValid()
			&& UArtilleryDispatch::SelfPtr->IsLiveKey(InstanceData.SourceBodyKey_SetOrRegret) != DEAD)
		{
			auto Bind = 	UBarrageDispatch::SelfPtr->GetShapeRef(InstanceData.SourceBodyKey_SetOrRegret);
			if (Bind)
			{
				JPH::IgnoreSingleBodyFilter StopHittingYourself = UBarrageDispatch::SelfPtr->GetFilterToIgnoreSingleBody(
					Bind->KeyIntoBarrage);
				UBarrageDispatch::SelfPtr->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
													  InstanceData.HitResultCache, BroadPhaseFilter,
													  ObjectLayerFilter, StopHittingYourself);
			}
		}
		else
		{
			UBarrageDispatch::SelfPtr->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
			                                      InstanceData.HitResultCache, BroadPhaseFilter,
			                                      ObjectLayerFilter, JPH::BodyFilter());
		}
	}
	InstanceData.Outcome = *(InstanceData.HitResultCache);
}

void FThistleDistanceToPOI::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bSourceSuccess = false;
	const FVector SourceLocation = InstanceData.Source.ShuckPoi(bSourceSuccess);

	bool bTargetSuccess = false;
	const FVector TargetLocation = InstanceData.Target.ShuckPoi(bTargetSuccess);

	if (bSourceSuccess && bTargetSuccess)
	{
		InstanceData.Distance = FVector::Distance(SourceLocation, TargetLocation);
	}
	else
	{
		// Indicate failure with a negative distance.
		InstanceData.Distance = -1.0f;
	}
}