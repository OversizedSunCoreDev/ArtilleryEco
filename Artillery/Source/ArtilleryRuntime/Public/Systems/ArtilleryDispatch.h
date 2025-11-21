//CC AR, Oversized Sun.

#pragma once

#include "CoreMinimal.h"
#include "AArtilleryController.h"
#include "Subsystems/WorldSubsystem.h"
#include "UCablingWorldSubsystem.h"
#include "ArtilleryCommonTypes.h"
#include "AtomicTagArray.h"
#include "FArtilleryStateTreesThread.h"
#include "Containers/TripleBuffer.h"
#include "FArtilleryBusyWorker.h"
#include "LocomotionParams.h"
#include "FArtilleryTicklitesThread.h"
#include "FJThread.h"
#include "GameplayTagContainer.h"
#include "KeyCarry.h"
#include "TransformDispatch.h"

THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include "libcuckoo/cuckoohash_map.hh"
typedef libcuckoo::cuckoohash_map<FSkeletonKey, AttrMapPtr> AttrCuckoo;

typedef libcuckoo::cuckoohash_map<FSkeletonKey, IdMapPtr> IdentCuckoo;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

#include "ArtilleryDispatch.generated.h"

/**
 * This is the backbone of artillery, and along with the ArtilleryGuns and UFireControlMachines, 
 * this is most of what any other code will see of artillery in C++. Blueprints will mostly not even see that
 * as they are already encapsulated in the artillerygun's ability sequencer. Abilities merely need to be uninstanced,
 * only modify attributes tracked in Artillery, and use artillery bp nodes where possible.
 * 
 * No unusual additional serialization or similar is required for determinism. I hope.
 * 
 * For an organized view of the code, start by understanding that most work is done outside the gamethread.
 * Bristlecone handles network inputs, Cabling handles keymapping and local inputs. Artillery handles outcomes.
 * 
 * Here's the core rule:
 * 
 * Core gameplay simulation happens in the ArtilleryBusyWorker at about 120hz.
 * Anything that cannot cause the core gameplay sim to become desyncrhonized happens in the game thread.
 * Surprisingly, this means _most things flow through UE normally and happen on the game thread & render threads._
 * 
 * Our goal is that cosmetic ability components, animation, IK, particle systems, cloth, and really, almost everything
 * happens in UE normally. Rollbacks don't happen in the broader UE system at all, and in fact, most of UE acts like its
 * running in a pretty simple Iris push config.
 * 
 * Artillery just pushes transform and ability updates upward as needed. That's a big just, but it really is a narrow scope.
 * 
 * Iris does normal replication on a slow cadence as a fall back and to provide attribute sync reassurances.
 */
struct FArtilleryGun;
class UArtilleryGameplayTagContainer;
namespace Arty
{
	DECLARE_MULTICAST_DELEGATE(OnArtilleryActivated);
	
	DECLARE_DELEGATE_ThreeParams(FArtilleryFireGunFromDispatch,
		TSharedPtr<FArtilleryGun> Gun,
		bool InputAlreadyUsedOnce,
		EventBufferInfo FiringAction);
	
	//returns true if-and-only-if the duration of the input intent was exhausted.
	DECLARE_DELEGATE_RetVal_FourParams(bool,
		FArtilleryRunLocomotionFromDispatch,
		FArtilleryShell PreviousMovement,
		FArtilleryShell Movement,
		bool RunAtLeastOnce,
		bool Smear);

	// runs AI locomotion methods	
	DECLARE_DELEGATE(FArtilleryRunAILocomotionFromDispatch);

	// updates enemy states
	DECLARE_DELEGATE_OneParam(FArtilleryUpdateEnemyControllerSubsystem, uint64_t CurrentTick)
	typedef FStateTreesWorker<UArtilleryDispatch> AIWorker;
	typedef FArtilleryTicklitesWorker<UArtilleryDispatch> TickliteWorker;
	typedef TSharedPtr<FGameplayTagContainer> GameplayTagContainerPtr;
	
	typedef TSharedPtr<FGameplayTagContainer> GameplayTagContainerPtrInternal;
}

