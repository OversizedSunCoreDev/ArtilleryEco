// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "ArtilleryBPLibs.h"
#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"

// 10-24-2024 - To my knowledge we're currently running at about 90 ticks per second, so this should give us a max
//				projectile time of about 10 seconds. No projectile we currently have should be alive that long
static constexpr int DEFAULT_MAX_TICKS_ALLOWED = 900; 

class FTProportionalNavigation : public UArtilleryDispatch::TL_ThreadedImpl /*Facaded*/
{
public:
	FSkeletonKey MissileKey;

	// If TargetKey is null, TargetPosition is used instead as a fixed point
	FSkeletonKey TargetKey;
	FVector3f TargetPosition;
	float ProximityFuseRadius;
	float MissileMaxSpeed; // Maximum velocity to clamp to
	float MissileAcceleration;
	float NavigationConstant; // Usually 3
	int LockOnTime = 0; // # of ticks to wait before starting nav
	std::function<void(FSkeletonKey)> LockOnCompleteCallback;
	bool ProxmityFuseTriggered = false;
	const static inline FVector3f UNSET = FVector3f(0.0,NAN,0.0);
	
	FVector3f AccelerationVector = FVector3f::ZeroVector;

	uint32_t ConsecutiveTicksKeysAreInvalid = 0;
	int TicksElapsed = 0;

	int TicksAliveTime;

	bool ResetVelocityOnLockon;
	bool LockOnCompleted;

	FTProportionalNavigation()
	{
		MissileKey = FSkeletonKey::Invalid();
		TargetKey = FSkeletonKey::Invalid();
		TargetPosition = FVector3f::ZeroVector;
		MissileMaxSpeed = 0.f;
		MissileAcceleration = 0.f;
		NavigationConstant = 3.f;
		LockOnCompleteCallback = nullptr;
		ProximityFuseRadius = 0.f;
		TicksAliveTime = DEFAULT_MAX_TICKS_ALLOWED;
		ResetVelocityOnLockon = true;
		LockOnCompleted = false;
	}
	
	FTProportionalNavigation(
		FSkeletonKey Missile,
		FSkeletonKey Target,
		float MaxSpeed,
		float AllowedAcceleration,
		float NavigationConstantIn,
		bool ResetVelocityWhenLockOnCompletes = false,
		int LockOnTimeDelay = 0,
		const std::function<void(FSkeletonKey)>& LockOnFunc = nullptr,
		float ProximityRadius = 0.25)
	{
		MissileKey = Missile;
		TargetKey = Target;
		TargetPosition = FVector3f::ZeroVector;
		ProximityFuseRadius = ProximityRadius;
		MissileMaxSpeed = MaxSpeed;
		MissileAcceleration = AllowedAcceleration;
		NavigationConstant = NavigationConstantIn;
		LockOnTime = LockOnTimeDelay;
		LockOnCompleteCallback = LockOnFunc;
		TicksAliveTime = DEFAULT_MAX_TICKS_ALLOWED;
		LockOnCompleted = false;
		ResetVelocityOnLockon = ResetVelocityWhenLockOnCompletes;
	}

	FTProportionalNavigation(
		FSkeletonKey Missile,
		FVector3f Target,
		float MaxSpeed,
		float AllowedAcceleration,
		float NavigationConstantIn,
		bool ResetVelocityWhenLockOnCompletes = false,
		int LockOnTimeDelay = 0,
		const std::function<void(FSkeletonKey)>& LockOnFunc = nullptr,
		float ProximityRadius = 0.25)
	{
		MissileKey = Missile;
		TargetKey = FSkeletonKey::Invalid();
		TargetPosition = Target;
		ProximityFuseRadius = ProximityRadius;
		MissileMaxSpeed = MaxSpeed;
		MissileAcceleration = AllowedAcceleration;
		NavigationConstant = NavigationConstantIn;
		LockOnTime = LockOnTimeDelay;
		LockOnCompleteCallback = LockOnFunc;
		TicksAliveTime = DEFAULT_MAX_TICKS_ALLOWED;
		LockOnCompleted = false;
		ResetVelocityOnLockon = ResetVelocityWhenLockOnCompletes;
	}

	void TICKLITE_StateReset()
	{
	}
	
