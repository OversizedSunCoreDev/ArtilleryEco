#include "FArtilleryBusyWorker.h"
#include "ArtilleryDispatch.h"
#include "Windows/WindowsSystemIncludes.h"
#include <timeapi.h>
#include "LowLogTimeAndRate.h"
#include "ArtilleryBPLibs.h"
#include "ArtilleryGameSim.h"
#include "ArtilleryGunBlueprint.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "Containers/TripleBuffer.h"

FArtilleryBusyWorker::FArtilleryBusyWorker()
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Constructing Artillery"));
}

FArtilleryBusyWorker::~FArtilleryBusyWorker()
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Destructing Artillery"));
}

bool FArtilleryBusyWorker::Init()
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Initializing Artillery thread"));
	

	bInitialized = true;
	// NB: running is monotonic now (true at construction). Do NOT set it true here -- see header.
	return true;
}

void FArtilleryBusyWorker::RunStandardFrameSim(bool& missedPrior, uint64_t& currentIndexCabling,
                                               bool& burstDropDetected, PacketElement& current,
                                               bool& RemoteInput)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryBusyWorker::RunStandardFrameSim)
	
	auto InputFunction = [&]()
	{
		//this is an odd thing to do, I know, but we have some book-keeping we want to reserve for each code path.
		//once this settles a little, I'll refactor, but I'm going to end up reworking this next weekend.
		if (InputRingBuffer != nullptr && !InputRingBuffer.Get()->IsEmpty())
		{
			while (InputRingBuffer != nullptr && !InputRingBuffer.Get()->IsEmpty())
			{
				const Packet_tpl* packedInput = InputRingBuffer.Get()->Peek();
				const long indexInput = packedInput->GetCycleMeta() + 3; //faster than 3xabs or a branch.
				//unlike the old design, we use an array of inputs from first -> current
				//so we want to add oldest first, then next, then next.
				//we'll need to amend this to handle correct defaulting of missing input,
				//which we can detect by both cycle skips and arrival window misses.
				//we then need a way, during rollbacks, to perform the rewrite.
				//right now, we just wait until we get the remote input.
				if (missedPrior)
				{
					if (burstDropDetected)
					{
						BristleconeControlStream->Add(*const_cast<Packet_tpl*>(packedInput)->GetPointerToElement((indexInput - 2) % 3),
						                              packedInput->GetTransferTime());
					}

					BristleconeControlStream->Add(*const_cast<Packet_tpl*>(packedInput)->GetPointerToElement((indexInput - 1) % 3),
					                              packedInput->GetTransferTime());
				}
				BristleconeControlStream->Add(*const_cast<Packet_tpl*>(packedInput)->GetPointerToElement(indexInput % 3),
				                              packedInput->GetTransferTime());

				RemoteInput = true; //we check for empty at the start of the while. no need to check again.
				InputRingBuffer.Get()->Dequeue();
			}

			if (RemoteInput == true)
			{
				missedPrior = false;
				burstDropDetected = false;
			}
			else
			{
				if (burstDropDetected)
				{
					//add rolling average switch-over here
				}
				if (missedPrior)
				{
					burstDropDetected = true;
				}
				missedPrior = true;
			}
		}
		else if (InputSwapSlot != nullptr && !InputSwapSlot.Get()->IsEmpty())
		{
			//though it's probably more elegant and faster to index over the control streams
			while (InputSwapSlot != nullptr && !InputSwapSlot.Get()->IsEmpty())
			{
				current = *InputSwapSlot.Get()->Peek();
				CablingControlStream->Add(current);

				InputSwapSlot.Get()->Dequeue();
			}
		}
		else
		{
			//----------------------------------
			//if we got nothing, repeat prior.
			//0000000000000000000000000000000000

			CablingControlStream->Add(CablingControlStream->get(CablingControlStream->highestInput - 1)->MyInputActions, TickliteNow);
		}
#define ARTILLERY_FIRE_CONTROL_MACHINE_HANDLING (false)
		//First, locomotions are pushed. Patterns run here. The thread queues the locomotions and fires.
		//the dispatch fires guns via the machines on the gamethread.

		//Pattern matchers match, set events, and then those events are handed to the dispatch for now.
		//gradually, we'll be able to run more and more of them on this thread, freeing us from the tyranny.
		//Per input stream, run their patterns here. god in heaven.
		EventBuffer& refDangerous_LifeCycleManaged_Abilities_TripleBuffered = RequestorQueue_Abilities_TripleBuffer->GetWriteBuffer();

		if (currentIndexCabling < CablingControlStream->highestInput)
		{
			//today's sin is PRIDE, bigbird!
			for (int i = currentIndexCabling; i < CablingControlStream->highestInput; ++i)
			{
				//TODO: does this leak memory?
				ActorKey StreamActorKey = CablingControlStream->GetActorByInputStream();
				if (StreamActorKey)
				{
					Locomos_BufferNotThreadSafe->Add(LocomotionParams(CablingControlStream->peek(i)->SentAt,
					                                                  StreamActorKey,
					                                                  *CablingControlStream->peek(i - 1),
					                                                  *CablingControlStream->peek(i)));

					// this looks wrong but I'm pretty sure it ain' since we reserve highest.
					CablingControlStream->MyPatternMatcher->runOneFrameWithSideEffects(true,
					                                                                   0,
					                                                                   0,
					                                                                   i,
					                                                                   refDangerous_LifeCycleManaged_Abilities_TripleBuffered);
				}
				//even if this doesn't get played for some reason, this is the last chance we've got to make a
				//truly informed decision about the matter. By the time we reach the dispatch system, that chance is gone.
				//Better to skip a cosmetic once in a while than crash the game.
				CablingControlStream->get(CablingControlStream->highestInput - 1)->RunAtLeastOnce = true;
			}
		}
	};
	
	FArtillerySimContext Context;
	Context.bIsVerified = true;
	Context.Tick = currentIndexCabling-1;
	Context.InputGatherFunction = InputFunction;
	// Not used here because we use InputFunction instead but I am going to leave these here because otherwise I would forget
	Context.PrevInputs = {};
	Context.Inputs = {};

	Game->Simulate(Context);
}

