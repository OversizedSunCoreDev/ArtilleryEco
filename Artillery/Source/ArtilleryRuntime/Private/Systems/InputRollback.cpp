#include "InputRollback.h"

#include "ABarragePlayerController.h"
#include "ArtilleryNetworkTypes.h"
#include "ArtilleryBPLibs.h"
#include "FCablingRunner.h"
#include "CanonicalInputStreamECS.h"
#include "Algo/RemoveIf.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<int32> CVarForceMispredictionEnabled(
	TEXT("Artillery.ForceMisprediction.Enabled"),
	0,
	TEXT("Enable/disable forced misspredictions (0 = disabled, 1 = enabled)"),
	ECVF_Default);

//see ArtilleryGame.cpp for the rationale; same gate, different value type.
template <typename TValue>
static TArray<PlayerKey> SortedPlayerKeys(const TMap<PlayerKey, TValue>& InMap)
{
	TArray<PlayerKey> Keys;
	InMap.GenerateKeyArray(Keys);
	Keys.Sort([](const PlayerKey& A, const PlayerKey& B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});
	return Keys;
}


FInputRollback::FInputRollback()
{
    InputSwapSlot = MakeShareable(new IncQ(256));
}

void FInputRollback::Initialize(UWorld* World)
{
	GameWorld = World;
    CanonicalInput = World->GetSubsystem<UCanonicalInputStreamECS>();
    NetworkDispatch = World->GetSubsystem<UBristleconeWorldSubsystem>();
    bIsServer = World->GetGameInstance()->IsDedicatedServerInstance();
    UCablingWorldSubsystem* DirectLocalInputSystem = World->GetSubsystem<UCablingWorldSubsystem>();
    
    // @fixme is this seems to eat local input! Requesting Jake assistance
    // DirectLocalInputSystem->DestructiveChangeLocalOutboundQueue(InputSwapSlot);
}

void FInputRollback::ProcessPredictiveInputs(uint32 Sequence, ArtilleryTime TickliteNow)
{
	if (!GameWorld.IsValid())
	{
		return;
	}

    uint64_t Current;
    // Get The local inputs
    bool hasFocus = true;//ArtilleryDispatch->ShouldProcessInputs();
    if (hasFocus)
    {
        if (InputSwapSlot.IsValid() && !InputSwapSlot->IsEmpty())
        {
            while (InputSwapSlot != nullptr && !InputSwapSlot.Get()->IsEmpty())
            {
                Current = *InputSwapSlot.Get()->Peek();
                auto streamkey = CanonicalInput->GetStreamForPlayer(UArtilleryLibrary::GetLocalPlayerKey(GameWorld.Get()));
                auto sptr = CanonicalInput->GetStream(streamkey);
                sptr->Add(Current);
                InputSwapSlot.Get()->Dequeue();
            }
        }
        else
        {
            auto streamkey = CanonicalInput->GetStreamForPlayer(UArtilleryLibrary::GetLocalPlayerKey(GameWorld.Get()));
            auto sptr = CanonicalInput->GetStream(streamkey);

            // Reuse the last local input
            sptr->Add(sptr->peek(sptr->highestInput - 1)->MyInputActions,
                  TickliteNow);
        }
    }
    else
    {
        if (InputSwapSlot.IsValid() && !InputSwapSlot->IsEmpty())
        {
            while (!InputSwapSlot->IsEmpty())
            {
                InputSwapSlot->Dequeue();
            }
        }

        auto streamkey = CanonicalInput->GetStreamForPlayer(UArtilleryLibrary::GetLocalPlayerKey(GameWorld.Get()));
        auto sptr = CanonicalInput->GetStream(streamkey);

        sptr->Add(FCabling::BlankKeyboard,
              TickliteNow);
    }

    //predict other players by repeating their last input. sorted; future cross-stream
    //predictors should not be the place where determinism breaks.
    for (PlayerKey ThisPlayer : SortedPlayerKeys(CanonicalInput->SessionPlayerToStreamMapping))
    {
        auto streamkey = CanonicalInput->GetStreamForPlayer(ThisPlayer);
        auto sptr = CanonicalInput->GetStream(streamkey);

        sptr->Add(sptr->peek(sptr->highestInput - 1)->MyInputActions,
              TickliteNow);
    }
}

TMap<PlayerKey, FArtilleryShell> FInputRollback::GetInputsForSequence(uint32 Sequence, ArtilleryTime TickliteNow)
{
	TMap<PlayerKey, FArtilleryShell> ReturnInputs;
    if (!CanonicalInput.IsValid())
    {
        return ReturnInputs;
    }
    //LastProcessedInputIndex is per-player, but ReturnInputs build-order should match
    //the server's. sort.
    for (PlayerKey pkey : SortedPlayerKeys(CanonicalInput->SessionPlayerToStreamMapping))
    {
        InputStreamKey skey = CanonicalInput->SessionPlayerToStreamMapping[pkey];

        TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> stream =
            CanonicalInput->GetStream(skey);

        if (!stream.IsValid()) {
            continue;
        }

        uint64 highest = stream->highestInput;
        uint64 lastProcessed = LastProcessedInputIndex.FindOrAdd(pkey, 0);
        for (uint64 i = lastProcessed; i < highest; i++)
        {
            std::optional<FArtilleryShell> MaybeShell = stream->peek(i);

            if (!MaybeShell.has_value()) {
                continue;
            }

            if (MaybeShell->RunAtLeastOnce || ReturnInputs.Contains(pkey))
            {
                continue;
            }

            ReturnInputs.Add(pkey, MaybeShell.value());

            LastProcessedInputIndex[pkey] = highest;
        }
    }

    return ReturnInputs;
}

