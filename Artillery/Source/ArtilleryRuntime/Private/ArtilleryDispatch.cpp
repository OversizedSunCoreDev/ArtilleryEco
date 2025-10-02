// ReSharper disable CppMemberFunctionMayBeConst
#include "ArtilleryDispatch.h"
#include "FArtilleryGun.h"
#include "NeedA.h"
#include <FTEntityFinalTickResolver.h>
#include <FTGunFinalTickResolver.h>
#include <FTJumpTimer.h>

#include "FTApplyForceOnExpire.h"
#include "LocomotionParams.h"
#include "FTProjectileFinalTickResolver.h"
#include "ModularGameplayTags.h"
#include "NiagaraParticleDispatch.h"
#include "StaticAssetLoader.h"
#include "Threads/FArtilleryStateTreesThread.h"
#include "Threads/FArtilleryTicklitesThread.h"

bool UArtilleryDispatch::RegistrationImplementation()
{
	GameplayTagContainerToDataMapping->Init();
	UE_LOG(LogTemp, Warning, TEXT("ArtilleryDispatch:Subsystem: Online"));
	AttributeSetToDataMapping = MakeShareable(new AttrCuckoo());
	RequestRouter = MakeShareable(new F_INeedA());
	TL_ThreadedImpl::ADispatch = &ArtilleryTicklitesWorker_LockstepToWorldSim;
	UBarrageDispatch* PhysicsECS = GetWorld()->GetSubsystem<UBarrageDispatch>();
	TransformUpdateQueue = PhysicsECS->GameTransformPump;
	UCanonicalInputStreamECS* InputECS = GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
	ArtilleryAsyncWorldSim.CablingControlStream = InputECS->getNewStreamConstruct(APlayer::CABLE);
	ArtilleryAsyncWorldSim.BristleconeControlStream = InputECS->getNewStreamConstruct(APlayer::ECHO);
	UE_LOG(LogTemp, Warning, TEXT("ArtilleryDispatch:Subsystem: World beginning play"));
	// getting input from Bristle
	UseNetworkInput.store(true);
	UBristleconeWorldSubsystem* NetworkAndControls = GetWorld()->GetSubsystem<UBristleconeWorldSubsystem>();
	UBarrageDispatch* GameSimPhysics = GetWorld()->GetSubsystem<UBarrageDispatch>();
	HoldOpen = GameSimPhysics->JoltGameSim;
	ArtilleryTicklitesWorker_LockstepToWorldSim.DispatchOwner = this;
	ArtilleryTicklitesWorker_LockstepToWorldSim.StartTicklitesApply = StartTicklitesApply;
	ArtilleryTicklitesWorker_LockstepToWorldSim.StartTicklitesSim = StartTicklitesSim;
	ArtilleryAIWorker_LockstepToWorldSim.DispatchOwner = this;
	ArtilleryAIWorker_LockstepToWorldSim.RunAheadStateTrees = StartRunAhead;
	ArtilleryAIWorker_LockstepToWorldSim.EnemyRegisterHook = EnemyRegisterHook;
	ArtilleryAIWorker_LockstepToWorldSim.RequestRouter = RequestRouter;
	ArtilleryAsyncWorldSim.RequestRouter = RequestRouter;
	ArtilleryAsyncWorldSim.StartTicklitesApply = StartTicklitesApply;
	ArtilleryAsyncWorldSim.StartTicklitesSim = StartTicklitesSim;
	ArtilleryAsyncWorldSim.StartRunAhead = StartRunAhead;
	ArtilleryAsyncWorldSim.InputRingBuffer = MakeShareable(new PacketQ(256));
	NetworkAndControls->QueueOfReceived = ArtilleryAsyncWorldSim.InputRingBuffer;
	UCablingWorldSubsystem* DirectLocalInputSystem = GetWorld()->GetSubsystem<UCablingWorldSubsystem>();
	ArtilleryAsyncWorldSim.InputSwapSlot = MakeShareable(new IncQ(256));
	DirectLocalInputSystem->DestructiveChangeLocalOutboundQueue(ArtilleryAsyncWorldSim.InputSwapSlot);
	UCanonicalInputStreamECS* InputStreamECS = GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
	ArtilleryAsyncWorldSim.ContingentInputECSLinkage = InputStreamECS;
	ArtilleryAsyncWorldSim.ContingentPhysicsLinkage = GameSimPhysics;
	//IF YOU REMOVE THIS. EVERYTHING EXPLODE. IN A BAD WAY.
	//TARRAY IS A VALUE TYPE. SO IS TRIPLEBUFF I THINK.
	ArtilleryAsyncWorldSim.RequestorQueue_Abilities_TripleBuffer = RequestorQueue_Abilities_TripleBuffer;
	//OH BOY. REFERENCE TIME. GWAHAHAHA.
	ArtilleryAsyncWorldSim.Locomos_BufferNotThreadSafe = RequestorQueue_Locomos;
	GunToFiringFunctionMapping->Empty();
	ThreadSetup();

	WorldSim_Thread->Create(&ArtilleryAsyncWorldSim, TEXT("ARTILLERY_WORLDSIM_ONLINE."), 0, TPri_AboveNormal);
	WorldSim_AI_Thread->Create(&ArtilleryAIWorker_LockstepToWorldSim, TEXT("ARTILLERY_AISIM_ONLINE."));
	WorldSim_Ticklites_Thread->Create(&ArtilleryTicklitesWorker_LockstepToWorldSim,TEXT("ARTILLERY_TICKLITES_ONLINE."));

	SelfPtr = this;
	return true;
}

