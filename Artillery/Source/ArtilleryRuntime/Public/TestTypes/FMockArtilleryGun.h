 // Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include <unordered_map>
#include "FGunKey.h"
#include "GameplayEffectTypes.h"
#include "UArtilleryAbilityMinimum.h"
#include "FArtilleryGun.h"
#include "FMockArtilleryGun.generated.h"

/**
 * This class will be a data-driven instance of a gun that encapsulates a generic structured ability,
 * then exposes bindings for the phases of that ability as a component to be bound as specific gameplay abilities.
 *
 * 
 * Artillery gun is a not a UObject. This allows us to safely interact with it off the game thread.
 * Triggering the abilities is likely a no-go off the thread, but we can modify the attributes as needed.
 * This allows us to do some very powerful stuff to ensure that we always have the most up to date data.
 * Some dark things.
 *
 * Ultimately, we'll need something like https://github.com/facebook/folly/blob/main/folly/concurrency/ConcurrentHashMap.h
 * if we want to get serious about this.
 */
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FMockArtilleryGun : public FArtilleryGun 
{
	GENERATED_BODY()
	
public:
	// this can be handed into abilities.
	friend class UAGunBitBP;

	//this just sets the gunkey.
	//this mock doesn't use the delegate chaining, because it doesn't use abilities.
	FMockArtilleryGun(const FGunKey& KeyFromDispatch, UArtilleryDispatch* Dispatch)
	{
		MyGunKey = KeyFromDispatch;
	};

	virtual void PreFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile, bool VerifiedFrame = false, const EventBufferInfo FireAction = EventBufferInfo::Default())
		override
	{
	}

	virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile)
		override
	{
	}

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile) 
		override
	{
	}
	
	virtual bool Initialize(
		const FGunKey& KeyFromDispatch,
		const bool MyCodeWillHandleKeys,
		UAGunBitBP* PF = nullptr,
		UAGunBitBP* PFC = nullptr,
		UAGunBitBP* F = nullptr,
		UAGunBitBP* FC = nullptr,
		UAGunBitBP* PtF = nullptr,
		UAGunBitBP* PtFc = nullptr,
		UAGunBitBP* FFC = nullptr)
		override
	{
		return ARTGUN_MACROAUTOINIT(MyCodeWillHandleKeys);
	}

	FMockArtilleryGun()
	{
		MyGunKey = Default;
	}
	
private:
	//Our debug value remains M6D.
	static const inline FGunKey Default = FGunKey("M6D");
};