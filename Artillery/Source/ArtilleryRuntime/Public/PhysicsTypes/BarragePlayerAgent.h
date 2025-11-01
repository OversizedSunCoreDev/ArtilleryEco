// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryDispatch.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "FBarragePrimitive.h"
#include "Components/ActorComponent.h"
#include "States/PlayerStates.h"
#include "FBPhysicsInputTypes.h"
#include "PhysicsFilters/FastBroadphaseLayerFilter.h"
#include "PhysicsFilters/FastObjectLayerFilters.h"

#include "BarragePlayerAgent.generated.h"

namespace Hitmark
{
	inline thread_local TSharedPtr<FHitResult> ShortCast = nullptr;
	inline thread_local TSharedPtr<FHitResult> AimFriction = nullptr;
}

static constexpr uint32 DEFAULT_DASH_DURATION = 18;
static constexpr double DEFAULT_DASH_MULTIPLIER = 200.f;

static const std::vector<EPhysicsLayer> ExclusionFilters = { EPhysicsLayer::ENEMYPROJECTILE, EPhysicsLayer::DEBRIS, EPhysicsLayer::HITBOX };

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTILLERYRUNTIME_API UBarragePlayerAgent : public UBarrageColliderBase
{
	GENERATED_BODY()

	//This leans HARD on the collider base but retains more uniqueness than the others.
public:
	using Caps = UE::Geometry::FCapsule3d;

	bool IsReady = false;
	// Sets default values for this component's properties
	UPROPERTY()
	double radius = 1; //this gets set by the outermost character. Bit of a gotcha, really.r
	UPROPERTY()
	double extent = 1; //well, we aint smaller than a centimeter, my friends.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement, meta=(ClampMin="0", UIMin="0"))
	float TurningBoost = 1.1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float MaxStickVelocity = 995;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float HardMaxVelocity = 1800;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float Deceleration = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float Acceleration = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float AirAcceleration = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float DeadzoneDecel = 13;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float GroundDecel = Deceleration * 0.75;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float AirDecel =  AirAcceleration * 0.6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float JumpImpulse = 1100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float JumpDelay = 55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float WallClingAfterJumpDelay = 45;	//Jumpticks count DOWN, so a higher value here is ironically shorter. Sigh. whoops.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float WallJumpImpulse = 600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float WallClingGravity = 200;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float NormalGravity = 1880;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float GroundingForceCoefficient = 0.08;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Movement)
	float DeadZoneSnapRegion = 220;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Movement)
	float ContactErrorMarginMultiplier = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Movement)
	//This appears to act like it's in meters. Given that it's compared to a "naked" jolt constant, that's not shocking but...
	float HowCloseIsGroundClose = 18;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Movement)
	// TODO: this should be lower but I set it high so you can reproduce the "run into a pillar and player jumps up for a frame" bug
	float HowCloseIsGroundWithinError = 5; // in cm
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Movement)
	FPlayerStates States;
	//you can't touch this from blueprint.
	//honestly, you shouldn't touch this at all.
	//it controls the scaling of inertia, gravity, locomotion, and forces.
	FQuat4d ThrottleModel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	int DashDuration = DEFAULT_DASH_DURATION;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	double DashForwardMultiplier = DEFAULT_DASH_MULTIPLIER;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	double DashYawMultiplier = DEFAULT_DASH_MULTIPLIER;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	int AirDashDuration = DEFAULT_DASH_DURATION;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	double AirDashForwardMultiplier = DEFAULT_DASH_MULTIPLIER;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Dash)
	double AirDashYawMultiplier = DEFAULT_DASH_MULTIPLIER;

	// Aim Friction scalars
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Aim)
	double MovingTowardsCritMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Aim)
	double MovingTowardsBaseMarkerMultiplier = 0.95f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Aim)
	double MovingAwayFromMarkersFrictionMultiplier = 0.8f;

	double ShortcastMaxRange = 500; //emergency default ONLY. normally set in constructor!!!!
	int32  MungeSafety = 0xffffffff;
	[[nodiscard]] FVector Chaos_LastGameFrameRightVector() const
	{
		return CHAOS_LastGameFrameRightVector.IsNearlyZero() ? FVector::RightVector : CHAOS_LastGameFrameRightVector;
	}

	[[nodiscard]] FVector Chaos_LastGameFrameForwardVector() const
	{
		return CHAOS_LastGameFrameForwardVector.IsNearlyZero() ? FVector::ForwardVector : CHAOS_LastGameFrameForwardVector ;
	}

