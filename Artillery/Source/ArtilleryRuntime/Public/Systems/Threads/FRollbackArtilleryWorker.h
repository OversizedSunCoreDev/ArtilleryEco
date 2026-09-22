// PARALLEL-SYSTEM STARTER (renamed from patch's FArtilleryBusyWorker).
// Current main already has its own FArtilleryBusyWorker that diverged after this
// patch was written. This file lands the patch's reworked worker shell alongside
// the existing one so the rollback architecture can be exercised without ripping
// the live worker out. See RollbackBundle/INTEGRATION_NOTES.md for wiring guidance.
#pragma once

#include "CoreMinimal.h"
#include "ArtilleryActorControllerConcepts.h"
#include "ArtilleryGameSim.h"
#include "HAL/RunnableThread.h"
#include "CanonicalInputStreamECS.h"
#include "BristleconeCommonTypes.h"
#include "Containers/TripleBuffer.h"
#include "LocomotionParams.h"
#include "BarrageDispatch.h"
#include "RequestRouter.h"

// FRollbackArtilleryWorker delegates the entire simulation to FArtilleryGame,
// only retaining: thread housekeeping, the deferred-start gate
// (CheckNeedToDelaySimulation), and Loading-Process-Interface registration.
//TODO (prefer merge)
//This needs to be merged with or supersede ArtilleryBusyWorker to bring rollback online.
class IArtilleryLoadingProcessInterface;

class FRollbackArtilleryWorker : public FRunnable {
	public:
	FRollbackArtilleryWorker();
	virtual ~FRollbackArtilleryWorker() override;

	void RegisterLoadingProcessor(TScriptInterface<IArtilleryLoadingProcessInterface> Interface);
	void UnregisterLoadingProcessor(TScriptInterface<IArtilleryLoadingProcessInterface> Interface);
	bool CheckNeedToDelaySimulation(FString& OutReason);
	virtual uint32 Run() override;

	uint32 SeqNumber = 0;

	virtual bool Init() override;

	bool IsServer() const;

	ArtilleryTime GetTickliteNow() const {	return ContingentInputECSLinkage->Now();; }

	void SetProjectileDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		if (ensure(Game))
		{
			Game->SetProjectileDispatch(ReferenceToSubsystem);
		}
	}

	void SetParticleDispatch(ITickHeavy* ReferenceToSubsystem)
	{
		if (ensure(Game))
		{
			Game->SetParticleDispatch(ReferenceToSubsystem);
		}
	}

	virtual void Exit() override
	{
		Cleanup();
	};

	virtual void Stop() override
	{
		Cleanup();
	};

	bool IsRollingBack() const { return Game->IsRolling(); }

	TObjectPtr<UBarrageDispatch> ContingentPhysicsLinkage;
	TObjectPtr<UCanonicalInputStreamECS> ContingentInputECSLinkage;
private:
	TSharedPtr<FArtilleryGameSim> Game;
	TArray<TWeakInterfacePtr<IArtilleryLoadingProcessInterface>> ExternalLoadingProcessors;
	void Cleanup();

	bool running = true;
	bool bStartedSimulation = false;
};