class UCanonicalInputStreamECS;
UCLASS()
class ARTILLERYRUNTIME_API UArtilleryDispatch : public UTickableWorldSubsystem, public ICanReady, public ISkeletonLord
{
	GENERATED_BODY()

	friend class FArtilleryBusyWorker;
	friend class FArtilleryTicklitesWorker<UArtilleryDispatch>;
	friend class UCanonicalInputStreamECS;
	friend class UArtilleryLibrary;

public:
	static inline UArtilleryDispatch* SelfPtr = nullptr;
	using MachineLet = IArtilleryControllite*;
	using Machlet = MachineLet;

	UArtilleryDispatch()
	{
		GameplayTagContainerToDataMapping = MakeShareable(new AtomicTagArray());
		RequestorQueue_Abilities_TripleBuffer = MakeShareable(new BufferedEvents());
		RequestorQueue_Locomos = MakeShareable(new BufferedMoveEvents());
		GunToFiringFunctionMapping = MakeShareable(new TMap<FGunKey, FArtilleryFireGunFromDispatch>());
		AttributeSetToDataMapping = MakeShareable(new AttrCuckoo());
		IdentSetToDataMapping = MakeShareable(new IdentCuckoo());
		KeyToControlliteMapping = MakeShareable(new TMap<FSkeletonKey, Machlet>());
		VectorSetToDataMapping = MakeShareable(new TMap<FSkeletonKey, Attr3MapPtr>());
		GunByKey = MakeShareable(new TMap<FSkeletonKey, TSharedPtr<FArtilleryGun>>());
	};
	
	// dependencies expressed: ALL(transform pillar, cabling, bristlecone, input pillar, barrage) -> this.
	constexpr static int OrdinateSeqKey = ORDIN::ArtilleryOnline;
	virtual bool RegistrationImplementation() override; 
	
	OnArtilleryActivated BindToArtilleryActivated;
	friend class F_INeedA;
	TSharedPtr<F_INeedA> RequestRouter;

	ArtilleryTime GetShadowNow() const { return ArtilleryAsyncWorldSim.TickliteNow; }
	
	void REGISTER_ENTITY_FINAL_TICK_RESOLVER(const ActorKey& Self);
	void REGISTER_PROJECTILE_FINAL_TICK_RESOLVER(uint32 MaximumLifespanInTicks, const FSkeletonKey& Self);
	void REGISTER_GUN_FINAL_TICK_RESOLVER(const FGunKey& Self, const FArtilleryGun* ExistCheck);
	void INITIATE_JUMP_TIMER(const FSkeletonKey& Self);
	void Bop(FSkeletonKey Target, uint16 TicksFromNow, FVector ForceAppliedOnce);

	//Forwarding for the TickliteThread.
	TOptional<FTransform> GetTransformShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const
	{
		UTransformDispatch* TransformECSPillar = UWorld::GetSubsystem<UTransformDispatch>(GetWorld());
		return TransformECSPillar ? TransformECSPillar->CopyOfTransformByObjectKey(Target) : TOptional<FTransform>();
	}

