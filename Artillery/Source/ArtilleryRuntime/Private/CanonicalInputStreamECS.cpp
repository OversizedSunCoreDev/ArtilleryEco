// Fill out your copyright notice in the Description page of Project Settings.

#include "CanonicalInputStreamECS.h"

void UCanonicalInputStreamECS::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("Artillery::CanonicalInputStream is Online."));
	
	MyNetworkDispatch = Collection.InitializeDependency<UBristleconeWorldSubsystem>();
	
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UCanonicalInputStreamECS::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
}

void UCanonicalInputStreamECS::Deinitialize()
{
	SelfPtr = nullptr;
	UE_LOG(LogTemp, Warning, TEXT("Artillery::CanonicalInputStream is Shutting Down."));
	Super::Deinitialize();
}

void UCanonicalInputStreamECS::PostInitialize()
{
	Super::PostInitialize();
}

bool UCanonicalInputStreamECS::RegistrationImplementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Artillery::CanonicalInputStream is Operational"));
	SelfPtr = this; 
	return true;
}

ActorKey UCanonicalInputStreamECS::ActorByStream(InputStreamKey Stream)
{
	return StreamToActorMapping.FindRef(Stream);
}

InputStreamKey UCanonicalInputStreamECS::StreamByActor(ActorKey Actor)
{
	return ActorToStreamMapping.FindRef(Actor);
}

void UCanonicalInputStreamECS::Tick(float DeltaTime)
{
}

TStatId UCanonicalInputStreamECS::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCanonicalInputStreamECS, STATGROUP_Tickables);
}


//OH NO. HIDDEN ORDERING DEPENDENCY EMERGED! if this gets called AFTER getstreamforplayer, which can happen because we now handle
//the CDO correctly, we can end up in a situation where things just sort of flywheel off to hell because there's not actually any stream
//constructs extant yet.
TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> UCanonicalInputStreamECS::getNewStreamConstruct( PlayerKey ByPlayerConcept)
{
	TSharedPtr<ArtilleryControlStream> ManagedStream = MakeShareable(
		new FConservedInputStream(this, InputStreamKey(ByPlayerConcept)) //using++ vs ++would be wrong here. inc then ret.
	);
	auto BifurcateOwnership = new TSharedPtr<ArtilleryControlStream>(ManagedStream);
	//fun fucking story, this was working by ACCIDENT because we were somehow ZEROING OUT the pointers, causing things to JUST BARELY map.
	//here we go again.
	SessionPlayerToStreamMapping.Add(ByPlayerConcept, ManagedStream->MyKey);//
	StreamKeyToStreamMapping.Add(ManagedStream->MyKey, *BifurcateOwnership);//This is the key driver for the ordering problem
	return ManagedStream; 
}

InputStreamKey UCanonicalInputStreamECS::GetStreamForPlayer(PlayerKey ThisPlayer)
{
	//TODO: this can actually fail if the start up sequence happens in a really unusual order.
	return SessionPlayerToStreamMapping.FindChecked(ThisPlayer);
}

TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> UCanonicalInputStreamECS::GetStream(InputStreamKey StreamKey) const
{
	const auto SP = StreamKeyToStreamMapping.FindRef(StreamKey);
	return SP; // creates a copy.
}

bool UCanonicalInputStreamECS::registerPattern(IPM::CanonPattern ToBind,
                                               FActionPatternParams FCM_Owner_ActorParams)
{
	TSharedPtr<FConservedInputStream>* thisInputStreamPtr = StreamKeyToStreamMapping.Find(FCM_Owner_ActorParams.MyInputStream);
	if (
#ifndef LOCALISCODEDSPECIAL
		InputStreamKey(APlayer::CABLE) == FCM_Owner_ActorParams.MyInputStream ||
#endif // !LOCALISCODEDSPECIAL
		thisInputStreamPtr != nullptr)
	{
		ArtIPMKey ToBindName = ToBind->getName();
		FConservedInputStream* thisInputStream = thisInputStreamPtr->Get();
		TSharedPtr<FConservedInputPatternMatcher> PatternMatcher = thisInputStream->MyPatternMatcher;
		TMap<ArtIPMKey, TSharedPtr<TMap<FActionBitMask, FActionPatternParams>>>* PatternBinds = &PatternMatcher->AllPatternBinds;
		TSharedPtr<TMap<FActionBitMask, FActionPatternParams>> ActionPatternForToBind = PatternBinds->FindRef(ToBindName);
		if (ActionPatternForToBind.IsValid())
		{
			//names are never removed. sets are only added to or removed from.
			ActionPatternForToBind.Get()->Add(FCM_Owner_ActorParams.ToSeek, FCM_Owner_ActorParams);
		}
		else
		{
			PatternMatcher->Names.Add(ToBindName);
			PatternMatcher->AllPatternsByName.Add(ToBindName, ToBind);
			TSharedPtr<TMap<FActionBitMask, FActionPatternParams>> newMap = MakeShareable(new TMap<FActionBitMask, FActionPatternParams>());
			newMap.Get()->Add(FCM_Owner_ActorParams.ToSeek, FCM_Owner_ActorParams);
			thisInputStream->MyPatternMatcher->AllPatternBinds.Add(ToBind->getName(), newMap);
		}
		return true;
	}
	return false;
}

bool UCanonicalInputStreamECS::removePattern(IPM::CanonPattern ToBind, FActionPatternParams FCM_Owner_ActorParams)
{
	TSharedPtr<FConservedInputStream>* thisInputStream = StreamKeyToStreamMapping.Find(FCM_Owner_ActorParams.MyInputStream);
	if (
#ifndef LOCALISCODEDSPECIAL
		 FCM_Owner_ActorParams.MyInputStream == InputStreamKey(APlayer::CABLE) ||
#endif // !LOCALISCODEDSPECIAL
		thisInputStream != nullptr && thisInputStream->IsValid())
	{
		TSharedPtr<TMap<FActionBitMask, FActionPatternParams>> pinSharedPtr = thisInputStream->Get()->MyPatternMatcher->AllPatternBinds.FindRef(ToBind->getName());
		if (pinSharedPtr.IsValid())
		{
			//names are never removed. sets are only added to or removed from.
			pinSharedPtr.Get()->Remove(FCM_Owner_ActorParams.ToSeek);
			return true;
		}
	}
	return false;
}
TPair<ActorKey, InputStreamKey> UCanonicalInputStreamECS::RegisterKeysToParentActorMapping(FireControlKey MachineKey, bool IsActorForLocalPlayer , const ActorKey ParentKey) 
{
	LocalActorToFireControlMapping.Add(ParentKey, MachineKey);

	//this is a hack. this is such a hack. oh god.
	if(IsActorForLocalPlayer)
	{
		//this relies on a really ugly hack using the ENUM. do not ship this without being sure you want to.
		InputStreamKey LocalKey = GetStreamForPlayer(APlayer::CABLE);
		StreamToActorMapping.Add(LocalKey, ParentKey); //ONE OF THE TWO THINGS IS WRONG NOW, CONGRATS, HERO.
		ActorToStreamMapping.Add(ParentKey, LocalKey);
		return TPair<ActorKey, InputStreamKey>(ParentKey, LocalKey);			
	}
	
	ensure(false);
	
	return {};
}
