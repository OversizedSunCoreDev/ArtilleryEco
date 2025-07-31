	#include "ThistleBehavioralist.h"
#include "ArtilleryDispatch.h"
#include "ThistleStateTreeCore.h"
#include "NativeGameplayTags.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "SmartObjectRequestTypes.h"
#include "ThistleDispatch.h"
#include "TransformDispatch.h"
#include "Public/GameplayTags.h"

UThistleBehavioralist::UThistleBehavioralist(): MyDispatch(nullptr), SmartObjectSubsystem(nullptr)
{
	ActorToAILocomotionMapping = MakeShareable(new TMap<ActorKey, TObjectPtr<AThistleInject>>());
	EntityToArtilleryBehavior = MakeShareable(new TMap<FSkeletonKey, UThistleStateTreeLease*>());
	ExpirationDeadliner = MakeShareable(new Deadliner());
	BehavioralistTagState = NewObject<UArtilleryGameplayTagContainer>();
}

bool UThistleBehavioralist::RegistrationImplementation()
{
	FalseActorKey = MAKE_ACTORKEY(this);
	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
	CurrentEnemies.Reserve(MAX_ENEMY_COUNT);
	// TODO - Add debug option to toggle messages as these are starting to impact perf
	//UE_LOG(LogTemp, Warning, TEXT("ThistleBehavioralist:Subsystem: Inbound and Outbound Queues set to null."));
	FArtilleryUpdateEnemyControllerSubsystem Callback;
	Callback.BindUObject(this, &UThistleBehavioralist::Update);
	FArtilleryAddEnemyToControllerSubsystem Register;
	Register.BindUObject(this, &UThistleBehavioralist::RegisterEnemy);
	MyDispatch->RegisterEnemySubsystem(Callback, Register);
	SelfPtr = this;
	return true;
}

void UThistleBehavioralist::BounceTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int Duration) const
{
	bool found = false;
	int stamp = DeadlinerTime + Duration;
	if (found)
	{
		MyDispatch->RemoveTagFromEntity(Key, Tag);
	}
	DeadlineArray* StampDeadline = ExpirationDeadliner->Find(stamp);
	if (StampDeadline == nullptr)
	{
		ExpirationDeadliner->Add(stamp, {{Key, Tag, true}});
	}
	else
	{
		StampDeadline->Add({Key, Tag, true});
	}
}

void UThistleBehavioralist::DelayedTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int Duration)
{
	int stamp = DeadlinerTime + Duration;
	DeadlineArray* StampDeadline = ExpirationDeadliner->Find(stamp);
	if (StampDeadline == nullptr)
	{
		ExpirationDeadliner->Add(stamp, {{Key, Tag, true}});
	}
	else
	{
		StampDeadline->Add({Key, Tag, true});
	}
}

void UThistleBehavioralist::ExpireTag(FSkeletonKey Key, FNativeGameplayTag& Tag, int Duration)
{
	bool found = false;
	int stamp = DeadlinerTime + Duration;
	if (found)
	{
		MyDispatch->AddTagToEntity(Key, Tag);
	}

	DeadlineArray* StampDeadline = ExpirationDeadliner->Find(stamp);
	if (StampDeadline == nullptr)
	{
		ExpirationDeadliner->Add(stamp, {{Key, Tag, false}});
	}
	else
	{
		StampDeadline->Add({Key, Tag, false});
	}
}

void UThistleBehavioralist::TimedTagsMaintenance(int CurrentTck)
{
	DeadlinerTime = CurrentTck; //odd, I know, but it allows for rollbacks.
	if (ExpirationDeadliner->Contains(CurrentTck))
	{
		DeadlineArray AnyToExpire = ExpirationDeadliner->FindAndRemoveChecked(CurrentTck);
		for (TTuple<FSkeletonKey, FNativeGameplayTag&, bool>& Goner : AnyToExpire)
		{
			bool found = false;
			FConservedTags tagc = UArtilleryLibrary::InternalTagsByKey(Goner.Get<0>(), found);
			if (found)
			{
				if (Goner.Get<2>())
				{
					tagc->Add(Goner.Get<1>());
				}
				else
				{
					tagc->Add(Goner.Get<1>());
				}
			}
		}
	}
}

void UThistleBehavioralist::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
	//huh, this idiom for breaking up ordered registration is actually really nice and provides a really obvious separation.
	UOrdinatePillar* OrdinatePillar = GetWorld()->GetSubsystem<UOrdinatePillar>();
	OrdinatePillar->REGISTERLORD(ORDIN::E_D_C::EnemyTagState, &(this->TagRegistration), &(this->TagRegistration));
}

