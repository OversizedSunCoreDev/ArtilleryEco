// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "ArtilleryBPLibs.h"
#include "ArtilleryDispatch.h"
#include "FArcShot.h"

class FTArcingProjectile : public UArtilleryDispatch::TL_ThreadedImpl
{
public:
	FSkeletonKey MissileKey;

	FVector3d StartPosition;
	FVector3d TargetPosition;
	int TickTimeToTarget;
	int TicksElapsed;

	FSimpleArcShot ArcCalculator;

	FVector CurrentProjectileLocation;
	FQuat Direction;

	bool ShouldDeleteProjectile;
	std::function<void()> OnExpireCallback;

	FTArcingProjectile(
		FSkeletonKey Missile,
		const FVector3d& TargetPositionIn,
		const FVector3d& StartPositionIn,
		int TimeToTargetIn,
		double ApogeeModifier,
		bool DeleteProjectileAtEnd = true,
		std::function<void()> ExpirationCallback = nullptr)
	{
		MissileKey = Missile;
		TargetPosition = TargetPositionIn;
		StartPosition = StartPositionIn;
		TickTimeToTarget = TimeToTargetIn;
		TicksElapsed = 0;
		CurrentProjectileLocation = FVector::Zero();
		Direction = FQuat::Identity;
		ArcCalculator = FSimpleArcShot(StartPosition,ApogeeModifier, TargetPosition, TickTimeToTarget);
		ShouldDeleteProjectile = DeleteProjectileAtEnd;
		OnExpireCallback = ExpirationCallback;
	}
	
	FTArcingProjectile() : FTArcingProjectile(
		FSkeletonKey::Invalid(),
		FVector3d::Zero(),
		FVector3d::Zero(),
		0,
		0.f) {}

	void TICKLITE_StateReset()
	{
	}
	
	void TICKLITE_Calculate()
	{	auto prior = CurrentProjectileLocation;
		CurrentProjectileLocation = ArcCalculator.Get(TicksElapsed);
		Direction = ArcCalculator.GetCurveTangent(TicksElapsed).ToOrientationQuat(); //this variable was originally NewDirections. I can't have anything nice.
	}
	
	void TICKLITE_Apply()
	{
		ArtilleryTime Now = this->GetShadowNow();
		FBLet MissilePhysicsObject = this->ADispatch->GetFBLetByObjectKey(MissileKey, Now);
		FBarragePrimitive::SetPosition(CurrentProjectileLocation, MissilePhysicsObject);
		FBarragePrimitive::ApplyRotation(Direction, MissilePhysicsObject);
		TicksElapsed++;
	}
	
	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		return TicksElapsed >= TickTimeToTarget;
	}

	void TICKLITE_OnExpiration()
	{
		if (ShouldDeleteProjectile)
		{
			// TODO - add a cool explosion
			this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>()->DeleteProjectile(MissileKey);
		}
		if (OnExpireCallback)
		{
			OnExpireCallback();
		}
	}
};

typedef Ticklites::Ticklite<FTArcingProjectile> TL_ArcingProjectile;