	void SetProjectileDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ArtilleryAsyncWorldSim.ProjectileSystemPointer = ReferenceToSubsystem;
	}
	
	void SetParticleDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ArtilleryAsyncWorldSim.ParticleSystemPointer = ReferenceToSubsystem;
	}

	void SetEventLogSystem(ITickHeavy* ReferenceToSubsystem)
	{
		ArtilleryAsyncWorldSim.EventLogSystemPointer = ReferenceToSubsystem;
	}
	
	FBLet GetFBLetByObjectKey(FSkeletonKey Target, ArtilleryTime Now);

	//Executes necessary preconfiguration for threads owned by this dispatch. Likely going to be factored into the
	//dispatch API so that we can use stronger type guarantees throughout our codebase.
	//Called FROM the thread being set up.
	void ThreadSetup();

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	TSharedPtr<FWorldSimOwner> HoldOpen;
	
	//TODO: this needs to be switched over to LibCuckoo. It's a little more delicate than the others, though.
	TSharedPtr<TMap<FGunKey, FArtilleryFireGunFromDispatch>> GunToFiringFunctionMapping;
	TSharedPtr<BufferedEvents> RequestorQueue_Abilities_TripleBuffer;

	//This is more straightforward than the guns problem.
	//We can actually map this quite directly.
	FArtilleryUpdateEnemyControllerSubsystem EnemyUpdateHook;
	FArtilleryAddEnemyToControllerSubsystem EnemyRegisterHook;
	TSharedPtr<AttrCuckoo> AttributeSetToDataMapping;
	//TODO: Figure out how to apply the learnings from the design of the controller with the defaulting.
	//It'll be necessary, I'm afraid. This can't use raw pointers safely. Likely we can use defaulting + the fblet design.
	TSharedPtr<TMap<FSkeletonKey, Machlet>> KeyToControlliteMapping;
	TSharedPtr<IdentCuckoo> IdentSetToDataMapping;
	TSharedPtr<TMap<FSkeletonKey, Attr3MapPtr>> VectorSetToDataMapping;
	
	/** skeleton key to map of gameplay tags */
	TSharedPtr<AtomicTagArray> GameplayTagContainerToDataMapping;
	
	TSharedPtr<TransformUpdatesForGameThread> TransformUpdateQueue;
	
public:
	virtual void PostInitialize() override;
	void RunEnemySim(uint64_t CurrentTick) const;
	
protected:
	//todo: convert conserved attribute to use a timestamp for versioning to create a true temporal shadowstack.
	AttrMapPtr GetAttribSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const;
	
	IdMapPtr GetIdSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const;
	
	Attr3MapPtr GetVectorSetShadowByObjectKey(const FSkeletonKey& Target, ArtilleryTime Now) const;
	void ProcessRequestRouterGameThread();

	TSharedPtr<BufferedMoveEvents> RequestorQueue_Locomos;
	static inline long long TotalFirings = 0; //2024 was rough.
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//you don't wanna look at this.
	FGunKey GetGun(const FString& GunDefinitionID, const ActorKey& ProbableOwner) const;
	
	//fully specifying the type is necessary to prevent spurious warnings in some cases.
	TSharedPtr<TCircularQueue<std::pair<FGunKey, ArtilleryTime>>> ActionsToOrder;
	//These two are the backbone of the Artillery gun lifecycle.
	TSharedPtr< TMap<FSkeletonKey, TSharedPtr<FArtilleryGun>>> GunByKey;
	TMultiMap<FString, TSharedPtr<FArtilleryGun>> PooledGuns;
	
	/**
	 * Will hold the configuration for the gun definitions
	 */
	TObjectPtr<UDataTable> GunDefinitionsManifest;
	//TODO: This is a dummy function and should be replaced NLT 10/20/24.
	//It loads from the PROJECT directory. This cannot ship, but will work for all purposes at the moment.
	//Note: https://www.reddit.com/r/unrealengine/comments/160mjkx/how_reliable_and_scalable_are_the_data_tables/
	void LoadGunData();
	
	TSharedPtr<TCircularQueue<std::pair<FGunKey, ArtilleryTime>>> ActionsToReconcile;

	void QueueFire(FGunKey Key, ArtilleryTime Time);
	void QueueResim(FGunKey Key, ArtilleryTime Time) const;

	//the separation of tick and frame is inspired by the Serious Engine and others.
	//In fact, it's pretty common to this day, with Unity also using a similar model.
	//However, our particular design is running fast relative to most games except quake.
	void RunGuns() const;
	void RunLocomotions() const;
	void RunGunFireTimers();
	void CheckFutures();
	//The current start of the tick boundary that ticklites should run on. this allows the ticklites
	//to run in frozen time.
	//********************************
	//DUMMY. USED BY RECONCILE AND RERUN.
	//Here to demonstrate that we actually queue normal and rerun separately.
	//We can't risk intermingling them, which should never happen, but...
	//c'mon. Seriously. you wanna find that bug?
	//********************************
	void RERunGuns();
	void RERunLocomotions();

