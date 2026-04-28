#pragma once
#include "ArtilleryBPLibs.h"
#include "ModularGameplayTags.h"
#include "ArtilleryECSOnlyArtilleryTickable.h"

struct FPassthroughAttribute : public FTickECSOnly
{
	using ConditionFunction = std::function<bool(const FSkeletonKey, const FSkeletonKey)>;
	ConditionFunction ApplyEligible;
	FSkeletonKey MyParentObjectKey;
	TArray<AttribKey> AttributesToPass;
	bool clear = false;

	FPassthroughAttribute(const FSkeletonKey TargetIn,
	                      const FSkeletonKey MyParentObjectKey,
	                      const TArray<AttribKey> AttributesToPass,
	                      const ConditionFunction ApplyEligible)
		: FTickECSOnly(TargetIn),
		  ApplyEligible(ApplyEligible),
		  MyParentObjectKey(MyParentObjectKey),
		  AttributesToPass(AttributesToPass)
	{
	}

	FPassthroughAttribute() = default;

	virtual void ArtilleryTick(uint64_t TicksSoFar) override
	{
		if (ApplyEligible(Target, MyParentObjectKey))
		{
			for (auto Attr : AttributesToPass)
			{
				auto From = ADispatch->GetAttrib(Target, Attr);
				auto To = ADispatch->GetAttrib(MyParentObjectKey, Attr);

				if (From && From.IsValid() && To && To.IsValid())
				{
					To->SetCurrentValue(From->GetCurrentValue());
					if (clear)
					{
						From->SetCurrentValue(0.0);
					}
				}
			}
		}
	}

	static void CreatePassthrough(FSkeletonKey me, FSkeletonKey destination, TArray<AttribKey> attribs,
	                              ConditionFunction condition);
};

typedef FPassthroughAttribute FPTAttr;
typedef Ticklites::Ticklite<FPTAttr> FPTAttr_TL;

//TODO: this won't work for local multi testing, as the static will get set incorrectly despite being compile specified per type. the PIE
//spins up a bunch of IN PROCESS COPIES so it just scrobbles everything.
inline void FPassthroughAttribute::CreatePassthrough(FSkeletonKey me, FSkeletonKey destination,
                                                     TArray<AttribKey> attribs, ConditionFunction condition)
{
	ADispatch->RequestAddTicklite(new FPTAttr_TL(FPTAttr(me, destination, attribs, condition)),
	                                            PASS_THROUGH);
}


struct FPassDamage : public FTickECSOnly
{
	typedef std::function<bool(const FSkeletonKey, const FSkeletonKey)> ConditionFunction;
	FSkeletonKey MyParentObjectKey;
	AttribKey CheckThis = E_AttribKey::Health;
	float AgainstThisThreshold = 0.0;
	bool clear = true;

	FPassDamage(const FSkeletonKey TargetIn, const FSkeletonKey MyParentObjectKey, AttribKey CheckThis,
	            float AgainstThisThreshold, bool bClear)
		: FTickECSOnly(TargetIn),
		  MyParentObjectKey(MyParentObjectKey),
		  CheckThis(CheckThis),
		  AgainstThisThreshold(AgainstThisThreshold),
		  clear(bClear)
	{
	}

	FPassDamage() = default;

	virtual void ArtilleryTick(uint64_t TicksSoFar) override
	{
		auto From = ADispatch->GetAttrib(Target, AttribKey::ProposedDamage);
		auto To = ADispatch->GetAttrib(MyParentObjectKey, E_AttribKey::ProposedDamage);
		auto Check = ADispatch->GetAttrib(Target, CheckThis);
		if (Check && Check.IsValid() && From && From.IsValid() && To && To.IsValid() && Check->GetCurrentValue() ==
			AgainstThisThreshold)
		{
			To->SetCurrentValue(To->GetCurrentValue() + From->GetCurrentValue());
			if (clear)
			{
				From->SetCurrentValue(0.0);
			}
		}
	}

	static void CreatePassthrough(FSkeletonKey me, FSkeletonKey destination, AttribKey Check = E_AttribKey::Health,
	                              float Threshold = 0.0, bool Clear = true);
};

typedef Ticklites::Ticklite<FPassDamage> FPTDam_TL;

//TODO: this won't work for local multi testing, as the static will get set incorrectly despite being compile specified per type. the PIE
//spins up a bunch of IN PROCESS COPIES so it just scrobbles everything.
inline void FPassDamage::CreatePassthrough(FSkeletonKey me, FSkeletonKey destination, AttribKey Check, float Threshold,
                                           bool Clear)
{
	ADispatch->RequestAddTicklite( new FPTDam_TL(FPassDamage(me, destination, Check, Threshold, Clear)),
	                              PASS_THROUGH);
}


struct FForwardDamageEvent : public FTickECSOnly
{
	FSkeletonKey MyParentObjectKey;

	FForwardDamageEvent(const FSkeletonKey TargetIn, const FSkeletonKey ParentKey)
		: FTickECSOnly(TargetIn),
		MyParentObjectKey(ParentKey)
	{
	}

	FForwardDamageEvent() = default;

	virtual void ArtilleryTick(uint64_t TicksSoFar) override
	{
		// Check if the Shield (Target) has the Damaged Tag
		if (ADispatch->DispatchOwner->DoesEntityHaveTag(Target, GameplayEvent_Damaged))
		{
			// Forward the Tag to the Parent AI
			// This ensures the Behavioralist picks up the event on the AI key
			ADispatch->DispatchOwner->AddTagToEntity(MyParentObjectKey, GameplayEvent_Damaged);

			// Forward the Damage Source Vector
			// This ensures the AI rotates towards what hit the shield
			Attr3Ptr ShieldSource = ADispatch->GetVecAttrib(Target, E_VectorAttrib::TargetLocation);
			Attr3Ptr ParentSource = ADispatch->GetVecAttrib(MyParentObjectKey, E_VectorAttrib::TargetLocation);

			if (ShieldSource && ParentSource)
			{
				ParentSource->SetCurrentValue(ShieldSource->CurrentValue);
			}

			// Consume the tag
			ADispatch->DispatchOwner->RemoveTagFromEntity(Target, GameplayEvent_Damaged);
		}
	}

	static void CreateForwarder(FSkeletonKey ShieldKey, FSkeletonKey ParentKey);
};

typedef Ticklites::Ticklite<FForwardDamageEvent> FForwardDmg_TL;

inline void FForwardDamageEvent::CreateForwarder(FSkeletonKey ShieldKey, FSkeletonKey ParentKey)
{
	// Run in normal phase to ensure we catch tags set by Resolvers in previous ticks/phases
	ADispatch->RequestAddTicklite(new FForwardDmg_TL(FForwardDamageEvent(ShieldKey, ParentKey)), Normal);
}