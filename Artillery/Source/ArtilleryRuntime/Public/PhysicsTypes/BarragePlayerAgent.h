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
		return IsReady && MyBarrageBody != nullptr ? FBarragePrimitive::GetVelocity(MyBarrageBody) : FVector3f::ZeroVector;
	}
	
	UFUNCTION(BlueprintCallable)
	FVector3f GetGroundNormal()
	{
		return IsReady && MyBarrageBody != nullptr ? FBarragePrimitive::GetCharacterGroundNormal(MyBarrageBody) : FVector3f::ZeroVector;
	}
	
	FBarragePrimitive::FBGroundState GetGroundState() const;
	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ApplyAimFriction(const ActorKey& ActorsKey, const FVector3d& ActorLocation, const FVector3d& Direction, const FVector& StartingAimVec, const FVector& DesiredAimVec, FRotator& OutAimRotatorDelta);

	bool CalculateAimVector(const ActorKey& ActorsKey, const FVector3d& ActorLocation, const FVector& Direction, FVector& OutTargetAimAtLocation, FSkeletonKey& TargetKey, AActor*& TargetActor) const;
	
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

inline void UBarragePlayerAgent::UpdateDetailedGroundState(FVector3d& ground)
{
	// If Barrage says they're on the ground, they're on the ground, period.
	if(GetGroundState() == FBarragePrimitive::FBGroundState::OnGround)
	{
		return States.Ground(States.GroundTouching);
	}
	
	// Check directional down casts to see if the player might be pretty close to the ground
	float downD = ShortCastTo(FVector3d::DownVector);
	float forwardD = ShortCastTo((FVector3d::DownVector + CHAOS_LastGameFrameForwardVector.GetSafeNormal()).GetSafeNormal());
	float rightD = ShortCastTo((FVector3d::DownVector + CHAOS_LastGameFrameRightVector.GetSafeNormal()).GetSafeNormal());
	float leftD = ShortCastTo((FVector3d::DownVector - CHAOS_LastGameFrameRightVector.GetSafeNormal()).GetSafeNormal());
	float rearD = ShortCastTo((FVector3d::DownVector - CHAOS_LastGameFrameForwardVector.GetSafeNormal()).GetSafeNormal());

	float max = FMath::Max3(FMath::Max(rightD, leftD), FMath::Max(forwardD, rearD), downD);
	float min = FMath::Min3( FMath::Min(rightD, leftD), forwardD, rearD);

	char value = States.GroundNone;//start by assuming we're in the air.
	
	if(min == -1)//degraded state, assume ground close!!! we must revert to normal gravity and begin to fall.
	{
		value |= States.GroundContactPoor;
		value |= States.GroundClose;// we don't actually know WHERE we are.
	}
	
	if (max < HowCloseIsGroundWithinError) //if we're very close to the ground, treat us as grounded.
	{
		value |= States.GroundTouching; 
	}
	else if (max < HowCloseIsGroundClose)
	{
		value |= States.GroundClose;
	}
	
	if( max > min * ContactErrorMarginMultiplier)
	{
		value |= States.GroundSlanted;
	}
	
	if( max > ShortcastMaxRange && downD < ShortcastMaxRange && min < ShortcastMaxRange)
	{
		value |= States.GroundContactPoor;
	}
	
	return States.Ground(value);
}

inline void UBarragePlayerAgent::UpdateDetailedWallState(FVector3d& WallNormal)
{
	// If the player is on the ground or close to the ground, return empty wall state as groundedness takes priority over being "on a wall"
	if ((States.Ground() & States.GroundTouching) || (States.Ground() & States.GroundClose))
	{
		return States.Wall(States.WallNone);
	}
	
	if(!WallNormal.IsNearlyZero())
	{
		return States.Wall(States.WallTouching);
	}

	return States.Wall(States.WallNone);
}

//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
inline UBarragePlayerAgent::UBarragePlayerAgent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), BroadPhaseFilter(ExclusionFilters), ObjectLayerFilter(ExclusionFilters)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	MyBarrageBody = nullptr;
	ShortcastMaxRange =  2 * (this->extent + this->NormalGravity);
	PrimaryComponentTick.bCanEverTick = true;
	MyObjectKey = 0;
	ThrottleModel = FQuat4d(1,1,1,1);
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

//Of these, get velo is the only one that could be considered kinda risky given when it's called. the others could need a hold open.
//inlining them means that you don't get quite the effects you might expect from a copy-by-value of a shared pointer.
inline FBarragePrimitive::FBGroundState UBarragePlayerAgent::GetGroundState() const
{
	return FBarragePrimitive::GetCharacterGroundState(MyBarrageBody);
}

//KEY REGISTER, initializer, and failover.
//----------------------------------