public:
	typedef FArtilleryTicklitesWorker<UArtilleryDispatch> FTicklitesWorker;
	
	struct ARTILLERYRUNTIME_API TL_ThreadedImpl 
	{
		virtual ~TL_ThreadedImpl() = default;
		//Each class generated gets a unique static. Each kind of dispatcher will get a unique class.
		//TODO: If you run more than one of the parent threads, this gets unsafe. We don't so...
		//As is, it saves a huge amount of memory and indirection costs.
		static inline FTicklitesWorker* ADispatch = nullptr;

		TL_ThreadedImpl()
		{
			if(ADispatch == nullptr)
			{
				throw; // dawg, you tryin' allocate shit against a thread that ain' there.
			}
		}

		static ArtilleryTime GetShadowNow()
		{
			return ADispatch->GetShadowNow();
		}
	};
	
	//DUMMY FOR NOW.
	//TODO: IMPLEMENT THE GUNMAP FROM INSTANCE UNTO CLASS
	//TODO: REMEMBER TO SAY AMMO A BUNCH
	FORCENOINLINE void RequestAddTicklite(TSharedPtr<TicklitePrototype> ToAdd, TicklitePhase Group)
	{
		ArtilleryTicklitesWorker_LockstepToWorldSim.RequestAddTicklite(ToAdd, Group);
	}
	
	FGunKey RegisterExistingGun(const TSharedPtr<FArtilleryGun>& toBind, const ActorKey& ProbableOwner) const;
	void UnregisterExistingGun(FGunKey GunKey) const { GunByKey->Remove(GunKey); }
	bool IsGunLive(FSkeletonKey Key); 
	bool IsActorTransformAlive(ActorKey Key) const;
	
	//CURRENTLY ONLY SUPPORTS GUNS AND ACTORS
	SKLiveness IsLiveKey(const FSkeletonKey Test)
	{
		switch (GET_SK_TYPE(Test.Obj))
		{
		case SKELLY::SFIX_ART_1GUN : return this->IsGunLive(Test) ? ALIVE : DEAD;
		case SKELLY::SFIX_ART_GUNS : return this->IsGunLive(Test) ? ALIVE : DEAD; // this is sort of a hack, sort of not.
		case SKELLY::SFIX_ART_ACTS : return this->IsActorTransformAlive(Test) ? UNKNOWN : DEAD;
		case SKELLY::SFIX_STELLAR  : return UNKNOWN;
		default: return UNKNOWN;
		}
	}
	
	bool ReleaseGun(const FGunKey& Key);
	TSharedPtr<FArtilleryGun> GetPointerToGun(const FGunKey& GunToGet) const;

	/**
	 * In theory having this will let us only have to fetch a pointer to the map once if we need to do things
	 * with multiple attributes on the same key at once. Potential perf gains.
	 * 
	 * @param Owner Key to search for
	 * @return Pointer to hashmap containing Attributes for the given key
	 */
	AttrMapPtr GetAttribMap(const FSkeletonKey Owner) const;

	/**
	 * @param Owner Key to add an attribute for
	 * @param Attrib Attribute we are adding to the map
	 * @param AttribValue Base value to set for the attribute
	 * @return The new attribute ptr. Returns nullptr if we failed to add the attribute for any reason.
	 */
	AttrPtr AddAttrib(const FSkeletonKey Owner, Attr Attrib, float AttribValue = 0.0f);
	
	//TODO: convert to object key to allow the grand dance of the mesh primitives.
	AttrPtr GetAttrib(const FSkeletonKey Owner, Attr Attrib) const;
	//DEPRECATED
	AttrPtr GetAttribRequired(const FSkeletonKey& Owner, AttribKey Attrib) const;
	
	bool GetAttribAndApplyIf(FSkeletonKey Target, AttribKey Attr, const auto& lambda)
	{
		AttrPtr attrib = this->GetAttrib(Target, Attr);
		return attrib ? lambda(attrib) : false;
	}

	//TODO: ensure that the use of FSkeletonKey& is safe here.
	IdentPtr GetIdent(const FSkeletonKey Owner, Ident Attrib) const;
	IdentPtr GetOrAddIdent(const FSkeletonKey Owner, Ident Attrib) const;
	IdMapPtr GetRelationships(const FSkeletonKey Owner) const;
	Attr3Ptr GetVecAttr(const FSkeletonKey Owner, Attr3 Attrib) const;
	virtual ~UArtilleryDispatch() override;

	//due to peculiarities in our object lifecycle, these should not be switched to reference parameters.
	//a skeletonkey copy is the same size as a pointer, so outside of inlining, this isn't a perf issue.
	/* Per rider:
	* struct FSkeletonKey 
	* Size: 8
	* Alignment: 8
	*/
	void AddTagToEntity(const FSkeletonKey Owner, const FGameplayTag& TagToAdd) const;
	void RemoveTagFromEntity(const FSkeletonKey Owner, const FGameplayTag& TagToRemove) const;
	bool DoesEntityHaveTag(const FSkeletonKey Owner, const FGameplayTag& TagToCheck) const;
	
	void RegisterReady(const FGunKey Key, const FArtilleryFireGunFromDispatch& Machine) const
	{
		GunToFiringFunctionMapping->Add(Key, Machine);
	}

	bool IsGunRegistered(const FGunKey Key) const
	{
		return GunToFiringFunctionMapping->Contains(Key);
	}

	void RegisterEnemySubsystem(FArtilleryUpdateEnemyControllerSubsystem Update, FArtilleryAddEnemyToControllerSubsystem Register)
	{
		EnemyUpdateHook = Update;
		EnemyRegisterHook = Register;
	}
	
	void Deregister(const FGunKey& Key) const;

	void RegisterControllite(const FSkeletonKey& in, Machlet LaputanMachine) const;

	void RegisterOrAddAttributes(FSkeletonKey in, AttrMapPtr Attributes);

	void RegisterOrAddRelationships(FSkeletonKey in, IdMapPtr Relationships);

	void RegisterOrAddVecAttribs(FSkeletonKey in, Attr3MapPtr Vectors);

	//adds or assumes responsibility for the lifecycle of a tag container keyed to this SK
	// ReSharper disable once CppMemberFunctionMayBeConst
	FConservedTags RegisterOrAddGameplayTags(FSkeletonKey in, GameplayTagContainerPtrInternal GameplayTags);

	// ReSharper disable once CppMemberFunctionMayBeConst
	FConservedTags GetExistingConservedTags(FSkeletonKey in);

	FConservedTags GetOrRegisterConservedTags(FSkeletonKey in);

	void DeregisterGameplayTags(FSkeletonKey in);

	void DeregisterAttributes(FSkeletonKey in);

	void DeregisterRelationships(FSkeletonKey in);

	void DeregisterVecAttribs(FSkeletonKey in);
	
	std::atomic_bool UseNetworkInput;
	bool missedPrior = false;
	bool burstDropDetected = false;

