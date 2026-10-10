#include "FArtilleryGun.h"
#include "ArtilleryBPLibs.h"


FArtilleryGun::FArtilleryGun(const FGunKey& KeyFromDispatch, UArtilleryDispatch* Dispatch)
{
	MyDispatch = Dispatch;
	MyProjectileDispatch = nullptr;
	MyTransformDispatch = nullptr;
	MyGunKey = KeyFromDispatch;
	MyTransformDispatch = MyDispatch->GetWorld()->GetSubsystem<UTransformDispatch>();
	MyProjectileDispatch = MyDispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
}

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

void FArtilleryGun::PreFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile,
		bool VerifiedFrame, const EventBufferInfo FireAction) 
{
	// Delegate type:
	// DECLARE_DELEGATE_FiveParams FArtilleryAbilityStateAlert
	Prefire->K2_ActivateViaArtillery(MyGunKey);
	if (!RerunDueToReconcile)
	{
		PrefireCosmetic->K2_ActivateViaArtillery(MyGunKey);
	}
}

void FArtilleryGun::FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile)
{
	if(!ReadyToFire)
	{
		return; //your gun is broken. if you don't like this, override this function.
	}
	
	// Confirmed we can fire, 
	ArtilleryTime now = MyDispatch->GetShadowNow();
	AttrMapPtr AttrMap = MyDispatch->GetAttribMap(MyGunKey);
	AttrPtr StartFireTimePtr = AttrMap->FindRef(AttribKey::StartFiredTimestamp);
	if (now >= StartFireTimePtr->GetCurrentValue() + AttrMap->FindRef(AttribKey::RefireTickCount)->GetCurrentValue())
	{
		StartFireTimePtr->SetCurrentValue(MyDispatch->GetShadowNow());
	}
		
	if (OutcomeStates == FArtilleryStates::Fired)
	{
		Fire->K2_ActivateViaArtillery(MyGunKey);
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		if (!RerunDueToReconcile)
		{
			//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
			FireCosmetic->K2_ActivateViaArtillery(MyGunKey);
		}
	}
	else if (!RerunDueToReconcile)
	{
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		FailedFireCosmetic->K2_ActivateViaArtillery(MyGunKey);
	}
}

void FArtilleryGun::PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile)
{
	if (OutcomeStates == FArtilleryStates::Fired)
	{
		PostFire->K2_ActivateViaArtillery(MyGunKey);
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		if (!RerunDueToReconcile)
		{
			//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
			PostFireCosmetic->K2_ActivateViaArtillery(MyGunKey);
		}
	}
	else if (!RerunDueToReconcile)
	{
		//TODO: BUILD CORRECT HANDLE HANDLING. HANDLES ARE OUR TICKET OUT OF THIS JOINT.
		FailedFireCosmetic->K2_ActivateViaArtillery(MyGunKey);
	}
}

bool FArtilleryGun::Initialize(const FGunKey& KeyFromDispatch, const bool MyCodeWillSetGunKey,
                               UAGunBitBP* PF, UAGunBitBP* PFC, UAGunBitBP* F,
                               UAGunBitBP* FC, UAGunBitBP* PtF, UAGunBitBP* PtFc,
                               UAGunBitBP* FFC)
{
	MyGunKey = KeyFromDispatch;
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
	InitialGunAttributes.Add(AttribKey::RefireTickCount, 12);
	InitialGunAttributes.Add(AttribKey::StartFiredTimestamp, 0);
	InitialGunAttributes.Add(AttribKey::LastFiredTimestamp, 0);
	InitialGunAttributes.Add(TRIGGER_PULLED, 0);
	MyAttributes = MakeShareable(new FAttributeMap(MyGunKey, MyDispatch, InitialGunAttributes));
	
	TWeakObjectPtr<AActor> ActorPointer = MyTransformDispatch->GetAActorByObjectKey(MyProbableOwner);
	if (ActorPointer.IsValid() && UArtilleryLibrary::GetLocalPlayer_UNSAFE(MyDispatch) == ActorPointer && ActorPointer != nullptr)
	{
		PlayerCameraComponent = ActorPointer->GetComponentByClass<UCameraComponent>();
	}
	else if (!ActorPointer.IsValid())
	{
		return false;
	}
		
	FiringPointComponent = Cast<USceneComponent, UObject>(ActorPointer->GetDefaultSubobjectByName(TEXT("BeamFiringPoint")));
	FiringPointComponentKey = MAKE_BONEKEY(&FiringPointComponent);
	MyTransformDispatch->RegisterSceneCompToShadowTransform(FiringPointComponentKey, FiringPointComponent.Get());
		
	//we'd like to do it earlier, but there's actually not a great moment to do this.
	if(Prefire == nullptr)
	{
		Prefire = PF ? PF : NewObject<UAGunBitBP>();
		Prefire->AddToRoot();
		Fire = F ? F :	NewObject<UAGunBitBP>();
		Fire->AddToRoot();
		PostFire = PtF ? PtF : NewObject<UAGunBitBP>();
		PostFire->AddToRoot();
			
		PrefireCosmetic  = PFC ? PFC : NewObject<UAGunBitBP>();
		PrefireCosmetic->AddToRoot();
		FireCosmetic = FC ? FC : NewObject<UAGunBitBP>();
		FireCosmetic->AddToRoot();
		PostFireCosmetic = PtFc ? PtFc : NewObject<UAGunBitBP>();
		PostFireCosmetic->AddToRoot();
		FailedFireCosmetic = FFC ? FFC : NewObject<UAGunBitBP>();
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

	UArtilleryLibrary::ApplyDamage(MyDispatch, HitEntity, 100);
}
