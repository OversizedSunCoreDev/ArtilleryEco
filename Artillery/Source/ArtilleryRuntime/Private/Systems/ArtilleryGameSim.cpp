#include "ArtilleryGameSim.h"

#include "ArtilleryDispatch.h"
#include "ArtilleryRuntimeModule.h"
#include "InputRollback.h"
#include "BarrageDispatch.h"
#include "CanonicalInputStreamECS.h"
#include "CoordinateUtils.h"
#include "FWorldSimOwner.h"
#include "InventoryDispatch.h"
#include "StateContainer.h"

//TMap iteration is hash-bucketed; bucket layout doesn't survive a process boundary.
//anything participating in sim ordering walks keys sorted.
static TArray<PlayerKey> SortedPlayerKeys(const TMap<PlayerKey, FArtilleryShell>& InMap)
{
	TArray<PlayerKey> Keys;
	InMap.GenerateKeyArray(Keys);
	Keys.Sort([](const PlayerKey& A, const PlayerKey& B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});
	return Keys;
}

//Stress-test only. When on, replaces the real misprediction path with a periodic
//forced rollback, so leaving it on in production hides every real divergence.
static TAutoConsoleVariable<int32> CVarForceRollbackEnabled(
	TEXT("Artillery.ForceLocalRollback.Enabled"),
	0,
	TEXT("Stress-test only: force a periodic rollback regardless of misprediction. 0 = off (production path), 1 = forced rollback every ~3rd frame."),
	ECVF_Default);

// "Dumb mode": until the Longboy server is wired, the bristle->artillery authoritative feed is empty,
// so the real ServerFrame never advances and the rollback wedges in catch-up forever (offline/no-server/
// automation). When true, EffectiveServerFrame() rides the live edge (ServerFrame == CurrentSequence) so
// the rollback never catches up / rolls back. MegafunkUtils-style CVar bound to a plain global (cheap to
// read in the hot tick). Default true for Phase-1; pin/flip via Config [ConsoleVariables]. -- JMK direction.
static bool GbArtilleryRollbackDumbServerFrame = true;
static FAutoConsoleVariableRef CVarArtilleryRollbackDumbServerFrame(
	TEXT("Artillery.Rollback.DumbServerFrame"),
	GbArtilleryRollbackDumbServerFrame,
	TEXT("Dumb mode: authoritative ServerFrame rides the live edge so rollback never catches up. For offline/no-server/test. 0 = normal server-driven rollback."),
	ECVF_Default);

// TODO: be less lazy and move to developer setting
static const int32 MaxRollbacks = 20;

FArtilleryGameSim::FArtilleryGameSim()

{
	StateManager = MakeShared<FArtilleryStateManager>(MaxRollbacks*1.5);
	InputManager = MakeShared<FInputRollback>();
	CurrentSequence = 0;
}

void FArtilleryGameSim::OnFrameUpdate()
{
	if (!InputManager.IsValid())
	{
		return;
	}

	// we dont need to parse data on simulation ticks since it does not break determinism
	if (IsServer())
	{
		// ParseClientsData removed (Phase-1): server-side relay is the Longboy reflector's job now.
		// TODO(goback): dedicated-server handling if/when we run one.
	}
	else
	{
		// Receive net updates and detect misspredictions, rollback and resimulate if necessary
		InputManager->ParseAuthorityData();

		// Check if forced rollback is enabled
		if (CVarForceRollbackEnabled.GetValueOnAnyThread() > 0 && CurrentSequence > MaxRollbacks)
		{
			if (CurrentSequence - LastMispredictionSequence > 2)
			{
				bIsRolling = true;
				uint32 MispredFrame = FMath::Max<uint32>((CurrentSequence - MaxRollbacks + 1), 1);
				RollbackAndResimulate(MispredFrame, false);
				bIsRolling = false;
			}
		}
		else
		{
			ProcessAuthorityData();
		}
	}

	if (!bRunning || bIsRolling)
	{
		InputManager->ClearBufferedInputs();
	}
}

