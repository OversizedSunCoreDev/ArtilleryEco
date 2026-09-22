// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryDispatch.h"
#include "InventoryEssentialTypes.h"
#include "GenericQuadTree.h"
#include "KeyedConcept.h"
#include "FBarrageKey.h"
#include "seq/SeqU64Prefix.hpp"
	
#include "EdGraphUtilities.h"
#include "SlateFwd.h"

#include "BarrageDispatch.h"
#include "FArtilleryGun.h"
#include "FlattenedBodyBox.h"
#include "ORDIN.h"
#include "Subsystems/WorldSubsystem.h"

#include "InventoryDispatch.generated.h"


class AUInventoryTriggerProxy;

USTRUCT(BlueprintType)
struct FSoundPinBag
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Data")
	FName AvailableSoundsAtEditTime;
};	

USTRUCT(Blueprintable)
struct FEventParameterPackage
{
	GENERATED_BODY()
	FSkeletonKey EventKey; //I don't remember what this was supposed to be. I go one week where I'm light on comments and I suffer immediately.
	double VolumePercentage0to100 = 0; 
	FVector Loc = {0,0,0};
	uint32 Hash()
	{
		return HashCombineFast(  GetTypeHash(EventKey), GetTypeHash(Loc));
	}
};

/**
 * Hi! If you're looking at Inventory, you're probably (hopefully) trying to either do fast queries that allow you to compose behaviors
 * or looking to do, you know, inventory. The secret is that we don't really see a difference between those two things. This is a 
 * design philosophy inherited from Destiny 2, where it proved pretty essential in, ya know, actually making the game. And that did
 * make about a billion dollars and live for a long time. I don't want to appeal to authority though. So let's talk about what this
 * does and you can make your own call. First, what it's not:
 * 
 * This is not a complete inventory system. I'm not sure a fully general one is possible.
 * It's not something we wanted to include in the core of artillery, because it is a little out of scope.
 * On the other hand, it's pretty hard to build this with just attribs and tags, so we do want to provide it.
 * Finally, this is headless - there's no UI included in Inventory. Eventually, a lot of it will appear in sunflower!
 * 
 * What it is:
 * A set of powerful and expressive primitives for describing any inventory system.
 * Enough actual meat that building most inventory systems with it will be pretty simple.
 * A basic Model Query Binding design where the model is an impure ECS, that is to say, Artillery
 * The query is set intersections. sets are entities themself, and referenced by key.
 * Sets have a subtype marker, unlike most other keys in our system.
 * So a vendor is a set, an inventory is a set, the enemies that can spawn in a level would be a set.
 * Sets have three kinds of members: 
 *		1) regular keys like you've seen everywhere else! These are used as behavior binders! 
 *		for example, you can get a state tree this way! These tend to be things you want to be able to reference quickly.
 *		And aren't always what the set actually stores. When they are, they just go in the set. When they're properties of a set, they go in...
 *		2) Sockets! Sockets hold keys. They allow one-to-many relationships, which is something we intentionally do not support seriously with our relationship keys.
 *		Where relationship keys should be thought of a noun-verb-noun triples, or facts, sockets cover all the bullshit that comes up in games all the time.
 *		like when some fucker puts on three rings instead of two, because someone made a hand of glory item? well, the hand of glory item just adds a third ring socket.
 *		That's not something that describes well with the triples. you can do it, but it's awkward, and as someone with a decade of experience using graph DBs:
 *		paradigm purity can fuck itself.
 *		3) Item Instance keys! These are special keys that contain a reference to their item Definition uniquely in their actual key layout!
 *		In simple systems, they may also contain a set reference! Sockets actually do, and it's very useful. This isn't a great fit for everything,
 *		so we don't enforce or assume it. (It also makes almost the whole key just the set and instance. This is Not Great for technical reasons.)
 *		Instance keys and variations on this hierarchical key concept form the backbone of what allows Inventory to answer certain kinds of queries in
 *		literally constant time. We don't use lookup tables for a lot of relationships. They're just encoded in the key.
 *		
 *		Here are the actual bits you'll see built out:
 *		Vendor Sets
 *		Inventory Sets
 *		Results Sets
 *		
 *		Item Definition Keys
 *		Item Instance Keys
 *		Attribute-keys-as-currency
 *		
 *		Sockets. So many sockets:
 *		In destiny 2, there was a privileged kind of item that specifically went into sockets called plugs. 
 *		We uh... just let you put whatever in there. So you might see _a lot_ of sockets show up in your design.
 *		You'll likely use those or ConservedAttributeKeys to connect guns (Abilities) to things.
 *		
 *		
 *		Why these? Well, I built a game on this design already, so I like it. In Destiny, most things were vendors.
 *		Seriously. Most things were just vendors. Perks? Sockets and plugs. 
 *		Xur, obviously a vendor. Bounties? Vendors. Lootable crates? Often vendors! 
 *		Your seasonal artifact? vendor that sold plugs for its own sockets.
 *		
 *		Look, man. I know it seems oversimplified. But it's actually a pretty normal design.
 *		
 *		
 * 
 */
