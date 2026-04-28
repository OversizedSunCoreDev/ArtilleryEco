// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "KeyedConcept.h"
#include "SkeletonTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/EngineSubsystem.h"
#include "ORDIN.generated.h"

//The following is ordinate bast, scepter 11.

/**
 * For any deterministic game, especially one that may use asset streaming, access to a central mechanism for the
 * creation, registration, and initialization of physics objects and ecs pillar components becomes essential.
 *
 * Deterministic ordering is bog simple.
 * It offers these categories.
 * Pillars - these come online during INITIALIZE.
 * During BeginPlay: (ugh, might need to change the lifecycle for this subsystem)
 * Player Key Carries (Reserved) - Player key carries, in the same order as players.
 * Key Carries (Reserved) - All key non-player carries, and only key carries.
 * Players (Reserved) - remote and local game instances must initialize the player objects in the same order for determinism. 
 * 1
 * Reserved
 * 2
 * Reserved
 * 3
 * Reserved
 * All Started
 *
 * And during its begin play, it runs anything registered with it. That's it. It may do more eventually.
 */

namespace ORDIN
{
	//Why yes, this is stupid, brutish, ugly, and hacky.
	//It's also fast, readable, and surprisingly maintainable.
	typedef TPair<int,IKeyedConstruct*> SequencedKey;
	typedef TArray<SequencedKey> InitSequence;
	typedef TPair<int,ICanReady*> SubsystemKey;
	typedef TArray<SubsystemKey> ForbiddenInitSequence;
	static constexpr int FirstSeqKey		= 10;
	static constexpr int Step = 100;
	//if you have more than 1000000 subsystems you need online before you can begin
	//setting up your ability system, I don't think an ordering system like this is your answer.

	//I've seen things you people wouldn't believe...

	//Enum DeCircularizing
	enum E_D_C
	{
		LastSubstrateKey		= Step * Step * Step,
		ArtilleryOnline = LastSubstrateKey + LastSubstrateKey,
		EnemyTagState = ArtilleryOnline + ORDIN::Step,
		SecondaryEnemyDispatch = EnemyTagState + 1,
		ParticleSystem = EnemyTagState + ORDIN::Step,
		ProjectileSystem = ParticleSystem + ORDIN::Step,
		UIECSSystem = ProjectileSystem + ORDIN::Step,
		EventLogSystem = UIECSSystem + ORDIN::Step,
		
		SkeletalAnimationSystem = EventLogSystem + ORDIN::Step,
		LastSuperstructureKey = ArtilleryOnline + LastSubstrateKey
	};
}

//we need a long-lived record divorced from the lifecycle of the world to provide verity in certain exigent cases.

UCLASS()
class SKELETONKEY_API ULongLivedRecords : public UEngineSubsystem
{
	GENERATED_BODY()
	//an awful lot of work to go to for this...
	TSharedPtr<TCircularBuffer<std::shared_ptr<WorldRecord>>> WorldRecords;
	std::atomic<uint32> WorldRecordCount;

public:
	ULongLivedRecords()
		: WorldRecordCount(0)
	{
		WorldRecords = MakeShared<TCircularBuffer<std::shared_ptr<WorldRecord>>>(1024);
	}
	//this should never be called from outside the WORLD lifecycle as we must guarantee that it will never hit a dead LLR.
	std::shared_ptr<WorldRecord> WorldRecordStart()
	{
		if (WorldRecords)
		{
			auto MyIndex = WorldRecords->GetNextIndex(WorldRecordCount.fetch_add(1, std::memory_order_relaxed));
			std::shared_ptr<WorldRecord> NewRecord(new WorldRecord());
			(*WorldRecords)[MyIndex] = NewRecord;
			return NewRecord;
		}
		return nullptr;
	}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};


#define SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD Collection.InitializeDependency<UOrdinatePillar>()->REGISTERLORD(OrdinateSeqKey, this, this);


//There are native ways to do this which might be better, but unfortunately, I can't be sure we'll
//only need to order UE-ish stuff for declaring dependencies.
//
//There's a couple other reasons to build it this way, since you really aren't supposed
//to do stuff like order the initialization of player objects separately from their creation order
//or order components on separate actors relative to each other but we do actually need to do that
//because for deterministic physics, the body creation order must be identical. This isn't a problem
//once play starts and we're building the sim off of the input stream but until then, we need ordering.
//I'd like to be wrong, but I don't think I am - a lot of games have something like this, a sort of
//hidden ordination mechanism built into resource loading. Especially if you have to lazy load.
//Additionally, each client would otherwise likely not possess the correct order as the local player object
//needs to be created in a totally different place and way from the proxies.
//Hopefully, as we learn more, we can remove this pillar, but I unfortunately doubt that.

//this will need, eventually, to be reworked to use the MassSubsystemAccess.h design for both sanity andd compatibility reasons.
UCLASS()
class SKELETONKEY_API UOrdinatePillar : public UWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
	FSkeletonKey DefaultObjectKey = FSkeletonKey();
	UOrdinatePillar();
	virtual ~UOrdinatePillar() override;
	virtual void Deinitialize() override;
	
	std::shared_ptr<WorldRecord> MyWorld;

public:

	//TODO: move the blob structure into the world record structure.
	//atm, want to change as little as I can.
	struct ORDIN_Blob
	{
	 ORDIN::ForbiddenInitSequence Subsystems	= ORDIN::ForbiddenInitSequence();		
	 ORDIN::InitSequence PlayerKeyCarries		= ORDIN::InitSequence();
	 ORDIN::InitSequence KeyCarries				= ORDIN::InitSequence();
	 ORDIN::InitSequence Players				= ORDIN::InitSequence();
	 ORDIN::InitSequence G1						= ORDIN::InitSequence();
	 ORDIN::InitSequence G1R					= ORDIN::InitSequence();
	 ORDIN::InitSequence G2						= ORDIN::InitSequence();
	 ORDIN::InitSequence G2R					= ORDIN::InitSequence();
	 ORDIN::InitSequence G3						= ORDIN::InitSequence();
	 ORDIN::InitSequence G3R					= ORDIN::InitSequence();
	 ORDIN::InitSequence AllStarted				= ORDIN::InitSequence();
	
	 ORDIN::InitSequence* GROUPS[10] = {
		&PlayerKeyCarries,
		&KeyCarries,
		&Players,
		&G1,
		&G1R,
		&G2,
		&G2R,
		&G3,
		&G3R,
		&AllStarted};
	};
	ORDIN_Blob Data;

	//this model supports TpM def of one dependency by saying constexpr SeqKey = ClassToDepend::SeqKey + UOrdinatePillar::Step
	//A class MUST declare a SeqKey as a static const int to use this system.
	//it must register DURING construction.
	void REGISTERORDER(int RegisterAs, int group, IKeyedConstruct* YourThisPointer);
	virtual void PostInitialize() override;
	void REGISTERLORD(int RegisterAs, ISkeletonLord* YourThisPointer, ICanReady* YourThisPointerAgain);
	//BEGIN OVERRIDES
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};