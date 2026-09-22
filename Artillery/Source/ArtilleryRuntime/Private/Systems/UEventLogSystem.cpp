#include "UEventLogSystem.h"

#include "ArtilleryBPLibs.h"

bool UEventLogSubsystem::RegistrationImplementation()
{
	UArtilleryDispatch::Get(GetWorld())->SetEventLogSystem(this);
	UE_LOG(LogTemp, Warning, TEXT("UEventLogSubsystem:Subsystem: Online"));
	return true;
}

void UEventLogSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
}

void UEventLogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UEventLogSubsystem::ArtilleryTick()
{
	if (MyDispatch != nullptr)
	{
		int32 now = UArtilleryLibrary::GetTotalsTickCount(UArtilleryDispatch::Get(GetWorld()));
		// RemoveAllSwap does not maintain ordering but is the faster option
		// For now we do not care about ordering
		EventLog.RemoveAllSwap([now] (const FArtilleryEvent& Event)
		{
			return Event.ExpiryTime < now;
		});
	}
}

void UEventLogSubsystem::LogEvent(E_EventLogType LoggingType, FSkeletonKey LoggingKey, FSkeletonKey Other)
{
	int32 now = UArtilleryLibrary::GetTotalsTickCount(UArtilleryDispatch::Get(GetWorld()));
	FArtilleryEvent NewEvent;
	NewEvent.LogTime = now;
	NewEvent.ExpiryTime = now + (5 * 120);
	NewEvent.Type = LoggingType;
	NewEvent.LoggingKey = LoggingKey;
	NewEvent.OtherKey = Other;
	EventLog.Add(NewEvent);
}

TArray<FArtilleryEvent> UEventLogSubsystem::GetEventsOfType(E_EventLogType TypeToFetch) const
{
	return EventLog.FilterByPredicate([TypeToFetch] (const FArtilleryEvent& Event)
	{
		return Event.Type == TypeToFetch;
	});
}
