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
	bool Shucked = false;
	FVector Source = InstanceData.Source.ShuckPoi(Shucked);
	if (!Shucked) { return; }
	
	FVector Target = InstanceData.Target.ShuckPoi(Shucked);
	// ReSharper disable once CppRedundantControlFlowJump
	if (!Shucked) { return; }
	
	FVector ToFrom = (Target - Source);
	const double Length = ToFrom.Length() + 1;
	if ((UArtilleryLibrary::GetTotalsTickCount() % InstanceData.TicksBetweenCastRefresh) == 0 &&
		UBarrageDispatch::SelfPtr)
	{
		if (InstanceData.SourceBodyKey_SetOrRegret.IsValid()
			&& UArtilleryDispatch::SelfPtr->IsLiveKey(InstanceData.SourceBodyKey_SetOrRegret) != DEAD)
		{
			JPH::IgnoreSingleBodyFilter StopHittingYourself = UBarrageDispatch::SelfPtr->GetFilterToIgnoreSingleBody(
				UBarrageDispatch::SelfPtr->GetShapeRef(InstanceData.SourceBodyKey_SetOrRegret)->KeyIntoBarrage);
			UBarrageDispatch::SelfPtr->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
			                                      InstanceData.HitResultCache, BroadPhaseFilter,
			                                      ObjectLayerFilter, StopHittingYourself);
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
