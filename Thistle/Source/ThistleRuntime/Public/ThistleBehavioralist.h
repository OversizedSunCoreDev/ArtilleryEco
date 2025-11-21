// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "SkeletonTypes.h"

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryBusyWorker.h"
#include "GenericSmartObject.h"
#include "SmartObjectSubsystem.h"
#include "ThistleInject.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "ThistleBehavioralist.generated.h"

class UThistleStateTreeLease;

DECLARE_DELEGATE_OneParam(AwakenTagQueryDecorator, TWeakObjectPtr<UBehaviorTreeComponent>)
static constexpr int32 MAX_ENEMY_COUNT = 2000;
/**
 * This tickable system manages providing faction information, group labels, enemies, and assigning behavior or state trees.
 * It also provides a shim from our ECS to blackboards as needed, though the ECS tooling mostly makes blackboards unnecessary complexity.
 *
 * Much of what we want to do is implemented in the Gameplay Behaviors subsystem.
 * https://zomgmoz.tv/unreal/Smart-Objects/UGameplayBehavior_BehaviorTree
 *
 * Behavioralist will likely become the manager for coordinating the Gameplay Behaviors and the smart objects.
 * 
 * https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/BehaviorTree/Blackboard
 * 
 */
UCLASS()
class THISTLERUNTIME_API UThistleBehavioralist : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()

public:
	friend class UThistleDispatch;
	static constexpr uint8_t WIDE_CADENCE = 8;
	
	UThistleBehavioralist();
	
	//pool behavior trees?
	using ActorToTreeMapping = TMap<ActorKey, TObjectPtr<UBehaviorTreeComponent>>;
	using AwakenToTreeMapping = TMap<AwakenTagQueryDecorator*, TObjectPtr<UBehaviorTreeComponent>>;
	using Deadline = TTuple<FSkeletonKey, FNativeGameplayTag&, bool>;
	using DeadlineArray = TArray<Deadline>;
	//deadliners MUST be factored out into a class to allow for both sanity and correct rollback.
	//specifically, we'll need to add a behavior where they don't _actually_ remove an array until the rollback window
	//on reversing that array's effects expires. the good news is that just looks like [contains(now), remove(now-window+1)]
	//same as our tombstoning system in fblets, really, just a little smaller because we're operating on things
	//that don't HAVE lifecycles here.
	using Deadliner = TSortedMap<int, DeadlineArray>;
	
	static inline UThistleBehavioralist* SelfPtr = nullptr;
	
protected:
	int DeadlinerTime = 0;
	
