#include "FArtilleryGun.h"
#include "ArtilleryBPLibs.h"

FArtilleryGun::~FArtilleryGun()
{
	if(MyDispatch && MyDispatch->IsGunLive(MyGunKey))
	{
		MyDispatch->Deregister(MyGunKey);
	}
	
	// IsValid is a better check than nullptr for UObjects that might have been GC'd or flagged already.
	// but that flag does not include destruction lifecycle flags, which invalidate internal index.
	// Is this really necessary at this point as it seems UE is doing it's UE thing?
	if(IsValid(Prefire) && !Prefire->HasAnyFlags(EObjectFlags::RF_FinishDestroyed | EObjectFlags::RF_BeginDestroyed)) //we always assign all or none, so we can just check prefire atm. this might change.
	{
		Prefire->RemoveFromRoot();
		Fire->RemoveFromRoot();
		PostFire->RemoveFromRoot();
		PrefireCosmetic->RemoveFromRoot();
		FireCosmetic->RemoveFromRoot();
		PostFireCosmetic->RemoveFromRoot();
		FailedFireCosmetic->RemoveFromRoot();
	}
		
	MyAttributes.Reset();
}

void FArtilleryGun::PreFireGun(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                               const FGameplayAbilityActivationInfo ActivationInfo, const EventBufferInfo FireAction,
                               const FGameplayEventData* TriggerEventData, bool RerunDueToReconcile, int DallyFramesToOmit)
{
	// Delegate type:
	// DECLARE_DELEGATE_FiveParams FArtilleryAbilityStateAlert
	Prefire->GunBinder.BindRaw(this, &FArtilleryGun::FireGun, RerunDueToReconcile, TriggerEventData, Handle);
	Fire->GunBinder.BindRaw(this, &FArtilleryGun::PostFireGun, RerunDueToReconcile, TriggerEventData, Handle);
	Prefire->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo, nullptr, TriggerEventData);
	if (!RerunDueToReconcile)
	{
		PrefireCosmetic->CallActivateAbility(Handle, ActorInfo, ActivationInfo, nullptr, TriggerEventData);
	}
}

void FArtilleryGun::FireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit,
                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                            bool RerunDueToReconcile, const FGameplayEventData* TriggerEventData, FGameplayAbilitySpecHandle Handle)
{
	if(!ReadyToFire)
	{
		return; //your gun is broken. if you don't like this, override this function.
	}
		
	if (OutcomeStates == FArtilleryStates::Fired)
	{
		Fire->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo, nullptr,
		                          TriggerEventData);
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		if (!RerunDueToReconcile)
		{
			//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
			FireCosmetic->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo, nullptr,
			                                  TriggerEventData);
		}
	}
	else if (!RerunDueToReconcile)
	{
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		FailedFireCosmetic->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo,
		                                        nullptr, TriggerEventData);
	}
}

void FArtilleryGun::PostFireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit,
                                const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                bool RerunDueToReconcile, const FGameplayEventData* TriggerEventData, FGameplayAbilitySpecHandle Handle)
{
	if (OutcomeStates == FArtilleryStates::Fired)
	{
		PostFire->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo, nullptr,
		                              TriggerEventData);
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		if (!RerunDueToReconcile)
		{
			//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
			PostFireCosmetic->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo, nullptr,
			                                      TriggerEventData);
		}
	}
	else if (!RerunDueToReconcile)
	{
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		FailedFireCosmetic->CallActivateAbility(FGameplayAbilitySpecHandle(), ActorInfo, ActivationInfo,
		                                        nullptr, TriggerEventData);
	}
}

