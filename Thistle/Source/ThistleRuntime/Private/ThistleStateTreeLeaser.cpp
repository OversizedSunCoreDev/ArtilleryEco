#include "ThistleStateTreeLeaser.h"

#include "Components/StateTreeComponentSchema.h"

void UThistleStateTreeLease::BeginDestroy()
{
	IsReady = false;
	bIsRunning = false;
	//this should be too late but...
	Super::BeginDestroy();
}



void UThistleStateTreeLease::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	IsReady = false;
	bIsRunning = false;
	//this is now handled by the thistlebehavioralist
	//this->ClearInternalFlags(EInternalObjectFlags::Async);
	Super::EndPlay(EndPlayReason);
}

TValueOrError<void, FString> UThistleStateTreeLease::HasValidStateTreeReference() const
{
	if (!StateTreeRef.IsValid())
	{
		return MakeError(TEXT("The State Tree asset is not set."));
	}

	//we require MUCH weaker guarantees for our state trees, as they are _far_ more widely used.
	//tbh, if you have An Schema, you aight.
	if (StateTreeRef.GetStateTree()->GetSchema() == nullptr)
	{
		return MakeError(TEXT("The State Tree schema is not compatible."));
	}

	return MakeValue();
}

FStateTreeReference UThistleStateTreeLease::GetGuard()
{
	return StateTreeRef;
}

void UThistleStateTreeLease::OnUnregister()
{
	IsReady = false;
	bIsRunning = false;
	Super::OnUnregister();
}

#if WITH_GAMEPLAY_DEBUGGER
FString UThistleStateTreeLease::GetDebugInfoString() const
{
	if (this && GetOwner() && StateTreeRef.IsValid())
	{
		if (!StateTreeRef.IsValid())
		{
			return FString("No StateTree to run.");
		}
		return FConstStateTreeExecutionContextView(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData).Get().GetDebugInfoString();
	}
	return FString("No StateTree to run.");
}
#endif // WITH_GAMEPLAY_DEBUGGER

void UThistleStateTreeLease::InitializeComponent()
{
	Super::InitializeComponent();
}

bool UThistleStateTreeLease::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	TArrayView<const FStateTreeExternalDataDesc> Descs,
	TArrayView<FStateTreeDataView> OutDataViews) const
{
	auto ErrantWays = const_cast<F_ArtilleryKeyInstanceData*>(&InstanceOwnerKey);
	bool First = UThistleStateTreeSchema::CollectExternalData(Context, ErrantWays, Descs, OutDataViews);
	return First && Super::CollectExternalData(Context, StateTree, Descs, OutDataViews);
}

void UThistleStateTreeLease::OnRegister()
{
	Super::OnRegister();
	AttemptRegister();
}

bool UThistleStateTreeLease::RegistrationImplementation()
{
	return GetWorld()->GetSubsystem<UThistleBehavioralist>() != nullptr;
}

void UThistleStateTreeLease::OnClusterMarkedAsPendingKill()
{
	IsReady = false;
	bIsRunning = false;
	Super::OnClusterMarkedAsPendingKill();
}

void UThistleStateTreeLease::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	IsReady = false;
	bIsRunning = false;
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

UThistleStateTreeLease::UThistleStateTreeLease(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer), CurrentRunStatus()
{
	bWantsInitializeComponent = true;
	bIsRunning = true;
	bIsPaused = false;
	IsReady = false;
	bStartLogicAutomatically = true;
}

UThistleStateTreeLease::~UThistleStateTreeLease()
{
	IsReady = false;
}

void UThistleStateTreeLease::BeginPlay()
{
	Super::BeginPlay();
	BindDel = FOnCollectStateTreeExternalData::CreateUObject(this, &UThistleStateTreeLease::CollectExternalData);
	InstanceData = FStateTreeInstanceData();
	FStateTreeExecutionContext Context(*GetOwner(), *StateTreeRef.GetStateTree(), InstanceData);
	GenerateGroupOffset();
	Context.Start();
	AttemptRegister();
}