inline void UBarragePlayerAgent::Register()
{
	if(MyObjectKey ==0 && GetOwner())
	{
		if(GetOwner()->GetComponentByClass<UKeyCarry>())
		{
			MyObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
		}

		if(MyObjectKey == 0)
		{
			MyObjectKey = MAKE_ACTORKEY(GetOwner());
			ThrottleModel = FQuat4d(1,1,1,1);
		}
	}
	
	if(!IsReady && MyObjectKey != 0 && !GetOwner()->GetActorLocation().ContainsNaN()) // this could easily be just the !=, but it's better to have the whole idiom in the example
	{
		FBCharParams params = FBarrageBounder::GenerateCharacterBounds(GetOwner()->GetActorLocation(), radius, extent, HardMaxVelocity);
		MyBarrageBody = GetWorld()->GetSubsystem<UBarrageDispatch>()->CreatePrimitive(params, MyObjectKey, Layers::MOVING);
		if(MyBarrageBody && MyBarrageBody->tombstone == 0 && MyBarrageBody->Me != FBShape::Uninitialized)
		{
			IsReady = true;
		}
	}
}

inline void UBarragePlayerAgent::AddBarrageForce(float Duration)
{
	//I'll be back for youuuu.
	throw;
}

//returns distance to target as magnitude (always positive) or -1 for no hit.
inline float UBarragePlayerAgent::ShortCastTo(const FVector3d& Direction)
{
	UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
	check(Physics);
	FVector3f MyPos = FBarragePrimitive::GetPosition(MyBarrageBody);
	if(!MyBarrageBody || MyPos.ContainsNaN())
	{
		return -1; // please, leave us be.
	}// The actor calling this sure as hell better be allocated already
	
	if(Hitmark::ShortCast)
	{
		Hitmark::ShortCast->Init();
	}
	else
	{
		Hitmark::ShortCast = MakeShared<FHitResult>();
	}
	
	//we shoot a lil pill. lmao.
	JPH::BodyID CastingBodyID;
	Physics->JoltGameSim->GetBodyIDOrDefault(MyBarrageBody->KeyIntoBarrage, CastingBodyID);
	const JPH::IgnoreSingleBodyFilter default_body_filter(CastingBodyID);
	
	Physics->SphereCast( 
		0.01f,
		ShortcastMaxRange,
		FVector3d(MyPos.X, MyPos.Y, MyPos.Z),
		Direction,
		Hitmark::ShortCast,
		BroadPhaseFilter,
		ObjectLayerFilter,
		default_body_filter);
	
	const int32 TestVar = Hitmark::ShortCast->MyItem;
	return  TestVar == JPH::BodyID::cInvalidBodyID || TestVar == MungeSafety ?  ShortcastMaxRange * 2 : Hitmark::ShortCast->Distance;
}

inline void UBarragePlayerAgent::ApplyRotation(float Duration, FQuat4f Rotation)
{
	//I'll be back for youuuu.
	throw;
}

inline void UBarragePlayerAgent::AddOneTickOfForce(FVector3d Force)
{
	FBarragePrimitive::ApplyForce(Force, MyBarrageBody);
}

inline void UBarragePlayerAgent::AddOneTickOfForce_LocomotionOnly(FVector3d Force)
{
	FBarragePrimitive::ApplyForce(Force, MyBarrageBody, PhysicsInputType::SelfMovement);
}

// negatives are ignored. I'm not dealing with that. As a result, -1 can be used to inherit
//the current throttle setting for that value. Please use this very carefully, as it can fuck movement up entirely.
//This is mostly used for instantly canceling momentum or reducing directional control during slides.
inline void UBarragePlayerAgent::SetThrottleModel(double carryover, double gravity, double locomotion, double forces)
{
	FQuat4d DANGER = FQuat4d(carryover >= 0 ? carryover : ThrottleModel.X,
		gravity >= 0 ? gravity : ThrottleModel.Y,
		locomotion >= 0 ? locomotion : ThrottleModel.Z,
		forces >= 0 ? forces : ThrottleModel.W);
	ThrottleModel = DANGER;
	FBarragePrimitive::Apply_Unsafe(DANGER, MyBarrageBody, PhysicsInputType::Throttle);
}

inline void UBarragePlayerAgent::AddOneTickOfForce(FVector3f Force)
{
	FBarragePrimitive::ApplyForce(FVector3d(Force.X, Force.Y, Force.Z), MyBarrageBody);
}

inline void UBarragePlayerAgent::SetCharacterGravity(FVector3f NewGravity)
{
	FBarragePrimitive::SetCharacterGravity(FVector3d(NewGravity.X, NewGravity.Y, NewGravity.Z), MyBarrageBody);
}

