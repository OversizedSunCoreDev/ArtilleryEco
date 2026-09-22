#include "InventoryDispatch.h"
#include "SGraphPin.h"
#include "EdGraph/EdGraphPin.h"

#include "ArtilleryBPLibs.h"
#include "EdGraphSchema_K2.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsTypes/InventoryTriggerProxy.h"
#include "Sound/SoundBase.h"

class UThistleDispatch;

bool UInventoryDispatch::RegistrationImplementation()
{
	LoadSoundsHelper(SFXDefinitionKeyToSound,
	                 FNameToSoundKey,
	                 SoundNamesForPulldown,
	                 Sounds);
	
	return true;
}


bool UInventoryDispatch::IDM_TrySFX(FSkeletonKey OwnerOrSource, 
	/*should always be a FSKSoundFXDefinitionKey but I left it as fskeleton cause that's our norm. does mean it'll be easy if we wanna generalize a bit.*/
	FSkeletonKey EffectDefinition,
	FEventParameterPackage EffectParameters,
	bool UseParametersForHash)
{
	auto Ticket = GetSoundEffectTicket(OwnerOrSource, EffectDefinition, EffectParameters, UseParametersForHash);
	return FireSoundEffect(Ticket);
}


FSKEffectTicket UInventoryDispatch::GetSoundEffectTicket(FSkeletonKey OwnerOrSource, 
	/*should always be a FSKSoundFXDefinitionKey but I left it as fskeleton cause that's our norm. does mean it'll be easy if we wanna generalize a bit.*/
	FSkeletonKey EffectDefinition,
	FEventParameterPackage EffectParameters,
	bool UseParametersForHash)
{
	auto input = UseParametersForHash ? HashCombineFast(OwnerOrSource, EffectParameters.Hash()) : OwnerOrSource;
	FSKEffectTicket ret = FSKEffectTicket(EffectDefinition, HashCombineFast(input, ArtilleryDispatch->GetShadowNow()));			
	if (FiredEffectSet.contains(ret))
	{
		return FSKEffectTicket();
	}
	else
	{
		EventParamsToFire.insert_or_assign(ret, EffectParameters);
		return ret;
	}
}

bool UInventoryDispatch::FireSoundEffect(FSKEffectTicket Nyooooom)
{
	bool a = false;
	if (EventParamsToFire.contains(Nyooooom) && !FiredEffectSet.contains(Nyooooom))
	{
		auto Key = FSKSoundFXDefinitionKey(Nyooooom.GetSK().GetShiftedMeta());
		auto send = SFXDefinitionKeyToSound.Find(Key);
		FEventParameterPackage A;
		if (send)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), *send /* lookup loaded asset here. What solution do you like, @megafunk? */, A.Loc);
		}
		a = FiredEffectSet.insert(Nyooooom);
	}
	return a;
}

bool UInventoryDispatch::FetchQuest(FSkeletonKey MustHaveValidMetaField, FInventoryQuest& OutParam)
{
		
	return 
		MetaFieldToQuest.visit((MustHaveValidMetaField.Obj & SFIX_MaskForMetaBits) >> 32, [&](auto& a) { OutParam = a.second; })
		!= 0;
}

void UInventoryDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	ContingentPhysicsLinkage = Collection.InitializeDependency<UBarrageDispatch>();
	ArtilleryDispatch = Collection.InitializeDependency<UArtilleryDispatch>();
	ProjectileDispatch = Collection.InitializeDependency<UArtilleryProjectileDispatch>();
	MyShadowCopies.Reserve(1024);
	MyTrackingSet.reserve(1024);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UInventoryDispatch::LoadSoundsHelper(TMap<FSKSoundFXDefinitionKey, 
                                               USoundBase*>& SFXDefinitionKeyToSound, 
                                          TMap<FSKSoundFXDefinitionKey, FSKSoundFXDefinitionKey>& FNameToSoundKey,
                                          TArray<FName>& SoundNamesForPulldown,
                                          TArray<FAssetData>& Sounds
)
{
	auto* Reg = IAssetRegistry::Get();
	//TODO: this needs to be async and should use softpaths for the output. but here we are.
	Reg->GetAssetsByPath("/Game/Content/Sound/Live/", Sounds, true,false);
	for (auto K : Sounds)
	{
		FSKSoundFXDefinitionKey A = FSKSoundFXDefinitionKey(K.GetPrimaryAssetId());
		auto Path =  K.GetSoftObjectPath();
		USoundBase* LoadedSound = Cast<USoundBase>(Path.TryLoad());
		SFXDefinitionKeyToSound.Add(A, LoadedSound);
		FNameToSoundKey.Add(LoadedSound->GetFName(), A);
		SoundNamesForPulldown.Add(LoadedSound->GetFName());
	}
}

void UInventoryDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	if ([[maybe_unused]] const UWorld* World = InWorld.GetWorld()) {
		UE_LOG(LogTemp, Warning, TEXT("Inventory:Subsystem: World beginning play"));
	}
	
	
}

void UInventoryDispatch::Deinitialize()
{
	Super::Deinitialize();
}

TPair<FSKSocketKey, FSkeletonKey> UInventoryDispatch::PlugToSocket(FSkeletonKey A, FSKSocketKey Into,
	FSkeletonKey SocketOwnedBy)
{
	return {};
}

TPair<FSKSocketKey, FSkeletonKey> UInventoryDispatch::PlugToSocket(FSkeletonKey A, FSKSocketKey Into,
	PlayerKey SocketOwnedBy)
{
	return {};
}

//this will make a set if a set does not already exist and associate it with Key A.
// an item instance of KeyB will then be made.
FInventorySetKey UInventoryDispatch::GiveNewInstanceToSet(FSkeletonKey A, FSKItemDefinitionKey B)
{
	return {};
}

FSKItemKey UInventoryDispatch::InstanceItem(FSkeletonKey OptionalOwner, FSKItemDefinitionKey A)
{
	return {};
}

FInventorySetKey UInventoryDispatch::GiveSpecificInstance(FSkeletonKey A, FSKItemKey B)
{
	return {};
}


FSKSocketKey UInventoryDispatch::GrantSocketToSet(FInventorySetKey Set)
{
	return {};
}

FInventorySetKey UInventoryDispatch::GrantSetToKey(FSkeletonKey Owner)
{
	return {};
}

///this could be quite slow. look out for it in the profiler.
FInventoryResultSet UInventoryDispatch::GetAllEntitiesWithinDistanceOfPoint(FVector2d& Point, double Radius)
{
	FBox2D bound = FBox2D(Point - Radius, Point + Radius);
	
	FInventoryResultSet ret;
	TArray<TPair<FBarrageKey, FVector2d>> Entities;
	auto localbind = QuadTreeForDistance;
	if (localbind)
	{
		localbind.Get()->GetElements(bound, Entities);
		for (auto pair : Entities)
		{
			ret.UnderlyingKeys.insert(pair.Key.KeyIntoBarrage);
		}
		ret.Rebuild();
	}
	return ret;
}

FSKPlugKey UInventoryDispatch::BindAbilityToSocket(FGunKey Instance, FSKSocketKey BindTo)
{
	return {};
}

FSKItemKey UInventoryDispatch::CreateOnVerTick(std::vector<FSkeletonKey>& ToCreate)
{
	return {};
}


FSKItemKey UInventoryDispatch::CreateOnVerTick(FTriggerInstance& ToCreate, bool HasBounds)
{
	//generate key
	ItemInstance(ToCreate);
	//bluntly, we don't support triggers without bounds and a location yet. I'm sure we will for quests.
	ensure(HasBounds);
	
	
	auto kR = ToCreate.radius;
	FVector2D center = {ToCreate.MyStartingLocation.X, ToCreate.MyStartingLocation.Y};
	FVector2D delt = {kR,kR};
	
	//we'll need the box to remove it from the quadtree for SOME STUPID REASON.
	//we should switch to a faster quadtree. jolt trigger-like colliders (sensors)
	//are unfortunately a little slow for good reasons,
	//because they cover a broader use case.
	ToCreate.BoxIfInQuadTrie = FBox2D(( center- delt), center + delt);
	ItemCollision->Insert(ToCreate.MyKey, ToCreate.BoxIfInQuadTrie );
	//~~otherwise add trigger to constraint machine.~~
	//actually, if you want 'em, you implement that. --J
	auto GunKey = ArtilleryDispatch->GetGun(ToCreate.MyGun.GunDefinitionID, ToCreate.MyKey.GetSK());
	TriggerGuns_BusyWorkerOnly.Add(ToCreate.MyKey, GunKey);
	return ToCreate.MyKey;
}

