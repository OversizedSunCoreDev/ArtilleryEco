// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Destructible.h"
#include "GameFramework/Pawn.h"

#include "GenericTeamAgentInterface.h"
#include "ArtilleryRuntime/Public/Systems/ArtilleryDispatch.h"
#include "FMockArtilleryGun.h"
#include "UEnemyMachine.h"
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
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArrivalAtDestination);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Thistle)
	USceneComponent* MyMainGun;
	//You are expected to call finish dying on your death being ready to tidy up.
	virtual void FinishDeath() override;
	// Sets default values for this pawn's properties
	AThistleInject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UPROPERTY(BlueprintAssignable, Category = "Thistle")
	FOnArrivalAtDestination OnArrivalAtDestination;
	//~ Begin INavAgentInterface Interface
	virtual const FNavAgentProperties& GetNavAgentPropertiesRef() const override;

	// flag to mark if enemy is idle or not
	bool Idle = false;
	
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
	FVector3f FinalDestination;
	FNavPathSharedPtr Path;
	int32_t NextPathIndex = 0;
	FVector3f LastTickPosition;

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
	
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool MoveToPoint( FVector3f To);
	FGenericTeamId myTeam = FGenericTeamId(7);

	// runs physics calls
	void LocomotionStateMachine();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};