void FArtilleryBusyWorker::RunFrameProcessingLoop(bool missedPrior, uint64_t currentIndexCabling, bool burstDropDetected, bool sent, uint32_t LastIncrementWindow, uint32_t lsbTime, const uint32_t SendHertzFactor, const uint32_t Period, const std::chrono::microseconds HalfStep, UArtilleryDispatch* ArtilleryDispatch)
{
	timeBeginPeriod(1);
	
	//create the game now that we're processing da frame
	GetOrCreateGameSim();
	Game->Initialize(ArtilleryDispatch->GetWorld());
	
	while (bRunning)
	{
		if (!sent &&
			!bPaused &&
			(
				InputRingBuffer != nullptr &&
				(!InputRingBuffer.Get()->IsEmpty() || SeqNumber % SendHertzFactor <= SendHertzFactor / 2)
				//we no longer slide all the way to the end.
				//Instead, we only slide to 1/2, so somewhat less latency than a curious version of 256hz.
				//I resent it, but the beast of nonetime had to go.
						//it got a last hit in.
						//jmk 12/6/25
			)
		)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryBusyWorker::RunFrameProcessingLoop)
			CustomTimer<"BusyWorkerCoreLoop"> Time;
			currentIndexCabling = CablingControlStream->highestInput;
			PacketElement current = 0;
			bool RemoteInput = false;
			RunStandardFrameSim(missedPrior, currentIndexCabling, burstDropDetected, current, RemoteInput);
			
			/*
			* Note: We also have Iris performing intermittent state stomps to recover from more serious desyncs.
			* Ultimately, rollback can never solve everything. The windows just get too wide.
			*/
			sent = true;
			TickliteNow = ContingentInputECSLinkage->Now(); // this updates ONCE PER CYCLE. ONCE. THIS IS INTENDED.
			
			
			
			// Manage stuff added from QueueFunctionFromAnyThreadAndWait
			PostRunFrameProcessingLoop->Trigger();
			
			{
				FScopeLock Lock(&EventsFromOtherThreadEndCriticalSection);
				
				for (FEvent* Event : EventsFromOtherThread_RequiresCriticalSection)
				{
					Event->Wait();
				}
				
				EventsFromOtherThread_RequiresCriticalSection.Reset();
			}
		}

		//unlike cabling, we do our time keeping HERE. It may be worth switching cabling to also follow this.
		//Alternatively, it may be worthwhile to switch to a wait/wake pattern against cabling, where we wait half the interval max, then
		//start simulating in the remaining half. We're already eating 4ms of latency, and the act of sleeping is getting
		//kind of expensive actually.
		if (LastIncrementWindow + Period <= lsbTime)
		{
			LastIncrementWindow = FMath::Min(LastIncrementWindow + Period, lsbTime);
			if (SeqNumber % SendHertzFactor == 0)
			{
				sent = false;
				
			}
			++SeqNumber;
		}//because we would be pushing our luck w. the error bars on sleep in certain cases, we try to detect those so we can instead spin. Hence the modifier.
		//this is saying if the current time + the sleep time + the margin for error is less than the target time, then we can sleep.
		else if (lsbTime + (1.3 * (HalfStep).count()) <= (LastIncrementWindow + Period))
		{
			std::this_thread::sleep_for(HalfStep);
		}
		
		lsbTime = ContingentInputECSLinkage->Now();
	}
}