void FArtilleryGameSim::Initialize(UWorld* World)
{
	GameWorld = World;
	if (!ensure(GameWorld.IsValid()))
	{
		UE_LOG(LogTemp, Error, TEXT("ArtilleryGame: Cannot initialize from null world"));
		return;
	}

	bIsServer = GameWorld->GetGameInstance()->IsDedicatedServerInstance();

	ContingentInputECSLinkage = GameWorld->GetSubsystem<UCanonicalInputStreamECS>();
	ArtilleryDispatch = GameWorld->GetSubsystem<UArtilleryDispatch>();
	if (!ensure(ArtilleryDispatch.IsValid()))
	{
		UE_LOG(LogTemp, Error, TEXT("ArtilleryGame: Required subsystems not found"));
		return;
	}
	
	// @todo arguably transform dispatch should be later but I moved dispatch into here
	TransformDispatch = GameWorld->GetSubsystem<UTransformDispatch>();

	
	PhysicsManager = GameWorld->GetSubsystem<UBarrageDispatch>();
	ItemsAndEventsManager = GameWorld->GetSubsystem<UInventoryDispatch>();


	if (ensure(InputManager.IsValid()))
	{
		InputManager->Initialize(GameWorld.Get());
	}

	if (ensure(StateManager.IsValid()))
	{
		StateManager->Initialize(GameWorld.Get());
	}

	RequestRouter = ArtilleryDispatch->RequestRouter;
	StartTicklitesApply = ArtilleryDispatch->StartTicklitesApply;
	StartTicklitesSim = ArtilleryDispatch->StartTicklitesSim;
	StartRunAhead = ArtilleryDispatch->StartRunAhead;

	//IF YOU REMOVE THIS. EVERYTHING EXPLODE. IN A BAD WAY.
	//TARRAY IS A VALUE TYPE. SO IS TRIPLEBUFF I THINK.
	RequestorQueue_Abilities_TripleBuffer = ArtilleryDispatch->RequestorQueue_Abilities_TripleBuffer;
	//OH BOY. REFERENCE TIME. GWAHAHAHA.
	Locomos_BufferNotThreadSafe = ArtilleryDispatch->RequestorQueue_Locomos;

	StartTicklitesSim->Trigger();
}

void FArtilleryGameSim::Tick()
{
	if (RequestorQueue_Abilities_TripleBuffer == nullptr)
	{
		return;
	}

	bRunning = true;

	TickliteNow = ContingentInputECSLinkage->Now();

	if (bIsServer)
	{
		ServerTick();
	}
	else
	{
		ClientTick();
	}
}

//Okay, so verb tense is gonna get fucked here.
//this runs the events accumulated over the simulation of the previous verified frame at the start
//of the current verified frame. Weirdly, this means we actually do have "half frames" as an engine construct.
//I don't think we ever wanna use that, but it exists. The game only keeps track of _when_ this stuff should run.
//Clover and other subsystems actually run the events. This ends up meaning that we can in fact guarantee that events are run At Most Once.
//This allows us to shove stuff into the past where once it runs, it will always have run.
//This is useful for really compute expensive operations, events that require extremely high certainty like player death,
//or ops that - if spuriously repeated - might blow up the gpu render pipeline, player experience, or web backend 
void FArtilleryGameSim::RunEventsRequiringVerifiedTicks(FArtillerySimContext& Context)
{
	if (Context.bIsVerified)
	{
		auto KeysToDeploy = VerifiedCreateDeadliner.UpdateAndConsume(); //todo: check if we really still want this.
		auto EventKeys = VerifiedEventDeadliner.UpdateAndConsume();
		auto TriggeredKeys = VerifiedTriggerDeadliner.UpdateAndConsume();
		//OkLetsGo
		ItemsAndEventsManager->CreateOnVerTick(KeysToDeploy);
		ItemsAndEventsManager->OnVerTick(EventKeys);
		ItemsAndEventsManager->TriggerOnVerTick(TriggeredKeys);
		ItemsAndEventsManager->RunDelayedTriggers();
	}
	
}