FSKItemKey UInventoryDispatch::CreateWithISMOnVerTick(FTriggerInstance& ToCreate)
{
	CreateOnVerTick(ToCreate, true);
	
	
	
	ToCreate.MyStaticMeshIfAny = ProjectileDispatch->QueueKinematicProjectileInstance(
		"", 
		ToCreate.MyGun, 
		ToCreate.MyStartingLocation, 
		{0,0,0},
		1,
		Layers::NUM_LAYERS,
		nullptr,
		-1);
	return ToCreate.MyKey;
}

FSKItemKey UInventoryDispatch::OnVerTick(std::vector<FSkeletonKey>& ToRun)
{
	return {};
}

FSKItemKey UInventoryDispatch::TriggerOnVerTick(std::vector<FSkeletonKey>& ToTrigger)
{
	return {};
}

FSKItemKey UInventoryDispatch::AddItemInstanceToSet(FSKItemKey Place, FInventorySetKey Into)
{
	return {};
}

bool UInventoryDispatch::TrackGameplayTagSet_NotRetroactive(FGameplayTag Tag)
{
	FInventoryResultSet empty;
	SetsByTag.emplace(Tag, empty);
	return true;
}

FSKPlugKey UInventoryDispatch::BindArbitraryKeyToSocket(FSkeletonKey NoWarranty, FSKSocketKey BindingTo)
{
	//this should either use the dependent key mechanism to bind to the socket or to the set, and I just can't decide which
	//I think to the socket, since the socket connects to the set? so that ends up being
	//do we use the socket key mechanism? I'm honestly not sure. we might condense sockets into an item type and free 
	// a whole keytype. I'll come back to this.
	return {};//should this be item instance or plug key? 
}

void UInventoryDispatch::ArtilleryTick(uint64_t TicksSoFar)
{
	
	//k-2: accumulate entities
	if (TicksSoFar-2 % Inventory_MAPPINGCADENCE)
	{
		MyTrackingSet.clear();
		auto HoldOpen = ContingentPhysicsLinkage->JoltGameSim;
		HoldOpen->GetBodiesList(MyTrackingSet);
	}
	//k-1: accumulate data
	if (TicksSoFar-1 % Inventory_MAPPINGCADENCE)
	{
		auto HoldOpen = ContingentPhysicsLinkage->JoltGameSim;
		if (HoldOpen)
		{
			PhysicsStateMaintence = true;
			MyShadowCopies.Empty();
			FrozenPhysicsState.clear();
			for (auto result : MyTrackingSet)
			{
				if (!result.IsInvalid())
				{
					auto flat = HoldOpen->physics_system.Get()->GetBodyLockInterface().TryGetBody(result);
					MyShadowCopies.Emplace(*flat);
					
					auto id = ContingentPhysicsLinkage->GenerateBarrageKeyFromBodyId(result);
					FrozenPhysicsState.emplace(id, *flat);
				}
			}
		}
	}
	std::atomic_signal_fence(std::memory_order_acq_rel);
	PhysicsStateMaintence = false;
	//build distance map
	if (TicksSoFar % Inventory_MAPPINGCADENCE == 0)
	{
		QuadTreeMaintenance = true;
		TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> HoldOpen = QuadTreeForDistance; // retain the ref to the old map until our tick is finished.
		TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> QuadTreeCandidate = MakeShareable(new TQuadTree<TPair<FBarrageKey, FVector2d>>(FBox2d(FVector2d::ZeroVector - 200000, FVector2d::ZeroVector + 200000)));  //swap now.
		for (int i = 0; i < MyTrackingSet.size(); ++i)
		{
			auto& body = MyShadowCopies[i];
			auto id = ContingentPhysicsLinkage->GenerateBarrageKeyFromBodyId(MyTrackingSet[i]);
			auto layer = body.GetObjectLayer();
			if (uint8(layer) != Layers::EJoltPhysicsLayer::NON_MOVING 
				&& layer != Layers::EJoltPhysicsLayer::HITBOX
				&& layer != Layers::EJoltPhysicsLayer::DEBRIS
				&& layer != Layers::ENEMYHITBOX) // TODO: CHECK THIS CHECK THIS CHECK THIS SERIOUSLY WE MIGHT WANT THESE
			{
				auto location = MyShadowCopies[i].GetCenter();
				auto box = body.GetWorldSpaceBounds();
				auto mini = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(box.mMin));
				auto maxi = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(box.mMax));
				auto UEBox = FBox2d(mini, maxi);
				FVector2d TwoDLoc = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(location));
				auto pair = TPair<FBarrageKey, FVector2d>(id, TwoDLoc);
				QuadTreeCandidate->Insert(pair,UEBox);
			}
		}
		QuadTreeForDistance = QuadTreeCandidate;
		QuadTreeMaintenance = false;
	}
	if (TicksSoFar+1 % Inventory_MAPPINGCADENCE == 0)
	{
		SetsByTag.visit_all([](std::pair<FGameplayTag, FInventoryResultSet>& Pair)
		{
			if (Pair.second.Dirtied_AllowsFalseFalses)
			{
				Pair.second.Rebuild();
			}
		});
	}
}

