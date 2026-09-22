// PARALLEL-SYSTEM STARTER (renamed from patch's FArtilleryBusyWorker.cpp).
// See FRollbackArtilleryWorker.h for context.
#include "Threads/FRollbackArtilleryWorker.h"
#include "ArtilleryDispatch.h"
#include "Windows/WindowsSystemIncludes.h"
#include <timeapi.h>
#include "Algo/AnyOf.h"
#include "ABarragePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ArtilleryBPLibs.h"
#include "InputRollback.h"
#include "BarrageDispatch.h"
#include "GameFramework/GameStateBase.h"
#include "Containers/TripleBuffer.h"
#include "ArtilleryLoadingProcessInterface.h"
#include "ArtilleryNetworkTypes.h"
#include "Algo/RemoveIf.h"
#include "Engine/GameInstance.h"
#include "HAL/ThreadManager.h"

constexpr uint32 ROLLBACK_WORKER_BufferSize = 30;

static TAutoConsoleVariable<int32> CVarForcedResimEnabled(
	TEXT("Artillery.ForcedResim.Enabled"),
	1,
	TEXT("Enable/disable forced periodic resimulation (0 = disabled, 1 = enabled)"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarForcedResimFrameCount(
	TEXT("Artillery.ForcedResim.FrameCount"),
	5,
	TEXT("Number of frames to resimulate during forced resimulation"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarForcedResimFrequency(
	TEXT("Artillery.ForcedResim.Frequency"),
	10,
	TEXT("How often to trigger forced resimulation (in frames)"),
	ECVF_Default);

FRollbackArtilleryWorker::FRollbackArtilleryWorker() : running(true)
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Constructing Artillery"));
}

FRollbackArtilleryWorker::~FRollbackArtilleryWorker()
{
	Cleanup();
}

bool FRollbackArtilleryWorker::Init()
{
	UE_LOG(LogTemp, Display, TEXT("Artillery:BusyWorker: Initializing Artillery thread"));
	Game = MakeShared<FArtilleryGameSim>();

	return ContingentPhysicsLinkage && ContingentPhysicsLinkage->GetWorld() && ContingentPhysicsLinkage->GetWorld()->IsGameWorld();
}

void FRollbackArtilleryWorker::RegisterLoadingProcessor(TScriptInterface<IArtilleryLoadingProcessInterface> Interface)
{
	if (!ensureMsgf(!bStartedSimulation, TEXT("Attempting to register loading processor when simulation already started")))
	{
		return;
	}

	ExternalLoadingProcessors.Add(Interface.GetObject());
}

void FRollbackArtilleryWorker::UnregisterLoadingProcessor(TScriptInterface<IArtilleryLoadingProcessInterface> Interface)
{
	ExternalLoadingProcessors.Remove(Interface.GetObject());
}

bool FRollbackArtilleryWorker::CheckNeedToDelaySimulation(FString& OutReason)
{
	if (!ContingentPhysicsLinkage)
	{
		OutReason = TEXT("Game or World invalid");
		return true;
	}

	UWorld* World = ContingentPhysicsLinkage->GetWorld();
	if (!World)
	{
		OutReason = TEXT("World is tearing down");
		return true;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (!GameState)
	{
		OutReason = TEXT("No AGameStateBase replicated yet");
		return true;
	}

	const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World);
	if (WorldContext)
	{
		if (!WorldContext->TravelURL.IsEmpty())
		{
			OutReason = TEXT("Pending travel");
			return true;
		}
		if (WorldContext->PendingNetGame != nullptr)
		{
			OutReason = TEXT("Connecting to server (PendingNetGame != nullptr)");
			return true;
		}
	}

	if (!World->HasBegunPlay())
	{
		OutReason = TEXT("World has not begun play");
		return true;
	}

	if (World->IsInSeamlessTravel())
	{
		OutReason = TEXT("Seamless travel");
		return true;
	}

	bool bFoundAnyLocalPC = false;
	APlayerController* PC = UGameplayStatics::GetPlayerController(ContingentInputECSLinkage, 0);
	if (PC)
	{
		bFoundAnyLocalPC = true;
	}

	if (!bFoundAnyLocalPC)
	{
		return true;
	}

	if (!GameState->HasMatchStarted())
	{
		OutReason = TEXT("Match not started");
		return true;
	}

	bool bAnyProcessorStillLoading = Algo::AnyOf(ExternalLoadingProcessors,
		[&OutReason](const TWeakInterfacePtr<IArtilleryLoadingProcessInterface>& Processor)
		{
			return (Processor.IsValid() &&
					!IArtilleryLoadingProcessInterface::ShouldStartArtillerySimulation(Processor.GetObject(), OutReason));
		});
	if (bAnyProcessorStillLoading)
	{
		return true;
	}

	return false;
}

uint32 FRollbackArtilleryWorker::Run()
{
	uint64_t currentIndexCabling = 0;
	bool sent = false;
	uint32_t LastIncrementWindow = ContingentInputECSLinkage->Now();
	uint32_t lsbTime = ContingentInputECSLinkage->Now();
	constexpr uint32_t sampleHertz = TheCone::LongboySendHertz;
	constexpr uint32_t RunHertz = LongboySendHertz;
	const uint32_t SendHertzFactor = sampleHertz / RunHertz;

	constexpr uint32_t Period = (1000000 / sampleHertz);
	constexpr uint32_t SleepError = 1500;
	constexpr uint32_t ErrorAdjustedPeriod = Period - SleepError;
	constexpr std::chrono::milliseconds Step(Period / 1000);

	UArtilleryDispatch* ArtilleryDispatch = ContingentInputECSLinkage->GetWorld()->GetSubsystem<UArtilleryDispatch>();
	ArtilleryDispatch->ThreadSetup();
	timeBeginPeriod(2);
	bool bReadyToStart = false;
	constexpr double CheckInterval = 2.5;
	double LastCheckTime = FPlatformTime::Seconds();
	FString DebugReasonForDelayingSimulation = "";
	Game->Initialize(ContingentPhysicsLinkage->GetWorld());

	while (running)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (!bReadyToStart && (CurrentTime - LastCheckTime) >= CheckInterval)
		{
			LastCheckTime = CurrentTime;

			if (!CheckNeedToDelaySimulation(DebugReasonForDelayingSimulation))
			{
				bReadyToStart = true;
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT(" FArtilleryBusyWorker::Run:simulation delayed due to [%s]"), *DebugReasonForDelayingSimulation);
			}
		}

		if (!ContingentPhysicsLinkage || !ContingentPhysicsLinkage->GetWorld() || ContingentPhysicsLinkage->GetWorld()->bIsTearingDown)
		{
			running = false;
			break;
		}
		if (!bReadyToStart)
		{
			std::this_thread::yield();
			continue;
		}
		Game->OnFrameUpdate();

		if (!Game->IsNetInitialized() && !IsServer())
		{
			std::this_thread::yield();
			continue;
		}

		bStartedSimulation = true;

		if (!sent &&
			(
				SeqNumber % SendHertzFactor <= SendHertzFactor / 2
			)
		)
		{
        	sent = true;
        	Game->Tick();
        }

		if (LastIncrementWindow + Period <= lsbTime)
		{
			LastIncrementWindow = FMath::Min(LastIncrementWindow + Period, lsbTime);
			if (SeqNumber % SendHertzFactor == 0)
			{
				sent = false;
			}
			++SeqNumber;
		}
		else if ((LastIncrementWindow + ErrorAdjustedPeriod) <= lsbTime)
		{
			std::this_thread::yield();
		}
		else
		{
			std::this_thread::sleep_for(Step);
		}
		lsbTime = ContingentInputECSLinkage->Now();
		if ((SeqNumber % sampleHertz) == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("Artillery Busy Worker hertz cycled: %u against seq %ld"),
				   lsbTime, SeqNumber);
		}

	}

	timeEndPeriod(2);
	UE_LOG(LogTemp, Display, TEXT("ARTILLERY EXIT BIG LOOP CYCLE."));
	return 0;
}

bool FRollbackArtilleryWorker::IsServer() const
{
	return IsValid(ContingentPhysicsLinkage) && ContingentPhysicsLinkage->GetWorld()->GetGameInstance()->IsDedicatedServerInstance();
}

void FRollbackArtilleryWorker::Cleanup()
{
	running = false;
	ExternalLoadingProcessors.Empty();
}