void FArtilleryGameSim::UpdateInputDriven(FArtillerySimContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryGameSim::UpdateInputDriven)
	
	auto& AbilitiesWriteBuffer = RequestorQueue_Abilities_TripleBuffer->GetWriteBuffer();
	
	if (Context.InputGatherFunction)
	{
		Context.InputGatherFunction();
	}
	else
	{
		//sorted; locomotion-Add and pattern-matcher invocation order must agree for everyone.
		for (PlayerKey Player : SortedPlayerKeys(Context.Inputs))
		{
			const FArtilleryShell& Shell = Context.Inputs[Player];

			// Get stream for pattern matcher and actor info
			auto streamkey = ContingentInputECSLinkage->GetStreamForPlayer(Player);
			auto sptr = ContingentInputECSLinkage->GetStream(streamkey);
			if (!sptr.IsValid()) continue;

			ActorKey controllingActor = sptr->GetActorByInputStream();
			if (!controllingActor) continue;

			const auto PrevInput = Context.PrevInputs.Find(Player);

			Locomos_BufferNotThreadSafe->Add(LocomotionParams(Shell.SentAt, controllingActor, PrevInput ? *PrevInput : FArtilleryShell(), Shell));

			// Run pattern matcher with authoritative input
			if (sptr->MyPatternMatcher.IsValid())
			{
				sptr->MyPatternMatcher->runOneFrameWithSideEffects(
					/* isResim= */ true,
					               // Indicate this is a resimulation
					               /* leftTrim= */
					               0,
					               /* rightTrim= */
					               0,
					               /* inputCycleNumber= */
					               Context.Tick,
					               // Use frame number as index
					               /* eventsOut= */
					               AbilitiesWriteBuffer);
			}
		}
	}

	Locomos_BufferNotThreadSafe->Sort();
	AbilitiesWriteBuffer.Sort();
}

void FArtilleryGameSim::Simulate(FArtillerySimContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryGameSim::Simulate)

	RunEventsRequiringVerifiedTicks(Context);
	
	UpdateInputDriven(Context);
	
	if (!RequestorQueue_Abilities_TripleBuffer->IsDirty())
	{
		RequestorQueue_Abilities_TripleBuffer->SwapWriteBuffers();
	}

	ProcessRequestRouterBusyWorkerThread(Context.Tick);
	
	ArtilleryDispatch->RunGuns();  // ALL THIS WORK. FOR THIS?! (Okay, that's really cool)

	ArtilleryDispatch->RunLocomotions();
	if (PhysicsManager.IsValid())
	{
		PhysicsManager->StackUp();
		StartTicklitesApply->Trigger();
		StartRunAhead->Trigger();
		PhysicsManager->StepWorld(TickliteNow, Context.Tick);
		PhysicsManager->BroadcastContactEvents();
		if (ParticleSystemPointer)
		{
			ParticleSystemPointer->ArtilleryTick();
		}
		if (ProjectileSystemPointer)
		{
			ProjectileSystemPointer->ArtilleryTick();
		}
		if (EventLogSystemPointer)
		{
			EventLogSystemPointer->ArtilleryTick();
		}
	}

	TSharedPtr<TMap<FBarrageKey, TSharedPtr<FBCharacterBase>>> HoldOpenCharacters = PhysicsManager->JoltGameSim->CharacterToJoltMapping;
	if(HoldOpenCharacters)
	{
		// ReSharper disable once CppTemplateArgumentsCanBeDeduced - disabled to clear warning that causes compiler error if "fixed"
		for (const TPair<FBarrageKey, TSharedPtr<FBCharacterBase>>& CharacterKeyAndBase : *HoldOpenCharacters)
		{
			if (CharacterKeyAndBase.Value.Get()->mCharacter)
			{
				UE_LOG(LogTemp, Warning, TEXT("Position After simulation %u, pos: %s"), Context.Tick, *CoordinateUtils::FromJoltCoordinates(CharacterKeyAndBase.Value.Get()->mCharacter->GetPosition()).ToString());
				break;
			}
		}
	}
	
	FArtilleryDataBuffer Data;
	Data.SequenceNumber = Context.Tick;
	Data.Inputs = Context.Inputs;
	Data.bIsValid = true;
	Data.TimeStamp = TickliteNow;
	
	StoreState(Context, Data);
	
	TRACE_CPUPROFILER_EVENT_SCOPE(Simualate CacheLayer)

	//tag container save-off currently happens before player and player-like locomotion.
	//this SHOULD be the right place, by my limited reasoning, but I could be wrong.
	for (auto TagSet : TagRollbackManagement) // do not change to ref.
	{
		if (TagSet.Value)
		{
			TagSet.Value->CacheLayer();	
		}
	}
}