void UInventoryDispatch::FireCue(FSKCueTicket Nyooooom)
{
	if (CueParamsToFire.contains(Nyooooom) && !FiredCueSet.contains(Nyooooom))
	{
		uint32 effect = Nyooooom.GetSK().GetShiftedMeta() ;
		auto send = CueDefinitionKeyToAssetName.Find(effect);
		FEventParameterPackage A;
		//gabbledegook goes here.
		bool a = FiredCueSet.insert(Nyooooom);
	}
}


void UInventoryDispatch::Tick(float DeltaTime)
{
}

TStatId UInventoryDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UInventoryDispatch, STATGROUP_Tickables);
}

uint64 UInventoryDispatch::GetSubtypedPrefix(SKELLY type, uint32 Notched28BitKey)
{
	uint64 ret = (type >> 28) | ((Notched28BitKey & SKELLY::SFIX_NotchKeyForMetaUse) << 4);
	return ret;
}

uint64 UInventoryDispatch::BuildKnownKey(SKELLY type, uint32 Notched28BitKey, SpecialPlugKeySlices KnownSlice)
{
	uint64 ret = GetSubtypedPrefix(type, Notched28BitKey);
	ret = (ret << 28) | KnownSlice;
	return ret;
}


bool UInventoryDispatch::GetSetKeysByOwner(FSkeletonKey Owner, UInventoryDispatch::OwnedSets& OutParam)
{
	bool found = bool(SetsByOwner.visit(Owner, [&OutParam](auto& a) { OutParam = a.second; }));
	return found;
}



FInventoryResultSet UInventoryDispatch::IntersectSets(FInventoryResultSet smaller, FInventoryResultSet larger)
{
	return smaller.IntersectSets(larger).Finalize();
}

FInventoryResultSet UInventoryDispatch::IntersectSetsByKey(FInventorySetKey AKey, FInventorySetKey BKey)
{
	UInventoryDispatch::OwnedSets ASet, BSet;
	bool found = bool(SetsByOwner.visit(AKey.GetSK(), [&ASet](auto& a) { ASet = a.second; }));
	if (found)
	{
		found = bool(SetsByOwner.visit(BKey.GetSK(), [&BSet](auto& a) { BSet = a.second; }));
		if (found)
		{
			auto& smaller = ASet.UnderlyingKeys.size() < BSet.UnderlyingKeys.size() ? ASet : BSet;
			auto& larger = ASet.UnderlyingKeys.size() < BSet.UnderlyingKeys.size() ? BSet : ASet;
			return smaller.IntersectSets(larger).Finalize();
		}
	}
	return {};
}

bool UInventoryDispatch::GetPlugsForSocketKey(FSKSocketKey ASocket, OwnedSets& OutParamOptionallyLiveMemory)
{
	return GetSetKeysByOwner(ASocket.GetSK(), OutParamOptionallyLiveMemory);
}