void FInputRollback::SendInputsToServer(uint32 StartSequence, uint32 EndSequence, const TMap<PlayerKey, FArtilleryShell>& Inputs)
{
    if (!GameWorld.IsValid() || !NetworkDispatch.IsValid() || Inputs.IsEmpty())
    {
        return;
    }

    ABarragePlayerController* LocalPC = Cast<ABarragePlayerController>(
        UGameplayStatics::GetPlayerController(CanonicalInput->GetWorld(), 0));
    if (!LocalPC)
    {
        return;
    }
    const PlayerKey LocalKey = LocalPC->TrueName;
    if (!Inputs.Contains(LocalKey))
    {
        return;
    }

    InputHistory.FindOrAdd(EndSequence) = Inputs;

    // modern flat send (Phase-1, JMK: "single queue to send, we only send ours, per world"):
    // enqueue OUR local input onto the single per-world QueueToSend; the Longboy reflector relays.
    // Old per-player FArtilleryPacket build + MulticastPacket/TriggerSend removed.
    // TODO(goback): the testing hack JMK flagged, and confirm this does not double-send with the
    //   existing Cabling->FBristleconeSender path that already feeds QueueToSend.
    const FArtilleryShell& OurShell = Inputs.FindChecked(LocalKey);
    if (NetworkDispatch->QueueToSend.IsValid())
    {
        NetworkDispatch->QueueToSend->Enqueue(OurShell.MyInputActions);
    }
}

void FInputRollback::ParseAuthorityData()
{
    if (!CanonicalInput.IsValid() || !NetworkDispatch.IsValid())
    {
        return;
    }

    // modern flat-packet authoritative ingest (Phase-1, JMK direction "ParseAuthority rehomes,
    // modernflatpack"): drain the SINGLE per-world receive queue of flat FBristleconePacket; the
    // Longboy reflector relays remote input here. Old per-player GetPlayerNetworks()/
    // GetQueueOfReceived(key)/FArtilleryPacket removed.
    // TODO(goback): coordinate with FArtilleryBusyWorker's drain of the same queue; multi-player
    //   demux; exact clone selection. First pass: newest clone = local player's input at packet cycle.
    TheCone::RecvQueue Queue = NetworkDispatch->QueueOfReceived;
    if (!Queue.IsValid())
    {
        return;
    }

    const PlayerKey LocalKey = UArtilleryLibrary::GetLocalPlayerKey(GameWorld.Get());
    while (!Queue->IsEmpty())
    {
        const TheCone::Packet_tpl* Pkt = Queue->Peek();
        if (!Pkt)
        {
            Queue->Dequeue();
            continue;
        }
        const uint32 FrameNumber = static_cast<uint32>(Pkt->GetCycleMeta());
        ServerFrame = FMath::Max(ServerFrame, FrameNumber);

        FArtilleryShell Shell;
        Shell.MyInputActions = *const_cast<TheCone::Packet_tpl*>(Pkt)->GetPointerToElement(0);
        AuthoritativeInputs.FindOrAdd(FrameNumber).Add(LocalKey, Shell);

        bReceivedInitialNetUpdate = true;
        Queue->Dequeue();
    }
}

void FInputRollback::ClearBufferedInputs()
{
	if (bIsServer)
	{
		// why would you want this on server?
	}
	else
	{
		if (InputSwapSlot.IsValid() && !InputSwapSlot->IsEmpty())
		{
			while (!InputSwapSlot->IsEmpty())
			{
				InputSwapSlot->Dequeue();
			}
		}
	}
}

TMap<PlayerKey, FArtilleryShell> FInputRollback::GetBufferedInputs(uint32 FrameNumber)
{
    return InputHistory.Contains(FrameNumber) ? InputHistory[FrameNumber] : TMap<PlayerKey, FArtilleryShell>();
}

// SendInputsToClients removed (Phase-1, JMK): the Longboy reflector relays server->clients; we
// only send ours. Old per-player FArtilleryPacket multicast (MulticastPacket/TriggerSend) gone.

void FInputRollback::ProcessBufferedInputs(uint32 Sequence)
{
    if (!CanonicalInput.IsValid() || !InputHistory.Contains(Sequence))
    {
        return;
    }

    TMap<PlayerKey, FArtilleryShell>& FrameInputs = InputHistory[Sequence];

    //sorted; per-stream Add order is fine either way, but cross-stream consumers care.
    for (PlayerKey Player : SortedPlayerKeys(FrameInputs))
    {
        const FArtilleryShell& Shell = FrameInputs[Player];

        InputStreamKey StreamKey = CanonicalInput->GetStreamForPlayer(Player);
        TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> Stream = CanonicalInput->GetStream(StreamKey);

        if (!Stream.IsValid())
        {
            continue;
        }

        Stream->Add(Shell.MyInputActions, Shell.SentAt);
    }

    InputHistory.Remove(Sequence);
}

void FInputRollback::ClearBuffers()
{
    InputHistory.Empty();
    AuthoritativeInputs.Empty();
    LastProcessedInputIndex.Empty();
}

void FInputRollback::Shutdown()
{
    ClearBuffers();
    CanonicalInput = nullptr;
    NetworkDispatch = nullptr;
}