void FArtilleryGameSim::StoreState(FArtillerySimContext& Context, FArtilleryDataBuffer Data)
{
	PhysicsManager->SaveState(Data.PhysicsData);
	
	if (UArtilleryDispatch* Dispatch = ArtilleryDispatch.Get())
	{
		Dispatch->StoreArtilleryState(Data);
	}
	
	StateManager->StoreTick(Context.Tick, Data, Context.bIsVerified);
}

bool FArtilleryGameSim::RollbackToState(FArtilleryDataBuffer& RollbackData)
{
	bool bSuccess = true;
	
	PhysicsManager->RestoreState(RollbackData.PhysicsData);
	
	if (UArtilleryDispatch* Dispatch = ArtilleryDispatch.Get())
	{
		Dispatch->LoadArtilleryState(RollbackData);
	}
	else
	{
		bSuccess = false;
	}
	
	return bSuccess;
}

void FArtilleryGameSim::RollbackAndResimulate(uint32 FromFrame, bool bUseAuthorityIfAvailable)
{
	bIsRolling = true;

	if (!StateManager.IsValid() || !PhysicsManager.IsValid())
	{
		return;
	}

	uint32 Start = (FromFrame > 0) ? (FromFrame - 1) : 0;
	FArtilleryDataBuffer* RollbackData = StateManager->GetTick(Start);
	if (!RollbackData)
	{
		UE_LOG(LogTemp, Warning, TEXT("No data for rollback frame %u"), Start);
		return;
	}
	

	LastMispredictionSequence = CurrentSequence;
	bool bRollbackSuccessful = RollbackToState(*RollbackData);

	TMap<PlayerKey,FArtilleryShell> FrameInputs;
	for (uint32 Frame = Start + 1; Frame <= CurrentSequence-1; ++Frame)
	{
		bool bHasServerData = false;
		TMap<PlayerKey,FArtilleryShell> Prev;
		if (InputManager.IsValid())
		{
			if (bUseAuthorityIfAvailable && InputManager->GetAuthoritativeInputs().Contains(Frame))
			{
				bHasServerData = true;
				FrameInputs = InputManager->GetAuthoritativeInputs()[Frame];
			}
			else
			{
				if (InputManager->GetLocalInputHistory().Contains(Frame))
				{
					FrameInputs = *InputManager->GetLocalInputHistory().Find(Frame);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Tried to resimulate sequence %u but found no inputs in the local history"), Frame);
				}
			}

			// TODO: Do not use local misspredicted inputs from other players, continue applying their last input instead
			// and only use inputs from history for local player to reapply predicted inputs
			if (bUseAuthorityIfAvailable && InputManager->GetAuthoritativeInputs().Contains(Frame-1))
			{
				Prev = InputManager->GetAuthoritativeInputs()[Frame-1];
			}
			else if (const TMap<PlayerKey,FArtilleryShell>* PrevPtr = InputManager->GetLocalInputHistory().Find(Frame-1))
			{
				Prev = *PrevPtr;
			}
		}

		FArtillerySimContext Context;
		Context.Tick = Frame;
		Context.bIsVerified = bHasServerData;
		Context.PrevInputs = Prev;
		Context.Inputs = FrameInputs;
		Simulate(Context);
	}

	InputManager->SendInputsToServer(FromFrame,CurrentSequence, FrameInputs);
}

