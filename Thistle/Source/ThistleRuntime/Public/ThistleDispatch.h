// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryDispatch.h"
#include "GenericQuadTree.h"
#include "KeyedConcept.h"
#include "ORDIN.h"
#include "Subsystems/WorldSubsystem.h"
#include "ThistleInject.h"

#include "ThistleDispatch.generated.h"

class UArtillerySkeletalMeshDispatch;
class UThistleBehavioralist;
/**
 * Component for running the dispatch of tasks to units, allowing us to mix Mass and Behaviortree and Statetree
 * transparently, as well as shim RPAI if we like that. Might not be useful at all, see other headers for more!
 * 
 * In particular, start with Treemanager and ThistleInject
 */
UCLASS()
class THISTLERUNTIME_API UThistleDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy
{
	GENERATED_BODY()
    
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
protected:
	UPROPERTY()
	TObjectPtr<UThistleBehavioralist> ThistleBehavioralist;
	
	UPROPERTY()
	TObjectPtr<UArtillerySkeletalMeshDispatch> SkeletalMeshDispatch;

public:
	TSharedPtr<TQuadTree<TPair<ActorKey, FVector2d>>> QuadTreeForDistance;
	bool QuadTreeMaintenance = false;
	
	virtual void ArtilleryTick(uint64_t TicksSoFar) override;
	
	// @todo temp anim state register code until I make something much better
	bool RegisterNewActorAnimState(const ActorKey NewKey, AActor* Actor);
	void UnregisterActorAnimState(const ActorKey ActorKey);

private:
	constexpr static int OrdinateSeqKey = ORDIN::E_D_C::SecondaryEnemyDispatch;
	virtual bool RegistrationImplementation() override; 
};
