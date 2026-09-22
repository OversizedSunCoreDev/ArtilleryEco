// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryBPLibs.h"
#include "ArtilleryProjectileDispatch.h"
#include "ThistleBehavioralist.h"
#include "Public/GameplayTags.h"
#include "FakeRandom.h"
#include "FArtilleryGun.h"
#include "FTSphereCast.h"
#include "TransformDispatch.h"
#include "SkeletonTypes.h"
#include "ThistleDispatch.h"
#include "UArtilleryAbilityMinimum.h"

#include "SampleKick.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FQuickKick : public FArtilleryGun
{
	GENERATED_BODY()

	friend class UAGunBitBP;

public:
	int Radius;

	FQuickKick(const FGunKey& KeyFromDispatch, int MaxAmmoIn, int FirerateIn, int ReloadTimeIn, int AoE, UArtilleryDispatch* Dispatch)
	{
		MyGunKey = KeyFromDispatch;
		MaxAmmo = MaxAmmoIn;
		Firerate = FirerateIn;
		ReloadTime = ReloadTimeIn;
		Radius = AoE;
		MyDispatch = nullptr;
		MyProjectileDispatch = nullptr;
	};

	FQuickKick(): Radius(0)
	{
		MyGunKey = Default;
		MaxAmmo = 3;
		Firerate = 600;
		ReloadTime = 60;

		MyDispatch = nullptr;
		MyProjectileDispatch = nullptr;
	}
	;

	virtual bool Initialize(
		const FGunKey& KeyFromDispatch,
		const bool MyCodeWillHandleKeys,
		UAGunBitBP* PF = nullptr,
		UAGunBitBP* PFC = nullptr,
		UAGunBitBP* F = nullptr,
		UAGunBitBP* FC = nullptr,
		UAGunBitBP* PtF = nullptr,
		UAGunBitBP* PtFc = nullptr,
		UAGunBitBP* FFC = nullptr) override
	{
		ARTGUN_MACROAUTOINIT(MyCodeWillHandleKeys);
		return true;
	}

	virtual void PreFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile, bool VerifiedFrame = false, const EventBufferInfo FireAction = EventBufferInfo::Default())
		override
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
			FireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
		}
	}

virtual void FireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile) override
	{
		FBLet GameSimPhysicsObject = this->MyDispatch->GetFBLetByObjectKey(
			MyProbableOwner, this->MyDispatch->GetShadowNow());
		FVector ForwardInitial;
		UArtilleryLibrary::SimpleEstimator(MyDispatch->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>(), ForwardInitial, 4);
		if (FBarragePrimitive::IsNotNull(GameSimPhysicsObject))
		{
			FVector BaseLoc(FBarragePrimitive::GetPosition(GameSimPhysicsObject));
			FVector Loc = BaseLoc + (ForwardInitial * ((2*Radius)/3));
			FBox2d SearchBox = FBox2d({Loc.X - Radius, Loc.Y - Radius}, {Loc.X + Radius, Loc.Y + Radius});
			auto Thistle = MyDispatch->GetWorld()->GetSubsystem<UThistleDispatch>();
			
			TArray<TPair<ActorKey, FVector2d>> EnemiesInRange;
			if (!Thistle->QuadTreeMaintenance)
			{
				Thistle->QuadTreeForDistance.Get()->GetElements(SearchBox, EnemiesInRange);
			}

			if (EnemiesInRange.IsEmpty())
			{
				return;
			}

			for (auto Enemy : EnemiesInRange)
			{
				if (
				FVector::Distance(
				MyTransformDispatch->GetKineByObjectKey( Enemy.Key)->CopyOfTransformLike()->GetLocation(),
				BaseLoc)
				<= 2.3*Radius
				)
				{
				//damage and apply force
					FBLet EnemyPhysicsObject = this->MyDispatch->GetFBLetByObjectKey(Enemy.Key, MyDispatch->GetShadowNow());
					FBarragePrimitive::ApplyForce(350000 * FVector::UpVector, EnemyPhysicsObject);
					FBarragePrimitive::ApplyForce(350000 * -ForwardInitial, EnemyPhysicsObject);
				}
			}
			
			PostFireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
		}
		//apply small-small-small self force with lunge
		FBarragePrimitive::ApplyForce(VelocityVec(ForwardInitial.X * (InitialKick), ForwardInitial.Y * (InitialKick), 0), GameSimPhysicsObject, PhysicsInputType::LungeForce);
		FBarragePrimitive::ApplyForce(VelocityVec(ForwardInitial.X * (InitialKick/2), ForwardInitial.Y * (InitialKick/2), 0), GameSimPhysicsObject, PhysicsInputType::OtherForce);
		
	}

	virtual void PostFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile)  override
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
	
	//A resolver would normally be used in conjunction with either a sphere search or sphere cast ticklite, often as part of the
	// collided function. Here, we do everything as a hitscan for efficiency, though we actually use the 
	// void Resolver(UE::Math::TVector<double> Center, FBLet Hit)
	// {
	// }

private:
	float DamageToApply = 200;
	static const inline FGunKey Default = FGunKey("QuickKick");

	int InitialKick = 840;
	static const inline FName ParticleName =
		"/Script/Niagara.NiagaraSystem'/Game/Projectiles/Particles/NS_MissileTrail.NS_MissileTrail'";
};
