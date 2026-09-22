// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <unordered_map>

#include "GameplayEffectTypes.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpecHandle.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "FGunKey.h"

#include "GameplayTaskOwnerInterface.h"
#include "GameplayTask.h"
#include "Engine/Blueprint.h"
#include "UArtilleryAbilityMinimum.generated.h"

enum FArtilleryStates
{
	Fired, Canceled, CanceledAfterCommit
};

DECLARE_DELEGATE_FourParams(FArtilleryAbilityStateAlert, FArtilleryStates, int, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo);

UCLASS(BlueprintType)
class ARTILLERYRUNTIME_API UAGunBitBP : public UBlueprint
{
	GENERATED_BODY()

	friend struct FArtilleryGun;
	
public:
	//As you can see, they all call through to commit ability.

	FArtilleryAbilityStateAlert GunBinder;
	//ALMOST EVERYTHING THAT IS INTERESTING HAPPENS HERE RIGHT NOW.
	//ONLY ATTRIBUTES ARE REPLICATED. _AGAIN_. ONLY ATTRIBUTES ARE REPLICATED.
	UAGunBitBP(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer), MyGunKey()
	{
	};
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Latency Hiding")
	int AvailableDallyFrames = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gun")
	FGunKey MyGunKey;
	
	//InstancingPolicy is ALWAYS EGameplayAbilityInstancingPolicy::NonInstanced for artillery abilities.
	//Storing state outside of tags and game simulation attributes will not be replicated and will cause bugs during rollback.
	//Only implementation graphs in THIS function are called by artillery. Anything else will be ignored.
	//You MUST call end ability and commit ability as appropriate, or execution will not continue.
	//Prefire should use commit\end vs. cancel to signal if execution should continue, but all abilities can.
	UFUNCTION(BlueprintNativeEvent, Category = Ability, DisplayName = "Artillery Ability Implementation", meta=(ScriptName = "ArtilleryActivation"))
	void K2_ActivateViaArtillery( const FGunKey& MyGun);

	//Default behavior, override to use C++!
	virtual void K2_ActivateViaArtillery_Implementation( const FGunKey& MyGun);
};