FSkeletonKey UThistleStateTreeLease::GetMyKey() const
{
	IKeyedConstruct* KeyedConstruct = Cast<IKeyedConstruct>(GetOwner());
	return KeyedConstruct ? KeyedConstruct->GetMyKey() : FSkeletonKey();
}

UGameplayTasksComponent* UThistleStateTreeLease::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	UE_LOG(LogTemp, Error,
	       TEXT("UStateTreeLease runs in Artillery cadence and just tried to provide a gameplay tasks component from main thread. This could be real bad."));
	return Super::GetGameplayTasksComponent(Task);
}

bool UThistleStateTreeLease::SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors)
{
	Context.SetLinkedStateTreeOverrides(LinkedStateTreeOverrides);
	InstanceOwnerKey = GetMyKey(); // failsafe.
	Context.SetCollectExternalDataCallback(BindDel);
	return UThistleStateTreeSchema::SetContextRequirements(GetMyKey(), Context)
	; //&& UStateTreeComponentSchema::SetContextRequirements(*this, Context);
}

void UThistleStateTreeLease::GenerateGroupOffset()
{
	if (UseGroupOffset)
	{
		UThistleStateTreeLeaseGroupMeter = (UThistleStateTreeLeaseGroupMeter +1) % UThistleBehavioralist::WIDE_CADENCE;
		CycleGroup = UThistleStateTreeLeaseGroupMeter;
	}
}

void UThistleStateTreeLease::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	if (!IsReady)
	{
		AttemptRegister();
	}
	else
	{
		this->SetInternalFlags(EInternalObjectFlags::Async);
		SetComponentTickEnabled(false);
	}
}

void UThistleStateTreeLease::ArtilleryTick(uint64_t TicksSoFar)
{
	
		if (this && GetOwner() && IsReady && StateTreeRef.IsValid())
		{
			FStateTreeReference IncrementRefAsGuard = StateTreeRef; //Don't delete this. No, seriously. Don't you dare.
			if (IncrementRefAsGuard.IsValid())
			{
				if (MessagesToProcess.Num() > 0)
				{
					const int32 NumMessages = MessagesToProcess.Num();
					for (int32 Idx = 0; Idx < NumMessages; Idx++)
					{
						// create a copy of message in case MessagesToProcess is changed during loop
						const FAIMessage MessageCopy(MessagesToProcess[Idx]);
						for (int32 ObserverIndex = 0; ObserverIndex < MessageObservers.Num(); ObserverIndex++)
						{
							MessageObservers[ObserverIndex]->OnMessage(MessageCopy);
						}
					}
					MessagesToProcess.RemoveAt(0, NumMessages, EAllowShrinking::No);
				}
				InstanceOwnerKey = GetMyKey(); //freshen up, me hearties, yo ho.
			
				FStateTreeExecutionContext Context(*GetOwner(), *IncrementRefAsGuard.GetStateTree(), InstanceData);

				if ((!bIsRunning || bIsPaused) || !Context.IsValid() || !IncrementRefAsGuard.IsValid() || !StateTreeRef.IsValid())
				{
					return;
				}
				if (((TicksSoFar+CycleGroup) % UThistleBehavioralist::WIDE_CADENCE) == 0)
				{
				if (SetContextRequirements(Context))
				{
					const EStateTreeRunStatus PreviousRunStatus = Context.GetStateTreeRunStatus();
					// hi yeah GBuffered
					// * @note The current implementation of the lifetime events doesn't properly support same instance getting ticked in different threads.
					if (PreviousRunStatus != EStateTreeRunStatus::Unset && Context.IsValid() //fine. FINE.
						&& Context.GetMutableInstanceData()->GetExecutionState() != nullptr
						&& Context.GetMutableInstanceData()->GetExecutionState()->TreeRunStatus != EStateTreeRunStatus::Unset
						 && bIsRunning && GetOwner() && GetWorld() && IsReady)
					{
						CurrentRunStatus = Context.Tick(UThistleBehavioralist::WIDE_CADENCE / ArtilleryTickHertz);
					}
				}
			}
		}
	}
}


////////////////////////
