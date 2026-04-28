// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

#include "ArtilleryCommonTypes.h"
#include "ArtilleryProjectileDispatch.h"
#include "FAttributeMap.h"
#include "FGunKey.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "UArtilleryAbilityMinimum.h"
#include "Camera/CameraComponent.h"
#include "FArtilleryGun.generated.h"

/**
 * * GUNS MUST BE INITIALIZED. This is handled in the various loaders and builders, but any unique gun MUST be initialized.
 * This class will be a data-driven instance of a gun that encapsulates a generic structured ability,
 * then exposes bindings for the phases of that ability as a component to be bound as specific gameplay abilities.
 *
 * 
 * Artillery gun is a not a UObject. This allows us to safely interact with it off the game thread TO AN EXTENT.
 *
 * Ultimately, we'll need something like https://github.com/facebook/folly/blob/main/folly/concurrency/ConcurrentHashMap.h
 * if we want to get serious about this.
 *
 */
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FArtilleryGun
{
	GENERATED_BODY()
	
public:
	// this can be handed into abilities.
	friend class UArtilleryPerActorAbilityMinimum;
	FGunKey MyGunKey;
	ActorKey MyProbableOwner;
	bool ReadyToFire = false;
	
	UArtilleryDispatch* MyDispatch;
	UTransformDispatch* MyTransformDispatch;
	UArtilleryProjectileDispatch* MyProjectileDispatch;
	TSharedPtr<FAttributeMap> MyAttributes;
	
	virtual FString LookInward() { return "FArtilleryGun"; }
	
	// Owner Components
	TWeakObjectPtr<UCameraComponent> PlayerCameraComponent;
	TWeakObjectPtr<USceneComponent> FiringPointComponent;
	FBoneKey FiringPointComponentKey;
	
	// 0 MaxAmmo = No Ammo system required
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int MaxAmmo = 30;
	// Frames to cooldown and fire again
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int Firerate = 50;
	// Frames to reload
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int ReloadTime = 120;

	//As these are UProperties, they should NOT need to become strong pointers or get attached to root
	//to _exist_ when created off main thread, but that doesn't solve the bulk of the issues and the guarantee
	//hasn't held up as well as I would like.
	
	//these need to be added to the rootset to prevent GC erasure. UProperty isn't enough alone, as this class
	//is not GC reachable. This means that as soon as the reference expires and the sweep completes, as was, you'll
	//get an error.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> Prefire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> PrefireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> FireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> PostFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> PostFireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UArtilleryPerActorAbilityMinimum> FailedFireCosmetic;

	//we use the GunBinder delegate to link the MECHANICAL abilities to phases.
	//cosmetics don't get linked the same way.
	FArtilleryGun(const FGunKey& KeyFromDispatch)
	{
		MyDispatch = nullptr;
		MyProjectileDispatch = nullptr;
		MyTransformDispatch = nullptr;
		MyGunKey = KeyFromDispatch;
	};

	virtual ~FArtilleryGun();

	void UpdateProbableOwner(ActorKey ProbableOwner)
	{
		MyProbableOwner = ProbableOwner;
	}
	
	//I'm sick and tired of the endless layers of abstraction.
	//Here's how it works. we fire the abilities from the gun.
	//OnGameplayAbilityEnded doesn't actually let you know if the ability was canceled.
	//That's... not so good. We use OnGameplayAbilityEndedWithData instead.
	virtual void PreFireGun(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const EventBufferInfo FireAction = EventBufferInfo::Default(),
		const FGameplayEventData* TriggerEventData = nullptr,
		bool RerunDueToReconcile = false,
		int DallyFramesToOmit = 0);

	/**************************************
	 *the following are delegates for Ability Minimum.
	 * We could likely use this with some cleverness to avoid object alloc while still getting
	 * per instance behavior but atm, that's not something I'm building.
	 * Dally frames don't work yet. But they will. Be ready.
	 *
	 * These are fired during end ability.
	 *****************************************
	 */

	/*
	 * This fires the gun when the prefire ability succeeds.
	 * It will be tempting to reorder the parameters. Don't do this.
	 * Again, this is a delegate function and the parametric order is what enables payloading.
	 */
	virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle);;

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle);;

	//The unusual presence of the modal switch AND a requirement for the related parameter is due to the
	//various fun vagaries of inheritance. IF you override this function, and any valid child class should,
	//then you'll want to have some assurance of fine-grained control there over all your parent classes.
	//for a variety of reasons, a gunkey might not be null, but might not be usable or desirable.
	//please ensure your child classes respect this as well. thank you!
	//returns readytofire
#define ARTGUN_MACROAUTOINIT(MyCodeWillHandleKeys) Super::Initialize(KeyFromDispatch, MyCodeWillHandleKeys, PF, PFC,F,FC,PtF,PtFc,FFC)
	virtual bool Initialize(
		const FGunKey& KeyFromDispatch,
		const bool MyCodeWillSetGunKey,
		UArtilleryPerActorAbilityMinimum* PF = nullptr,
		UArtilleryPerActorAbilityMinimum* PFC = nullptr,
		UArtilleryPerActorAbilityMinimum* F = nullptr,
		UArtilleryPerActorAbilityMinimum* FC = nullptr,
		UArtilleryPerActorAbilityMinimum* PtF = nullptr,
		UArtilleryPerActorAbilityMinimum* PtFc = nullptr,
		UArtilleryPerActorAbilityMinimum* FFC = nullptr);

	void SetGunKey(FGunKey NewKey);

	FArtilleryGun();

	//TODO: Refactor to take hit-entity key as well.
	virtual void ProjectileCollided(const FSkeletonKey ProjectileKey, const FSkeletonKey HitEntity);
};