private:
	void UpdateDetailedGroundState(FVector3d& ground);
	void UpdateDetailedWallState(FVector3d& WallNormal);
	
public:
	//why not do this in the physics engine?
	//well, it's quite a lot of spherecasts, and those are read ops
	//we don't need to lock for this, and it's a higher level concept
	//so we do it here, to keep this stuff all in one place.
	//the methods ARE sequence dependent, so they're private and grouped this
	//way intentionally.
	void LocomotionUpdateDetailedState(FVector3d& ground)
	{
		UpdateDetailedGroundState(ground);
		UpdateDetailedWallState(ground);
	}
	
	UBarragePlayerAgent(const FObjectInitializer& ObjectInitializer);
	virtual void Register() override;
	void AddBarrageForce(float Duration);
	float ShortCastTo(const FVector3d& Direction);
	void ApplyRotation(float Duration, FQuat4f Rotation);
	void AddOneTickOfForce(FVector3d Force);
	void AddOneTickOfForce_LocomotionOnly(FVector3d Force);
	// Kludge for now until we double-ify everything
	void AddOneTickOfForce(FVector3f Force);
	void SetThrottleModel(double carryover = -1, double gravity = -1, double locomotion = -1, double forces = -1);
	void SetCharacterGravity(FVector3f NewGravity);
	void SetCharacterGravity(FVector3d NewGravity);

	UFUNCTION(BlueprintPure)
	FVector3f GetVelocity() const
	{
		return IsReady && GetBarrageBody() != nullptr ? FBarragePrimitive::GetVelocity(GetBarrageBody()) : FVector3f::ZeroVector;
	}
	
	UFUNCTION(BlueprintCallable)
	FVector3f GetGroundNormal()
	{
		return IsReady && GetBarrageBody() != nullptr ? FBarragePrimitive::GetCharacterGroundNormal(GetBarrageBody()) : FVector3f::ZeroVector;
	}
	
	FBarragePrimitive::FBGroundState GetGroundState() const;
	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ApplyAimFriction(const ActorKey& ActorsKey, const FVector3d& ActorLocation, const FVector3d& Direction, const FVector& StartingAimVec, const FVector& DesiredAimVec, FRotator& OutAimRotatorDelta);

	bool CalculateAimVector(const ActorKey& ActorsKey, const FVector3d& ActorLocation, const FVector& Direction, FVector& OutTargetAimAtLocation, FSkeletonKey& TargetKey, AActor*& TargetActor) const;
	
	/**
	* Rendering shape visualization.
	**/
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	UPROPERTY(BlueprintReadOnly)
	FVector CHAOS_LastGameFrameRightVector = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly)
	FVector CHAOS_LastGameFrameForwardVector = FVector::ZeroVector;

private:
	static bool IsAimMovingTowardPoint(const FVector& StartingAimVector, const FVector& DesiredAimVector, const FVector& ToTargetVector)
	{
		return DesiredAimVector.Dot(ToTargetVector) < StartingAimVector.Dot(ToTargetVector);
	}
	
	// Currently targeted object
	FBLet TargetFiblet;
	TWeakObjectPtr<AActor> TargetPtr;
	FastExcludeBroadphaseLayerFilter BroadPhaseFilter;
	FastExcludeObjectLayerFilter ObjectLayerFilter;
};