//oh boy this is a mess.
UCLASS()
class ARTILLERYRUNTIME_API UInventoryDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy
{
	GENERATED_BODY()
	//TPair<FSkeletonKey, FVector2d> worked fine...
	friend class UArtilleryDispatch;
	friend class FRequestRouter;
public:
	
	//Okay, this gets a bit odd. These exist SPECIFICALLY so that we have a good way to do a very particular set of things
	//We need to be able to choose if an effect plays in predicted frames, and then ensure that if so, it plays at most once.
	//So cues run on verified frames and map to the UE cue concept.
	//Effect keys run in predicted frames
	//both have a concept of idempotence. this means that we need to EITHER be able to persist the keys across rollbacks, which is borderline satanic
	//since the things generating the keys won't persist, or we need a key to be its own idempotence token, which means every key must generate exactly the same way
	//no matter what. Thankfully, this is a pretty common problem and even doing it very fast and locklessly is pretty solved. still annoying though.
	TArray<FAssetData> Sounds;


	
	
	
	//TODO TODO TODO PERFORMANCE HAZARD
	//this design can be made WAY more elegant. We also do not care yet.
	//if we use it, I'll fix it up.
	seq::concurrent_set<FSKEffectTicket> FiredEffectSet;
	seq::concurrent_map<FSKEffectTicket, FEventParameterPackage> EventParamsToFire;
	
	seq::concurrent_set<FSKCueTicket> FiredCueSet;
	seq::concurrent_map<FSKCueTicket, FEventParameterPackage> CueParamsToFire;
	UPROPERTY()
	TMap<FSKSoundFXDefinitionKey, USoundBase*> SFXDefinitionKeyToSound;
	UPROPERTY()
	TMap<FSKSoundFXDefinitionKey, FSKSoundFXDefinitionKey> FNameToSoundKey;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TArray<FName> SoundNamesForPulldown;
	UFUNCTION()
	TArray<FName> GetSoundNamesOptions() const
	{
		return SoundNamesForPulldown;
	}
	
	TMap<uint32, FName> CueDefinitionKeyToAssetName;
	FSKEffectTicket GetSoundEffectTicket(FSkeletonKey OwnerOrSource, FSkeletonKey EffectDefinition, FEventParameterPackage EffectParameters, bool UseParametersForHash);

	//this can run any time but will only fire once per ticket.
	//tickets are an idempotent construct.
	bool FireSoundEffect(FSKEffectTicket Nyooooom);


	constexpr static uint32 Inventory_MAPPINGCADENCE = 32;
	seq::ordered_set<FSimpleTriggerGun> TriggerLinkedGuns;
	enum SpecialPlugKeySlices : uint64
	{
		InventorySetMarkers = SFIX_InventoryOf,
		
		MainInventory = 0xFFAABB,// FAB. IT IS FAB.
		Quests		  = 0x111111, //best if this one is really identifiable in binary, hex, and decimal.
		Conditions	  = 0x51c51c
	};

	std::atomic<uint64_t> MonotonicKey = 1;//starts at 1, in case we want item Definitions to simply be hash-blanked item keys, which frankly is sounding good.
	
	
	//note that ordered set is the order that things are added in, not a sorted order.
	//this is MUCH more useful for us, because it's very unlikely that each quest step will be conveniently sequential.
	//the default behavior is that quest steps go live one after another, but this can be extended pretty easily.
	//I don't recommend using single quests for really complex stuff like large branching main quests.
	//Instead, you'll be better served building a construct on top of these, one that might lay out a set of quests that can be advanced.
	//I'd call them meta quests but that name is prettyyyyyyyyyyyyyyyyyyyyyy cursed now.
	//that said, these guys lend themselves really nicely to being defined in data or config files or even just data assets, which is why they're very simple
	class  FQuestTriggerGun;
	struct ARTILLERYRUNTIME_API FInventoryQuest
	{
		FSkeletonKey MyKey = FSkeletonKey(SKELLY::SFIX_Quest);
		UInventoryDispatch* MyDispatch;
		int index = 0;
		//these are normally gonna be trigger instance keys.
		//this should only be called from something running in the verified step unless you are... like.... a space wizard.
		//It's probably best to use a Quest Trigger Gun for your quests. Just guessin' tho. we don't enforce that cause you might wanna just reuse simple triggers.
		bool ProgressQuest(FTriggerInstance& QuestStepTriggerToFinish, bool SimpleStepForward = true, int SuggestedIndex = 0)
		{
			if (SimpleStepForward)
			{
				QuestStepTriggerToFinish.CloseTrigger();
				++index;
				MyDispatch->CreateOnVerTick(Steps[index]);
				return true;
			}
			return false;
		};
		
