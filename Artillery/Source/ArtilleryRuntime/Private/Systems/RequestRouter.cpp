#include "RequestRouter.h"

#include "PhysicsTypes/InventoryTriggerProxy.h"

//if we could make a promise about when threads are allocated, we could probably get rid of this
//since the accumulator is in the world subsystem and so gets cleared when the world spins down.
//This is identical to the design found in Barrage, since it ended up working beautifully.
inline thread_local extern int32 MyARTILLERYIndex = ALLOWED_THREADS_FOR_ARTILLERY + 1;

void FRequestRouter::Feed()
{
	FScopeLock GrantFeedLock(&GrowOnlyAccLock);
		
	//TODO: expand if we need for rollback powers. could be sliiiick
	BusyWorkerAcc[ThreadAccTicker] = FeedMap(std::this_thread::get_id(), 4096);
	GameThreadAcc[ThreadAccTicker] = GameFeedMap(std::this_thread::get_id(), 4096);
	AIThreadAcc[ThreadAccTicker] = FeedMap(std::this_thread::get_id(), 4096);
	TLThreadAcc[ThreadAccTicker] = TickliteFeedMap(std::this_thread::get_id(), 4096);
	MyARTILLERYIndex = ThreadAccTicker;
	++ThreadAccTicker;
}

FGrantWith FRequestRouter::NewUnboundGun(FSkeletonKey Self, FGunKey NameSetIDUnset,  FARelatedBy EquippedAs, ArtilleryTime Stamp)
{
	FRequestThing MyRequest(ArtilleryRequestType::GetAnUnboundGun);
	MyRequest.Stamp = Stamp;
	MyRequest.SourceOrSelf = Self;
	MyRequest.Gun = NameSetIDUnset;
	MyRequest.Relationship = EquippedAs;
	BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
	return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Bound | FGrantWith::GameThread);
}