void UThistleBehavioralist::OnWorldBeginPlay(UWorld& InWorld)
{
	//UE_LOG(LogTemp, Warning, TEXT("ThistleBehavioralist:Subsystem: World beginning play"));
	Super::OnWorldBeginPlay(InWorld);
	SmartObjectSubsystem = InWorld.GetSubsystem<USmartObjectSubsystem>();
}

void UThistleBehavioralist::Deinitialize()
{
	EntityToArtilleryBehavior->Empty();
	// unlike the others, we can't trust this one. Actually, we prolly can't trust them either.
	ActorToAILocomotionMapping->Empty();
	ExpirationDeadliner->Empty();
	Super::Deinitialize();
}

FGameplayTag UThistleBehavioralist::RallyStateTag()
{
	return TAG_Status_Multimode_Rallying;
}

void UThistleBehavioralist::Tick(float DeltaTime)
{
	EmptyRecent();
}

TStatId UThistleBehavioralist::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UThistleDispatch, STATGROUP_Tickables);
}

bool UThistleBehavioralist::AttemptInvokePathingOnKey(FSkeletonKey Target, FVector Location)
{
	if (UThistleBehavioralist::SelfPtr)
	{
		TSharedPtr<TMap<ActorKey, TObjectPtr<AThistleInject>>> HoldOpen = UThistleBehavioralist::SelfPtr->ActorToAILocomotionMapping;
		if (HoldOpen)
		{
			TObjectPtr<AThistleInject>* Hold = HoldOpen->Find(Target);
			if (Hold)
			{
				//todo get rid of these fucking floats.
				return Hold->Get()->MoveToPoint(FVector3f(Location));
			}
		}
	}
	return false;
}

bool UThistleBehavioralist::AttemptAimFromKey(FSkeletonKey From, FRotator TargetRotation)
{
	if (UThistleBehavioralist::SelfPtr)
	{
		TSharedPtr<TMap<ActorKey, TObjectPtr<AThistleInject>>> HoldOpen = UThistleBehavioralist::SelfPtr->ActorToAILocomotionMapping;
		if (HoldOpen)
		{
			TObjectPtr<AThistleInject>* Hold = HoldOpen->Find(From);
			if (Hold)
			{
				//todo get rid of these fucking floats.
				return Hold->Get()->RotateMainGun(TargetRotation, RTS_World);
			}
		}
	}
	return false;
}

bool UThistleBehavioralist::AttemptAttackFromKey(FSkeletonKey From)
{
	if (UThistleBehavioralist::SelfPtr)
	{
		TSharedPtr<TMap<ActorKey, TObjectPtr<AThistleInject>>> HoldOpen = UThistleBehavioralist::SelfPtr->ActorToAILocomotionMapping;
		if (HoldOpen)
		{
			TObjectPtr<AThistleInject>* Hold = HoldOpen->Find(From);
			if (Hold)
			{
				//todo get rid of these fucking floats.
				Hold->Get()->FireAttack();
				return true;
			}
		}
	}
	return false;
}

void UThistleBehavioralist::RegisterEnemy(const ActorKey NewKey, uint64_t Stamp)
{
	CurrentEnemies.Add(NewKey);
	UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
	if (TransformDispatch)
	{
		TWeakObjectPtr<AActor> EnemyActor = TransformDispatch->GetAActorByObjectKey(NewKey);
		MyDispatch->AddTagToEntity(NewKey, FGameplayTag::RequestGameplayTag("Enemy"));
		if (EnemyActor.Get())
		{
			//YOU MUST CONSTRUCT. LMAO. scope this lol
			//this might end up sandblasting the object when the pointer is destroyed. lmao.
			TObjectPtr<AThistleInject> Enemy = Cast<AThistleInject, AActor>(EnemyActor.Get());
			ActorToAILocomotionMapping->Add(NewKey, Enemy);
			if (UThistleStateTreeLease* AThingToTick = Enemy->GetComponentByClass<UThistleStateTreeLease>())
			{
				EntityToArtilleryBehavior->Add(NewKey, AThingToTick);
			}
		}
	}
	else
	{
		DeregisterEnemy(NewKey); //die. TODO: fix.
	}
}

void UThistleBehavioralist::RegisterRallyPoint(const FSkeletonKey& NewKey, AGenericSmartObject* RallyRegistering)
{
	ManagedRallyPointSmartObjects.Add(NewKey, RallyRegistering);
	SmartObjectSubsystem->RegisterSmartObject(*RallyRegistering->GetComponentByClass<USmartObjectComponent>());
}