bool FArtilleryGun::Initialize(const FGunKey& KeyFromDispatch, const bool MyCodeWillSetGunKey,
                               UArtilleryPerActorAbilityMinimum* PF, UArtilleryPerActorAbilityMinimum* PFC, UArtilleryPerActorAbilityMinimum* F,
                               UArtilleryPerActorAbilityMinimum* FC, UArtilleryPerActorAbilityMinimum* PtF, UArtilleryPerActorAbilityMinimum* PtFc,
                               UArtilleryPerActorAbilityMinimum* FFC)
{
	MyGunKey = KeyFromDispatch;
		
	MyDispatch = UArtilleryLibrary::GetArtilleryDispatch_LowSafety();
	MyTransformDispatch = MyDispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
	MyProjectileDispatch = MyDispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();

	TMap<AttribKey, double> InitialGunAttributes = TMap<AttribKey, double>();
	// TODO: load more stats and dynamically rather than fixed demo values
	InitialGunAttributes.Add(AMMO, MaxAmmo);
	InitialGunAttributes.Add(MAX_AMMO, MaxAmmo);
	InitialGunAttributes.Add(COOLDOWN, Firerate);
	InitialGunAttributes.Add(COOLDOWN_REMAINING, 0);
	InitialGunAttributes.Add(RELOAD, ReloadTime);
	InitialGunAttributes.Add(RELOAD_REMAINING, 0);
	InitialGunAttributes.Add(TICKS_SINCE_GUN_LAST_FIRED, 0);
	InitialGunAttributes.Add(AttribKey::LastFiredTimestamp, 0);
	InitialGunAttributes.Add(TRIGGER_PULLED, 0);
	MyAttributes = MakeShareable(new FAttributeMap(MyGunKey, MyDispatch, InitialGunAttributes));
		
	UTransformDispatch* TransformDispatch = MyDispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
	TWeakObjectPtr<AActor> ActorPointer = TransformDispatch->GetAActorByObjectKey(MyProbableOwner);
	if (ActorPointer.IsValid() && UArtilleryLibrary::GetLocalPlayer_UNSAFE() == ActorPointer && ActorPointer != nullptr)
	{
		PlayerCameraComponent = ActorPointer->GetComponentByClass<UCameraComponent>();
	}
	else if (!ActorPointer.IsValid())
	{
		return false;
	}
		
	FiringPointComponent = Cast<USceneComponent, UObject>(ActorPointer->GetDefaultSubobjectByName(TEXT("BeamFiringPoint")));
	FiringPointComponentKey = MAKE_BONEKEY(&FiringPointComponent);
	TransformDispatch->RegisterSceneCompToShadowTransform(FiringPointComponentKey, FiringPointComponent.Get());
		
	//we'd like to do it earlier, but there's actually not a great moment to do this.
	if(Prefire == nullptr)
	{
		Prefire = PF ? PF : NewObject<UArtilleryPerActorAbilityMinimum>();
		Prefire->AddToRoot();
		Fire = F ? F :	NewObject<UArtilleryPerActorAbilityMinimum>();
		Fire->AddToRoot();
		PostFire = PtF ? PtF : NewObject<UArtilleryPerActorAbilityMinimum>();
		PostFire->AddToRoot();
			
		PrefireCosmetic  = PFC ? PFC : NewObject<UArtilleryPerActorAbilityMinimum>();
		PrefireCosmetic->AddToRoot();
		FireCosmetic = FC ? FC : NewObject<UArtilleryPerActorAbilityMinimum>();
		FireCosmetic->AddToRoot();
		PostFireCosmetic = PtFc ? PtFc : NewObject<UArtilleryPerActorAbilityMinimum>();
		PostFireCosmetic->AddToRoot();
		FailedFireCosmetic = FFC ? FFC : NewObject<UArtilleryPerActorAbilityMinimum>();
		FailedFireCosmetic->AddToRoot();
	}
		
	if(!MyCodeWillSetGunKey)
	{
		SetGunKey(MyGunKey);
	}
		
	MyDispatch->REGISTER_GUN_FINAL_TICK_RESOLVER(MyGunKey, this);
	ReadyToFire = ReadyToFire || !MyCodeWillSetGunKey;
	return ReadyToFire;
}

void FArtilleryGun::SetGunKey(FGunKey NewKey)
{
	MyGunKey = NewKey;
	Prefire->MyGunKey = MyGunKey;
	PrefireCosmetic->MyGunKey = MyGunKey;
	Fire->MyGunKey = MyGunKey;
	FireCosmetic->MyGunKey = MyGunKey;
	PostFire->MyGunKey = MyGunKey;
	PostFireCosmetic->MyGunKey = MyGunKey;
	FailedFireCosmetic->MyGunKey = MyGunKey;
}

FArtilleryGun::FArtilleryGun(): MyDispatch(nullptr), MyTransformDispatch(nullptr), MyProjectileDispatch(nullptr)
{
	MyGunKey = DefaultGunKey;
}

void FArtilleryGun::ProjectileCollided(const FSkeletonKey ProjectileKey, const FSkeletonKey HitEntity)
{
	FVector SourceLoc = FVector::ZeroVector;
	if (MyTransformDispatch)
	{
		// Use the Owner location as the source of damage
		TOptional<FTransform> OwnerTransform = MyTransformDispatch->CopyOfTransformByObjectKey(MyProbableOwner);
		if (OwnerTransform.IsSet())
		{
			SourceLoc = OwnerTransform.GetValue().GetLocation();
		}
	}

	UArtilleryLibrary::ApplyDamage(HitEntity, 100);
}
