#include "ThistleEvaluators.h"
#include "StateTreeExecutionContext.h"
#include "PhysicsFilters/FastBroadphaseLayerFilter.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"
#include "PhysicsTypes/BarragePlayerAgent.h"

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
	InstanceData.OutputKey = UArtilleryLibrary::GetLocalPlayerKey_LOW_SAFETY(UCanonicalInputStreamECS::Get(Context.GetWorld()));
}

void FThistleKeyToRelationship::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool found = false;
	InstanceData.OutputKey = UArtilleryLibrary::GetIdentity(UArtilleryDispatch::Get(Context.GetWorld()), InstanceData.InputKey, InstanceData.Relationship,found);
	InstanceData.Found = found;
}

void FThistleSphereCast::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto Barrage = Context.GetWorld()->GetSubsystem<UBarrageDispatch>();
	auto Artillery = Context.GetWorld()->GetSubsystem<UArtilleryDispatch>();
	if ((UArtilleryLibrary::GetTotalsTickCount(Artillery) % InstanceData.TicksBetweenCastRefresh) == 0 &&
	Barrage)
	{
	bool Shucked = false;
	FVector Source = InstanceData.Source.ShuckPoi(Context,Shucked);
	if (!Shucked) { return; }
	
	FVector Target = InstanceData.Target.ShuckPoi(Context, Shucked);
	// ReSharper disable once CppRedundantControlFlowJump
	if (!Shucked) { return; }
	
	FVector ToFrom = (Target - Source);
	const double Length = ToFrom.Length() + 1;

		if (InstanceData.SourceBodyKey_SetOrRegret.IsValid()
			&& Artillery->IsLiveKey(InstanceData.SourceBodyKey_SetOrRegret) != DEAD)
		{
			auto Bind = 	Barrage->GetShapeRef(InstanceData.SourceBodyKey_SetOrRegret);
			if (Bind)
			{
				JPH::IgnoreSingleBodyFilter StopHittingYourself = Barrage->GetFilterToIgnoreSingleBody(
					Bind->KeyIntoBarrage);
				Barrage->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
													  InstanceData.HitResultCache, BroadPhaseFilter,
													  ObjectLayerFilter, StopHittingYourself);
			}
		}
		else
		{
			Barrage->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
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
	const FVector SourceLocation = InstanceData.Source.ShuckPoi(Context,bSourceSuccess);

	bool bTargetSuccess = false;
	const FVector TargetLocation = InstanceData.Target.ShuckPoi(Context,bTargetSuccess);

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