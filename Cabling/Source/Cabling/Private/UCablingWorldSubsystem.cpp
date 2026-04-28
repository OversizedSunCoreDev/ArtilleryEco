#include "UCablingWorldSubsystem.h"

//THIS IS A GENERALLY UNDESIRABLE INCLUDE PATTERN

//Goal: Cabling is a thin threaded layer that pulls input from the controller, and provides it to: 
// the Cabling world subsystem for making accessible to the game thread...
// AND
// a threadsafe queue supporting a single consuming thread, then triggers an event.

//Destructive method for switching lifecycle and consumer ownership for the "gamethread" queue.
void UCablingWorldSubsystem::DestructiveChangeLocalOutboundQueue(Cabling::SendQueue NewlyAllocatedQueue)
{
	GameThreadControlQueue = NewlyAllocatedQueue;
	controller_runner.GameThreadControlQueue = NewlyAllocatedQueue;
}

//We're going to wire up the RT system and oversample at 3x expected control input hertz, so 360
//This is because XB1+ Controllers have a max sample rate of 120.
//If we don't have a new input after 3 polls, we ship the old one.
// 
//We only ship 128 inputs per second, so we'll need to do something about this.
void UCablingWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

bool UCablingWorldSubsystem::RegistrationImplementation()
{
	GameThreadControlQueue = MakeShareable(new TCircularQueue<uint64_t>(256));
	CabledThreadControlQueue = MakeShareable(new TCircularQueue<uint64_t>(256));
	UE_LOG(LogTemp, Warning, TEXT("UCablingWorldSubsystem: Subsystem world initialized"));
	controller_runner.CabledThreadControlQueue = this->CabledThreadControlQueue;
	controller_runner.GameThreadControlQueue = this->GameThreadControlQueue;
	controller_thread.Reset(FRunnableThread::Create(&controller_runner, TEXT("Cabling Runner")));
	SelfPtr = this;
	return true;
}

void UCablingWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld) {
	Super::OnWorldBeginPlay(InWorld);
	
}

void UCablingWorldSubsystem::Deinitialize() {
	UE_LOG(LogTemp, Warning, TEXT("UCablingWorldSubsystem: Deinitializing Cabling subsystem"));
	if (controller_thread)
	{
		controller_thread->Kill();
	}
	Super::Deinitialize();
}

void UCablingWorldSubsystem::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
}

TStatId UCablingWorldSubsystem::GetStatId() const {
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFCablingWorldSubsystem, STATGROUP_Tickables);
}