		TArray<FTriggerInstance> Steps;
	};
	
	//this can be used to look up the quest that owns a given trigger using just the meta field embedded in the trigger's key if needed.
	seq::concurrent_map<uint32, FInventoryQuest> MetaFieldToQuest;
	bool FetchQuest(FSkeletonKey MustHaveValidMetaField, FInventoryQuest& OutParam);

	//uses a double buffer model
	//could be considered a shadowcopy I guess.
	struct FSafeDataSet
	{
		FInventoryResultSet DataCopyA;
		FInventoryResultSet DataCopyB;
		FSkeletonKey MyKey;
		FSkeletonKey MyBind;
		FInventoryResultSet* Read  = &DataCopyA;
		FInventoryResultSet* Write = &DataCopyB;
		int TicksBetweenRefresh;
		
		virtual void Refresh()
		{
			Swap(Read, Write);
		}
		
	};
	//uses a double buffer model
	//could be considered a shadowcopy I guess.
	struct FSafeTrackingDataSet : public FSafeDataSet
	{
		FInventorySetKey MyTrackedSet;
		UInventoryDispatch* MyDispatch;
		void Refresh() override
		{
			MyDispatch->SetsByOwner.visit(MyTrackedSet.GetSK(), [this](auto& a) { *(this->Write) = a.second; });
			Swap(Read, Write);
		}
		
	};
	
	

	
	using OwnedSets = FInventoryResultSet;//GOT THAT OUROBOROS IN ME, OUROBOROS
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	//each supported type ends up basically having a load helper,
	//a graph pin factory, and a pulldown implementation.
	static void LoadSoundsHelper(TMap<FSKSoundFXDefinitionKey, 
	USoundBase*>& SFXDefinitionKeyToSound, 
	TMap<FSKSoundFXDefinitionKey, FSKSoundFXDefinitionKey>& FNameToSoundKey,
	TArray<FName>& SoundNamesForPulldown,
	TArray<FAssetData>& Sounds
	);
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	uint64 GetSubtypedPrefix(SKELLY type, uint32 Notched28BitKey) // if you didn't notch it, it will _GET_ notched
	;

	uint64 BuildKnownKey(SKELLY type, uint32 Notched28BitKey, SpecialPlugKeySlices KnownSlice) // if you didn't notch it, it will _GET_ notched
	;


	UArtilleryDispatch* ArtilleryDispatch;
	UArtilleryProjectileDispatch* ProjectileDispatch;
	bool GetSetKeysByOwner(FSkeletonKey Owner, OwnedSets& OutParamOptionallyLiveMemory);
	//Result sets are not concurrent datatypes. If you must use them across threads, copy them.
	//result sets do not have their own keys, they are a transient primitive unique to Inventory.
	
	
	//CoreFunction
	bool TrackGameplayTagSet_NotRetroactive(FGameplayTag Tag);
	//TArray<FInventorySetKey> FilterBySubtype(ESetSubtype Subtype, FInventorySetKey ToFilter); //inventory, vendor, loot table, etc
	FInventoryResultSet IntersectSets(FInventoryResultSet smaller, FInventoryResultSet larger);
	FInventoryResultSet IntersectSetsByKey(FInventorySetKey A, FInventorySetKey B);
	bool GetPlugsForSocketKey(FSKSocketKey ASocket, OwnedSets& OutParamOptionallyLiveMemory);
	FSkeletonKey AddToSet(FSkeletonKey Place, FInventorySetKey Into);
	
	//Second-order functions
	FInventoryResultSet GetPlayerInventories(PlayerKey TargetPlayer);
	FInventoryResultSet GetNPInventories(FSkeletonKey Target);
	std::optional<FInventorySetKey> GetMainPlayerInventory(PlayerKey A);
	std::optional<FInventorySetKey> GetPlayerQuests(PlayerKey TargetPlayer);
	std::optional<FInventorySetKey> GetPlayerConditions(PlayerKey TargetPlayer);
	std::optional<FInventorySetKey> GetNPConditions(FSkeletonKey Target);
	FName GetHumanReadable(FSKPlugKey A); //generally for debug
	FInventoryResultSet GetSubsetByTag(FGameplayTag Tag, FInventoryResultSet B);
	seq::radix_set<unsigned long long>::const_iterator GetInventoriesOfKey(FSkeletonKey Owner); // privileged for sanity reasons. i'm not a monster.
	TArray<FInventorySetKey> GetKeysFromResultSet(FInventoryResultSet ToUnpack);
	;
	FSkeletonKey CreateQueuedTriggerOnVerifiedFrame(FTransform LocationnAndDimensions);
	//pairwise
	TPair<FSKSocketKey, FSkeletonKey> PlugToSocket(FSkeletonKey A, FSKSocketKey Into, FSkeletonKey SocketOwnedBy); 
	TPair<FSKSocketKey, FSkeletonKey> PlugToSocket(FSkeletonKey A, FSKSocketKey Into, PlayerKey SocketOwnedBy);
	//Bind Operators
	FSKItemKey AddItemInstanceToSet(FSKItemKey Place, FInventorySetKey Into);
	FInventorySetKey GiveNewInstanceToSet(FSkeletonKey A, FSKItemDefinitionKey B);
	FSKItemKey InstanceItem(FSkeletonKey OptionalOwner, FSKItemDefinitionKey A);
	FInventorySetKey GiveSpecificInstance(FSkeletonKey A, FSKItemKey B);
	FSKSocketKey GrantSocketToSet(FInventorySetKey Set);
	FInventorySetKey GrantSetToKey (FSkeletonKey Owner);
	