void FArtilleryGameSim::RollbackToVerified()
{
	if (!StateManager.IsValid() || !PhysicsManager.IsValid() || !StateManager->HasVerifiedFrame())
	{
		return;
	}
	if (FArtilleryDataBuffer* VerifiedState = StateManager->GetTick(StateManager->GetLastVerifiedSequence()))
	{
		RollbackToState(*VerifiedState);
	}
}

void FArtilleryGameSim::Shutdown()
{
    if (InputManager)
    {
        InputManager->Shutdown();
    }

    InputManager = nullptr;
    StateManager = nullptr;
    PhysicsManager = nullptr;
    ArtilleryDispatch = nullptr;
    GameWorld = nullptr;
}

uint32 FArtilleryGameSim::GetLastVerifiedSequence() const
{
	return StateManager->GetLastVerifiedSequence();
}

uint32 FArtilleryGameSim::GetOldestStoredSequence() const
{
	return StateManager->GetOldestSequence();
}

bool FArtilleryGameSim::IsNetInitialized() const
{
	return InputManager->IsNetInitialized() && InputManager->GetLatestServerFrame() > 0;
}

void FArtilleryGameSim::ServerTick()
{
	InputManager->ProcessBufferedInputs(CurrentSequence);
	TMap<PlayerKey, FArtilleryShell> Inputs = InputManager->GetInputsForSequence(CurrentSequence, TickliteNow);

	TMap<PlayerKey,FArtilleryShell> PrevInputs;
	if (FArtilleryDataBuffer* PrevFrame = StateManager->GetTick(CurrentSequence-1))
	{
		PrevInputs = PrevFrame->Inputs;
	}

	FArtillerySimContext Context;
	Context.Tick = CurrentSequence;
	Context.bIsVerified = true;
	Context.PrevInputs = PrevInputs;
	Context.Inputs = Inputs;
	Simulate(Context);
	// SendInputsToClients removed (Phase-1): the Longboy reflector relays; we only send ours.
	++CurrentSequence;
}

void FArtilleryGameSim::ClientTick()
{
	if (IsCatchUpTick())
	{
		DoCatchUp();
		return;
	}

	InputManager->ProcessPredictiveInputs(CurrentSequence, TickliteNow);
	TMap<PlayerKey, FArtilleryShell> Inputs = InputManager->GetInputsForSequence(CurrentSequence, TickliteNow);

	TMap<PlayerKey,FArtilleryShell> PrevInputs;
	if (FArtilleryDataBuffer* PrevFrame = StateManager->GetTick(CurrentSequence-1))
	{
		PrevInputs = PrevFrame->Inputs;
	}

	FArtillerySimContext Context;
	Context.Tick = CurrentSequence;
	Context.bIsVerified = false;
	Context.PrevInputs = PrevInputs;
	Context.Inputs = Inputs;
	Simulate(Context);

	InputManager->SendInputsToServer(GetLastVerifiedSequence(), CurrentSequence, Inputs);
	++CurrentSequence;

	bIsRolling = false;
}

bool FArtilleryGameSim::IsCatchUpTick() const
{
	return IsTooFarBehind() || IsTooFarAhead();
}

uint32 FArtilleryGameSim::EffectiveServerFrame() const
{
	// Dumb mode: pretend the authoritative server frame rides the live edge, so the rollback never
	// catches up / rolls back. See GbArtilleryRollbackDumbServerFrame. 0 = normal (real server frame).
	return GbArtilleryRollbackDumbServerFrame ? CurrentSequence : InputManager->GetLatestServerFrame();
}