void UThistleBehavioralist::RegisterPatrolZone(const FSkeletonKey& NewKey, AActor* PatrolZoneRegistering)
{
	ManagedPatrolZones.Add(NewKey, PatrolZoneRegistering);
}

void UThistleBehavioralist::RegisterTagQueryCapableDecorator(
	TObjectPtr<UBehaviorTreeComponent> UBehaviorTreeComponent, AwakenTagQueryDecorator* BindAwaken)
{
	DecoratorsMap.Add(BindAwaken, UBehaviorTreeComponent);
}

void UThistleBehavioralist::DeregisterTagQueryCapableDecorator(AwakenTagQueryDecorator* BindAwaken)
{
	DecoratorsMap.Remove(BindAwaken);
}

int UThistleBehavioralist::GetLiveEnemyCount()
{
	return ActorToAILocomotionMapping->Num();
}

TArray<AGenericSmartObject*> UThistleBehavioralist::GetSomeRallyPoints(FVector Location, float Range)
{
	TArray<AGenericSmartObject*> RetSet;
	for (TTuple<FSkeletonKey, TObjectPtr<AGenericSmartObject>>& x : ManagedRallyPointSmartObjects)
	{
		if ((Location - x.Value->GetActorLocation()).Length() < Range)
		{
			RetSet.Add(x.Value);
			if (RetSet.Num() > Some)
			{
				return RetSet;
			}
		}
	}
	return RetSet;
}

void UThistleBehavioralist::SightLinesUpdate(const TArray<AActor*>& VisibleByActor, FSkeletonKey Perceptor)
{
	if (RecentlyProcessed.Contains(Perceptor))
	{
		return;
	}

	//remove this when we want to do stuff involving enemies that can see each OTHER.
	if (BehavioralistTagState->HasTag(TAG_Perception_Player_Seen))
	{
		return;
	}
	
	int VisibleAllies = 0;
	RecentlyProcessed.Add(Perceptor, true);

	for (AActor* Seen : VisibleByActor)
	{
		if (Seen && Seen->Implements<UGenericTeamAgentInterface>())
		{
			IGenericTeamAgentInterface* BindIntFunc = reinterpret_cast<IGenericTeamAgentInterface*>(Seen);
			if (BindIntFunc->GetGenericTeamId() == 1)
			{
				//player
				BehavioralistTagState->AddTag(TAG_Perception_Player_Seen);
			}
			else if (BindIntFunc->GetGenericTeamId() == 7)
			{
				VisibleAllies++;
				//enemy
				if (BehavioralistTagState->HasTag(TAG_Perception_Player_Seen))
				{
					//we can use this to guide AI with tag state.
				}
			}
		}
	}
}

FSkeletonKey UThistleBehavioralist::GetCurrentPlayer()
{
	return UArtilleryLibrary::GetLocalPlayerKey_LOW_SAFETY();
}