	//Socket ops
	FInventoryResultSet GetAllEntitiesWithinDistanceOfPoint(FVector2d& Point, double Radius);
	//Man I dunno about this. I'm gonna implement this if we need it.
	FSKPlugKey BindAbilityToSocket(FGunKey Instance, FSKSocketKey BindTo);
	
	FSKItemKey CreateOnVerTick(std::vector<FSkeletonKey>& ToCreate);
	FSKItemKey CreateOnVerTick(FTriggerInstance& ToCreate, bool HasBounds=true);
	//don't use this cross thread. it's unsafe in many ways!
	TMap<FSKItemInstance, FGunKey> TriggerGuns_BusyWorkerOnly;

	FSKItemKey CreateWithISMOnVerTick(FTriggerInstance& ToCreate);
	FSKItemKey OnVerTick(std::vector<FSkeletonKey>& ToRun);
	FSKItemKey TriggerOnVerTick(std::vector<FSkeletonKey>& ToTrigger);
	//This is provided without warranty for people who don't wanna use guns.
	FSKPlugKey BindArbitraryKeyToSocket(FSkeletonKey NoWarranty, FSKSocketKey BindingTo); 
	

	
	class TagHasher
	{
	public:
		SEQ_STR_INLINE_STRONG auto operator()(const FGameplayTag A) const noexcept -> size_t
		{
			return MashFunctions::FastHash32( A.GetTagName().GetNumber());
		}
	};
	seq::concurrent_map<FSkeletonKey, OwnedSets> SetsByOwner;//we don't need to go the other way around.
	seq::concurrent_map<PlayerKey, OwnedSets> SetsByPlayer; //speed and sanity demands we separate these out.
	//TODO: expose properly for use!!!
	seq::concurrent_map<FBarrageKey, JPH::BodyBoxFlatCopy> FrozenPhysicsState;
protected:
	//tracked tags only.
	seq::concurrent_map<FGameplayTag, FInventoryResultSet, TagHasher> SetsByTag;

	//These SHOULD be ItemKeys but that is not enforced for some pretty important reasons, namely, you might want to flick on
	//triggers around players and similar here and there.
	seq::concurrent_map<FSkeletonKey, FTriggerInstance> Triggers;
	TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> QuadTreeForDistance = {};
	TSharedPtr<TQuadTree<FSKItemInstance>> ItemCollision = {};
	bool QuadTreeMaintenance = true;
	bool PhysicsStateMaintence = true;
	
	//TODO: unstub me.
	//used by the request router for deployables and triggers.
	UBarrageDispatch* ContingentPhysicsLinkage = nullptr;
	JPH::BodyIDVector MyTrackingSet = {};
	TArray<JPH::BodyBoxFlatCopy> MyShadowCopies = {};
	FSKItemKey PlaceItemInstanceInWorldOnceCanon(FSKItemKey Item, FVector Location);
public:
	virtual void ArtilleryTick(uint64_t TicksSoFar) override;
	
	UFUNCTION(Blueprintable, Category="Artillery|ReadOnly|Attributes")
	bool IDM_TrySFX(FSkeletonKey OwnerOrSource, FSkeletonKey EffectDefinition, FEventParameterPackage EffectParameters, bool UseParametersForHash);
	
	void RunDelayedTriggers() {};

protected:
	//this ONLY runs on verified frames.
	void FireCue(FSKCueTicket Nyooooom);

private:
	constexpr static int OrdinateSeqKey = ORDIN::E_D_C::InventorySystem;
	virtual bool RegistrationImplementation() override;

	void ItemInstance(FTriggerInstance& ToCreate)
	{
		ToCreate.MyKey = FSKItemInstance(MonotonicKey++, ToCreate.MyKey.GetSK().GetShiftedMeta(), SFIX_SubtypeSelector::Tradiitional_Item);
	}

};


