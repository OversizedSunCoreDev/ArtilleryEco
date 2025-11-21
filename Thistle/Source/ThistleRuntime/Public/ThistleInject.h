// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Destructible.h"
#include "GameFramework/Pawn.h"
#include "GenericTeamAgentInterface.h"
#include "ArtilleryRuntime/Public/Systems/ArtilleryDispatch.h"
#include "FMockArtilleryGun.h"
#include "RequestDrivenKine.h"
#include "UEnemyMachine.h"
#include "NavigationSystem.h"
#include "PhysicsTypes/BarrageAutoBox.h"
#include "ThistleInject.generated.h"

/*
* One of the two hearts of thistle
* This is intended to be a either a slight expansion of pawn or an alternative to physicalized pawns.
* Our goal is to allow behavior trees to control squads and similar in elegant ways and that means we want
* something that lets large numbers of actors subscribe to and fulfill tasks from Mass and BehaviorTrees.
* 
* It's TOTALLY possible this'll prove a hassle or that we can use the InjectionComponent here in thistle instead. 
* That latter option? it'd be ideologically preferable, as a pawn is a pretty heavyweight inheritance\asset chunk 
* that basically ends up at least partially owning the rigid body, though often NOT the mesh. I've seen projects 
* that ended up with quite a few pawns in an asset's hierarchy, and that was..... grim.
* 
* Traditionally, this sort of stuff works by building infrastructure around AIControllers. See:
* https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/AAIController
* 
* For us, we'd like to stick with that core paradigm absolutely as much as possible, but we have a problem:
* AIControllers normally only run on the server in networked games.
* 
* There are VERY good reasons for this. It's borderline impossible to make a truly deterministic game otherwise,
* without the aggressive use of rollback and network prediction. This can lead to situations where an AI actor shoots
* one character on their player's machine, and another on some other player's machine. Reconciling this in a network-predicted
* game is really quite unpleasant. A lot of thought and work has gone into it, and Iris has powerful support for it.
* 
* The problem is that we don't really want to have our AI run at such a delay, and we'd really like to clock our server quite slow.
* 
* There are four good solves: 
*	Have the AI produce slightly longer tasks than usual.
*	Manage only tasks that must be coherent across all players on the server-side, but manage them entirely.
*		So shoot and similar only happen server side, but pathing is local. 
*	Grin and bear it, and hope that the fully server defaults are fast enough.
*	Mad shit. Destiny 2 style. Server picks targets. Local executes.
* 
* It's not clear to me which way to go yet. If we go with 3, this component is just an attachment point for the standard AI controller
* or a way of consuming commands from it in the case of a squad. This isn't the worst! Actually, it's pretty good!
* 
* Of probable interest are:
* https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/BehaviorTree/Tasks/UBTTask_RunBehavior
* https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/BehaviorTree/Tasks/UBTTask_MoveTo
* 
* 
* Mass Entity and Mass Avoidance:
* https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-entity-in-unreal-engine
* https://dev.epicgames.com/documentation/en-us/unreal-engine/mass-avoidance-overview-in-unreal-engine
* https://dev.epicgames.com/community/learning/tutorials/JXMl/unreal-engine-your-first-60-minutes-with-mass
* 
*/

UENUM()
enum EnemyCategory
{
	Ground = 0,
	Flyer = 1,
};

UENUM(BlueprintType)
enum class EThistleMoveState : uint8
{
	Idle,
	Moving,
	SlowingDown,
	ReactingToPush,
	FollowingLeader
};