//Place at the end of the latest initialization-like phase.
void UArtilleryDispatch::REGISTER_ENTITY_FINAL_TICK_RESOLVER(const ActorKey& Self)
{
	TLEntityFinalTickResolver temp = TLEntityFinalTickResolver(Self); //this semantic sucks. gotta fix it.
	this->RequestAddTicklite(MakeShareable(new EntityFinalTickResolver(temp)), FINAL_TICK_RESOLVE);
}

void UArtilleryDispatch::REGISTER_PROJECTILE_FINAL_TICK_RESOLVER(uint32 MaximumLifespanInTicks,
                                                                 const FSkeletonKey& Self)
{
	TLProjectileFinalTickResolver temp = TLProjectileFinalTickResolver(MaximumLifespanInTicks, Self);
	this->RequestAddTicklite(MakeShareable(new ProjectileFinalTickResolver(temp)), FINAL_TICK_RESOLVE);
}

void UArtilleryDispatch::REGISTER_GUN_FINAL_TICK_RESOLVER(const FGunKey& Self, const FArtilleryGun* ExistCheck)
{
	TLGunFinalTickResolver temp = TLGunFinalTickResolver(Self, ExistCheck); //this semantic sucks. gotta fix it.
	this->RequestAddTicklite(MakeShareable(new GunFinalTickResolver(temp)), FINAL_TICK_RESOLVE);
}

void UArtilleryDispatch::INITIATE_JUMP_TIMER(const FSkeletonKey& Self)
{
	FTJumpTimer JumpTimer = FTJumpTimer(Self);
	this->RequestAddTicklite(MakeShareable(new TL_JumpTimer(JumpTimer)), Normal);
}

void UArtilleryDispatch::Bop(FSkeletonKey Target, uint16 TicksFromNow, FVector ForceAppliedOnce)
{
	FTDelayedForce ForceAtTime = FTDelayedForce(Target, ForceAppliedOnce, TicksFromNow);
	this->RequestAddTicklite(MakeShareable(new TL_Bop(ForceAtTime)), Normal);
}

FBLet UArtilleryDispatch::GetFBLetByObjectKey(FSkeletonKey Target, ArtilleryTime Now)
{
	UBarrageDispatch* PhysicsECSPillar = UWorld::GetSubsystem<UBarrageDispatch>(GetWorld());
	return PhysicsECSPillar ? PhysicsECSPillar->GetShapeRef(Target) : FBLet();
}

void UArtilleryDispatch::ThreadSetup()
{
	UBarrageDispatch* PhysicsECSPillar = GetWorld()->GetSubsystem<UBarrageDispatch>();
	if (PhysicsECSPillar)
	{
		PhysicsECSPillar->GrantClientFeed();
	}

	if (RequestRouter)
	{
		RequestRouter->Feed();
	}
}

void UArtilleryDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GetWorld()->GetSubsystem<UOrdinatePillar>()->REGISTERLORD(OrdinateSeqKey, this, this);
}

void UArtilleryDispatch::PostInitialize()
{
	Super::PostInitialize();
}

void UArtilleryDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	if ([[maybe_unused]] const UWorld* World = InWorld.GetWorld())
	{
	}
}

void UArtilleryDispatch::Deinitialize()
{
	IsReady = false;
	SelfPtr = nullptr;
	if (__LIVE__)
	{
		___LIVING->IsReady = false;
	}
	StartTicklitesSim->Trigger();
	ArtilleryAIWorker_LockstepToWorldSim.Stop();
	ArtilleryTicklitesWorker_LockstepToWorldSim.Stop();
	ArtilleryTicklitesWorker_LockstepToWorldSim.Exit();
	ArtilleryAIWorker_LockstepToWorldSim.Exit();
	StartTicklitesApply->Trigger();
	StartRunAhead->Trigger();
	//We have to wait on worldsim, but we actually can just hard kill ticklites.

	if (WorldSim_Ticklites_Thread.IsValid())
	{
		//otoh, we need to hard kill the ticklites, so far as I can tell, and keep rolling. This should actually generally
		//not proc.
		WorldSim_Ticklites_Thread->Kill(true);
		WorldSim_Ticklites_Thread.Reset();
	}
	if (WorldSim_AI_Thread.IsValid())
	{
		WorldSim_AI_Thread->Kill((true));
		WorldSim_AI_Thread.Reset();
	}
	ArtilleryAsyncWorldSim.Stop();
	ArtilleryAsyncWorldSim.Exit();

	//we save the world sim for last. this means we'll always have a thread to cycle the events, in case we miss our timing.
	if (WorldSim_Thread.IsValid())
	{
		//if we don't wait, this will crash when ECS facts are referenced. That's just... uh... the facts.
		WorldSim_Thread->Kill(true);
		WorldSim_Thread.Reset();
	}

	GameplayTagContainerToDataMapping->Empty();
	KeyToControlliteMapping->Empty();
	VectorSetToDataMapping->Empty();
	GunByKey->Empty();
	HoldOpen.Reset();

	Super::Deinitialize();
}