uint32 FArtilleryBusyWorker::Run()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryBusyWorker::Run)

	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Running Artillery thread"));
	if (RequestorQueue_Abilities_TripleBuffer == nullptr)
	{
		return -1;
	}
	
	bool missedPrior = false;
	uint64_t currentIndexCabling = 0;
	bool burstDropDetected = false;
	bool sent = false;
	//TODO: remember why this needs to be an int. 
	//if you wanna use this for a really long lived session, you'll need to fix it. you know. one longer than 34 years.
	SeqNumber = 0;
	//Hi! Jake here! Reminding you that this will CYCLE
	//That's known. Isn't that fun? :) Don't reorder these, by the way.
	uint32_t LastIncrementWindow = ContingentInputECSLinkage->Now();
	uint32_t lsbTime = ContingentInputECSLinkage->Now();
	constexpr uint32_t sampleHertz = TheCone::CablingSampleHertz;
	constexpr uint32_t RunHertz = LongboySendHertz;
	const uint32_t SendHertzFactor = sampleHertz / RunHertz; // THIS ROUNDS DOWN. IT IS INT MATH.
	//in other words, artillery will always run at powers of two right now. that's intended for prototype.
	//we actually run a LITTLE fast to offset us against cabling.
	constexpr uint32_t Period = 999900 / sampleHertz;

	// we prefer to land near the _start_ of a period, so we bias.
	constexpr auto HalfStep = std::chrono::microseconds(Period / 2);


	//we are started by Artillery Dispatch, but we can't use it in the .h file to avoid dependencies.
	//so we know it's live, but we don't take a ref to it until this point.
	//we only use it for GrantFeed, but it's important that we start abiding by separation of concerns
	//where we can, so we're trying to hide the barrage dependency here in a sense. We can't fully, but.
	UArtilleryDispatch* ArtilleryDispatch = ContingentInputECSLinkage->GetWorld()->GetSubsystem<UArtilleryDispatch>();
	
	// This sets up a thread-local ptr to this dispatch, which is useful mostly for BP libraries currently
	FArtilleryDispatchThreadScope DispatchScope(ArtilleryDispatch);
	ArtilleryDispatch->ThreadSetup();
	
	//Run loop is in here.
	RunFrameProcessingLoop(missedPrior, currentIndexCabling, burstDropDetected, sent, LastIncrementWindow, lsbTime,
	                       SendHertzFactor, Period, HalfStep, ArtilleryDispatch);

	//just in case we end up unrolling or something weird.
	timeEndPeriod(1);
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Run Ended."));
	return 0;
}

void FArtilleryBusyWorker::Exit()
{
	UE_LOG(LogTemp, Display, TEXT("ARTILLERY OFFLINE."));
	Cleanup();
}

void FArtilleryBusyWorker::Stop()
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Stopping Artillery Busyworker thread."));
	Cleanup();
}

bool FArtilleryBusyWorker::QueueFunctionFromAnyThreadAndWait(TFunction<void()> Callback, float SecondsToWait)
{
	FEvent* NewEventForArtilleryThread = FPlatformProcess::GetSynchEventFromPool(true);
	{
		FScopeLock Lock(&EventsFromOtherThreadEndCriticalSection);
		EventsFromOtherThread_RequiresCriticalSection.Add(NewEventForArtilleryThread);
	}
	
	bool bWaitComplete = false;
	// This will return false if it could not wait, which is good... we don't want this to deadlock I think
	if (PostRunFrameProcessingLoop->Wait(FTimespan::FromSeconds(SecondsToWait)))
	{
		bWaitComplete = true;
		Callback();
	}
	
	// Now let artillery continue
	NewEventForArtilleryThread->Trigger();
	
	return bWaitComplete;
}

TSharedPtr<FArtilleryGameSim> FArtilleryBusyWorker::GetOrCreateGameSim()
{
	// Shameful bandaid fix to init order being weird. We definitely do not want to make thjis kind of vibes-based
	if (Game)
	{
		return Game;
	}
	
	Game = MakeShared<FArtilleryGameSim>();
	return Game;
}

void FArtilleryBusyWorker::Cleanup()
{
	bRunning = false;
}
