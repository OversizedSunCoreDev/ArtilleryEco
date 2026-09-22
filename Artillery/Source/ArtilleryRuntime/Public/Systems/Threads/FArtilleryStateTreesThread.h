#pragma once

#include <thread>

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "ArtilleryCommonTypes.h"
#include "RequestRouter.h"

//this is a busy-style thread, which runs AI systems in predetermined order. 
template <typename UDispatch>
class FStateTreesWorker : public FRunnable
{
	//This isn't super safe but like busy worker, ticklites only runs in one spot.
	friend class UArtilleryDispatch;
	ArtilleryTime LocalNow;

protected:
	FSharedEventRef RunAheadStateTrees;
	
public:
	TSharedPtr<FRequestRouter> RequestRouter;
	FArtilleryAddEnemyToControllerSubsystem EnemyRegisterHook;
	//Templating here is used to both make reparenting easier if needed later and to simplify our dependency tree
	UDispatch* DispatchOwner;

	void ProcessRequestRouterAIWorkerThread()
	{
		if (RequestRouter)
		{
			for (FRequestRouter::FeedMap& WorkerFeedMap : RequestRouter->AIThreadAcc)
			{
				TSharedPtr<FRequestRouter::ThreadFeed> HoldOpen;
				if (WorkerFeedMap.Queue && ((HoldOpen = WorkerFeedMap.Queue)) && WorkerFeedMap.That != std::thread::id()) //if there IS a thread.
				{
					FRequestThing RouterQueue;
					while (HoldOpen->Dequeue(RouterQueue))
					{
						//PINPOINT: YAAITHREADBOYRUNNETHREQUESTSHERE
						if (RouterQueue.GetType() == ArtilleryRequestType::BindAI)
						{
							EnemyRegisterHook.ExecuteIfBound(RouterQueue.SourceOrSelf, RouterQueue.Stamp);
						}
					}
				}
			}
		}
	}

	ArtilleryTime GetShadowNow() const
	{
		return DispatchOwner->GetShadowNow();
	}

	virtual ~FStateTreesWorker() override
	{
		UE_LOG(LogTemp, Display, TEXT("Artillery: Destructing AI thread."));
	}
	
	virtual bool QueueRollback()
	{
		//rollback is not implemented yet, but works by removing ticklikes added after the rollback's timestamp.
		//then adding back in any expired ticklikes that should be revived, clearing the current tick, and beginning resim.
		//Implementing this will not be easy, but it will suck a lot less than trying to do this with gameplay effects.
		//This is one reason we advocate STRONGLY for the use of KEYS over references, as references to memmory location
		//are not durable across rollbacks.
		return false; 
	}

	virtual bool Init() override
	{
		LocalNow = 0;
		UE_LOG(LogTemp, Display, TEXT("Artillery: Booting StateTrees thread."));
		return true;
	}
	
	virtual uint32 Run() override
	{
		int SeqNumber = 0;
		DispatchOwner->ThreadSetup();
		while(running) {
			DispatchOwner->RunEnemySim(SeqNumber);
			RunAheadStateTrees->Wait();
			RunAheadStateTrees->Reset(); // we can run long on sim, not on apply.
			
			++SeqNumber;
			
			ProcessRequestRouterAIWorkerThread();
		}
	
		return 0;
	}

	virtual void Exit() override
	{
		UE_LOG(LogTemp, Display, TEXT("Artillery: Exiting AI thread."));
		running = false;
		Cleanup();
	}

	virtual void Stop() override
	{
		running = false;
		UE_LOG(LogTemp, Display, TEXT("Artillery:AIWorker: Stopping Artillery AI thread."));
		Cleanup();
	}

private:
	void Cleanup()
	{
		running = false;
	};

	bool running = true;
};