AttrMapPtr UArtilleryDispatch::GetAttribSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const
{
	AttrMapPtr In;
	AttributeSetToDataMapping->find(Target, In);
	return In ? In : nullptr;
}

IdMapPtr UArtilleryDispatch::GetIdSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const
{
	IdMapPtr In;
	IdentSetToDataMapping->find(Target, In);
	return In ? In : nullptr;
}

Attr3MapPtr UArtilleryDispatch::GetVectorSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const
{
	return VectorSetToDataMapping->FindChecked(Target);
}

//this approach is risky for determinism, because, again, the thread accumulator map's ordering is random.
//to fix this, we'll need to enforce orderings in a LOT of places OR sort by time then order by target, I think.
void UArtilleryDispatch::ProcessRequestRouterGameThread()
{
	if (__LIVE__ && RequestRouter)
	{
		for (F_INeedA::GameFeedMap& FeedMap : RequestRouter->GameThreadAcc)
		{
			TSharedPtr<F_INeedA::GameThreadRequestQ> HoldOpenQueue;
			if (FeedMap.Queue && ((HoldOpenQueue = FeedMap.Queue)) && FeedMap.That != std::thread::id())
			//if there IS a thread.
			{
				FRequestGameThreadThing Request;
				while (HoldOpen && HoldOpenQueue->Dequeue(Request))
				{
					//PINPOINT: YAGAMETHREADBOYRUNNETHREQUESTSHERE
					switch (Request.GetType())
					{
					case ArtilleryRequestType::FireAGun:
						{
							TSharedPtr<FArtilleryGun> GunHoldOpen = GunByKey->FindRef(Request.Gun);
							TDelegate<void(TSharedPtr<FArtilleryGun>, bool, EventBufferInfo)>* FireFunction =
								GunToFiringFunctionMapping->Find(Request.Gun);

							if (FireFunction != nullptr && GunHoldOpen)
							{
								EventBufferInfo def = EventBufferInfo::Default();
								def.Action = ArtIPMKey::InternallyStateless;
								TotalFirings += FireFunction->ExecuteIfBound(GunHoldOpen, false, def);
							}
							else
							{
								// TODO - we are absolutely going to want to turn this and things like it into a periodic
								//		  log call to avoid clogging log files
								UE_LOG(
									LogTemp,
									Error,
									TEXT(
										"ArtilleryDispatch::ProcessRequestRouterGameThread: Error processing FireGun request with gun key [id: %llu, name: %s]"
									),
									Request.Gun.GunInstanceID.Obj,
									*Request.Gun.GunDefinitionID);
							}
						}
						break;
					// *****************
					// * Particle Handling
					// *****************
					case ArtilleryRequestType::ParticleSystemActivateOrDeactivate:
						{
							UNiagaraParticleDispatch* ParticleDispatch = GetWorld()->GetSubsystem<
								UNiagaraParticleDispatch>();
							FParticleID PID(Request.SourceOrSelf);
							if (Request.ActivateIfPossible)
							{
								ParticleDispatch->ActivateInternal(PID);
							}
							else
							{
								ParticleDispatch->DeactivateInternal(PID);
							}
						}
						break;
					case ArtilleryRequestType::SpawnParticleSystemAttached:
						{
							FSkeletonKey AttachToKey = Request.SourceOrSelf;

							if (Request.ActivateIfPossible)
							{
								UArtilleryProjectileDispatch* ProjectileDispatch = GetWorld()->GetSubsystem<
									UArtilleryProjectileDispatch>();
								TWeakObjectPtr<USceneComponent> SceneComp = ProjectileDispatch->
									GetSceneComponentForProjectile(Request.SourceOrSelf);
								if (SceneComp.IsValid())
								{
									FBoneKey AttachToAsBoneKey = MAKE_BONEKEY(SceneComp.Get());
									UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<
										UTransformDispatch>();
									TransformDispatch->RegisterSceneCompToShadowTransform(
										AttachToAsBoneKey, SceneComp.Get());
									AttachToKey = AttachToAsBoneKey;
								}
							}

							UNiagaraParticleDispatch* ParticleDispatch = GetWorld()->GetSubsystem<
								UNiagaraParticleDispatch>();
							[[maybe_unused]] FParticleID PID = ParticleDispatch->SpawnAttachedNiagaraSystem(
								*Request.ThingName.ToString(),
								AttachToKey,
								NAME_None,
								EAttachLocation::Type::SnapToTarget);
						}
						break;
					case ArtilleryRequestType::GetAnUnboundGun:
						{
							IdMapPtr WordsOfPower = GetRelationships(Request.SourceOrSelf);
							FGunKey Gun = GetGun(Request.Gun.GunDefinitionID, ActorKey(Request.SourceOrSelf));
							FGunInstanceKey BANG = Gun.GunInstanceID;
							if (WordsOfPower)
							{
								TSharedPtr<FConservedAttributeKey> MaterialComponent = WordsOfPower.Get()->FindOrAdd(
									Request.Relationship);
								if (MaterialComponent)
								{
									MaterialComponent.Get()->SetCurrentValue(BANG);
								}
								else
								{
									TSharedPtr<FConservedAttributeKey> PowerWordGun = MakeShareable(
										new FConservedAttributeKey);
									PowerWordGun->SetBaseValue(BANG);
									PowerWordGun->SetCurrentValue(BANG);
									WordsOfPower.Get()->Add(Request.Relationship, PowerWordGun);
								}
							}
							else
							{
								TSharedPtr<TMap<Ident, IdentPtr>> RelationshipMap = MakeShareable(new IdentityMap());

								//TODO: swap this to loading values from a data table, and REMOVE this fallback.
								//If we want defaults, those defaults should ALSO live in a data table, that way when a defaulting bug screws us
								//maybe we can fix it without going through a full cert using a data only update.
								TSharedPtr<FConservedAttributeKey> PowerWordGun = MakeShareable(
									new FConservedAttributeKey);
								RelationshipMap->Add(Request.Relationship, PowerWordGun);
								PowerWordGun->SetBaseValue(BANG);
								PowerWordGun->SetCurrentValue(BANG);
								RegisterOrAddRelationships(Request.SourceOrSelf, RelationshipMap);
							}
						}
						break;
					case ArtilleryRequestType::SpawnParticleSystemAtLocation:
						{
							UNiagaraParticleDispatch* ParticleDispatch = GetWorld()->GetSubsystem<
								UNiagaraParticleDispatch>();
							[[maybe_unused]] FParticleID PID = ParticleDispatch->SpawnFixedNiagaraSystem(
								*Request.ThingName.ToString(),
								Request.ThingVector,
								Request.ThingRotator,
								FVector(1));
						}
						break;
					// *****************
					// * Mesh Handling
					// *****************
					case ArtilleryRequestType::SpawnStaticMesh:
						{
							UArtilleryProjectileDispatch* ProjectileDispatch = GetWorld()->GetSubsystem<
								UArtilleryProjectileDispatch>();
							ProjectileDispatch->CreateProjectileInstance(
								Request.SourceOrSelf,
								Request.Gun,
								Request.ThingName,
								FTransform(Request.ThingVector),
								Request.ThingVector3,
								Request.ThingVector2.X,
								true,
								true,
								Request.Layer,
								Request.CanExpire,
								Request.TicksDuration);
							AddTagToEntity(Request.SourceOrSelf, InitState_GameplayReady);
						}
						break;
					default:
						UE_LOG(
							LogTemp,
							Fatal,
							TEXT(
								"ArtilleryDispatch::ProcessRequestRouterGameThread: Received Request Router request for unimplemented request type: [%d]"
							),
							Request.GetType());
						throw;
					}
				}
			}
		}
	}
}

void UArtilleryDispatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RunGuns(); // ALL THIS WORK. FOR THIS?! (Okay, that's really cool)
	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	// both transform dispatch and gamesim must be ready. Otherwise, let the queue build up.
	if (UTransformDispatch::SelfPtr && BarrageDispatch)
	{
		UTransformDispatch::SelfPtr->ApplyTransformUpdates<TSharedPtr<TransformUpdatesForGameThread>>(
			BarrageDispatch->GameTransformPump);
	}
	//dumbfire it with the ol' skibidi

	//TODO: this can be moved to the artillery worker thread with some changes to how we do instanced meshes
	//specifically, by putting meshes into a free list when we "delete" their associated gameplay entity, projectile or otherwise
	//and then "allocating" from that free list, we can then move resizing the pool of available instances onto the manager tick.
	//this split likely needs a bit of hemming and hawing to ensure determinism, but I think it's tractable.
	ProcessRequestRouterGameThread();
}

TStatId UArtilleryDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArtilleryDispatch, STATGROUP_Tickables);
}

// TODO - Rename to ConjureGun, not renaming right now
FGunKey UArtilleryDispatch::GetGun(const FString& GunDefinitionID, const ActorKey& ProbableOwner) const
{
	const UWorld* World = GetWorld();
	if (World != nullptr)
	{
		const UGameInstance* GameInst = World->GetGameInstance();
		if (GameInst != nullptr)
		{
			UStaticGunLoader* Arsenal = GameInst->GetSubsystem<UStaticGunLoader>();
			if (Arsenal && Arsenal->CommonNameToProperNameMapping.Contains(GunDefinitionID))
			{
				TSharedPtr<FArtilleryGun> FreshBakedGun = Arsenal->GetNewInstanceUninitialized(GunDefinitionID);
				TSharedPtr<TMap<FSkeletonKey, TSharedPtr<FArtilleryGun>>> HoldOpenGuns = GunByKey;
				if (FreshBakedGun && HoldOpen)
				{
					FreshBakedGun->UpdateProbableOwner(ProbableOwner);

					//TODO find an alternative that's truly deterministic and doesn't suck ten million bees. we need a ticker that's monotonic
					// do we? or can we achieve outcome determinism without it? I think we can...
					FGunKey Key = FGunKey(GunDefinitionID, F_INeedA::HashDownTo32(ProbableOwner + ++monotonkey));
					//TODO: replace with probable owner?

					FreshBakedGun->Initialize(Key, false);
					if (FreshBakedGun->ReadyToFire)
					{
						HoldOpenGuns->Add(Key, FreshBakedGun);
						// TODO - add tooling to toggle messages like this as we are starting to see enough of them that it affects framerate
						// UE_LOG(LogTemp, Warning,
						// 	   TEXT(
						// 		   "UArtilleryDispatch::GetGun: Gun Definition ID [%s] is valid! New instance created. (Probable Owner = [%llu])"
						// 	   ), *(GunDefinitionID), ProbableOwner.Obj);
						return Key;
					}
					return DefaultGunKey;
				}
				// UE_LOG(LogTemp, Warning,
				//        TEXT(
				// 	       "UArtilleryDispatch::GetGun: Gun Definition ID [%s] is invalid! Instance could not be created. Check that your GunDefinitionID matches the entry in GunDefinitions. (Probable Owner = [%llu])"
				//        ), *(GunDefinitionID), ProbableOwner.Obj);
			}
		}
	}
	return FGunKey();
}

