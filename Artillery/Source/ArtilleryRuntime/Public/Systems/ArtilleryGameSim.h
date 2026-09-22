#pragma once
#include <InventoryDispatch.h>

#include "ArtilleryActorControllerConcepts.h"
#include "ArtilleryCommonTypes.h"
#include "LocomotionParams.h"
#include "RequestRouter.h"
#include "Structures/ConcurrencyTypes/ParallelFixedQueueTypes.h"

class UCanonicalInputStreamECS;
class UBarrageDispatch;
class UArtilleryDispatch;
class FInputRollback;
class FArtilleryStateManager;

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
	void RunEventsRequiringVerifiedTicks(uint32 Sequence, bool bIsVerified);
	void Simulate(uint32 Sequence, const TMap<PlayerKey, FArtilleryShell>& PrevInputs, const TMap<PlayerKey, FArtilleryShell>& Inputs, bool bIsVerified);

	void RollbackAndResimulate(uint32 FromFrame, bool bUseAuthorityIfAvailable = true);
	void RollbackToVerified();

	bool IsServer() const { return bIsServer; }
	uint32 GetCurrentSequence() const { return CurrentSequence; }
	uint32 GetLastVerifiedSequence() const;
	ArtilleryTime GetTickliteNow() const {	return TickliteNow; }

	void SetProjectileDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ProjectileSystemPointer = ReferenceToSubsystem;
	}

	void SetParticleDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		ParticleSystemPointer = ReferenceToSubsystem;
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
	void ProcessRequestRouterBusyWorkerThread();

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
	TSharedPtr<FArtilleryStateManager> StateManager = nullptr;
	TSharedPtr<FInputRollback> InputManager = nullptr;
	TWeakObjectPtr<UArtilleryDispatch> ArtilleryDispatch = nullptr;
	TObjectPtr<UCanonicalInputStreamECS> ContingentInputECSLinkage = nullptr;
	TWeakObjectPtr<UBarrageDispatch> PhysicsManager = nullptr;
	TWeakObjectPtr<UInventoryDispatch> ItemsAndEventsManager = nullptr;
	TWeakObjectPtr<UWorld> GameWorld = nullptr;
	bool bIsServer = false;
	bool bIsRolling = false;
	uint32 CurrentSequence = 0;
	uint32 LastMispredictionSequence = 0;
};