UCLASS()
class THISTLERUNTIME_API AThistleInject : public ADestructible
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FString GunDefinitionID;
	/** Properties that define how the component can move.
	 * normally on character movement... but we don't USE that.
	 * We can't.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NavMovement, meta = (DisplayName = "Movement Capabilities", Keywords = "Nav Agent"))
	FNavAgentProperties NavAgentProps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NavMovement, meta = (DisplayName = "Can I Move?", Keywords = "Nav Agent"))
	bool EnableTakeMoveOrder = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = NavMovement, meta = (DisplayName = "Can I Move?", Keywords = "Nav Agent"))
	bool SoftFollowMode = false;
	signed int AdjustSpeedBy = 0;
	bool bGroundful = true;

	static inline constexpr int RoughTickPeriod = 1000000 / HERTZ_OF_BARRAGE; //swap to microseconds. standardizing.
	static inline constexpr int GroundfulPeriod = RoughTickPeriod * 28;
	ArtilleryTime LastGroundful = 0;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArrivalAtDestination);

	ManagedRequestingKine MainGunKine;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Thistle)
	USceneComponent* MyMainGun;
	//You are expected to call finish dying on your death being ready to tidy up.
	virtual void FinishDeath() override;
	// Sets default values for this pawn's properties
	AThistleInject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UPROPERTY(BlueprintAssignable, Category = "Thistle")
	FOnArrivalAtDestination OnArrivalAtDestination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NavMovement, meta = (DisplayName = "Movement Capabilities", Keywords = "Nav Agent"))
	float SearchRangeForFollow = 800; //8 meters.

	// flag to mark if enemy is idle or not
	bool Idle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float MaxHP = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float MaxShields = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float MaxWalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float Acceleration = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float StoppingTime = 0.2f;

	// this isn't gonna be thread safe but I honestly just want to try it out
	UPROPERTY()
	bool atDestination = false;
	virtual bool RegistrationImplementation() override;
	FVector FinalDestination;
	FNavPathSharedPtr Path;
	int32_t NextPathIndex = 0;
	FVector LastTickPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Thistle)
	TEnumAsByte<EnemyCategory> EnemyType;

	virtual FGenericTeamId GetGenericTeamId() const override { return myTeam; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGunKey Attack = DefaultGunKey;

	UFUNCTION(BlueprintCallable, Category = "Abilities", meta = (BlueprintThreadSafe))
	void FireAttack();

	//Rotate Main Gun is intended to abstract away the difference between humanoid, non-humanoid, squad, and disjoint
	//enemies by allowing a blueprint user to specify a rotator representing the world direction that the final aim should follow
	//however, it may be that we'll want to switch to something more declarative, namely a target and mode (direct, indirect, occluded)
	//or it may be that this abstraction simply proves too leaky. It should get us through February though.
	//TODO: Revisit 2/20/25 --J
	UFUNCTION(BlueprintCallable, Category = "Thistle")
	virtual bool RotateMainGun(FRotator RotateTowards, ERelativeTransformSpace OperatingSpace);
	bool EngageNavSystem(FVector3f To);

	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool MoveToPoint(FVector3f To);
	FGenericTeamId myTeam = FGenericTeamId(7);

	// runs physics calls
	void LocomotionStateMachine();

	// Temporary flag to align component rotations with expected Aim direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thistle")
	bool bRotationCorrection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior", meta = (DisplayName = "Minimum Engagement Range"))
	float MinEngagementRange = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior", meta = (DisplayName = "Maximum Engagement Range"))
	float MaxEngagementRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Navmesh Following")
	bool bShouldFollowNavmeshHeight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Navmesh Following", meta = (DisplayName = "Navmesh Hover Height"))
	float NavmeshHoverHeight = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Navmesh Following", meta = (DisplayName = "Navmesh Hover Forward Project"))
	float NavmeshForwardProject = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Navmesh Following", meta = (DisplayName = "Navmesh Projection Search Extent"))
	FVector NavmeshProjectionSearchExtent = FVector(0, 0, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior", meta = (DisplayName = "Detection Range"))
	float DetectionRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior", meta = (DisplayName = "Activated State Tag"))
	FGameplayTag ActivatedStateTag;

	//Step
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Max Step Height", ClampMin = "0.0", UIMin = "0.0"))
	float MaxStepHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Step Detection Forward Distance", ClampMin = "0.0", UIMin = "0.0"))
	float StepProbeDistance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Step Up Velocity Boost", ClampMin = "0.0", UIMin = "0.0"))
	float StepUpVerticalVelocity = 300.f;

	float StepUpCooldownTimer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Step Up Impulse Strength", ClampMin = "0.0", UIMin = "0.0"))
	float StepUpImpulse = 400.f;

	// Added a tunable cooldown to prevent spamming step checks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Step Up Cooldown", ClampMin = "0.0", UIMin = "0.0"))
	float StepUpCooldown = 0.2f;

	float StuckTimer = 0.f;
	float UnStuckTimer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Stuck Time Threshold", ClampMin = "0.0", UIMin = "0.0"))
	float StuckTimeThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Stuck Velocity Threshold", ClampMin = "0.0", UIMin = "0.0"))
	float StuckVelocityThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement|Stepping", meta = (DisplayName = "Min Stuck Velocity", ClampMin = "0.0", UIMin = "0.0"))
	float MinStuckVelocity = 0.0f;

	bool CheckStuck(float DeltaSeconds);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior", meta = (DisplayName = "Rotation Speed"))
	float RotationSpeed = 1.f;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Handler for physics collisions
	void OnPhysicsCollision(const BarrageContactEvent ContactEvent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Movement")
	EThistleMoveState MoveState = EThistleMoveState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement")
	float MaxForce = 500.0f;


	FVector CurrentVelocity = FVector::ZeroVector;

	// State specific handlers
	void HandleIdleState();
	void HandleMovingState();
	void HandleSlowingDownState();

	FVector Seek(const FVector& Target);

	// Damping factor to apply to velocity, acts like friction to prevent sliding and reduce jitter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damping = 0.5f;

	// Damping to apply to linear velocity while in the ReactingToPush state. Controls how much the AI slides.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (DisplayName = "Linear Damping When Pushed", ClampMin = "0.0", UIMin = "0.0"))
	float LinearDampingWhenPushed = 0.5f;

	// Damping to apply to angular velocity while in the ReactingToPush state. Controls how much the AI resists spinning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (DisplayName = "Angular Damping When Pushed", ClampMin = "0.0", UIMin = "0.0"))
	float AngularDampingWhenPushed = 0.1f;

	// The radius around a destination where the AI will begin to slow down.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ArrivalRadius = 200.0f;

	// How far along the path to look ahead for steering. This enables corner cutting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PathLookaheadDistance = 300.0f;

	// How long (in seconds) the AI will yield after being pushed by the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PushReactionTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SteerInterp = 1.f;


	float PushReactionTimer = 0.0f;
	EThistleMoveState StateBeforePush = EThistleMoveState::Idle;

	FVector CalculateSteeringForce(const FVector& Target, bool bUseArrival);


	//UFUNCTION()
	//void OnPathfindingComplete(uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr FoundPath);

	FVector PushVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Movement")
	float PlayerPushForceMagnitude = 1000.0f;

	void UpdatePathAfterDisplacement();

	float CompareNavMeshHeight();

	void AimRotateMeshComponent();

};