//this requires a registered bonekine/bonekey pair, or a requestor kine and a key and a damn good reason.
FGrantWith FRequestRouter::SceneComponentMoved(FBoneKey ComponentArtilleryKey, ArtilleryTime Stamp, FVector Pos, FRotator Rot)
{
	if (this)
	{
		FRequestThing MyRequest(ArtilleryRequestType::FakeTransformUpdate);
		MyRequest.Stamp = Stamp;
		MyRequest.SourceOrSelf = ComponentArtilleryKey.AsSkeletonKey();
		MyRequest.ThingRotator = Rot;
		MyRequest.ThingVector = Pos;
		BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::CreateTriggerOnVerifiedFrame(AUInventoryTriggerProxy* Basis, ArtilleryTime Stamp)
{
	
	///////////////////
	//build request
	//////////////////
	auto K =  Basis->GetTransform();
	return CreateTriggerOnVerifiedFrame(K, Stamp);
}

FGrantWith FRequestRouter::CreateTriggerOnVerifiedFrame(FTransform Basis, ArtilleryTime Stamp)
{
	FRequestGameThreadThing MyRequest(ArtilleryRequestType::CreateTriggerOnVerifiedTick);
	MyRequest.Stamp = Stamp;
	MyRequest.ThingVector = Basis.GetLocation();
	MyRequest.ThingRotator = Basis.Rotator();
	MyRequest.ThingVector2 = Basis.GetScale3D();
	BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
	return FGrantWith(Stamp).Set(FGrantWith::Eventual);
}

FGrantWith FRequestRouter::MobileAI(FSkeletonKey AIEntity, ArtilleryTime Stamp)
{
	FRequestThing MyRequest(ArtilleryRequestType::BindAI);
	MyRequest.Stamp = Stamp;
	MyRequest.SourceOrSelf = AIEntity;
	AIThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
	return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
}
	
//the following statement must return a non-null element
//GunToFiringFunctionMapping->Find(Request.Gun)->ExecuteIfBound(GunByKey->FindRef(Request.Gun), false);
//in other words, it MUST be bound already as per any other gun you wish to fire. globspeebcormbrad
FGrantWith FRequestRouter::GunFired(FGunKey Target, ArtilleryTime Stamp)
{
	if(this)
	{
		///////////////////
		//build request
		//////////////////
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::FireAGun);
		MyRequest.Stamp = Stamp;
		MyRequest.Gun = Target;
		BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::GameThread);
}

FGrantWith FRequestRouter::GunFiredAtTime(FGunKey Target, ArtilleryTime Stamp)
{
	if(this)
	{
		///////////////////
		//build request
		//////////////////
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::CreateATicklite);
		MyRequest.Stamp = Stamp;
		MyRequest.Gun = Target;
		BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::TagReferenceModel(FSkeletonKey Target, ArtilleryTime Stamp, FConservedTags ValidSharedPtr)
{
	if (this)
	{
		FRequestThing MyRequest(ArtilleryRequestType::TagReferenceModel);
		MyRequest.ConservedTags =  ValidSharedPtr;
		MyRequest.Stamp = Stamp;
		MyRequest.SourceOrSelf = Target;
			
		BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::NoTagReferenceModel(FSkeletonKey Target, ArtilleryTime Stamp)
{
	if (this)
	{
		FRequestThing MyRequest(ArtilleryRequestType::NoTagReferenceModel);
		MyRequest.Stamp = Stamp;
		MyRequest.SourceOrSelf = Target;
			
		BusyWorkerAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

// Particle Systems
FGrantWith FRequestRouter::ParticleSystemActivatedOrDeactivated(FParticleID PID, bool ShouldBeActive, ArtilleryTime Stamp)
{
	if (this)
	{
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::ParticleSystemActivateOrDeactivate);
		// Munging the uint32 we use for a Particle ID into the uint64 we use for a skeleton key should be safe
		MyRequest.SourceOrSelf = PID.ParticleId;
		MyRequest.Stamp = Stamp;
		MyRequest.ActivateIfPossible = ShouldBeActive;
		GameThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::ParticleSystemSpawnedAttached(FName ThingName, const FSkeletonKey& ComponentToAttachTo, const FSkeletonKey& Owner, ArtilleryTime Stamp, bool CreateSceneComponentOnKey)
{
	if (this)
	{
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::SpawnParticleSystemAttached);
		MyRequest.ThingName = ThingName;
		MyRequest.Stamp = Stamp;
		MyRequest.SourceOrSelf = ComponentToAttachTo;
		MyRequest.TargetOrNonSelfAffected = Owner;
		MyRequest.ActivateIfPossible = CreateSceneComponentOnKey;
		GameThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::DeferredTickliteInstantiation(TicklitePrototype* constructed, ArtilleryTime Stamp, TicklitePhase Group)
{
	if (this)
	{
		FTickliteRequest MyRequest(Stamp, ArtilleryRequestType::DeferredTickliteInstantiation, constructed, Group);
		TLThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Nullable | FGrantWith::WideCadence);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::ParticleSystemSpawnAtLocation(FName ThingName, const FVector& Location, const FRotator& Rotation, ArtilleryTime Stamp)
{
	if (this)
	{
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::SpawnParticleSystemAtLocation);
		MyRequest.ThingName = ThingName;
		MyRequest.Stamp = Stamp;
		MyRequest.ThingVector = Location;
		MyRequest.ThingRotator = Rotation;
		GameThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}

FGrantWith FRequestRouter::Bullet(
	FName ThingName,
	FVector Location,
	double Scale,
	FVector StartingVelocity,
	FSkeletonKey NewProjectileKey,
	const FGunKey& Gun,
	ArtilleryTime Stamp,
	Layers::EJoltPhysicsLayer Layer,
	int LifeInTicks,
	bool HasPhysics)
{
	if (this)
	{
		FRequestGameThreadThing MyRequest(ArtilleryRequestType::SpawnInstancedStaticMesh);
		MyRequest.ThingName = ThingName;
		MyRequest.Stamp = Stamp;
		MyRequest.ThingVector = Location;			// Start Location
		MyRequest.ThingVector2.X = Scale;			// Spawn Scale
		MyRequest.ThingVector3 = StartingVelocity;	// Spawn Velocity
		MyRequest.SourceOrSelf = NewProjectileKey;
		MyRequest.Gun = Gun;
		MyRequest.Layer = Layer;
		MyRequest.CanExpire = true;
		MyRequest.TicksDuration = LifeInTicks;
		MyRequest.WithPhysicsIfApplicable = HasPhysics;
		GameThreadAcc[MyARTILLERYIndex].Queue->Enqueue(MyRequest);
		return FGrantWith(Stamp).Set(FGrantWith::Eventual | FGrantWith::Within1Tick);
	}
	return FGrantWith(Stamp).Set(FGrantWith::Nullable);
}