bool FArtilleryGameSim::IsTooFarBehind() const
{
	uint32 ServerFrame = EffectiveServerFrame();
	return ServerFrame > 0 && (int32)CurrentSequence < (int32)ServerFrame;
}

bool FArtilleryGameSim::IsTooFarAhead() const
{
	uint32 ServerFrame = EffectiveServerFrame();
	return (int32)CurrentSequence > (int32)ServerFrame + MaxRollbacks;
}

void FArtilleryGameSim::DoCatchUp()
{
	bIsRolling = true;

	// TODO: not hard to do, soonish i will add to increase/decrease this window dynamically based on ping
	uint32 PreCatchupSeq = CurrentSequence;

	while (IsTooFarBehind())
	{
		InputManager->ClearBufferedInputs();
		TMap<PlayerKey,FArtilleryShell> FrameInputs;
		TMap<PlayerKey,FArtilleryShell> Prev;
		bool bHasAuthorityData = false;
		if (InputManager.IsValid())
		{
			if (InputManager->GetAuthoritativeInputs().Contains(CurrentSequence))
			{
				bHasAuthorityData = true;
				FrameInputs = InputManager->GetAuthoritativeInputs()[CurrentSequence];
			}
			else
			{
				InputManager->ProcessPredictiveInputs(CurrentSequence, TickliteNow);
				FrameInputs = InputManager->GetInputsForSequence(CurrentSequence, TickliteNow);
			}

			// TODO: Do not use local misspredicted inputs from other players, continue applying their last input instead
			// and only use inputs from history for local player to reapply predicted inputs
			if (InputManager->GetAuthoritativeInputs().Contains(CurrentSequence-1))
			{
				Prev = InputManager->GetAuthoritativeInputs()[CurrentSequence-1];
			}
			else
			{
				if (FArtilleryDataBuffer* PrevFrame = StateManager->GetTick(CurrentSequence-1))
				{
					Prev = PrevFrame->Inputs;
				}
			}
		}

		FArtillerySimContext Context;
		Context.Tick = CurrentSequence;
		Context.bIsVerified = bHasAuthorityData;
		Context.PrevInputs = Prev;
		Context.Inputs = FrameInputs;
		Simulate(Context);
		InputManager->SendInputsToServer(PreCatchupSeq,CurrentSequence, FrameInputs);
		CurrentSequence++;
	}

	if (IsTooFarAhead())
	{
		return;
	}

	bIsRolling = false;
}

void FArtilleryGameSim::ProcessAuthorityData()
{
	const TMap<uint32, TMap<PlayerKey,FArtilleryShell>>& AuthMap = InputManager->GetAuthoritativeInputs();
	if (!AuthMap.Num()) { return; }

	uint32 LastVerified = GetLastVerifiedSequence();
	uint32 ServerFrame = InputManager->GetLatestServerFrame();
	if (ServerFrame <= LastVerified || CVarForceRollbackEnabled.GetValueOnAnyThread() > 0)
	{
		return;
	}

	uint32 MispredFrame = FindEarliestMisprediction(LastVerified + 1, ServerFrame, AuthMap);
	if (MispredFrame != MAX_uint32)
	{
		RollbackAndResimulate(MispredFrame);
	}
}


