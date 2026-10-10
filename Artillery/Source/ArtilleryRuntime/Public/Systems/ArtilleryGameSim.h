#pragma once


#include "AtomicTagArray.h"
#include "LocomotionParams.h"
#include "StateContainer.h"
#include "Structures/ConcurrencyTypes/ParallelFixedQueueTypes.h"

class FRequestRouter;
class ITickHeavy;
class UTransformDispatch;
class UInventoryDispatch;
class UCanonicalInputStreamECS;
class UBarrageDispatch;
class UArtilleryDispatch;
class FInputRollback;
class FArtilleryStateManager;


struct FArtillerySimContext
{
	uint32 Tick = 0;
	uint8 bIsVerified : 1 = false;
	std::function<void()> InputGatherFunction = nullptr;
	TMap<PlayerKey, FArtilleryShell> PrevInputs = {};
	TMap<PlayerKey, FArtilleryShell> Inputs = {};
};

class FArtilleryGameSim {
public:
	FArtilleryGameSim();

	
	FParallelFixedSequencingQueue VerifiedCreateDeadliner;
	FParallelFixedSequencingQueue VerifiedEventDeadliner;
	FParallelFixedSequencingQueue VerifiedTriggerDeadliner;
	void Initialize(UWorld* World);
	void Shutdown();
	
	// called each possible frame, not fixed
	void OnFrameUpdate();
	// Game simulation tick
	void Tick();
	void RunEventsRequiringVerifiedTicks(FArtillerySimContext& Context);
	void UpdateInputDriven(FArtillerySimContext& Context);
	void Simulate(FArtillerySimContext& Context);
	void StoreState(FArtillerySimContext& Context, FArtilleryDataBuffer Data);
	bool RollbackToState(FArtilleryDataBuffer& RollbackData);

	void RollbackAndResimulate(uint32 FromFrame, bool bUseAuthorityIfAvailable = true);
	void RollbackToVerified();

	bool IsServer() const { return bIsServer; }
	uint32 GetCurrentSequence() const { return CurrentSequence; }
	uint32 GetLastVerifiedSequence() const;
	uint32 GetOldestStoredSequence() const;
	
	ArtilleryTime GetTickliteNow() const {	return TickliteNow; }

	void SetProjectileDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ProjectileSystemPointer = ReferenceToSubsystem;
	}

	void SetParticleDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ParticleSystemPointer = ReferenceToSubsystem;
	}	
	
	void SetEventLogDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		EventLogSystemPointer = ReferenceToSubsystem;
	}

	bool IsNetInitialized() const;

	bool IsRolling() const { return bIsRolling; }

protected:
	void ServerTick();
	void ClientTick();
	bool IsCatchUpTick() const;
	bool IsTooFarBehind() const;
	bool IsTooFarAhead() const;
	uint32 EffectiveServerFrame() const;
	void DoCatchUp();
	void ProcessAuthorityData();
	uint32 FindEarliestMisprediction(uint32 StartFrame, uint32 EndFrame,
	const TMap<uint32,TMap<PlayerKey,FArtilleryShell>>& InAuthoritative);
	void ProcessRequestRouterBusyWorkerThread(const uint32 SeqNumber);

private:
	bool bRunning = false;
	TSharedPtr<FRequestRouter> RequestRouter;
	uint32_t TickliteNow = 0;
	FSharedEventRef StartRunAhead;
	TSharedPtr<BufferedMoveEvents>  Locomos_BufferNotThreadSafe;
	TSharedPtr<BufferedEvents> RequestorQueue_Abilities_TripleBuffer;
	TSharedPtr<BufferedAIMoveEvents> RequestorQueue_AI_Locomos_TripleBuffer;
	FSharedEventRef StartTicklitesSim;
	FSharedEventRef StartTicklitesApply;
	//Going forward, it is potentially worthwhile for us switch to this...
	ITickHeavy* ParticleSystemPointer = nullptr;
	ITickHeavy* ProjectileSystemPointer = nullptr;
	ITickHeavy* EventLogSystemPointer = nullptr;
	TSharedPtr<FArtilleryStateManager> StateManager = nullptr;
	TSharedPtr<FInputRollback> InputManager = nullptr;
	TWeakObjectPtr<UArtilleryDispatch> ArtilleryDispatch = nullptr;
	TObjectPtr<UCanonicalInputStreamECS> ContingentInputECSLinkage = nullptr;
	TWeakObjectPtr<UTransformDispatch> TransformDispatch = nullptr;
	TWeakObjectPtr<UBarrageDispatch> PhysicsManager = nullptr;
	TWeakObjectPtr<UInventoryDispatch> ItemsAndEventsManager = nullptr;
	TWeakObjectPtr<UWorld> GameWorld = nullptr;
	bool bIsServer = false;
	bool bIsRolling = false;
	uint32 CurrentSequence = 0;
	uint32 LastMispredictionSequence = 0;
	
	
	using FTMap = TMap<FSkeletonKey, FConservedTags>;
	//this needs to remain private and only be modified or used on this thread.
	//if you want to add the ability to expose this off-thread, first, see if the ATA already present in ArtilleryDispatch is good enough.
	//second, assess if you can use a shadow-copy-and-swap pattern identical to the one used for generating the quadtree we expose for radar.
	//third, if neither is true, let me know what you come up with! just ping me on github -JMK
	FTMap TagRollbackManagement = FTMap();
};
