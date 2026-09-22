// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include <unordered_map>
#include "ArtilleryCommonTypes.h"
#include "FAttributeMap.h"
#include "InventoryEssentialTypes.h"
#include "TransformDispatch.h"
#include "Components/ActorComponent.h"
#include "ArtilleryControlComponent.generated.h"

//Base class for player, enemy, and system state machines for controlling guns, managing binds, and integrating with actors.
//Also supports GAS legacy code.
struct FArtilleryGun;
UCLASS()
class ARTILLERYRUNTIME_API UArtilleryFireControl : public UAbilitySystemComponent
{
	friend FArtilleryGun;
	GENERATED_BODY()
	
public:
	static inline int orderInInitialize = 0;
	UPROPERTY(BlueprintReadOnly)
	UArtilleryGameplayTagContainer* MyTags;
	UArtilleryDispatch* MyDispatch;
	UTransformDispatch* TransformDispatch;
	ActorKey ParentKey;
	//this needs to be replicated in iris, interestin'ly.
	TSet<FGunKey> MyGuns;

	TSharedPtr<FAttributeMap> MyAttributes;
	FireControlKey MyKey;
	bool Usable = false;

	
	//these are named by controller position for the sake of my sanity.
	UPROPERTY(BlueprintReadWrite)
	TSet<FArtilleryGunProperties> LoadoutOfGuns;
	
	virtual void PushGunToFireMapping(const FGunKey& ToFire);
	//it is strongly recommended that you understand
	// FGameplayAbilitySpec and FGameplayAbilitySpecDef, as well as Handle.
	// I'm deferring the solve for how we use them for now, in a desperate effort to
	// make sure we can preserve as much of the ability framework as possible
	// but spec management is going to be mission critical for determinism
	void FireGun(TSharedPtr<FArtilleryGun> Gun, bool InputAlreadyUsedOnce, EventBufferInfo BufferInfo);

	virtual void PopGunFromFireMapping(const FGunKey& ToRemove);

	virtual void InitializeComponent() override;;
	//this happens post init but pre begin play, and the world subsystems should exist by this point.
	//we use this to help ensure that if the actor's begin play triggers first, things will be set correctly
	//I've left the same code in begin play as a fallback.
	virtual void ReadyForReplication() override;

	//We don't have as many phases on our components as we do actors. The bool Usable helps control our state instead.
	//This is, ironically, not a problem in actual usage, only testing, for us.
	virtual void BeginPlay() override;;
	
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;;
};