FGunKey UArtilleryDispatch::RegisterExistingGun(const TSharedPtr<FArtilleryGun>& ToBind,
                                                const ActorKey& ProbableOwner) const
{
	//TODO: see if this code path needs to evolve to do more sophisticated management of the gunkey itself
	ToBind->UpdateProbableOwner(ProbableOwner);
	GunByKey->Add(ToBind->MyGunKey, ToBind);
	return ToBind->MyGunKey;
}

bool UArtilleryDispatch::IsGunLive(FSkeletonKey Key)
{
	if (__LIVE__)
	{
		TSharedPtr<TMap<FSkeletonKey, TSharedPtr<FArtilleryGun>>> HoldOpenGuns = GunByKey;
		if (UArtilleryDispatch::SelfPtr != nullptr && HoldOpenGuns && HoldOpenGuns.IsValid() && HoldOpenGuns->Num() > 0
			&& HoldOpenGuns.Get() && IsReady)
		{
			return HoldOpenGuns->IsEmpty() ? false : HoldOpenGuns->Contains(Key);
		}
	}
	return false;
}

bool UArtilleryDispatch::IsActorTransformAlive(ActorKey Key) const
{
	if (__LIVE__)
	{
		UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
		return TransformDispatch ? TransformDispatch->GetActorKineByObjectKey(Key) != nullptr : false;
	}
	return false;
}

//returns false if already released.
bool UArtilleryDispatch::ReleaseGun(const FGunKey& Key)
{
	//We know it. We have known it. We continue to know it.
	//See you soon, Chief.
	if (__LIVE__)
	{
		TSharedPtr<FArtilleryGun> tracker = nullptr;
		if (GunByKey->RemoveAndCopyValue(Key, tracker))
		{
			PooledGuns.Add(Key.GunDefinitionID, tracker);
			return true;
		}
	}
	return false;
}

TSharedPtr<FArtilleryGun> UArtilleryDispatch::GetPointerToGun(const FGunKey& GunToGet) const
{
	TSharedPtr<FArtilleryGun>* Gun = GunByKey->Find(GunToGet);
	return Gun != nullptr ? *Gun : nullptr;
}

void UArtilleryDispatch::QueueResim(FGunKey Key, ArtilleryTime Time) const
{
	if (ActionsToReconcile && ActionsToReconcile.IsValid())
	{
		ActionsToReconcile->Enqueue(std::pair(Key, Time));
	}
}

AttrMapPtr UArtilleryDispatch::GetAttribMap(const FSkeletonKey Owner) const
{
	AttrMapPtr result;
	return AttributeSetToDataMapping->find(Owner, result) ? result : nullptr;
}

AttrPtr UArtilleryDispatch::AddAttrib(const FSkeletonKey Owner, AttribKey Attrib, float AttribValue)
{
	// For now this will overwrite the value of any existing attribute.
	if (AttributeSetToDataMapping != nullptr)
	{
		AttrMapPtr AttributeMap;
		if (AttributeSetToDataMapping->find(Owner, AttributeMap) && AttributeMap != nullptr)
		{
			AttrPtr& NewAttrPtr = AttributeMap->Add(Attrib, MakeShareable(new FConservedAttributeData()));
			NewAttrPtr->SetBaseValue(AttribValue);
			NewAttrPtr->SetCurrentValue(AttribValue);
			return NewAttrPtr;
		}
	}
	return nullptr;
}

//Do not swap this to a ref, because a ref is a ref. If you want a no copy op, first off,
//our keys are the same size as a pointer so unless your code is guaranteed to inline, it's
//not more efficient and may be less efficient. second, use GetAttribRequired. it's for that.
AttrPtr UArtilleryDispatch::GetAttrib(const FSkeletonKey Owner, AttribKey Attrib) const
{
	AttrMapPtr AttributeMap;
	if (AttributeSetToDataMapping != nullptr)
	{
		AttributeSetToDataMapping->find(Owner, AttributeMap);
		if (AttributeMap != nullptr)
		{
			TSharedPtr<FConservedAttributeData>* AttributeData = AttributeMap->Find(Attrib);
			return AttributeData != nullptr ? *AttributeData : nullptr;
		}
	}
	return nullptr;
}