uint32 FArtilleryGameSim::FindEarliestMisprediction(uint32 StartSequence, uint32 EndSequence,
	const TMap<uint32,TMap<PlayerKey,FArtilleryShell>>& InAuthoritative)
{
	for (uint32 Sequence = StartSequence; Sequence <= EndSequence; ++Sequence)
	{
		if (!InAuthoritative.Contains(Sequence)) { continue; }

		const TMap<PlayerKey,FArtilleryShell>& AuthInputs = InAuthoritative[Sequence];
		FArtilleryDataBuffer* LocalBuf = StateManager->GetTick(Sequence);
		if (!LocalBuf)
		{
			return FMath::Max((int32)Sequence,1);
		}

		for (auto& AuthPair : AuthInputs)
		{
			PlayerKey PK = AuthPair.Key;
			const FArtilleryShell& AuthShell = AuthPair.Value;
			if (!LocalBuf->Inputs.Contains(PK))
			{
				return Sequence;
			}
			const FArtilleryShell& PredShell = LocalBuf->Inputs[PK];
			if (PredShell.MyInputActions != AuthShell.MyInputActions)
			{
				return Sequence;
			}
		}

		//empty AuthInputs is protocol padding from SendInputsToClients, not divergence.
		//treating it as misprediction would fire a rollback on every idle frame.

		StateManager->StoreVerified(Sequence);
	}

	return MAX_uint32;
}