FSkeletonKey UInventoryDispatch::AddToSet(FSkeletonKey Place, FInventorySetKey Into)
{
	UInventoryDispatch::OwnedSets ASet;
	bool found = bool(SetsByOwner.visit(Into.GetSK(), [&ASet](auto& a) { ASet = a.second; }));
	if (found)
	{
		ASet.UnderlyingKeys.insert(Place);
		std::atomic_signal_fence(std::memory_order_acq_rel);
		ASet.Bloomlike.Add(Place); 
		return Place;
	}
	return FSkeletonKey();
}

FInventoryResultSet UInventoryDispatch::GetPlayerInventories(PlayerKey TargetPlayer)
{
	UInventoryDispatch::OwnedSets ASet;
	bool found = bool(SetsByPlayer.visit(TargetPlayer, [&ASet](auto& a) { ASet = a.second; }));
	return ASet;//we may want to filter by the Inventory type and subtype just to be real sure.
}

FInventoryResultSet UInventoryDispatch::GetNPInventories(FSkeletonKey Target)
{
	UInventoryDispatch::OwnedSets ASet;
	bool found = bool(SetsByOwner.visit(Target, [&ASet](auto& a) { ASet = a.second; }));
	return ASet;//we may want to filter by the Inventory type and subtype just to be real sure.
}

std::optional<FInventorySetKey> UInventoryDispatch::GetMainPlayerInventory(PlayerKey TargetPlayer)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::MainInventory);
	return FInventorySetKey().FromSK(FSkeletonKey(key));
}

std::optional<FInventorySetKey> UInventoryDispatch::GetPlayerQuests(PlayerKey TargetPlayer)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::Quests);
	return FInventorySetKey().FromSK(FSkeletonKey(key));
}

std::optional<FInventorySetKey> UInventoryDispatch::GetPlayerConditions(PlayerKey TargetPlayer)
{
	UInventoryDispatch::OwnedSets ASet = GetPlayerInventories(TargetPlayer);
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::Conditions);
	
	return FInventorySetKey().FromSK(FSkeletonKey(key));
}

std::optional<FInventorySetKey> UInventoryDispatch::GetNPConditions(FSkeletonKey Target)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(Target), SpecialPlugKeySlices::Conditions);
	return FInventorySetKey().FromSK(FSkeletonKey(key));
}

FName UInventoryDispatch::GetHumanReadable(FSKPlugKey A)
{
	
	return {};
}


FInventoryResultSet UInventoryDispatch::GetSubsetByTag(FGameplayTag Tag, FInventoryResultSet B)
{
	//TODO: add check for "tracked tag" which would allow the extremely fast set intersect.
	FInventoryResultSet TagSet;
	if (SetsByTag.contains(Tag))
	{
		auto Set = SetsByTag.visit(Tag, [&TagSet](auto& a) { TagSet = a.second; });
		IntersectSets(B, TagSet);
		return B;
	}
	else
	{
		for (auto Test : B.UnderlyingKeys)
		{
			bool TagPresent = ArtilleryDispatch->DoesEntityHaveTag(FSkeletonKey(Test), Tag);
			if (TagPresent)
			{
				//since we didn't use the visit, we don't map to a tracked set and we can just reuse our dec'd var since construction isn't free.
				TagSet.UnderlyingKeys.insert(Test);
			}
		}
		TagSet.Rebuild();
	}
	return TagSet;
}

seq::radix_set<unsigned long long>::const_iterator UInventoryDispatch::GetInventoriesOfKey(FSkeletonKey Owner)
{
	auto K = GetSubtypedPrefix(SKELLY::SFIX_InventoryOf, Owner);
	FInventoryResultSet OutParam;
	bool found = bool(SetsByOwner.visit(Owner, [&OutParam](auto& a) { OutParam = a.second; }));
	return SeqU64Prefix(OutParam.UnderlyingKeys, K << 32 , 36); //double check this line...
}

TArray<FInventorySetKey> UInventoryDispatch::GetKeysFromResultSet(FInventoryResultSet ToUnpack)
{
	TArray<FInventorySetKey> Ret;
	for (auto key : ToUnpack.UnderlyingKeys)
	{
		Ret.Add(FInventorySetKey::FromSK(FSkeletonKey(key)));
	}
	return Ret;
}

FSkeletonKey UInventoryDispatch::CreateQueuedTriggerOnVerifiedFrame(FTransform LocationnAndDimensions)
{
	return FSkeletonKey(); // but mashha, it is cold outside.
}