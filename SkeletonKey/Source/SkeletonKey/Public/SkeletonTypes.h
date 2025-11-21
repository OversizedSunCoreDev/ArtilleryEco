#pragma once

#include "skeletonize.h"
#include "Containers/CircularQueue.h"
#include <thread>

#include "MashFunctions.h"
#include "SkeletonTypes.generated.h"

using BristleTime = long; //this will become uint32. don't bitbash this.
using ArtilleryTime = BristleTime;
typedef uint32_t InputStreamKey;

struct FBoneKey;
struct FConstellationKey;

//OBJECT KEY DOES NOT SKELETONIZE AUTOMATICALLY. OTHER KEY TYPES MUST DO THAT.
USTRUCT(BlueprintType)
struct SKELETONKEY_API FSkeletonKey
{
	GENERATED_BODY()
	friend class ActorKey;
public:
	uint64_t Obj;
	
	explicit FSkeletonKey()
	{
		//THIS WILL REGISTER AS NOT AN OBJECT KEY PER SKELETONIZE (SFIX_NONE)
		Obj = 0x0;
	}
	//you don't really want to use this one.
	explicit FSkeletonKey(const unsigned int rhs) = delete; //I mean it. don't implement this.
	explicit FSkeletonKey(uint64 ObjIn)
	{
		Obj=ObjIn;
	}

	static FSkeletonKey Invalid()
	{
		return FSkeletonKey();
	}

	static bool IsValid(const FSkeletonKey& Other)
	{
		return Other != FSkeletonKey::Invalid();
	}
	bool IsValid() const
	{
		return Obj != FSkeletonKey::Invalid().Obj;
	}
	operator uint64() const {return Obj;};
	
	operator ActorKey() const;
	
	friend uint32 GetTypeHash(const FSkeletonKey& Other)
	{
		//typehash is the devil. if you replace the below with a typehash, may god have mercy on your soul, because UE won't.
		return FMMM::FastHash6432(Other.Obj);
	}
	
	FSkeletonKey& operator=(const FSkeletonKey& rhs) {
		if (this != &rhs) {
			Obj = rhs.Obj;
		}
		return *this;
	}
	
	FSkeletonKey& operator=(const ActorKey& rhs);
	FSkeletonKey& operator=(const FBoneKey& rhs);
	FSkeletonKey& operator=(const FConstellationKey& rhs);
	FSkeletonKey& operator=(const uint64 rhs) {
		Obj = rhs;
		return *this;
	}
	
	inline bool operator<(FSkeletonKey const& rhs) {
		return (Obj < rhs.Obj);
	}

	inline bool operator==(FSkeletonKey const& rhs) {
		return (Obj == rhs.Obj);
	}

	inline bool operator!=(FSkeletonKey const& rhs) {
		return (Obj != rhs.Obj);
	}
	//This effectively embeds a "parent" in the metadata bits of a key, which has 2 effects.
	//one, repeated primary keys with differing parent keys will work correctly.
	//two, some key types may be notched by default, which would allow you to extract the meta value and use it directly as a key
	//this gives us a limited hierarchical mechanism, but it's actually used mostly to avoid needing reverse lookups. after all,
	//the parent key could be any key you needed to store, I suppose. if we see a lot of that, I'll rename the param.
	static FSkeletonKey GenerateDependentKey(uint64_t parent, uint32_t localunique, uint64_t type = SKELLY::SFIX_ART_FACT)
	{
		auto ret = parent & SKELLY::SFIX_KEYTOMETA;
		ret = (ret << 32) | localunique;
		ret = FORGE_SKELETON_KEY(ret, type); // I forgot this and lost rather a lot of time.
		return FSkeletonKey(ret);
	};
};

template<>
struct std::hash<FSkeletonKey>
{
	std::size_t operator()(const FSkeletonKey& other) const noexcept
	{
		return FMMM::FastHash64(other.Obj);
	}
};

class SKELETONKEY_API ActorKey
{
	friend struct FSkeletonKey;
public:
	uint64_t Obj;
	explicit ActorKey()
	{
		//THIS FAILS THE OBJECT CHECK AND THE ACTOR CHECK. THIS IS INTENDED. THIS IS THE PURPOSE OF SKELETON KEY.
		Obj=0;
	}
	
	explicit ActorKey(const unsigned int rhs) {
		Obj = rhs;
		Obj <<= 32;
		//this doesn't seem like it should work, but because the SFIX bit patterns are intentionally asym
		//we actually do reclaim a bit of randomness.
		Obj += rhs; 
		Obj = FORGE_SKELETON_KEY(Obj, SKELLY::SFIX_ART_ACTS);
	}
	
	explicit ActorKey(uint64 ObjIn)
	{
		Obj=FORGE_SKELETON_KEY(ObjIn, SKELLY::SFIX_ART_ACTS);
	}
	
	operator uint64() const {return Obj;};
	operator FSkeletonKey() const {return FSkeletonKey(Obj);};
	
	friend uint32 GetTypeHash(const ActorKey& Other)
	{
		//it looks like get type hash can be a 64bit return? 
		return FMMM::FastHash6432(Other.Obj);
	}
	
	ActorKey& operator=(const uint64 rhs) {
		//should be idempotent.
		Obj = FORGE_SKELETON_KEY(rhs, SKELLY::SFIX_ART_ACTS);
		return *this;
	}
	
	ActorKey& operator=(const uint32 rhs) {
		//should be idempotent.
		Obj = rhs;
		Obj <<= 32;
		//this doesn't seem like it should work, but because the SFIX bit patterns are intentionally asym
		//we actually do reclaim a bit of randomness.
		Obj |= rhs; 
		Obj = FORGE_SKELETON_KEY(Obj, SKELLY::SFIX_ART_ACTS);
		return *this;
	}
	
	ActorKey& operator=(const ActorKey& rhs) {
		//should be idempotent.
		if (this != &rhs) {
			Obj = FORGE_SKELETON_KEY(rhs.Obj, SKELLY::SFIX_ART_ACTS);
		}
		return *this;
	}
	
	ActorKey& operator=(const FSkeletonKey& rhs)
	{
		//should be idempotent.
		if((BoneKey_Infix & rhs.Obj) == BoneKey_Infix)
		{
			UE_LOG(LogTemp, Warning, TEXT("%llu is a bonekey and converting it to an actor key is not a legal operation."), rhs.Obj);
			Obj = 0;
		} else if((GunInstance_Infix & rhs.Obj) == GunInstance_Infix)
		{
			UE_LOG(LogTemp, Warning, TEXT("%llu is a GunInstance and converting it to an actor key is not a legal operation."), rhs.Obj);
			Obj = 0;
		}
		else
		{
			Obj = FORGE_SKELETON_KEY(rhs.Obj, SKELLY::SFIX_ART_ACTS);
		}
		return *this;
	}

	// you cannot cross the typetree here. if you wish to do this, you must explicitly discard by going up to skeletonkey
	// and down to bonekey. The reverse is not permitted at all, as bonekey offers fewer guarantees than actorkey.
	ActorKey& operator=(const FBoneKey& rhs) = delete;

	static ActorKey Invalid() { return ActorKey(); }
};
static bool operator<(ActorKey const& lhs, FSkeletonKey const& rhs) {
	return (lhs.Obj < rhs.Obj);
}

inline FSkeletonKey::operator ActorKey() const { return ActorKey(Obj); }

//FOR LEGACY REASONS, this applies the skeletonization.
inline FSkeletonKey& FSkeletonKey::operator=(const ActorKey& rhs)
{
	Obj = FORGE_SKELETON_KEY(rhs.Obj, SKELLY::SFIX_ART_ACTS);
	return *this;
}

struct TransformUpdate
{
	FSkeletonKey ObjectKey;
	uint64 sequence;
	FQuat4f Rotation;// this alignment looks wrong. Like outright wrong.
	FVector3f Position;
	uint32 speed;// unused at the moment, here to support smearing if needed.
};

template<class FeedType>
struct FeedMap
{
	using ThreadFeed = TCircularQueue<FeedType>;
	std::thread::id That = std::thread::id();
	TSharedPtr<ThreadFeed, ESPMode::ThreadSafe> Queue = nullptr;

	FeedMap()
	{
		That = std::thread::id();
		Queue = nullptr;
	}

	FeedMap(std::thread::id MappedThread, uint16 MaxQueueDepth)
	{
		That = MappedThread;
		Queue = MakeShareable(new ThreadFeed(MaxQueueDepth));
	}
};

using FBOutputFeed = FeedMap<TransformUpdate>;

//Engine\Source\Runtime\CoreUObject\Public\UObject\FObjectKey is the vanilla UE equivalent.
//Unfortunately, it's pretty heavy, since it's intended to operate in more circumstances. It requires a template arg
//to operate, which makes it ill-suited to the truly loose coupling we're after here.



//unfortunately, we set the tradition with actor key, so bonekey self-skeletonizes.
//almost anything can be converted into a bonekey, but many things won't be valid.
//Bonekeys represent actor components that have an operable transform. In particular,
//we use them for firing points, gun models, turrets, particle system attach points,
//the sort of stuff you might normally socket. But using sockets from BP is pretty annoying,
//and you don't really want to figure out which socket a gun should go in or could go in.
//so instead, with bonekeys, you can use the KeyAttributes to describe relationships
//in a hierarchy free and instance uniqued way. Sockets are still really useful,
//and eventually, bonekeys will provide a fuller integration with them.
USTRUCT(BlueprintType)
struct SKELETONKEY_API FBoneKey
{
	GENERATED_BODY()
	friend class ActorKey;
public:
	uint64_t Obj;
	
	explicit FBoneKey()
	{
		//THIS WILL REGISTER AS NOT AN OBJECT KEY PER SKELETONIZE (SFIX_NONE)
		Obj = 0;
	}
	
	explicit FBoneKey(uint64 ObjIn)
	{
		Obj = FORGE_SKELETON_KEY(ObjIn, BoneKey_Infix);
	}

	FSkeletonKey AsSkeletonKey() const
	{
		return FSkeletonKey(this->Obj);
	}

	static FBoneKey Invalid()
	{
		return FBoneKey(0);
	}
	
	operator uint64() const {return Obj;};
	
	explicit operator ActorKey() const = delete;
	
	friend uint32 GetTypeHash(const FBoneKey& Other)
	{
		return FMMM::FastHash6432(Other.Obj);
	}
	
	FBoneKey& operator=(const FBoneKey& rhs) {
		if (this != &rhs) {
			Obj = FORGE_SKELETON_KEY(rhs.Obj, BoneKey_Infix);
		}
		return *this;
	}
	// you cannot cross the typetree here. if you wish to do this, you must explicitly discard by going up to skeletonkey
	// and down to bonekey. The reverse is not permitted at all, as bonekey offers fewer guarantees than actorkey.
	FBoneKey& operator=(const ActorKey& rhs) = delete;

	FBoneKey& operator=(const uint64 rhs) {
		Obj = FORGE_SKELETON_KEY(rhs, BoneKey_Infix);
		return *this;
	}
};

//YOU CANNOT ESCAPE YOUR BONINESS.
inline FSkeletonKey& FSkeletonKey::operator=(const FBoneKey& rhs)
{
	Obj = FORGE_SKELETON_KEY(rhs.Obj, BoneKey_Infix);
	return *this;
}

//Constellations represent entities of entities, and are a fully privileged skeleton key associated with a
//relationship attribute map. This is used for describing the current state of a player, for example.
USTRUCT(BlueprintType)
struct SKELETONKEY_API FConstellationKey
{
	GENERATED_BODY()
public:
	uint64_t Obj;
	
	explicit FConstellationKey()
	{
		//THIS WILL REGISTER AS NOT AN OBJECT KEY PER SKELETONIZE (SFIX_NONE)
		Obj = 0;
	}
	
	explicit FConstellationKey(uint64 ObjIn)
	{
		Obj = FORGE_SKELETON_KEY(ObjIn, SKELLY::SFIX_STELLAR);
	}

	static FSkeletonKey Invalid()
	{
		return FSkeletonKey();
	}

	bool IsValid(const FSkeletonKey& Other) const
	{
		return Other != FSkeletonKey::Invalid();
	}
	
	operator uint64() const {return Obj;};
	
	friend uint32 GetTypeHash(const FConstellationKey& Other)
	{
		return FMMM::FastHash6432(Other.Obj);
	}
	
	FConstellationKey& operator=(const FConstellationKey& rhs) {
		if (this != &rhs) {
			Obj = FORGE_SKELETON_KEY(rhs.Obj, SKELLY::SFIX_STELLAR);
		}
		return *this;
	}
	
	// you cannot cross the typetree here. if you wish to do this, you must explicitly discard by going up to skeletonkey
	// and down to bonekey. The reverse is not permitted at all, as bonekey offers fewer guarantees than actorkey.
	FConstellationKey& operator=(const ActorKey& rhs) = delete;

	FConstellationKey& operator=(const uint64 rhs) {
		Obj = FORGE_SKELETON_KEY(rhs, SKELLY::SFIX_STELLAR);
		return *this;
	}
};

//YOU CANNOT ESCAPE YOUR BONINESS.
inline FSkeletonKey& FSkeletonKey::operator=(const FConstellationKey& rhs)
{
	Obj = FORGE_SKELETON_KEY(rhs.Obj, SKELLY::SFIX_STELLAR);
	return *this;
}

using TransformUpdatesForGameThread = TCircularQueue<TransformUpdate>;

typedef TArray<ActorKey> ActorKeyArray;

//I didn't have time to completely replace the machinery around the old FGunKeys but they now encapsulate one of these
//nice little fellows, meaning that 0 is now actually meaningfully a null value.
//we should probably generalize this to hold all of the really persnickety keys (bullet, particle, gun instance)
//but not right now ty lol
USTRUCT(BlueprintType)
struct SKELETONKEY_API FGunInstanceKey
{
	GENERATED_BODY()
	friend struct FSkeletonKey;
public:
	uint64_t Obj;
	
	explicit FGunInstanceKey()
	{
		//THIS FAILS THE OBJECT CHECK AND THE ACTOR CHECK. THIS IS INTENDED. THIS IS THE PURPOSE OF SKELETON KEY.
		Obj=0;
	}
	
	explicit FGunInstanceKey(const unsigned int rhs) {
		Obj = 0;
		Obj |= FMMM::FastHash32(rhs); //en-bloody-sure that's hashed
		Obj &= SKELLY::SFIX_KEYTOMETA; //notch the key. a true hash can lose any bit without losing validity.
		Obj = FORGE_SKELETON_KEY(Obj, GunInstance_Infix);
	}
	
	explicit FGunInstanceKey(FSkeletonKey ObjIn)
	{
		if((GunInstance_Infix & ObjIn) == GunInstance_Infix)
		{
			Obj=FORGE_SKELETON_KEY(ObjIn.Obj, GunInstance_Infix);
		}
		else
		{

			//UE_LOG(LogTemp, Error, TEXT("%llu is NOT GunInstance and converting it to a gun instance key is not a legal operation."), ObjIn.Obj);
			Obj = 0;
		}
	}
	
	operator uint64() const { return Obj; }
	operator FSkeletonKey() const { return FSkeletonKey(Obj); }
	
	friend uint32 GetTypeHash(const FGunInstanceKey& Other)
	{
		//it looks like get type hash can be a 64bit return? 
		return FMMM::FastHash6432(Other.Obj);
	}
	
	FGunInstanceKey& operator=(const uint64 rhs) {
		//should be idempotent.
		Obj = FORGE_SKELETON_KEY(rhs, GunInstance_Infix);
		return *this;
	}
	
	FGunInstanceKey& operator=(ActorKey rhs) = delete;

	//GunInstanceKeys are pretty picky.
	FGunInstanceKey& operator=(const FSkeletonKey& rhs)
	{
		if((GunInstance_Infix & rhs.Obj) == GunInstance_Infix)
		{
			Obj = rhs.Obj;
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("%llu is not GunInstance and converting it to one is not a legal operation."), rhs.Obj);
			Obj = 0;
		}
		return *this;
	}
};

//when sorted, gunkeys MOSTLY follow their instantiation order. Mostly. (the bit mask causes some weirdness)
static bool operator<(FGunInstanceKey const& lhs, FGunInstanceKey const& rhs) {
	return (lhs.Obj < rhs.Obj);
}


struct SKELETONKEY_API FProjectileInstanceKey
{
	friend struct FSkeletonKey;
public:
	uint64_t Obj;
	
	explicit FProjectileInstanceKey()
	{
		//THIS FAILS THE OBJECT CHECK AND THE ACTOR CHECK. THIS IS INTENDED. THIS IS THE PURPOSE OF SKELETON KEY.
		Obj=0;
	}
	
	explicit FProjectileInstanceKey(unsigned int rhs) {
		Obj = rhs;
		Obj <<= 32;
		//this doesn't seem like it should work, but because the SFIX bit patterns are intentionally asym
		//we actually do reclaim a bit of randomness.
		Obj += rhs; 
		Obj = FORGE_SKELETON_KEY(Obj, SKELLY::SFIX_GUN_SHOT);
	}

	explicit FProjectileInstanceKey(uint64 rhs) {
		Obj = FORGE_SKELETON_KEY(rhs, SKELLY::SFIX_GUN_SHOT);
	}
	
	operator uint64() const { return Obj; }
	operator FSkeletonKey() const { return FSkeletonKey(Obj); }
	
	FProjectileInstanceKey& operator=(const uint64 rhs) {
		//should be idempotent.
		Obj = FORGE_SKELETON_KEY(rhs, SKELLY::SFIX_GUN_SHOT);
		return *this;
	}
	
	FGunInstanceKey& operator=(ActorKey rhs) = delete;

	//bullets are real picky.
	FProjectileInstanceKey& operator=(const FSkeletonKey& rhs) = delete;
};


USTRUCT(BlueprintType)
struct SKELETONKEY_API FLordKey //FLord.
{
	GENERATED_BODY()
public:
	uint64_t Obj;
	uint32_t WorldID;

	//generally, these are used to uniquely identify subsystems, but you can also use them to uniquely identify world-bound
	//objects for ease of use or for clarity. A good example might be the concept of the local player, though we don't currently
	//use them for that. Another good example might be the set of stats and records that we expect to persist for world outcomes.
	explicit FLordKey(unsigned int WorldHash, unsigned int HashOfWorldConstructPointer) 
	{
		Obj = WorldHash;
		WorldID = WorldHash;
		Obj <<= 32;
		//this doesn't seem like it should work, but because the SFIX bit patterns are intentionally asym
		//we actually do reclaim a bit of randomness.
		Obj |= HashOfWorldConstructPointer; 
		Obj = FORGE_SKELETON_KEY(Obj, SKELLY::SFIX_SK_LORD);
	}
	
	explicit FLordKey(): WorldID(0)
	{
		//THIS FAILS THE OBJECT CHECK AND THE ACTOR CHECK. THIS IS INTENDED. THIS IS THE PURPOSE OF SKELETON KEY.
		Obj = 0;
	}

	friend uint32 GetTypeHash(const FLordKey& Other)
	{
		return FMMM::FastHash6432(Other.Obj);
	}

	//Lordly Keys are super picky and can really only be assigned like this.
	FLordKey& operator=(const FLordKey& rhs)
	{
		Obj = rhs.Obj;
		return *this;
	}
};