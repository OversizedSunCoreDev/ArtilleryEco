#pragma once

#include "CoreMinimal.h"
#include "EAttributes.h"
#include "EPhysicsLayer.h"
#include "GameFramework/Actor.h"
#include "FGunKey.h"
#include "MashFunctions.h"
#include "RequestRouterTypes.h"
#include "NiagaraParticleDispatch.h"

//In general, you will not need to use this class directly.
//For some requests, or if you are building a Harvester Ticklite (see: https://github.com/JKurzer/sd/blob/main/README.md )
//It may be necessary. It is slower, bulkier, less immediate, and more annoying than the processes you are accustomed to,
//but for certain things that touch the game thread, it may be the best option. New stuff like that should get added here.
//it is also made available to autoguns as a way of shimming out some pretty nasty concurrency problems.

constexpr int ALLOWED_THREADS_FOR_ARTILLERY = 64;

//these functions all create and enqueue a thing request and can be called from any thread that has called Feed().
//When F_INeedA is set up, these requests will go into the Threaded Accumulator (the BusyWorkerAcc machinery)...
//Then some thread will process the requests, likely the busy worker, doing the actual resolution.
//this is why the GrantedWith results have conditions that can be specified to let you know when or if the skeleton key is usable.
//Until the key is usable, it should not be separated from the GrantedWith structure or copied.
//I haven't spent a lot of time thinking about if the skeletonkey should get set later or if we should allocate a skeletonkey
//THEN try to build everything it represents, in this model.
//That would require an indirection mapping, which I find highly highly distasteful, but on the other hand, we're in
//extremely deep waters. On the grasping hand, everything we do to improve the UX NOW is a hundred bugs a year we don't have
//to _______________ ________ until ____ __ _________ and fix later.
// Find in files for YAGAMETHREADBOYRUNNETHREQUESTSHERE will show you the code in artillery dispatch that processes gamethread
// and YABUSYTHREADBOYRUNNETHREQUESTSHERE will show you the equivalent code in our mutual friend artillery busy worker.
class ARTILLERYRUNTIME_API F_INeedA //Frick
{
public:
	using GameThreadRequestQ = TCircularQueue<FRequestGameThreadThing>;
	using ThreadFeed = TCircularQueue<FRequestThing>;
	
	static uint32 HashDownTo32(uint64 inValue)
	{
		return FMMM::FastHash6432( FMMM::FastHash64(inValue));
	}
	
	F_INeedA()
	{
	}
	
	struct FeedMap
	{
		std::thread::id That = std::thread::id();
		TSharedPtr<ThreadFeed> Queue = nullptr;

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
	
	struct GameFeedMap
	{
		std::thread::id That = std::thread::id();
		TSharedPtr<GameThreadRequestQ> Queue = nullptr;

		GameFeedMap()
		{
			That = std::thread::id();
			Queue = nullptr;
		}

		GameFeedMap(std::thread::id MappedThread, uint16 MaxQueueDepth)
		{
			That = MappedThread;
			Queue = MakeShareable(new GameThreadRequestQ(MaxQueueDepth));
		}
	};

	FeedMap BusyWorkerAcc[ALLOWED_THREADS_FOR_ARTILLERY];
	GameFeedMap GameThreadAcc[ALLOWED_THREADS_FOR_ARTILLERY];
	FeedMap AIThreadAcc[ALLOWED_THREADS_FOR_ARTILLERY];
	int32 ThreadAccTicker = 0;
	mutable FCriticalSection GrowOnlyAccLock;
	
	void Feed();

	// Gun Handling
	// This requests a new gun by name which will then be bound by attribute key to skeletonkey provided.
	// When your get attrib for the relationship type returns something usable, it can be used.
	// TODO: Get this off the fucking main thread or we'll never be deterministic.
	FGrantWith NewUnboundGun(FSkeletonKey Self, FGunKey NameSetIDUnset,  FARelatedBy EquippedAs, ArtilleryTime Stamp);
	FGrantWith SceneComponentMoved(FBoneKey ComponentArtilleryKey, ArtilleryTime Stamp, FVector Pos, FRotator Rot);

	FGrantWith NewAutoGun()
	{
		throw; //not implemented yet
	};

	FGrantWith Harvester()
	{
		throw; //not implemented yet
	};

	FGrantWith MobileAI(FSkeletonKey AIEntity, ArtilleryTime Stamp);
	
	//the following statement must return a non-null element
	//GunToFiringFunctionMapping->Find(Request.Gun)->ExecuteIfBound(GunByKey->FindRef(Request.Gun), false);
	//in other words, it MUST be bound already as per any other gun you wish to fire. globspeebcormbrad
	FGrantWith GunFired(FGunKey Target, ArtilleryTime Stamp);
	
	//this will just create a lil ticklite that has the number of ticks as its duration, and fires the gun on expire
	FGrantWith GunFiredAtTime(FGunKey Target, ArtilleryTime Stamp);

	//this will just create a lil ticklite that has the number of ticks as its duration, and fires the gun on expire
	FGrantWith GunFiredFromATicklite(FRequestGameThreadThing FireMeElmo)
	{
		throw; //not implemented yet
	};

	FGrantWith TagReferenceModel(FSkeletonKey Target, ArtilleryTime Stamp, FConservedTags ValidSharedPtr);

	FGrantWith NoTagReferenceModel(FSkeletonKey Target, ArtilleryTime Stamp);

	FGrantWith GunFiredWhenATagGetsAdded()
	{
		throw; //not implemented yet
	}
	
	FGrantWith GunFiredWhenATagExpires()
	{
		throw; //not implemented yet
	}
	
	bool AutoGunTurnedOff(FGunKey Target)
	{
		throw;
	}
	
	bool AutoGunTurnedOn(FGunKey Target)
	{
		throw; //not implemented yet
	}

	// Particle Systems
	FGrantWith ParticleSystemActivatedOrDeactivated(FParticleID PID, bool ShouldBeActive, ArtilleryTime Stamp);
	
	FGrantWith ParticleSystemSpawnedAttached(
		FName ThingName,
		const FSkeletonKey& ComponentToAttachTo,
		const FSkeletonKey& Owner,
		ArtilleryTime Stamp,
		bool CreateSceneComponentOnKey = false);

	FGrantWith ParticleSystemSpawnAtLocation(FName ThingName, const FVector& Location, const FRotator& Rotation, ArtilleryTime Stamp);

	// Mesh Creation AND Barrage object creation. Later, this will be split for better sim rollback and faster sim responsivity.
	// The game thread should pretty much never directly be responsible for creating barrage objects, its timing is too variable.
	// -1 ticks is default
	FGrantWith Bullet(
		FName ThingName,
		FVector Location,
		double Scale,
		FVector StartingVelocity,
		FSkeletonKey NewProjectileKey,
		const FGunKey& Gun,
		ArtilleryTime Stamp,
		Layers::EJoltPhysicsLayer Layer = Layers::PROJECTILE,
		int LifeInTicks = -1,
		bool HasExpiration = true);
};


