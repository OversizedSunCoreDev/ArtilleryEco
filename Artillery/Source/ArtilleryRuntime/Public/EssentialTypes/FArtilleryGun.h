// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ArtilleryCommonTypes.h"
#include "ArtilleryProjectileDispatch.h"
#include "FAttributeMap.h"
#include "FGunKey.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "UArtilleryAbilityMinimum.h"
#include "Camera/CameraComponent.h"
#include "Templates/TypeHash.h"
#include "FArtilleryGun.generated.h"

//Will be used in conjunction with the gun definition rows to set up player guns.
//currently just testing the ergonomics of it.
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FArtilleryGunProperties
{
	GENERATED_BODY()
	friend uint32 GetTypeHash(const FArtilleryGunProperties& Arg)
	{
		uint32 Hash = HashCombine(GetTypeHash(Arg.Intent), GetTypeHash(Arg.GunKey));
		return Hash;
	}

	UPROPERTY(BlueprintReadWrite)
	E_ArtilleryIntents Intent;
	UPROPERTY(BlueprintReadWrite)
	FGunKey GunKey;
	UPROPERTY(BlueprintReadWrite)
	TMap<E_AttribKey, float> Attributes;
	UPROPERTY(BlueprintReadWrite)
	TMap<E_VectorAttrib, FVector> VectorAttributes;
};
	

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
	friend class UAGunBitBP;
	UPROPERTY(BlueprintReadOnly)
	FGunKey MyGunKey;
	UPROPERTY(BlueprintReadOnly)
	FSkeletonKey MyProbableOwner;
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
	TObjectPtr<UAGunBitBP> Prefire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> PrefireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> FireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> PostFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> PostFireCosmetic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAGunBitBP> FailedFireCosmetic;

	//we use the GunBinder delegate to link the MECHANICAL abilities to phases.
	//cosmetics don't get linked the same way.
	FArtilleryGun(const FGunKey& KeyFromDispatch, UArtilleryDispatch* Dispatch)
	{
		MyDispatch = Dispatch;
		MyProjectileDispatch = nullptr;
		MyTransformDispatch = nullptr;
		MyGunKey = KeyFromDispatch;
		MyTransformDispatch = MyDispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
		MyProjectileDispatch = MyDispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
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
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile, bool VerifiedFrame = false, const EventBufferInfo FireAction = EventBufferInfo::Default());


	/*
	 * This fires the gun when the prefire ability succeeds.
	 * It will be tempting to reorder the parameters. Don't do this.
	 * Again, this is a delegate function and the parametric order is what enables payloading.
	 */
	virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile);

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile);;

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
		UAGunBitBP* PF = nullptr,
		UAGunBitBP* PFC = nullptr,
		UAGunBitBP* F = nullptr,
		UAGunBitBP* FC = nullptr,
		UAGunBitBP* PtF = nullptr,
		UAGunBitBP* PtFc = nullptr,
		UAGunBitBP* FFC = nullptr);

	void SetGunKey(FGunKey NewKey);

	FArtilleryGun();

	//TODO: Refactor to take hit-entity key as well.
	virtual void ProjectileCollided(const FSkeletonKey ProjectileKey, const FSkeletonKey HitEntity);
};