private:
	static inline long long monotonkey = 0;
	//If you're trying to figure out how artillery works, read the busy worker knowing it's a single thread coming off of Dispatch.
	//this handles input from bristlecone, patching it into streams from the CanonicalInputStreamECS (ACIS), using the ACIS to perform mappings,
	//and processing those streams using the pattern matcher. right now, we also expect it to own rollback and jolt when that's implemented.
	//
	// 
	// This is quite a lot and for performance or legibility reasons there is a good chance that this thread will
	//need to be split up. On the other hand, we want each loop iteration to run for around 2-4 milliseconds.
	//this is quite a bit.
	FArtilleryBusyWorker ArtilleryAsyncWorldSim;
	//This runs the tickables alongside the worldsim, ticking each time the worldsim ticks
	//but NOT using locks. So the processing is totally independent. This is extremely fast,
	//extremely powerful, and the reason why we don't use ticklites when we don't need to.
	//it's dangerous as __________ _____________________ _ _________.
	AIWorker ArtilleryAIWorker_LockstepToWorldSim;
	TickliteWorker ArtilleryTicklitesWorker_LockstepToWorldSim;
	TUniquePtr<FJThread> WorldSim_Thread = MakeUnique<FJThread>();
	TUniquePtr<FRunnableThread> WorldSim_AI_Thread;
	TUniquePtr<FRunnableThread> WorldSim_Ticklites_Thread;
	FSharedEventRef StartTicklitesSim;
	FSharedEventRef StartTicklitesApply;
	FSharedEventRef StartRunAhead;
};