void UThistleBehavioralist::ProcessRallyPoint()
{
	for (TTuple<FSkeletonKey, TObjectPtr<AGenericSmartObject>>& RallyKeyAndSmartObject : ManagedRallyPointSmartObjects)
	{
		USmartObjectComponent* Smartness = RallyKeyAndSmartObject.Value->GetComponentByClass<USmartObjectComponent>();
		if (Smartness && SmartObjectSubsystem)
		{
			const FSmartObjectHandle LiveHandle = Smartness->GetRegisteredHandle();
			if (!LiveHandle.IsValid())
			{
				continue; // a problem for later
			}
			FSmartObjectRequestFilter Claimed;
			Claimed.bShouldIncludeClaimedSlots = true;
			TArray<FSmartObjectSlotHandle> Slots;
			SmartObjectSubsystem->FindSlots(LiveHandle, Claimed, Slots);

			float count = 0; // currently, we just check to see if a rally is "fullish" before dispatching.
			for (FSmartObjectSlotHandle& slot : Slots)
			{
				if (slot.IsValid())
				{
					FSmartObjectSlotView that = SmartObjectSubsystem->GetSlotView(slot);
					count += that.GetState() == ESmartObjectSlotState::Claimed || !that.IsEnabled();
				}
			}

			const bool RallyReady = count / Slots.Num() >= RallyWatermark;
			if (RallyReady)
			{
				FBox box = Smartness->GetSmartObjectBounds();
				int radius = box.GetSize().Length();
				//we effectively double the size of the rally point by doing this. this is intended.
				ActorKeyArray AxisPowers;
				AxisPowers.Init(ActorKey(), MAX_ENEMY_COUNT);

				const uint32 EnemyCount = GetEnemiesWithinRangeOfPoint(box.GetCenter(), radius, AxisPowers);
				for (uint32 EnemyIndex = 0; EnemyIndex < EnemyCount; ++EnemyIndex)
				{
					ActorKey& Enemy = AxisPowers[EnemyIndex];
					MyDispatch->RemoveTagFromEntity(Enemy, TAG_Status_Multimode_Rallying);
					MyDispatch->AddTagToEntity(Enemy, TAG_Status_Multimode_NoRallying);
					MyDispatch->AddTagToEntity(Enemy, TAG_Status_Multimode_Assault);
				}
			}
		}
	}
	//eventually, this will start a SINGLE run of the test satisfaction for the rally point in such a way that it becomes
	//deterministic. EG: we will check if it WAS satisfied at a point in the past, then kick off the next cycle of the
	//rally point logic on the game thread. this is an example of slow determinism, but I have some serious anxieties.
	//that said, because it's a point in SPACE with a wide bound in TIME it should be deterministic the vast majority of the time.
	//This means that we can just spin the resim tumblers until we reach a fixed universe if we can't come up with a better solve.
	//(there are better solves, namely force-killing the task if it's not done by the time we hit the end of the wide cadence window.)
}

void UThistleBehavioralist::DeregisterEnemy(const ActorKey& KeyToRemove)
{
	EntityToArtilleryBehavior->Remove(KeyToRemove);
	// TODO: replace this with something less rancid if it ever shows up in perf profiling.
	ActorToAILocomotionMapping->Remove(KeyToRemove);
	CurrentEnemies.Remove(KeyToRemove);
	//this is a strong tobject ptr, so we actually won't kill the actor til this is released.
}

void UThistleBehavioralist::CueEmptyRecent()
{
	EmptyRecentCued = true;
}

void UThistleBehavioralist::EmptyRecent()
{
	if (EmptyRecentCued)
	{
		RecentlyProcessed.Empty();
		AActor* PK = UArtilleryLibrary::GetLocalPlayer_UNSAFE();
		if (PK)
		{
			FVector PlayerLoc = PK->GetActorLocation();
			ActorKeyArray AKA;
			if (GetEnemiesWithinRangeOfPoint(PlayerLoc, 300, AKA) <= 0)
			{
				BehavioralistTagState->RemoveTag(TAG_Perception_Player_Seen);
			}
		}

		EmptyRecentCued = false;
	}
}

// Here we place all of the things we want the system to do on game update, this method is connected to artillery
// and runs every artillery update in a similar way to LocomotionStateMachine
void UThistleBehavioralist::Update(uint64_t CurrentTck)
{
	//this maybe be replaced by a full Harvester.
	if (CurrentTck % WIDE_CADENCE == 0)
	{
		ProcessRallyPoint();
		CueEmptyRecent();
	}
	CullDeadEnemies();
	RunStateTrees(CurrentTck);
	RunAILocomotions();
	TimedTagsMaintenance(CurrentTck);
	GetWorld()->GetSubsystem<UThistleDispatch>()->ArtilleryTick(CurrentTck);
}

void UThistleBehavioralist::RunAILocomotions() const
{
	if (ActorToAILocomotionMapping.IsValid())
	{
		for (const TTuple<ActorKey, TObjectPtr<AThistleInject>>& Entry : *ActorToAILocomotionMapping)
		{
			Entry.Value->LocomotionStateMachine(); // NOW this can turn into a bool that actually provides info for the
			//behavioralist so we know when to bloody repath. lord in hebby.
		}
	}
}

void UThistleBehavioralist::RunStateTrees(uint64_t CurrentTck) const
{
	if (EntityToArtilleryBehavior)
	{
		for (TPair<FSkeletonKey, UThistleStateTreeLease*>& Entry : *EntityToArtilleryBehavior)
		{
			//we'll actually want to pass these in, but getting them here is out of scope at the moment.
			//TODO: use prior-prior tick and prior tick instead of current to increase determinability of the AI system
			//and give players the slimmest fighting chance if someone decides input reading is a good idea.
			Entry.Value->ArtilleryTick(CurrentTck); // NOW this can turn into a bool that actually provides info for the
		}
	}
}

