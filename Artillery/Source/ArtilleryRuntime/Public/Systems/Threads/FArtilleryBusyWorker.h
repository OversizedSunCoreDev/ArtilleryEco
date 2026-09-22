#pragma once

#include "CoreMinimal.h"
#include "ArtilleryActorControllerConcepts.h"
#include "HAL/Runnable.h"
#include "CanonicalInputStreamECS.h"
#include "BristleconeCommonTypes.h"
#include "Containers/TripleBuffer.h"
#include "LocomotionParams.h"

#include "BarrageDispatch.h"
#include "RequestRouter.h"

//this is a busy-style thread, which runs preset bodies of work in a specified order. Generally, the goal is that it never
//actually sleeps. In fact, it yields rather than sleeps, in general operation.
// 
// This is similar to but functionally very different from a work-stealing or task model like what we see in rust.
// 
// 
// The artilleryworker needs to be kept fairly busy or it will melt one cpu core yield-cycling. to be honest, worth it.
// no, seriously. with all the other sacrifices we've made occupying one core with game-sim physics, reconciliation,
// rollbacks, and pattern matching is a pretty good bargain. we'll want to revisit this for servers, of course.

class FArtilleryGameSim;

class FArtilleryBusyWorker : public FRunnable {
	public:
	FArtilleryBusyWorker();
	virtual ~FArtilleryBusyWorker() override;
	//This isn't super safe, but Busy Worker is used in exactly one place
	//and the dispatcher that owns this memory MUST manage this lifecycle.
	TSharedPtr<BufferedMoveEvents>  Locomos_BufferNotThreadSafe;
	TSharedPtr<BufferedEvents> RequestorQueue_Abilities_TripleBuffer;
	TSharedPtr<BufferedAIMoveEvents> RequestorQueue_AI_Locomos_TripleBuffer;

	ArtilleryTime TickliteNow = 0;
	FSharedEventRef StartTicklitesSim;
	FSharedEventRef StartTicklitesApply;
	FSharedEventRef StartRunAhead;
	FSharedEventRef PostRunFrameProcessingLoop; //Intended to allow things to know when a full frame is done
	uint32 SeqNumber = 0;
	//Going forward, it is potentially worthwhile for us switch to this...
	ITickHeavy* ParticleSystemPointer = nullptr;
	ITickHeavy* ProjectileSystemPointer = nullptr;
	ITickHeavy* EventLogSystemPointer = nullptr;
	//Thanks to OrdIn, we can guarantee what is up when, and by expanding OrdIn, we can control what is
	//deinitialized when if we need to, as well. It wouldn't even be that hard, simply add a deregister to SkeletonLord
	
	virtual bool Init() override;
	void RunStandardFrameSim(bool& missedPrior,
		uint64_t& currentIndexCabling,
		bool& burstDropDetected,
		TheCone::PacketElement& current,
		bool& RemoteInput);
	// Currently accepts the dispatch ptr to run guns, this is a temporary thing though
	void ProcessRequestRouterBusyWorkerThread(UArtilleryDispatch* MyDispatch);
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;
	void RunFrameProcessingLoop(bool missedPrior, uint64_t currentIndexCabling, bool burstDropDetected, bool sent, uint32_t LastIncrementWindow, uint32_t lsbTime, const uint32_t SendHertzFactor, const uint32_t Period, const std::chrono::microseconds HalfStep, UArtilleryDispatch* ArtilleryDispatch);
	// stop me if you've heard this one before
	
	//this is a hack and MIGHT be replaced with an ECS lookup
	//though the clarity gain is quite nice, and privileging Cabling makes sense
	TSharedPtr<ArtilleryControlStream>  CablingControlStream;
	TSharedPtr<ArtilleryControlStream> BristleconeControlStream;
	TSharedPtr<ArtilleryControlStream> ThistleControlStream;
	TSharedPtr<FRequestRouter> RequestRouter;
	TheCone::RecvQueue InputRingBuffer;
	TheCone::SendQueue InputSwapSlot;
	UCanonicalInputStreamECS* ContingentInputECSLinkage = nullptr;
	UBarrageDispatch* ContingentPhysicsLinkage = nullptr;
	UTransformDispatch* UTransformLink = nullptr;
	// This is atomic so the unreal gamethread can set it
	std::atomic<bool> bPaused = false;
	
	
	// Intended for debugging safely from the game thread. You should not use this for impactful gameplay code
	bool QueueFunctionFromAnyThreadAndWait(TFunction<void()> Callback, float SecondsToWait = .1f);

	FCriticalSection EventsFromOtherThreadEndCriticalSection;
	TArray<FEvent*> EventsFromOtherThread_RequiresCriticalSection;
	
	
	FORCEINLINE bool IsRunning() const { return bRunning; };
	// Distinct from Running... Why? I needed something to indicate this is actually trying to run and not just constructed
	FORCEINLINE bool IsInitialized() const { return bInitialized; };
private:
	void Cleanup();
	
	TSharedPtr<FArtilleryGameSim> Game;
	// monotonic (race-fix 2026-06-25): true at construction, only ever set false by Cleanup()/Stop()/Exit().
	// Init() must NOT re-raise it -- a late-scheduled thread re-setting running=true after teardown set it false
	// is what orphaned the worker and hung the jthread join on exit.
	bool bRunning = true;
	bool bInitialized = false;
	using FTMap = TMap<FSkeletonKey, FConservedTags>;
	//this needs to remain private and only be modified or used on this thread.
	//if you want to add the ability to expose this off-thread, first, see if the ATA already present in ArtilleryDispatch is good enough.
	//second, assess if you can use a shadow-copy-and-swap pattern identical to the one used for generating the quadtree we expose for radar.
	//third, if neither is true, let me know what you come up with! just ping me on github -JMK
	FTMap TagRollbackManagement;
};