inline void UBarragePlayerAgent::SetCharacterGravity(FVector3d NewGravity)
{
	FBarragePrimitive::SetCharacterGravity(NewGravity, MyBarrageBody);
}

// Called when the game starts
inline void UBarragePlayerAgent::BeginPlay()
{
	Super::BeginPlay();
	Register();
}

inline void UBarragePlayerAgent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if(!IsReady)
	{
		Register();// ...
	}

	CHAOS_LastGameFrameRightVector = GetOwner()->GetActorRightVector();
	CHAOS_LastGameFrameForwardVector = GetOwner()->GetActorForwardVector();
}

inline void UBarragePlayerAgent::ApplyAimFriction(
	const ActorKey& ActorsKey,
	const FVector3d& ActorLocation,
	const FVector3d& Direction,
	const FVector& StartingAimVec,
	const FVector& DesiredAimVec,
	FRotator& OutAimRotatorDelta)
{
	check(GEngine);
	
	double FrictionMultiplier = 1.0f;
	
	UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
	check(Physics);
	
	FBLet MyFiblet = Physics->GetShapeRef(ActorsKey);
	check(MyFiblet); // The actor calling this sure as hell better be allocated already

	//hitmark is threadlocal.
	if(Hitmark::AimFriction)
	{
		Hitmark::AimFriction->Init();
	}
	else
	{
		Hitmark::AimFriction = MakeShared<FHitResult>();
	}

	JPH::BodyID CastingBodyID;
	Physics->JoltGameSim->GetBodyIDOrDefault(MyFiblet->KeyIntoBarrage, CastingBodyID);
	const JPH::IgnoreSingleBodyFilter default_body_filter(CastingBodyID);
	
	Physics->SphereCast(
		0.01f,
		1000.0f, // Hard-coding range for now until we determine how we want to handle range on this
		ActorLocation,
		Direction,
		Hitmark::AimFriction,
		BroadPhaseFilter,
		ObjectLayerFilter,
		default_body_filter);

	FBarrageKey HitBarrageKey = Physics->GetBarrageKeyFromFHitResult(Hitmark::AimFriction);

	// Determine if we've changed targets
	if (HitBarrageKey != 0)
	{
		if (!TargetFiblet.IsValid() || HitBarrageKey != TargetFiblet->KeyIntoBarrage)
		{
			TargetFiblet = Physics->GetShapeRef(HitBarrageKey);
			if (TargetFiblet)
			{
				UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
				check(TransformDispatch);

				UArtilleryDispatch* ADispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
				if (ADispatch->DoesEntityHaveTag(TargetFiblet->KeyOutOfBarrage, FGameplayTag::RequestGameplayTag("Enemy")))
				{
					TargetPtr = TransformDispatch->GetAActorByObjectKey(TargetFiblet->KeyOutOfBarrage);
				}
			}
		}
	}
	else
	{
		TargetFiblet.Reset();
		TargetPtr.Reset();
	}

	if (TargetPtr.Get() && TargetPtr.IsValid())
	{
		FVector CritWorldLocation;
		FVector CockpitWorldLocation;
		// Grab important points on target
		UStaticMeshComponent* ActorStaticMesh = TargetPtr->GetComponentByClass<UStaticMeshComponent>();
		if(ActorStaticMesh)
		{
			CritWorldLocation = ActorStaticMesh->GetSocketLocation(FName("Mount_Top"));
			CockpitWorldLocation  = ActorStaticMesh->GetSocketLocation(FName("Mount_Cockpit"));
		}
		else
		{
			CritWorldLocation = TargetPtr->GetComponentsBoundingBox().GetCenter();
			CockpitWorldLocation = TargetPtr->GetComponentsBoundingBox().GetClosestPointTo(ActorLocation);
		}

		FVector VectorToTargetCrit = (CritWorldLocation - ActorLocation).GetSafeNormal();
		FVector VectorToTargetCockpit = (CockpitWorldLocation - ActorLocation).GetSafeNormal();
		
		if (IsAimMovingTowardPoint(StartingAimVec, DesiredAimVec, VectorToTargetCrit))
		{
			FrictionMultiplier = MovingTowardsCritMultiplier; // Keep it easy to aim towards the crit spot
		}
		else if (IsAimMovingTowardPoint(StartingAimVec, DesiredAimVec, VectorToTargetCockpit))
		{
			FrictionMultiplier = MovingTowardsBaseMarkerMultiplier; // Moving away from crit spot but still towards an important point, clamp sensitivity a lil bit
		}
		else
		{
			FrictionMultiplier = MovingAwayFromMarkersFrictionMultiplier; // Moving away from both, clamp harder
		}
	}

	OutAimRotatorDelta *= FrictionMultiplier;
}