	void TICKLITE_Calculate()
	{
	ArtilleryTime Now = this->GetShadowNow();
     			FBLet MissilePhysicsObject = this->ADispatch->GetFBLetByObjectKey(MissileKey, Now);
     			if (FBarragePrimitive::IsNotNull(MissilePhysicsObject))
     			{
		if (TicksElapsed > LockOnTime)
		{
			if (LockOnCompleteCallback != nullptr)
			{
				LockOnCompleteCallback(MissileKey);
				LockOnCompleteCallback = nullptr;
			}
			
			
				FVector3f TargetLocation;
				FVector3f TargetVelocity;
				if (TargetKey.IsValid())
				{
					FBLet TargetPhysicsObject = this->ADispatch->GetFBLetByObjectKey(TargetKey, Now);
					TargetLocation = FBarragePrimitive::GetPosition(TargetPhysicsObject);
					TargetVelocity = FBarragePrimitive::GetVelocity(TargetPhysicsObject);
				}
				else
				{
					TargetLocation = TargetPosition;
					TargetVelocity = FVector3f::ZeroVector;
				}

				auto MissileLocation = FBarragePrimitive::GetPosition(MissilePhysicsObject);
				auto MissileVelocity = FBarragePrimitive::GetVelocity(MissilePhysicsObject);
			
				if((TargetLocation - MissileLocation).Length() < ProximityFuseRadius)
				{
					ProxmityFuseTriggered = true;
				}
				auto MissileRelativeVelocity = TargetVelocity - MissileVelocity;
				auto MissileToTargetVector = TargetLocation - MissileLocation;

				auto RotationVector = FVector3f::CrossProduct(MissileToTargetVector, MissileRelativeVelocity) / MissileToTargetVector.SquaredLength();

				AccelerationVector = FVector3f::CrossProduct(NavigationConstant * MissileRelativeVelocity, RotationVector);
			
				// UE_LOG(LogTemp, Warning, TEXT("CurrVelocity: %f AccelerationVector: %f %f %f"), MissileVelocity.Length(), AccelerationVector.X, AccelerationVector.Y, AccelerationVector.Z);
			}
		}
	}
	
	//this isn't quite right. we should calculate the component in calculate
	//but for now, this is good enough for testing.
	void TICKLITE_Apply()
	{
			ArtilleryTime Now = this->GetShadowNow();
			FBLet MissilePhysicsObject = this->ADispatch->GetFBLetByObjectKey(MissileKey, Now);
			if(FBarragePrimitive::IsNotNull(MissilePhysicsObject))
			{
				ConsecutiveTicksKeysAreInvalid = 0;
				if(ProxmityFuseTriggered)
				{
					if (TargetKey.Obj != 0)
					{
						//begin collision nudge
						FBLet TargetPhysicsObject = this->ADispatch->GetFBLetByObjectKey(TargetKey, Now);
						auto A = FBarragePrimitive::GetPosition(TargetPhysicsObject);
						auto B = FBarragePrimitive::GetPosition(MissilePhysicsObject);
						auto C = FBarragePrimitive::GetVelocity(TargetPhysicsObject);
						FBarragePrimitive::SetVelocity(FBarragePrimitive::UpConvertFloatVector((A-B)+C), MissilePhysicsObject);
					}
				}
				else
				{
					FVector3f CurrentMissileVelocity = FBarragePrimitive::GetVelocity(MissilePhysicsObject);

					// Only apply velocity changes for nav if lock on time is elapsed
					if (TicksElapsed > LockOnTime)
					{
						if (!LockOnCompleted)
						{
							if (ResetVelocityOnLockon)
							{
								CurrentMissileVelocity = FVector3f::Zero();
							}
							LockOnCompleted = true;
						}
						FVector3f NormedMissileVelocity = CurrentMissileVelocity.GetSafeNormal();
						FVector3f NewMissileVelocity = (CurrentMissileVelocity + (AccelerationVector + NormedMissileVelocity * MissileAcceleration) * UBarrageDispatch::TickRateInDelta).GetClampedToSize(0, MissileMaxSpeed);
						FBarragePrimitive::SetVelocity(FBarragePrimitive::UpConvertFloatVector(NewMissileVelocity), MissilePhysicsObject);
					}
					FBarragePrimitive::ApplyRotation(FBarragePrimitive::UpConvertFloatVector(CurrentMissileVelocity).ToOrientationQuat(), MissilePhysicsObject);
				}
			}
			else
			{
				++ConsecutiveTicksKeysAreInvalid;
			}

		TicksElapsed++;
	}
	
	void TICKLITE_CoreReset()
	{
		ProxmityFuseTriggered = false;
	}

	bool TICKLITE_CheckForExpiration()
	{
		const bool TimeIsUp = TicksElapsed > TicksAliveTime;
		FBLet MissilePhysicsObject = this->ADispatch->GetFBLetByObjectKey(MissileKey, ADispatch->GetShadowNow());
		FBarragePrimitive::IsNotNull(MissilePhysicsObject);
			if (TimeIsUp )
			{
				UE_LOG(LogTemp, Warning, TEXT("FTProportionalNavigation::CheckForExpiration: Timing out projectile key [%llu]."), MissileKey.Obj);
			}
			// IsNotNull performs a nullity check and does not require an n-coalescing because the invoke is static & currying
			return
			TimeIsUp
			|| ConsecutiveTicksKeysAreInvalid > 3  ; // why does this need to be 3?
	}

	void TICKLITE_OnExpiration()
	{
		UArtilleryLibrary::TombstonePrimitive(MissileKey);
	}
};

typedef Ticklites::Ticklite<FTProportionalNavigation> TL_ProportionalNavigation;