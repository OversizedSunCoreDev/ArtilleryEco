#include "ThistleStateTreeConditions.h"

#include "ArtilleryBPLibs.h"
#include "ArtilleryDispatch.h"
#include "FWorldSimOwner.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ThistleStateTreeConditions)

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "StateTreeEditor"
#endif// WITH_EDITOR

//----------------------------------------------------------------------//
//  GameplayTagMatchCondition
//----------------------------------------------------------------------//

bool FArtilleryTagMatchCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool found = false;
	FConservedTags Container = UArtilleryLibrary::InternalTagsByKey(UArtilleryDispatch::Get(Context.GetWorld()),  InstanceData.KeyOf, found); //with the newer conserved tags, we COULD save this off..
	//TODO: add "has tag" support, not just has exact tag support.
	//return (bExactMatch ?  Container->Find(InstanceData.Tag) : Container->HasTag(InstanceData.Tag)) ^ bInvert;
	return found && Container->Find(InstanceData.Tag) ^ bInvert;
}

bool FArtilleryAttributeValueCondition::Test(float Value, float Target) const
{
	return TreeOperandTest(Value, Target, Operation);
}

bool FArtilleryAttributeValueCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	float Value;
	bool Found = UArtilleryLibrary::GetAttribute(UArtilleryDispatch::Get(Context.GetWorld()), InstanceData.KeyOf, InstanceData.AttributeName, Value);
	return Found && Test(Value, TestValue);
}

bool FArtilleryAttributeCompareCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	float Value;
	if (UArtilleryLibrary::GetAttribute(UArtilleryDispatch::Get(Context.GetWorld()), InstanceData.KeyOf, InstanceData.AttributeName, Value))
	{
		float TestAttrib;
		if (UArtilleryLibrary::GetAttribute(UArtilleryDispatch::Get(Context.GetWorld()), TestKey, InstanceData.AttributeName, TestAttrib))
		{
			return Test(Value, TestAttrib);
		}
		if (bFallbackToTestValue)
		{
			return Test(Value, TestValue);
		}
	}

	return false;
}


bool FArtilleryCompareRelatedCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool RelatedKey_AttributeFound = false;
	bool RelatedKey_FoundAtAll = false;

	auto ArtilleryDispatch = UArtilleryDispatch::Get(Context.GetWorld());

	auto Identity = UArtilleryLibrary::GetIdentity(ArtilleryDispatch, InstanceData.KeyOf, Relationship, RelatedKey_FoundAtAll);
	if (RelatedKey_AttributeFound)
	{
		float TestAttribValue;
		if (UArtilleryLibrary::GetAttribute(ArtilleryDispatch, Identity, InstanceData.AttributeName, TestAttribValue))
		{
			float SourceValue;
			bool SourceKey_AttributeFound = UArtilleryLibrary::GetAttribute(ArtilleryDispatch, InstanceData.KeyOf, InstanceData.AttributeName, SourceValue);
			if (bCompareWithTargetKeyAttribute && SourceKey_AttributeFound)
			{
				return Test(TestAttribValue, SourceValue);
			}
			return Test(TestAttribValue, TestValue);
		}
	}
	return false;
}

bool FArtilleryCompareKeys::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (	!InstanceData.SourceKey.IsValid()
		||	!InstanceData.TargetKey.IsValid()
		||	InstanceData.SourceKey != InstanceData.TargetKey)
	{
		return false;
	}
	return true;
}

bool FCheckLoStoPoI::TestCondition(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto Barrage = UBarrageDispatch::Get(Context.GetWorld());
	auto ArtilleryDispatch = UArtilleryDispatch::Get(Context.GetWorld());
	bool Shucked = false;
	auto Source = InstanceData.Source.ShuckPoi(Context,Shucked);
	if (!Shucked) { return false; }
	auto Target = InstanceData.Target.ShuckPoi(Context,Shucked);
	// ReSharper disable once CppRedundantControlFlowJump
	if (!Shucked) { return false; }
	auto ToFrom = (Target - Source);
	auto Length = ToFrom.Length() + 1;
	if ((UArtilleryLibrary::GetTotalsTickCount(UArtilleryDispatch::Get(Context.GetWorld())) % InstanceData.TicksBetweenCastRefresh) == 0 &&
		Barrage)
	{
		const JPH::DefaultBroadPhaseLayerFilter default_broadphase_layer_filter = Barrage->JoltGameSim
			->physics_system->GetDefaultBroadPhaseLayerFilter(Layers::CAST_QUERY);
		const JPH::DefaultObjectLayerFilter default_object_layer_filter = Barrage->JoltGameSim->
			physics_system->GetDefaultLayerFilter(Layers::CAST_QUERY);

		if (InstanceData.SourceBodyKey_SetOrRegret.IsValid()
			&& ArtilleryDispatch->IsLiveKey(InstanceData.SourceBodyKey_SetOrRegret) != DEAD)
		{
			JPH::IgnoreSingleBodyFilter StopHittingYourself = Barrage->GetFilterToIgnoreSingleBody(
				Barrage->GetShapeRef(InstanceData.SourceBodyKey_SetOrRegret)->KeyIntoBarrage);
			Barrage->SphereCast(InstanceData.Radius, Length, Source, ToFrom.GetSafeNormal(),
												  InstanceData.HitResultCache, default_broadphase_layer_filter,
												  default_object_layer_filter, StopHittingYourself);
			
			InstanceData.Outcome = *(InstanceData.HitResultCache);
			bool KeyExist = InstanceData.Target.PointOfInterestKey.IsValid();
			bool HitTarget = false;
			bool HitClose = false;
			if (KeyExist)
			{
				if (InstanceData.Outcome.MyItem != JPH::BodyID::cInvalidBodyID)
				{
					FBarrageKey HitBarrageKey = Barrage->GenerateBarrageKeyFromBodyId(
					static_cast<uint32>(InstanceData.Outcome.MyItem));
					FBLet HitObjectFiblet = Barrage->GetShapeRef(HitBarrageKey);
					if (HitObjectFiblet && HitObjectFiblet->KeyOutOfBarrage == InstanceData.Target.PointOfInterestKey)
					{
						HitTarget = true;
					}
				}
			}
			if (Target.Equals(InstanceData.Outcome.ImpactPoint, DistanceFromTargetToTolerate))
			{
				HitClose = true;
			}
			return HitTarget || HitClose;
		}
	}
	else
	{
		return false;
	}
	return false;
}

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR

