#pragma once

#include "SkeletonTypes.h"
#include "Skeletonize.h"
#include "Templates/TypeHash.h"
#include "DataTableEditorUtils.h"

#include "SGraphPinNameList.h"

THIRD_PARTY_INCLUDES_START
#include "seq/ordered_map.hpp"

#include "seq/SeqU64Prefix.hpp"
#include "seq/flat_map.hpp"
#include "Structures/ApproximateMembership/FLargeGate.h"
THIRD_PARTY_INCLUDES_END
#include "FArtilleryGun.h"
#include "InventoryEssentialTypes.generated.h"
#define Inventory_VERIFIEDFRAMETESTMODE true




UENUM(BlueprintType, Blueprintable)
enum class E_FidEffectParameter : uint8
{
	Volume,
	None
};


//hi! Using a radix trie here allows us to search by partial prefix! this lets us pull just plugs, or just sockets, very very
//very fast! it also allows us to search elegantly over the _meta_ in the keys themselves, which we've QUITE CAREFULLY
//set up to present a structured hierarchy. this lets you access it VERY VERY FAST by just saying [type][parent_key]
struct SEQGetSKKey
{
	uint64 operator()(const FSkeletonKey & p) const 
	{
		return p.Obj;
	}
};
using FCRKeyStruct = SeqSet64;

//These are basically skeleton keys with some extra convenience methods on them that waste storage space but significantly
//reduce incidence of insanity in even casual users. Sorry, but sometimes efficiency isn't efficient.
USTRUCT()
struct ARTILLERYRUNTIME_API FUSInventoryKeys
{
	
	GENERATED_BODY()
	virtual ~FUSInventoryKeys() = default;

	uint64_t MyKey = 0;
	
	friend uint32 GetTypeHash(const FUSInventoryKeys& Arg)
	{
		return MashFunctions::FastHash6432(Arg.MyKey);
	}
	
	virtual bool operator==(const FUSInventoryKeys& rhs) const {
		return (MyKey == rhs.MyKey);
	}
};

struct ARTILLERYRUNTIME_API FInventorySetKey : public  FUSInventoryKeys
{
	constexpr static auto MasterKeyType = SFIX_SetOf;
	FInventorySetKey(uint32 GetsSlicedTo28Bits, uint64_t ParentKey, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKey, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	//invalid
	FInventorySetKey()
	{
		MyKey = 0;
	}

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
	
	// in the core skeleton key class, we've had to really fiddle around to prevent some unwanted conversions that would
	// have been avoided if we'd just rolled a couple functions instead of using op overriding, casts, and constructors.
	static FInventorySetKey FromSK(FSkeletonKey From)
	{
		FInventorySetKey retval;
		(retval.MyKey=From);
		return retval;
	}
};

//item instances use the subtype field. they keep their definition hash in the meta, their instance hash in the hash field.
//so they are of the form:        Type|Definition|Subtype|InstanceID  ->              	 0xTDDD DDDD SIII IIII,
//where T is the 4 bit type field, S is the 4 bit subtype field, I is the 28 bit instance hash, and D is the 28 bit definition.
//Consolidating them this way makes a number of operations less stupid.
////////////////////////////////////////////////////////////////////////////
///
/// TESTING NOTE: 
/// we'll wannna use specific subtypes where we can. 
/// idk how to do that elegantly yet, so..
///
////////////////////////////////////////////////////////////////////////////
struct ARTILLERYRUNTIME_API FSKItemInstance : public FUSInventoryKeys
{
	constexpr static auto MasterKeyType = SFIX_InventoryItem;

	FSKItemInstance(uint32 GetsSlicedTo28Bits, uint64_t ParentKeyOfItemInstance, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKeyOfItemInstance, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	FSKItemInstance() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}


	inline bool operator==(const FSKItemInstance& rhs) const {
		return (MyKey == rhs.MyKey);
	}
	
	inline bool operator==(const FUSInventoryKeys& rhs) const override {
		return (MyKey == rhs.MyKey);
	}
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (Item Instance)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_PHDPHD(const FSKItemInstance& A, const FSKItemInstance& B)
	{
		return A.MyKey == B.MyKey;
	}
};

struct ARTILLERYRUNTIME_API FSKItemDefinitionKey : public  FUSInventoryKeys
{
	constexpr static auto MasterKeyType = SFIX_ItemDefinition;
	uint64_t MyKey = 0;
	FSKItemDefinitionKey(uint32 GetsSlicedTo28Bits, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			//oh hey, would you look at that. definition keys FORCE an empty instance so they're valid. Isn't that cute? :/
			SFIX_ImprintKeyDependency(GetsSlicedTo28Bits, 0, MasterKeyType),
			MySubtype);
		
	}

	FSKItemDefinitionKey() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};

USTRUCT()
struct ARTILLERYRUNTIME_API FSKSoundFXDefinitionKey : public  FUSInventoryKeys
{
	GENERATED_BODY()
	constexpr static auto MasterKeyType = SFIX_SoundEffect;
	FPrimaryAssetId AssetId; //convenience var'd.
	uint64_t MyKey = 0;
	FSKSoundFXDefinitionKey(FName SoundEffectName)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			//oh hey, would you look at that. definition keys FORCE an empty instance so they're valid. Isn't that cute? :/
			SFIX_ImprintKeyDependency(GetTypeHash(SoundEffectName), 0, MasterKeyType),
			SFX);
	}
	
	FSKSoundFXDefinitionKey(uint32 SoundEffectIdFromShiftedMeta)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			//oh hey, would you look at that. definition keys FORCE an empty instance so they're valid. Isn't that cute? :/
			SFIX_ImprintKeyDependency(SoundEffectIdFromShiftedMeta, 0, MasterKeyType),
			SFX);
	}
	
	explicit FSKSoundFXDefinitionKey(const FPrimaryAssetId& PrimaryAssetId)
	{
		AssetId = PrimaryAssetId;
		FSKSoundFXDefinitionKey(PrimaryAssetId.ToString());
	}

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
	FSKSoundFXDefinitionKey() = default;
	
	//generally, you shouldn't use this constructor, it's present in case I forgot some use case.
	explicit FSKSoundFXDefinitionKey(const FString& String)
	{
		FSKSoundFXDefinitionKey(GetTypeHash(String));	
	}
};


struct ARTILLERYRUNTIME_API FSKPlugKey : public  FUSInventoryKeys
{
	constexpr static auto MasterKeyType = SFIX_Socket;
	FSKPlugKey(FSKItemInstance GetsSlicedTo28Bits, SFIX_SubtypeSelector MySubtype, FInventorySetKey ParentSet)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentSet.MyKey, GetsSlicedTo28Bits.MyKey, MasterKeyType),
			MySubtype);
		
	}

	FSKPlugKey() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};

struct ARTILLERYRUNTIME_API FSKSocketKey : public  FUSInventoryKeys
{
	constexpr static auto MasterKeyType = SFIX_Socket;
	FSKSocketKey(uint32 GetsSlicedTo28Bits, FInventorySetKey ParentSet)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentSet.MyKey, GetsSlicedTo28Bits, MasterKeyType),
			static_cast<SFIX_SubtypeSelector>(MasterKeyType >> 32));
		
	}
	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}

	FSKSocketKey() = default;
};

using FSKItemKey = FSKItemInstance;


struct ARTILLERYRUNTIME_API FResultSetInterstitial;
struct ARTILLERYRUNTIME_API FInventoryResultSet
{
	FLargeGate Bloomlike;
	FCRKeyStruct UnderlyingKeys;
	FInventorySetKey SetRepresentedIfAny;
	FSkeletonKey OwnerIfAny;// this makes a ton of stuff a million times less awkward.
	bool Dirtied_AllowsFalseFalses = false; //not everything that dirties a set touches this, and not everything should.
	FInventoryResultSet() : Bloomlike(), SetRepresentedIfAny()
	{
	}

	//copy
	FInventoryResultSet(const FInventoryResultSet& Other)
		: Bloomlike(Other.Bloomlike),
		  UnderlyingKeys(Other.UnderlyingKeys),
		  SetRepresentedIfAny(Other.SetRepresentedIfAny),
		  OwnerIfAny(Other.OwnerIfAny)
	{
	}

	//move
	FInventoryResultSet(FInventoryResultSet&& Other) noexcept
		: Bloomlike(std::move(Other.Bloomlike)),
		  UnderlyingKeys(std::move(Other.UnderlyingKeys)),
		  SetRepresentedIfAny(std::move(Other.SetRepresentedIfAny)),
		  OwnerIfAny(std::move(Other.OwnerIfAny))
	{
	}

	FInventoryResultSet(FInventorySetKey MyKey) : Bloomlike(), SetRepresentedIfAny(MyKey)
	{
	}

	explicit FInventoryResultSet(const FCRKeyStruct::iterator& Prefix)
	{
		UnderlyingKeys =  FCRKeyStruct(Prefix, FCRKeyStruct::iterator());
		for (auto i : UnderlyingKeys)
		{
			Bloomlike.Add(i);
		}
	}

	FInventoryResultSet& operator=(const FInventoryResultSet& Other)
	{
		if (this == &Other)
			return *this;
		Bloomlike = Other.Bloomlike;
		UnderlyingKeys = Other.UnderlyingKeys;
		SetRepresentedIfAny = Other.SetRepresentedIfAny;
		OwnerIfAny = Other.OwnerIfAny;
		return *this;
	}

	FInventoryResultSet& operator=(FInventoryResultSet&& Other) noexcept
	{
		if (this == &Other)
			return *this;
		Bloomlike = std::move(Other.Bloomlike);
		UnderlyingKeys = std::move(Other.UnderlyingKeys);
		SetRepresentedIfAny = std::move(Other.SetRepresentedIfAny);
		OwnerIfAny = std::move(Other.OwnerIfAny);
		return *this;
	}

	FResultSetInterstitial IntersectSets(FResultSetInterstitial& rhs);
	FResultSetInterstitial IntersectSets(FInventoryResultSet& rhs);
	bool Find(FSkeletonKey CheckPresenceOf);
	void Rebuild();
};


struct FResultSetInterstitial
{
	FResultSetInterstitial(FInventoryResultSet& Contained)
		: Contained(Contained)
	{
	}
	
	FResultSetInterstitial(FInventoryResultSet&& Contained)
	: Contained(Contained)
	{
	}

	FResultSetInterstitial(const FResultSetInterstitial& Other)
		: Contained(Other.Contained)
	{
	}

	FResultSetInterstitial(FResultSetInterstitial&& Other) noexcept
		: Contained(Other.Contained)
	{
	}

	FResultSetInterstitial& operator=(const FResultSetInterstitial& Other)
	{
		if (this == &Other)
			return *this;
		Contained = Other.Contained;
		return *this;
	}

	FResultSetInterstitial& operator=(FResultSetInterstitial&& Other) noexcept
	{
		if (this == &Other)
			return *this;
		Contained = Other.Contained;
		return *this;
	}

	explicit FResultSetInterstitial(FResultSetInterstitial* Result);

	FInventoryResultSet Finalize()
	{
		for (auto k : Contained.UnderlyingKeys)
		{
			if (Contained.Bloomlike.ApproxFind(k))
			{
				continue; //False positives aren't possible because we start with the origin set
			}
			else // included for clarity.
			{
				Contained.UnderlyingKeys.erase(k); //false negatives are prevented
			}
		}
		return FInventoryResultSet(std::move(Contained));
	}
	
	FResultSetInterstitial* IntersectSets(FInventoryResultSet& larger)
	{
		Contained.Bloomlike.IntersectInPlace(larger.Bloomlike);
		return this;
	}
	
	FResultSetInterstitial* IntersectSets(FResultSetInterstitial& larger)
	{
		Contained.Bloomlike.IntersectInPlace(larger.Contained.Bloomlike);
		return this;
	}
	
private:
	FInventoryResultSet& Contained;
	
	
	using  FCSResults = FInventoryResultSet;
};

inline FResultSetInterstitial FInventoryResultSet::IntersectSets(FResultSetInterstitial& rhs)
{
	FResultSetInterstitial Result(  std::move(FInventoryResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline FResultSetInterstitial FInventoryResultSet::IntersectSets(FInventoryResultSet& rhs)
{
	FResultSetInterstitial Result(  std::move(FInventoryResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline bool FInventoryResultSet::Find(FSkeletonKey CheckPresenceOf)
{
	if (Bloomlike.ApproxFind(CheckPresenceOf))
	{
		return UnderlyingKeys.contains(CheckPresenceOf);
	}
	return false;
}

inline void FInventoryResultSet::Rebuild()
{
}


//suitable for slow polled data presentation that refreshes rarely.
struct ARTILLERYRUNTIME_API FInventoryData : public FInventoryResultSet
{
};

//"push model" for Inventory data, emits listenable blueprint events for data change at a configurable K change interval
struct ARTILLERYRUNTIME_API FEventedInventoryData : public FInventoryData
{
};



	enum class WhatMattersToThis : uint16  //I don't know that we'll really use these as bitflags, but there are damn good reasons you might.
	{
		None = 							0b0,
		Player = 						0b1,
		Enemy = 						0b10,
		AnyMob =						0b11,
		MorePlayersThanEnemies = 		0b100,
		WeightedMorePlayers	 =			0b1000, //player weight 2 
		MorePlayersThanPeerEnemies = 	0b10000, //Chaff doesn't count
		SpecificKey = 					0b100000,	//Only some specific key in the world
		SpecificKeyInPlayerInventory = 	0b1000000, //quest item support
		SpecificTag = 					0b10000000,
		InteractNeeded = 				0b100000000
	};
	
	
	//these are used in conjunction with the trigger guns to fire for basic stuff. I'll make five or six simple trigger guns.
	//anything more complex than this is gonna need a state tree if we don't want to quickly go insane writing hundreds of bespoke conditional state machines.
	//this also covers about 80% of triggers, and if you added the "dies in area" trigger, that combined with our existing OnDeath hooks would cover 95%.
	struct  FTriggerInstance
	{
		FTriggerInstance(const FSKItemInstance& MyKey, const FGunKey& MyGunIfAny, const TOptional<FGameplayTag>& AddToInventoryEntitlements, const TOptional<FGameplayTag>& TagNeededIfAny,
			const FSkeletonKey& KeyNeededIfAny, const FBoneKey& MyStaticMeshIfAny, const FBarrageKey& MyTransformLinkIfAny, float Radius, int EntitiesInRadiusToPrime, int TimesTriggeredToSetOff,
			int TimesTriggered, int LastTickPrimed, int MaxAllowedDelayBetweenPrimedFrames, int TimesAllowedToTrigger, WhatMattersToThis CategoryChecked, const FVector& MyStartingLocation)
			: MyGun(MyGunIfAny), MyKey(MyKey),
			  AddToInventoryEntitlements(AddToInventoryEntitlements),
			  TagNeededIfAny(TagNeededIfAny),
			  KeyNeededIfAny(KeyNeededIfAny),
			  MyStaticMeshIfAny(MyStaticMeshIfAny),
			  MyTransformLinkIfAny(MyTransformLinkIfAny),
			  radius(Radius),
			  EntitiesInRadiusToPrime(EntitiesInRadiusToPrime),
			  TimesTriggeredToSetOff(TimesTriggeredToSetOff),
			  TimesTriggered(TimesTriggered),
			  LastTickPrimed(LastTickPrimed),
			  MaxAllowedDelayBetweenPrimedFrames(MaxAllowedDelayBetweenPrimedFrames),
			  TimesAllowedToTrigger(TimesAllowedToTrigger),
			  CategoryChecked(CategoryChecked),
			  MyStartingLocation(MyStartingLocation)
		{
		}

		FTriggerInstance() : CategoryChecked()
		{
		};
		FGunKey MyGun;
		FSKItemInstance MyKey; // you can extract the Definition from this!
		//the following two or three fields should probably be excised by creating guns that actually follow the logic.
		TOptional<FGameplayTag> AddToInventoryEntitlements; //If set, all entities in the radius that matter to this trigger will get this tag added to them as an entitlement when it goes off
		TOptional<FGameplayTag> TagNeededIfAny;
		FSkeletonKey KeyNeededIfAny;
		FSkeletonKey MyStaticMeshIfAny;
		//this is literally smaller than an optional. *sigh*
		//if set, this trigger will act like an aura around that transform's center. it does not perform a minkowsky sum. Just measures from the center. Crudely.
		FBarrageKey MyTransformLinkIfAny;
		FBox2D BoxIfInQuadTrie = FBox2D(ForceInit);
		float radius = 100;		//sane initial value
		int EntitiesInRadiusToPrime = 1;
		int TimesTriggeredToSetOff = 1;
		int TimesTriggered = 0;
		int LastTickPrimed = -1;
		int MaxAllowedDelayBetweenPrimedFrames = -1;//this just uses the triggered count as a smoothing or dejittering tool for "charging" triggers. add an allowed delay if you want the trigger to require continuous presence.
		int TimesAllowedToTrigger = 1; //if you need more than this, you are a somewhat bad person.
		WhatMattersToThis CategoryChecked = WhatMattersToThis::None;
		FVector MyStartingLocation = FVector::ZeroVector;

		bool CloseTrigger()
		{
			return true;
		}
	};



	USTRUCT()
	struct FSimpleTriggerGun : public FArtilleryGun
	{
		
		GENERATED_BODY()
		FTriggerInstance MyTriggerInstance;
		UInventoryDispatch* MyInventoryDispatch;
	public:

		//This is the function to override to add triggering check logic. PrefireGun now contains the logic for the verified frame check.
		//triggers only go off on verified frames. Many many many things will work this way. 
		virtual bool Precheck()
		{
			return true;
		}
		
	virtual void PreFireGun(
		FArtilleryStates OutcomeStates,
		int DallyFramesToOmit,
		bool RerunDueToReconcile, bool VerifiedFrame = false, const EventBufferInfo FireAction = EventBufferInfo::Default()) override
		{
			if (VerifiedFrame || Inventory_VERIFIEDFRAMETESTMODE)
			{
				if (Precheck()){
					FireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
				}
			}
		}
		
	virtual void FireGun(
	FArtilleryStates OutcomeStates,
	int DallyFramesToOmit,
	bool RerunDueToReconcile) override
		{
			FArtilleryGun::PostFireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
		}

		friend uint32 GetTypeHash(const FSimpleTriggerGun& Arg)
		{
			return Arg.MyGunKey.GunInstanceID; //these are valid hashes, interestingly.
		}
		
		//unlike almost all types, for us, equality is the same as comparing hashes, because
		//our hash is our key and it's precomputed with a known min\non-collisive trick _somewhere_
		//in this case, the upper 32 bits are deterministic information necessary for not hating yourself forever
		//and the lower 32 bits are an instance hash. 
		virtual bool operator==(const FSimpleTriggerGun& other) const {
			return MyGunKey == other.MyGunKey; 
		}
	};


//Idempotent keys can ONLY be generated by the dispatch. It's the only spot with enough knowledge to correctly generate them.
struct ARTILLERYRUNTIME_API Idempotent
{
	friend class UInventoryDispatch;
	friend struct std::hash<Idempotent>;
	virtual ~Idempotent() = default;

	friend uint32 GetTypeHash(const Idempotent& Arg)
	{
		return MashFunctions::FastHash6432(Arg.MyKey);
	}
	
	virtual bool operator==(const Idempotent& rhs) const {
		return (MyKey == rhs.MyKey);
	}
		
protected:
	uint64_t MasterKeyType = 0;
	uint64_t MyKey = 0;

};
	



//This key is type-preserving, like FSkeletonKey, but offers a semantic guarantee that this key can
//be used for as a uniqueness ticket for idempotent effects like, say, gunshot sound effects
//this allows us to make sure we don't play the effect every time we resimulate the frame.
//So unless you want to hear sound effects a million times, maybe use this.
struct ARTILLERYRUNTIME_API FSKEffectTicket : public Idempotent
{
	friend struct std::hash<FSKEffectTicket>;
	FSKEffectTicket() = default;
	FSKEffectTicket(FSkeletonKey Definition, uint32 IdempotenceTrick)
	{
		MasterKeyType = Definition.GetUnshiftedType();
		MyKey = Definition.GenerateInstanceFromDefinition(Definition, IdempotenceTrick);
	}
	virtual  FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};

//Cues are generally actual UE Cues, and should only fire on verified frames.
//This class is used with 
struct ARTILLERYRUNTIME_API FSKCueTicket : public FSKEffectTicket
{
	friend struct std::hash<FSKCueTicket>;
	FSKCueTicket() = default;

	FSKCueTicket(FSkeletonKey Definition, uint32 IdempotenceTrick)
	{
		MasterKeyType = Definition.GetUnshiftedType();
		if (MasterKeyType != SFIX_Cue)
		{
			MyKey = 0; //invalid.
		}
		else{
			MyKey = Definition.GenerateInstanceFromDefinition(Definition, IdempotenceTrick);
		}
	}
	virtual FSkeletonKey GetSK() override
	{
		return FSkeletonKey(MyKey);
	}
};




// Add specializations to the std namespace
namespace std {
	template <>
	struct hash<Idempotent> {
		std::size_t operator()(const Idempotent& u) const noexcept {
			return MashFunctions::FastHash64(u.MyKey);
		}
	};
}
namespace std {
	template <>
	struct hash<FSKEffectTicket> {
		std::size_t operator()(const FSKEffectTicket& u) const noexcept {
			return MashFunctions::FastHash64(u.MyKey);
		}
	};
}
namespace std {
	template <>
	struct hash<FSKCueTicket> {
		std::size_t operator()(const FSKCueTicket& u) const noexcept {
			return MashFunctions::FastHash64(u.MyKey);
		}
	};
}

namespace std {
	template <>
	struct hash<FSimpleTriggerGun> {
		std::size_t operator()(const FSimpleTriggerGun& u) const noexcept {
			return u.MyGunKey.GunInstanceID;
		}
	};
}