//GetAttribRequired should ONLY be used where the lifecycle of the key's owner will not cause the ref'd mem
//to be deleted, where it can be inlined, and where the attribute is guaranteed to exist even if it not
//yet guaranteed to be set.
AttrPtr inline UArtilleryDispatch::GetAttribRequired(const FSkeletonKey& Owner, AttribKey Attrib) const
{
	AttrMapPtr AttributeMap;
	AttributeSetToDataMapping->find(Owner, AttributeMap);
	if (AttributeMap != nullptr)
	{
		TSharedPtr<FConservedAttributeData>* AttributeData = AttributeMap->Find(Attrib);
		checkf(AttributeData != nullptr,
		       TEXT("UArtilleryDispatch::GetAttribRequired: Required Attribute [%d] for key [%lld] was not found."),
		       Attrib, Owner.Obj);
		return *AttributeData;
	}
	return nullptr;
}

IdentPtr UArtilleryDispatch::GetIdent(const FSkeletonKey Owner, Ident Attrib) const
{
	if (IdentSetToDataMapping && Owner != 0)
	{
		IdMapPtr In;
		IdentSetToDataMapping->find(Owner, In);
		if (In != nullptr)
		{
			IdentPtr* Identity = In.Get()->Find(Attrib);
			return Identity != nullptr ? *Identity : nullptr;
		}
	}
	return nullptr;
}

IdentPtr UArtilleryDispatch::GetOrAddIdent(const FSkeletonKey Owner, Ident Attrib) const
{
	if (IdentSetToDataMapping && Owner != 0)
	{
		IdMapPtr In;
		IdentSetToDataMapping->find(Owner, In);
		if (In != nullptr)
		{
			IdentPtr* Identity = In.Get()->Find(Attrib);
			return Identity != nullptr ? *Identity : nullptr;
		}
		IdMapPtr NewIDMap = MakeShareable(new IdentityMap());
		IdentSetToDataMapping->insert_or_assign(Owner, NewIDMap);
		return NewIDMap->Add(Attrib);
	}
	return nullptr;
}

//MUTABLE ACCESS TO THE RELATIONSHIPS OF A KEY.
//Necessary, but be like me: demure. looking respectfully. creating a god damn mess.
IdMapPtr UArtilleryDispatch::GetRelationships(const FSkeletonKey Owner) const
{
	if (IdentSetToDataMapping && Owner != 0)
	{
		IdMapPtr In;
		IdentSetToDataMapping->find(Owner, In);
		if (In != nullptr)
		{
			return In;
		}
	}
	return nullptr;
}

Attr3Ptr UArtilleryDispatch::GetVecAttr(const FSkeletonKey Owner, Attr3 Attrib) const
{
	Attr3MapPtr* IDMap = VectorSetToDataMapping->Find(Owner);
	if (IDMap != nullptr)
	{
		Attr3Ptr* Identity = IDMap->Get()->Find(Attrib);
		return Identity != nullptr ? *Identity : nullptr;
	}
	return nullptr;
}


UArtilleryDispatch::~UArtilleryDispatch()
{
	TSharedPtr<AtomicTagArray> holdopen = GameplayTagContainerToDataMapping;
	if (std::shared_ptr<WorldRecord> mandius = MyWorldState.lock()) //despair lol
	{
		mandius->IsReady = false;
		mandius->IsReady = true;
	}

	IsReady = false;
	//These should NEVER come up. Deinit always runs before decon, but just in case, we do want to handle it here.
	if (WorldSim_Thread)
	{
		ArtilleryAsyncWorldSim.Exit();
		StartTicklitesSim->Trigger();
		WorldSim_Thread->Kill(true);
	}
	if (WorldSim_Ticklites_Thread)
	{
		ArtilleryTicklitesWorker_LockstepToWorldSim.Exit();
		StartTicklitesApply->Trigger();
		StartTicklitesSim->Trigger();
		WorldSim_Thread->Kill(true);
	}
	if (WorldSim_AI_Thread)
	{
		ArtilleryAIWorker_LockstepToWorldSim.Exit();
		StartTicklitesApply->Trigger();
		StartTicklitesSim->Trigger();
		WorldSim_AI_Thread->Kill(true);
	}
	WorldSim_Thread = nullptr;
	WorldSim_AI_Thread = nullptr;
	WorldSim_Ticklites_Thread = nullptr;
}

void UArtilleryDispatch::AddTagToEntity(const FSkeletonKey Owner, const FGameplayTag& TagToAdd) const
{
	GameplayTagContainerToDataMapping->Add(Owner, TagToAdd);
}

void UArtilleryDispatch::RemoveTagFromEntity(const FSkeletonKey Owner, const FGameplayTag& TagToRemove) const
{
	GameplayTagContainerToDataMapping->Remove(Owner, TagToRemove);
}

bool UArtilleryDispatch::DoesEntityHaveTag(const FSkeletonKey Owner, const FGameplayTag& TagToCheck) const
{
	return GameplayTagContainerToDataMapping->Find(Owner, TagToCheck);
}

void UArtilleryDispatch::Deregister(const FGunKey& Key) const
{
	if (__LIVE__)
	{
		TSharedPtr<TMap<FGunKey, FArtilleryFireGunFromDispatch>> holdopen = GunToFiringFunctionMapping;
		if (holdopen && holdopen.IsValid())
		{
			GunToFiringFunctionMapping->Remove(Key);
			GunByKey->Remove(Key);
		}
		//TODO: add the rest of the wipe here?
	}
}