//TODO right now, this incurs two serious determinism risks:
//The order that threads get queues is random, so if you just go down the line, that won't produce a deterministic execution order.
//Even if you fix that, you still need to order the requests as a gestalt, and now you have a problem where you don't know the
//correct\true order to run things with the same timestamp in. This is fixable but it's gonna need to wait.
void FArtilleryGameSim::ProcessRequestRouterBusyWorkerThread(const uint32 SeqNumber)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FArtilleryGameSim::ProcessRequestRouterBusyWorkerThread)
	
	if (!ensure(RequestRouter))
	{
		return;
	}
	
	auto MyDispatch = ArtilleryDispatch.Get();
	
	auto ContingentPhysicsLinkage = PhysicsManager.Get();
	
	auto TransformPtr = TransformDispatch.Get();
	
	for (FRequestRouter::FeedMap& WorkerFeedMap : RequestRouter->BusyWorkerAcc)
	{
		TSharedPtr<FRequestRouter::ThreadFeed> HoldOpen;
		if (WorkerFeedMap.Queue && ((HoldOpen = WorkerFeedMap.Queue)) && WorkerFeedMap.That != std::thread::id()) //if there IS a thread.
		{
			FRequestThing Request;
			while (HoldOpen->Dequeue(Request))
			{
				//PINPOINT: YABUSYTHREADBOYRUNNETHREQUESTSHERE
				switch (Request.GetType())
				{
				case ArtilleryRequestType::FireAGun:
				// Guns fired from the artillery thread is brand new, this mapping mighg not mean as much now
				{
					TSharedPtr<FArtilleryGun> GunHoldOpen = MyDispatch->GunByKey->FindRef(Request.Gun);
					TDelegate<void(TSharedPtr<FArtilleryGun>, bool, EventBufferInfo)>* FireFunction =
						MyDispatch->GunToFiringFunctionMapping->Find(Request.Gun);

					if (FireFunction != nullptr && GunHoldOpen)
					{
						EventBufferInfo def = EventBufferInfo::Default();
						def.Action = ArtIPMKey::InternallyStateless;
						UArtilleryDispatch::TotalFirings += FireFunction->ExecuteIfBound(GunHoldOpen, false, def);
					}
					else
					{
						// TODO - we are absolutely going to want to turn this and things like it into a periodic
						//		  log call to avoid clogging log files
						UE_LOG(
							LogArtillery,
							Error,
							TEXT(
								"FArtilleryBusyWorker::ProcessRequestRouterBusyWorkerThread: Error processing FireGun request with gun key [id: %llu, name: %s]"
							),
							Request.Gun.GunInstanceID.Obj,
							*Request.Gun.GunDefinitionID.ToString());
					}
				}
				break;
					
				case ArtilleryRequestType::GetAnUnboundGun:
				{
					IdMapPtr WordsOfPower = MyDispatch->GetRelationships(Request.SourceOrSelf);
					FGunKey Gun = MyDispatch->GetGun(Request.Gun.GunDefinitionID, ActorKey(Request.SourceOrSelf));
					FGunInstanceKey BANG = Gun.GunInstanceID;
					if (WordsOfPower)
					{
						TSharedPtr<FConservedAttributeKey> MaterialComponent = WordsOfPower.Get()->FindOrAdd(
							Request.Relationship);
						if (MaterialComponent)
						{
							MaterialComponent.Get()->SetCurrentValue(BANG);
						}
						else
						{
							TSharedPtr<FConservedAttributeKey> PowerWordGun = MakeShareable(
								new FConservedAttributeKey);
							PowerWordGun->SetBaseValue(BANG);
							PowerWordGun->SetCurrentValue(BANG);
							WordsOfPower.Get()->Add(Request.Relationship, PowerWordGun);
						}
					}
					else
					{
						TSharedPtr<TMap<Ident, IdentPtr>> RelationshipMap = MakeShareable(new IdentityMap());

						//TODO: swap this to loading values from a data table, and REMOVE this fallback.
						//If we want defaults, those defaults should ALSO live in a data table, that way when a defaulting bug screws us
						//maybe we can fix it without going through a full cert using a data only update.
						TSharedPtr<FConservedAttributeKey> PowerWordGun = MakeShareable(
							new FConservedAttributeKey);
						RelationshipMap->Add(Request.Relationship, PowerWordGun);
						PowerWordGun->SetBaseValue(BANG);
						PowerWordGun->SetCurrentValue(BANG);
						MyDispatch->RegisterOrAddRelationships(Request.SourceOrSelf, RelationshipMap);
					}
				}
				break;
				case ArtilleryRequestType::TagReferenceModel:
					{
						if (TagRollbackManagement.Find(Request.SourceOrSelf) == nullptr)
						{
							TagRollbackManagement.Add(Request.SourceOrSelf, Request.ConservedTags);
						}
						else
						{
							//CustomTimer<"ReferenceInitAttemptedOnInited"> RateCheck;
						}
					}
					break;
				case ArtilleryRequestType::NoTagReferenceModel:
					{
						TagRollbackManagement.Remove(Request.SourceOrSelf);
					}
					break;
				case ArtilleryRequestType::FakeTransformUpdate:
					{
						if (ContingentPhysicsLinkage && TransformPtr &&
							TransformPtr->GetKineByObjectKey(Request.SourceOrSelf))
						{
							TSharedPtr<TransformUpdatesForGameThread> HoldOpenTransformPump =  ContingentPhysicsLinkage->GameTransformPump;
							if (HoldOpenTransformPump)	
							{
								HoldOpenTransformPump->AddMove(
									Request.SourceOrSelf,
									Request.Stamp,
									FQuat4f(Request.ThingRotator.Quaternion()),
									FVector3f(Request.ThingVector));
							}
						}
					}
					break;
				case ArtilleryRequestType::CreateTriggerOnVerifiedTick:
					{
						if (ContingentPhysicsLinkage && SeqNumber <= GetLastVerifiedSequence()) //more than one frame might become verified at once in some scenarios.
						{
							auto inv = MyDispatch->GetWorld()->GetSubsystem<UInventoryDispatch>();
							FTransform LocationnAndDimensions;
							LocationnAndDimensions.SetIdentityZeroScale();
							LocationnAndDimensions.SetLocation(Request.ThingVector);
							LocationnAndDimensions.SetRotation(Request.ThingRotator.Quaternion());
							LocationnAndDimensions.SetScale3D(Request.ThingVector2);
							inv->CreateQueuedTriggerOnVerifiedFrame(LocationnAndDimensions);
						}
						else
						{
							//no-op. the request gets repeated over and over, since we know we CANNOT presim it.
						}
					}
					break;
				default:
					UE_LOG(
						LogTemp,
						Fatal,
						TEXT("ArtilleryDispatch::ProcessRequestRouterGameThread: Received Request Router request for unimplemented request type: [%d]"),
						Request.GetType());
				}
			}
		}
	}
}