public:
	//in ticks! in _TICKS_
	//we'll eventually want this customizable. hoof.
	const static int DelayBetweenMoveOrders = 32;
	const static int DelayBetweenAttackOrders = 32;
	const static int DelayBetweenRallyOrders = 32;
	const static int DelayBetweenTargetOrders = 32;
	//REGISTRATION MACHINERY
	constexpr static int CompletionOrdinationKey = ORDIN::E_D_C::EnemyTagState;
	constexpr static int OrdinateSeqKey = UBarrageDispatch::OrdinateSeqKey  + ORDIN::Step;
	
	virtual bool RegistrationImplementation() override;
	void BounceTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int BounceDuration) const;
	void DelayedTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int BounceTime);
	void ExpireTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int BounceTime);
	void TimedTagsMaintenance(int CurrentTck);

	//this could COMFORTABLY use the postinitializer. it was a simple and good case to demonstrate another
	//approach though. I hope this remains only a curiosity and not a standard pattern.
	struct THISTLERUNTIME_API FTagRegistration : public ISkeletonLord, public ICanReady
	{
		FTagRegistration()
		{
		}

		virtual bool RegistrationImplementation() override
		{
			if (UThistleBehavioralist::SelfPtr)
			{
				UThistleBehavioralist::SelfPtr->BehavioralistTagState->Initialize(
					UThistleBehavioralist::SelfPtr->FalseActorKey,
					UThistleBehavioralist::SelfPtr->MyDispatch);
				return true;
			}
			return false;
		}
	};
	
	AwakenToTreeMapping DecoratorsMap;
	UPROPERTY(BlueprintReadWrite)
	FSkeletonKey FalseActorKey;
	FTagRegistration TagRegistration = FTagRegistration();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure)
	static FGameplayTag RallyStateTag();
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static bool AttemptInvokePathingOnKey(FSkeletonKey Target, FVector Location);
	static bool AttemptAimFromKey(FSkeletonKey From, FRotator TargetRotation);
	static bool AttemptAttackFromKey(FSkeletonKey From);
	void RegisterEnemy(const ActorKey NewKey, uint64_t Stamp);

	UFUNCTION(BlueprintCallable, meta=(DefaultToSelf="RallyRegistering"))
	void RegisterRallyPoint(const FSkeletonKey& NewKey, AGenericSmartObject* RallyRegistering);
	UFUNCTION(BlueprintCallable, meta=(DefaultToSelf="PatrolZoneRegistering"))
	void RegisterPatrolZone(const FSkeletonKey& NewKey, AActor* PatrolZoneRegistering);
	void RegisterTagQueryCapableDecorator(TObjectPtr<UBehaviorTreeComponent> UBehaviorTreeComponent, AwakenTagQueryDecorator* BindAwaken);
	void DeregisterTagQueryCapableDecorator(AwakenTagQueryDecorator* BindAwaken);
	UPROPERTY()
	int Some = 3;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int TargetChaffEnemyCount = 300;
	
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	UArtilleryGameplayTagContainer* BehavioralistTagState;
	//ACCESS ONLY FROM GAME THREAD FOR NOW.
	TMap<FSkeletonKey, bool> RecentlyProcessed;
	UFUNCTION(BlueprintCallable)
	int GetLiveEnemyCount();	
	UFUNCTION(BlueprintCallable)
	TArray<AGenericSmartObject*> GetSomeRallyPoints(FVector Location, float Range);
	UFUNCTION(BlueprintCallable)
	void SightLinesUpdate(const TArray<AActor*>& VisibleByActor, FSkeletonKey Perceptor);
	UFUNCTION(BlueprintCallable)
	FSkeletonKey GetCurrentPlayer();
	void ProcessRallyPoint();
	void DeregisterEnemy(const ActorKey& KeyToRemove);
	bool EmptyRecentCued = false;
	void CueEmptyRecent();
	void EmptyRecent();
	void Update(uint64_t CurrentTick);
	uint32 GetEnemiesWithinRangeOfPoint(const FVector& Location, double Range, ActorKeyArray& OutEnemyKeyArray);
	void CullDeadEnemies();

	//TODO: GENERALIZE THIS TO NOT REQUIRE A SPECIFIC THISTLE INJECT. RIGHT NOW, WE KINDA GOTTA DO IT THIS WAY
	//OR WE'LL HAVE DELEGATE SOUP THAT CANNOT BE DEBUGGED. TBH, these could prolly be ticklites but I haven't brain for that.
	TSharedPtr<TMap<ActorKey, TObjectPtr<AThistleInject>>> ActorToThistleAIMapping;
	TSharedPtr<TMap<FBarrageKey, TObjectPtr<AThistleInject>>> BarrageToThistleAIMapping;
	TSharedPtr<TMap<FSkeletonKey, UThistleStateTreeLease*>> EntityToArtilleryBehavior;
	void RunAILocomotions() const;
	void RunStateTrees(uint64_t CurrentTck) const;
	bool IsPlayerInCombat() const;
	UArtilleryDispatch* MyDispatch;    
	ActorKeyArray CurrentEnemies;

	using RallyMap = TMap<FSkeletonKey, TObjectPtr<AGenericSmartObject>>;
	UPROPERTY()
	float RallyWatermark = 0.6;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FSkeletonKey, TObjectPtr<AGenericSmartObject>> ManagedRallyPointSmartObjects;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FSkeletonKey, TObjectPtr<AActor>> ManagedPatrolZones;
	ActorKeyArray DeadEnemies;
	USmartObjectSubsystem* SmartObjectSubsystem;

	void OnPhysicsCollision(const BarrageContactEvent& ContactEvent)
	{
		if (ContactEvent.ContactEntity1.MyLayer == Layers::EJoltPhysicsLayer::ENEMY
			)
		{
			ForwardCollision(ContactEvent.ContactEntity1, ContactEvent);
		}
		if (ContactEvent.ContactEntity2.MyLayer == Layers::EJoltPhysicsLayer::ENEMY
	)
		{
			ForwardCollision(ContactEvent.ContactEntity2, ContactEvent);
		}
	}

	void ForwardCollision(BarrageContactEntity which, const BarrageContactEvent& ContactEvent)
	{
		//Reenable this only if there's a good reason to: Processing these in this way gets very expensive.
		/*
		//why didn't we put skeleton keys in the contact events?
		if (UBarrageDispatch::SelfPtr)
		{
			FBLet quickfib = UBarrageDispatch::SelfPtr->GetShapeRef(which.ContactKey);
			bool ProbablyValid = FBarragePrimitive::IsNotNull(quickfib);
			if (ProbablyValid)
			{
				FSkeletonKey KeyIntoArtillery = quickfib->KeyOutOfBarrage;
				quickfib.Reset();
				if (KeyIntoArtillery.IsValid())
				{
					auto actor = ActorToThistleAIMapping->Find(KeyIntoArtillery);
					if (actor)
					{
						//actor->Get()->OnPhysicsCollision(ContactEvent);
					}
				}
			}
		}
		*/
	}
	
protected:
	TSharedPtr<Deadliner> ExpirationDeadliner;
};