void UArtilleryDispatch::RegisterControllite(const FSkeletonKey& in, Machlet LaputanMachine) const
{
	//for now, you can't remove these. right now, this is ONLY used by the player, but this will proliferate and fuck us
	KeyToControlliteMapping->Add(in, LaputanMachine); //I spill my drink.
}

void UArtilleryDispatch::RegisterOrAddAttributes(FSkeletonKey in, AttrMapPtr Attributes)
{
	if (__LIVE__)
	{
		if (TSharedPtr<AttrCuckoo> hold = AttributeSetToDataMapping)
		{
			AttrMapPtr Extant;
			hold->find(in, Extant);
			if (Extant)
			{
				if (Attributes)
				{
					for (auto& proposed : *Attributes)
					{
						//merge them, we must, not merely append.
						auto& possible = Extant->FindOrAdd(proposed.Key);
						if (possible)
						{
							possible->SetCurrentValue(proposed.Value->GetCurrentValue());
						}
						else
						{
							possible = proposed.Value;
						}
					}
				}
			}
			else
			{
				hold->insert_or_assign(in, Attributes);
			}

		}
	}
}

void UArtilleryDispatch::RegisterOrAddRelationships(FSkeletonKey in, IdMapPtr Relationships)
{
	if (__LIVE__ && IsReady)
	{
		IdMapPtr Extant;
		IdentSetToDataMapping->find(in, Extant);
		if (!Extant)
		{
			//we still perform insert OR assign for a lot of Threading Reasons.
			//it might exist by the time we get here.
			//it shouldn't in any of our existing use cases..
			//POTENTIAL BUG
			//TODO: swap to single atomic lambda
			IdentSetToDataMapping->insert_or_assign(in, Relationships);
		}
		else
		{
			if (Relationships)
			{
				for (auto& proposed : *Relationships)
				{
					//merge them, we must, not merely append.
					auto& possible = Extant->FindOrAdd(proposed.Key);
					if (possible)
					{
						possible->SetCurrentValue(proposed.Value->CurrentValue);
					}
					else
					{
						possible = proposed.Value;
					}
				}
			}
		}
	}
}

void UArtilleryDispatch::RegisterOrAddVecAttribs(FSkeletonKey in, Attr3MapPtr Vectors)
{
	auto Extant = VectorSetToDataMapping->Find(in);
	if (Extant && *Extant && Vectors)
	{
		for (auto& proposed : *Vectors)
		{
			//merge them, we must, not merely append.
			auto& possible = (*Extant)->FindOrAdd(proposed.Key);
			if (possible != nullptr && possible.IsValid())
			{
				possible->SetCurrentValue(proposed.Value->CurrentValue);
			}
			else
			{
				possible = proposed.Value;
			}
		}
	}
	else
	{
		VectorSetToDataMapping->Add(in, Vectors);
	}
}

FConservedTags UArtilleryDispatch::RegisterOrAddGameplayTags(FSkeletonKey in, GameplayTagContainerPtrInternal GameplayTags)
{
	if (!GameplayTagContainerToDataMapping->SkeletonKeyExists(in))
	{
		FConservedTags TerrorModuleOnline = GameplayTagContainerToDataMapping->NewTagContainer(in);
		if (__LIVE__ && this && GameplayTagContainerToDataMapping && GameplayTagContainerToDataMapping.IsValid() &&
			GameplayTags)
		{
			for (const FGameplayTag& tag : GameplayTags->GetGameplayTagArray())
			{
				GameplayTagContainerToDataMapping->Add(in, tag);
			}
		}
		RequestRouter->TagReferenceModel(in, GetShadowNow(), TerrorModuleOnline);
		return TerrorModuleOnline; // this is the only good way to get a fast reference.
	}
	if (__LIVE__ && this && GameplayTagContainerToDataMapping && GameplayTagContainerToDataMapping.IsValid() &&
		GameplayTags)
	{
		auto PreviousTerrorModuleAlreadyOnline = GameplayTagContainerToDataMapping->GetReference(in);
		for (const FGameplayTag& tag : GameplayTags->GetGameplayTagArray())
		{
			GameplayTagContainerToDataMapping->Add(in, tag); 
		}
		//This feels overly finicky.
		return PreviousTerrorModuleAlreadyOnline; // I think this works, because the ref module should be online.
	}
	return nullptr; //yer jacked bb. this shouldn't happen. but if it does...
}

FConservedTags UArtilleryDispatch::GetExistingConservedTags(FSkeletonKey in)
{
	if (__LIVE__ && GameplayTagContainerToDataMapping && GameplayTagContainerToDataMapping.IsValid())
	{
		return GameplayTagContainerToDataMapping->GetReference(in);
	}
	return nullptr;
}

FConservedTags UArtilleryDispatch::GetOrRegisterConservedTags(FSkeletonKey in)
{
	if (__LIVE__)
	{
		FConservedTags GetExistingConservedTagsResult = GetExistingConservedTags(in);
		return GetExistingConservedTagsResult != nullptr
			       ? GetExistingConservedTagsResult
			       : RegisterOrAddGameplayTags(in, nullptr);
	}
	return nullptr;
}