bool UThistleBehavioralist::IsPlayerInCombat() const
{
	return BehavioralistTagState->HasTag(TAG_Perception_Player_Seen);
}

uint32 UThistleBehavioralist::GetEnemiesWithinRangeOfPoint(
	const FVector& Location,
	double Range,
	ActorKeyArray& OutEnemyKeyArray)
{
	if (OutEnemyKeyArray.IsEmpty() || OutEnemyKeyArray.Num() < MAX_ENEMY_COUNT)
	{
		return 0;
	}

	UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
	if (TransformDispatch == nullptr)
	{
		return 0;
	}

	uint32 EnemyCountFound = 0;
	for (ActorKey& EnemyKey : CurrentEnemies)
	{
		// nullptr check since the enemy could have been destructed
		const AttrPtr EnemyHealthAttr = MyDispatch->GetAttrib(EnemyKey, HEALTH);
		if (!EnemyHealthAttr.IsValid())
		{
			continue;
		}
		const float CurrentEnemyHealth = EnemyHealthAttr->GetCurrentValue();

		// TODO - add 'dead' enemy keys to a dead array so we can clean up the array later
		if (CurrentEnemyHealth > 0.f)
		{
			TWeakObjectPtr<AActor> EnemyActor = TransformDispatch->GetAActorByObjectKey(EnemyKey);
			if (EnemyActor.IsValid())
			{
				if (FVector::Distance(Location, EnemyActor->GetActorLocation()) <= Range)
				{
					OutEnemyKeyArray[EnemyCountFound] = EnemyKey;
					EnemyCountFound++;
				}
			}
			// TODO - Add debugging toggles as we are starting to hit perf issues with these messages on all the time
			// else
			// {
			// 	UE_LOG(LogTemp, Warning, TEXT("ThistleBehavioralist: EnemyKey [%llu] is not an actor despite being registered as an enemy!"), EnemyKey.Obj)
			// }
		}
		else if (CurrentEnemyHealth <= 0.f)
		{
			TWeakObjectPtr<AActor> EnemyActor = TransformDispatch->GetAActorByObjectKey(EnemyKey);
			if (EnemyActor.IsValid())
			{
				if (FVector::Distance(Location, EnemyActor->GetActorLocation()) <= Range)
				{
					DeadEnemies.Add(EnemyKey);
				}
			}
			// else
			// {
			// 	UE_LOG(LogTemp, Warning, TEXT("ThistleBehavioralist: EnemyKey [%llu] is not an actor despite being registered as an enemy, and has 0 or less health!"), EnemyKey.Obj)
			// }
		}
	}
	return EnemyCountFound;
}

// update enemy status
void UThistleBehavioralist::CullDeadEnemies()
{
	if (MyDispatch)
	{
		for (int32 EnemyIndex = 0; EnemyIndex < CurrentEnemies.Num(); EnemyIndex++)
		{
			if (CurrentEnemies[EnemyIndex] != 0)
			{
				MyDispatch->GetAttribAndApplyIf(CurrentEnemies[EnemyIndex], HEALTH, [this, EnemyIndex](AttrPtr Health)
				{
					if (Health->GetCurrentValue() <= 0.f)
					{
						DeadEnemies.Add(CurrentEnemies[EnemyIndex]);
						CurrentEnemies.RemoveAtSwap(EnemyIndex);
						return true;
					}
					return false;
				});
			}
		}

		UTransformDispatch* TransformDispatch = UTransformDispatch::SelfPtr;
		UBarrageDispatch* Physics = UBarrageDispatch::SelfPtr;
		if (TransformDispatch == nullptr || Physics == nullptr)
		{
			return; //can't shake'em.
		}
		
		for (ActorKey DeadEnemy : DeadEnemies)
		{
			DeregisterEnemy(DeadEnemy); //actor destruction is keyed to the destruction of the barrage object.
			TransformDispatch->ReleaseKineByKey(DeadEnemy); //if we do not release the kine, bad things happen.
			MyDispatch->DeregisterAttributes(DeadEnemy);
			MyDispatch->DeregisterRelationships(DeadEnemy);
			MyDispatch->DeregisterVecAttribs(DeadEnemy);
			MyDispatch->DeregisterGameplayTags(DeadEnemy);

			FBLet posit = Physics->GetShapeRef(DeadEnemy);
			if (posit)
			{
				// ReSharper disable once CppExpressionWithoutSideEffects
				Physics->SuggestTombstone(posit);
			}
			// call proper OnDeath method in future versions
		}
	}

	DeadEnemies.Reset();
}
