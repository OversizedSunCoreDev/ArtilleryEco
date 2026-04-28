// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryProjectileDispatch.h"
#include "FakeRandom.h"
#include "FArtilleryGun.h"
#include "Ticklites/FTProportionalNavigation.h"
#include "UArtilleryAbilityMinimum.h"
#include "Public/GameplayTags.h"
#include "GunBPLibs.h"

#include "FGunWeeRocket.generated.h"

// This is a sample enemy weapon!~~!
USTRUCT(Blueprintable, BlueprintType)
struct FGunWeeRocket : public FArtilleryGun
{
	GENERATED_BODY()

	friend class UArtilleryPerActorAbilityMinimum;

public:
	FGunWeeRocket(const FGunKey& KeyFromDispatch, int MaxAmmoIn, int FirerateIn, int ReloadTimeIn)
	{
		MyGunKey = KeyFromDispatch;
		MaxAmmo = MaxAmmoIn;
		Firerate = FirerateIn;
		ReloadTime = ReloadTimeIn;

		MyDispatch = nullptr;
		MyProjectileDispatch = nullptr;
	};

	FGunWeeRocket()
	{
		MyGunKey = Default;
		MaxAmmo = 3;
		Firerate = 60;
		ReloadTime = 150;

		MyDispatch = nullptr;
		MyProjectileDispatch = nullptr;
	};

	virtual bool Initialize(
		const FGunKey& KeyFromDispatch,
		const bool MyCodeWillHandleKeys,
		UArtilleryPerActorAbilityMinimum* PF = nullptr,
		UArtilleryPerActorAbilityMinimum* PFC = nullptr,
		UArtilleryPerActorAbilityMinimum* F = nullptr,
		UArtilleryPerActorAbilityMinimum* FC = nullptr,
		UArtilleryPerActorAbilityMinimum* PtF = nullptr,
		UArtilleryPerActorAbilityMinimum* PtFc = nullptr,
		UArtilleryPerActorAbilityMinimum* FFC = nullptr) override
	{
		ARTGUN_MACROAUTOINIT(MyCodeWillHandleKeys);
		return true;
	}

	virtual void PreFireGun(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const EventBufferInfo FireAction = EventBufferInfo::Default(),
		const FGameplayEventData* TriggerEventData = nullptr,
		bool RerunDueToReconcile = false,
		int DallyFramesToOmit = 0) override
	{
		AttrMapPtr attribMap = MyDispatch->GetAttribMap(MyGunKey);
		if (attribMap == nullptr)
		{
			return;
		}
		
		AttrPtr CooldownRemainingPtr = attribMap->FindRef(COOLDOWN_REMAINING);
		AttrPtr AmmoRemainingPtr = attribMap->FindRef(AMMO);
		if (!CooldownRemainingPtr.IsValid() || CooldownRemainingPtr->GetCurrentValue() > 0.f)
		{
			// Cooldown not up yet!
			return;
		}

		if (!AmmoRemainingPtr.IsValid() || AmmoRemainingPtr->GetCurrentValue() <= 0.f)
		{
			// No ammo!
			return;
		}

		TWeakObjectPtr<AActor> Me = MyTransformDispatch->GetAActorByObjectKey(MyProbableOwner);
		if (Me.Get())
		{
			FireGun(Fired, 0, ActorInfo, ActivationInfo, false, TriggerEventData, Handle);
		}
	}

	virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle) override
	{
		TWeakObjectPtr<AActor> Me = MyTransformDispatch->GetAActorByObjectKey(MyProbableOwner);
		if (Me.Get())
		{
			UArtilleryProjectileDispatch* ProjectileDispatch = MyDispatch->GetWorld()->GetSubsystem<
				UArtilleryProjectileDispatch>();
			auto fphold = FiringPointComponent.Pin();
			if (fphold && fphold.Get() && !fphold->GetComponentLocation().ContainsNaN())
			{
				if (ProjectileDispatch)
				{
					//DrawDebugLine(MyDispatch->GetWorld(), StartLocation, StartLocation + RotationWithSpreadVector * 5000, FColor::Red, true, 5.f);
					const auto PerTickAimDeviation = UFakeRandom::GetBoxingDispersion(
						UArtilleryLibrary::GetTotalsTickCount(), 0, MyGunKey.GunInstanceID);
					const auto Spray = UFakeRandom::GetSimpleSprayNudge(
						UArtilleryLibrary::GetTotalsTickCount(), 0);
					
						auto AimAt = UArtilleryLibrary::GetPlayerLocationAsEstTarget(E_PlayerKEY::CABLE);
						if (AimAt.IsNearlyZero())
						{
							AimAt = UArtilleryLibrary::GetLocalPlayer_UNSAFE()->GetActorLocation();
						}
						AimAt.X += PerTickAimDeviation.X + Spray.X;
						AimAt.Y += PerTickAimDeviation.Y + Spray.Y;
						AimAt.Y += PerTickAimDeviation.Z;
						const FVector3d Direction = (AimAt - fphold->GetComponentLocation()).GetSafeNormal();
						TArray<FGameplayTag> ProjectileTags;
						ProjectileTags.Add(TAG_EnemyProjectile);
						ProjectileDispatch->QueueProjectileInstance(
							//a less elegant weapon for a less civilized age.
							TEXT("Pellet"), MyGunKey, FiringPointComponent->GetComponentLocation(), Direction * 1000,
							0.07f, Layers::ENEMYPROJECTILE, &ProjectileTags, HERTZ_OF_BARRAGE*12);
						PostFireGun(Fired, 0, ActorInfo, ActivationInfo, false, TriggerEventData, Handle);
				}
			}
		}
	}

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool RerunDueToReconcile,
		const FGameplayEventData* TriggerEventData,
		FGameplayAbilitySpecHandle Handle) override
	{
		AttrMapPtr MyAttribs = MyDispatch->GetAttribMap(MyGunKey);
		AttrPtr AmmoPtr = MyAttribs->FindRef(AMMO);
		AmmoPtr->SetCurrentValue(AmmoPtr->GetCurrentValue() - 1);

		AttrPtr CooldownPtr = MyAttribs->FindRef(COOLDOWN);
		AttrPtr CooldownRemainingPtr = MyAttribs->FindRef(COOLDOWN_REMAINING);
		if (CooldownPtr.IsValid() && CooldownRemainingPtr.IsValid())
		{
			CooldownRemainingPtr->SetCurrentValue(CooldownPtr->GetCurrentValue());
		}

		MyAttribs->FindRef(TICKS_SINCE_GUN_LAST_FIRED)->SetCurrentValue(0.f);
		MyAttribs->FindRef(AttribKey::LastFiredTimestamp)->SetCurrentValue(
			static_cast<double>(MyDispatch->GetShadowNow()));
	}

private:
	static const inline FGunKey Default = FGunKey("WeeRocket");
};