void UArtilleryDispatch::DeregisterGameplayTags(FSkeletonKey in)
{
	if (__LIVE__ && IsReady && GameplayTagContainerToDataMapping
		&& GameplayTagContainerToDataMapping.IsValid()
		&& GameplayTagContainerToDataMapping.Get()) // note the assign.
	{
		TSharedPtr<AtomicTagArray> GTCHOpen = GameplayTagContainerToDataMapping;
		if (GTCHOpen && IsReady)
		{
			GTCHOpen->Erase(in);
		}
	}
	RequestRouter->NoTagReferenceModel(in, GetShadowNow());
}

void UArtilleryDispatch::DeregisterAttributes(FSkeletonKey in)
{
	TSharedPtr<AttrCuckoo> hold;
	if (__LIVE__ && IsReady && AttributeSetToDataMapping && ((hold = AttributeSetToDataMapping)))
	{
		hold->erase(in);
	}
}

void UArtilleryDispatch::DeregisterRelationships(FSkeletonKey in)
{
	TSharedPtr<IdentCuckoo> hold;
	if (__LIVE__ && IsReady && IdentSetToDataMapping && ((hold = IdentSetToDataMapping)))
	{
		IdentSetToDataMapping->erase(in);
	}
}

void UArtilleryDispatch::DeregisterVecAttribs(FSkeletonKey in)
{
	TSharedPtr<TMap<FSkeletonKey, Attr3MapPtr>> hold;
	if (__LIVE__ && VectorSetToDataMapping && ((hold = VectorSetToDataMapping)))
	{
		VectorSetToDataMapping->Remove(in);
	}
}

void UArtilleryDispatch::RunGuns() const
{
	if (__LIVE__ && RequestorQueue_Abilities_TripleBuffer && RequestorQueue_Abilities_TripleBuffer->IsDirty())
	//Sort is not stable. Sortedness appears to be lost for operations I would not expect.
	{
		RequestorQueue_Abilities_TripleBuffer->SwapReadBuffers();
		for (TTuple<long, EventBufferInfo>& GunToRun : RequestorQueue_Abilities_TripleBuffer->Read())
		{
			auto Del = GunToFiringFunctionMapping->Find(GunToRun.Value.GunKey);
			if (Del)
			{
				TotalFirings += Del->
					ExecuteIfBound(
						GunByKey->FindRef(GunToRun.Value.GunKey),
						false,
						GunToRun.Value);
			}
		}
		RequestorQueue_Abilities_TripleBuffer->Read().Reset();
	}
}

//this needs work and extension.
//TODO: add smear support.
void UArtilleryDispatch::RunLocomotions() const
{
	if (__LIVE__ && RequestorQueue_Locomos)
	{
		//Sort is not stable. Sortedness appears to be lost for operations I would not expect.
		for (LocomotionParams& Params : *RequestorQueue_Locomos)
		{
			IArtilleryControllite** SpanningLinkage = KeyToControlliteMapping->Find(Params.parent);
			if (SpanningLinkage != nullptr)
			{
				(*SpanningLinkage)->ArtilleryTick(Params.previousIndex, Params.currentIndex, false, false);
				++TotalFirings;
			}
		}
		RequestorQueue_Locomos->Empty();
	}
}

void UArtilleryDispatch::RunEnemySim(uint64_t CurrentTick) const
{
	[[maybe_unused]] bool Updated = EnemyUpdateHook.ExecuteIfBound(CurrentTick);
}

void UArtilleryDispatch::RunGunFireTimers()
{
}

//this peeks the various queues of things to do in the future, such as the velocity queue or the gun timer queues
//it also checks the local sorted list, allowing us to manage timers in amortized log(n) time in the worst case.
//normally a design like this isn't practical, but since we can allocate a thread to the process of maintaining the sort
//and only push events ready to go, there's only one thread touching the sorted sets, allowing us to go totally lockfree
//in exchange for some extra copy ops that we'd have needed to incur anyway to allow data shadowing.
//this is one of the huge advantages to the data shadowing scheme, namely, it turns weakness into strength.
//this always gets called from the busy worker, and populates the velocity and gun events stacks. eventually, those
//will be obsoleted to some extent and at least some of the events can be run immediately on the artillery busy worker thread
//thanks again to data shadowing, which ensures that in a race condition, _both values are stored_
void UArtilleryDispatch::CheckFutures()
{
}

void UArtilleryDispatch::RERunGuns()
{
	if (__LIVE__ && ActionsToReconcile && ActionsToReconcile.IsValid())
	{
		//throw;
	}
}

void UArtilleryDispatch::RERunLocomotions()
{
	//throw;
}

void UArtilleryDispatch::LoadGunData()
{
	FString AccumulatePath = FPaths::Combine(FPaths::ProjectPluginsDir(), "Artillery", "Data", "GunData");
}

//unused atm, but will be the way to ask for an eventish or triggered gun to fire, probably.
void UArtilleryDispatch::QueueFire(FGunKey Key, ArtilleryTime Time)
{
	if (__LIVE__ && ActionsToOrder && ActionsToOrder.IsValid())
	{
		ActionsToOrder->Enqueue(std::pair(Key, Time));
	}
}