inline bool UBarragePlayerAgent::CalculateAimVector(
	const ActorKey& ActorsKey,
	const FVector3d& ActorLocation,
	const FVector& Direction,
	FVector& OutTargetAimAtLocation,
	FSkeletonKey& TargetKey,
	AActor*& TargetActor) const
{
	UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
	check(Physics);
	FBLet MyFiblet = Physics->GetShapeRef(ActorsKey);
	if(MyFiblet)
	{
		const JPH::IgnoreSingleBodyFilter BodyFilter = Physics->GetFilterToIgnoreSingleBody(MyFiblet);
		
		TSharedPtr<FHitResult> HitObjectResult = MakeShared<FHitResult>();
		Physics->SphereCast(
			0.1f,
			10000.0f, // Hard-coding range for now until we determine how we want to handle range on this
			ActorLocation,
			Direction,
			HitObjectResult,
			BroadPhaseFilter,
			ObjectLayerFilter,
			BodyFilter);

		//DrawDebugLine(GetWorld(), ActorLocation, Direction * 10000.f, FColor::Red, false, 0.4f, 0.f, 0.5f);
		FBarrageKey HitBarrageKey = Physics->GetBarrageKeyFromFHitResult(HitObjectResult);
		if (HitBarrageKey != 0)
		{
			//DrawDebugSphere(GetWorld(), HitObjectResult.Get()->Location, 10.f, 12, FColor::Red, true, 1, SDPG_Foreground, 1.f);
			FBLet AimAtFiblet = Physics->GetShapeRef(HitBarrageKey);
			if (AimAtFiblet)
			{
				UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
				check(TransformDispatch);
		
				TWeakObjectPtr<AActor> AimTarget = TransformDispatch->GetAActorByObjectKey(AimAtFiblet->KeyOutOfBarrage);
				UArtilleryDispatch* ADispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();

				if (AimTarget.IsValid())
				{
					if (ADispatch->DoesEntityHaveTag(AimAtFiblet->KeyOutOfBarrage, FGameplayTag::RequestGameplayTag("Enemy")))
					{
						OutTargetAimAtLocation = AimTarget->GetActorLocation();
						TargetKey = AimAtFiblet->KeyOutOfBarrage;
						TargetActor = AimTarget.Get();
						//DrawDebugSphere(GetWorld(), AimTarget->GetActorLocation(), 10.f, 12, FColor::Yellow, true, 1, SDPG_Foreground, 1.f);
						return true;
					}

					const FVector SearchLocation = HitObjectResult.Get()->Location;
					double DistanceToNearest = 1.f;
					FSkeletonKey ClosestCurrent = FSkeletonKey::Invalid();
					FVector ClosestCurrentLocation(0.f);
					uint32 BodiesFoundNearTarget = 0;
					TArray<uint32> BodyIDsFound;
					BodyIDsFound.Reserve(MAX_FOUND_OBJECTS);
					Physics->SphereSearch(
						MyFiblet->KeyIntoBarrage,
						SearchLocation,
						0.15f,
						BroadPhaseFilter,
						ObjectLayerFilter,
						BodyFilter,
						&BodiesFoundNearTarget,
						BodyIDsFound);

					for (uint32 ActorIndex = 0; ActorIndex < BodiesFoundNearTarget; ++ActorIndex)
					{
						const uint32 BodyID = BodyIDsFound[ActorIndex];
						FBarrageKey BodyBarrageKey = Physics->GenerateBarrageKeyFromBodyId(BodyID);
						FBLet BodyObjectFiblet = Physics->GetShapeRef(BodyBarrageKey);
						if (BodyObjectFiblet)
						{
							FSkeletonKey BodyObjectKey = BodyObjectFiblet->KeyOutOfBarrage;
							if (ADispatch->DoesEntityHaveTag(BodyObjectKey, FGameplayTag::RequestGameplayTag("Enemy")))
							{
								const FVector3f CurrentBodyLoc = FBarragePrimitive::GetPosition(BodyObjectFiblet);
								const double DistanceToCurrent = (SearchLocation - FVector(CurrentBodyLoc)).Length();
								if (DistanceToCurrent < DistanceToNearest)
								{
									DistanceToNearest = DistanceToCurrent;
									ClosestCurrent = BodyObjectKey;
									ClosestCurrentLocation = FVector(CurrentBodyLoc);
								}
							}
						}
					}

					if (ClosestCurrent.IsValid())
					{
						OutTargetAimAtLocation = ClosestCurrentLocation;
						TargetKey = ClosestCurrent;
						TargetActor = TransformDispatch->GetAActorByObjectKey(ClosestCurrent).Get();
					}
				}
		
				OutTargetAimAtLocation = HitObjectResult->Location;
				return false;
			}
		}
		OutTargetAimAtLocation = ActorLocation + (Direction * 3000.f);
		return false;
	}
	return false;
}
