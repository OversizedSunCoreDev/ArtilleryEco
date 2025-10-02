// ReSharper disable CppMemberFunctionMayBeConst
#pragma once

#include "SkeletonTypes.h"

#include "CoreMinimal.h"
#include "BarrageContactEvent.h"
#include "Subsystems/WorldSubsystem.h"
#include "FBarrageKey.h"
#include "Chaos/Particles.h"
#include "CapsuleTypes.h"
#include "FBarragePrimitive.h"
#include "FBPhysicsInput.h"
#include "Containers/CircularQueue.h"
#include "FBShapeParams.h"
#include "KeyedConcept.h"
#include "ORDIN.h"
#include "TransformDispatch.h"
#include "BarrageDispatch.generated.h"

#ifndef HERTZ_OF_BARRAGE
#define HERTZ_OF_BARRAGE 128
#endif

static constexpr uint32 MAX_FOUND_OBJECTS = 1024;

class BARRAGE_API FBarrageBounder
{
	friend class FBBoxParams;
	friend class FBSphereParams;
	friend class FBCapParams;
	//convert from UE to Jolt without exposing the jolt types or coordinates.
public:
	static FBBoxParams GenerateBoxBounds(
		const FVector3d& point,
		double xDiam, double yDiam, double zDiam,
		const FVector3d& OffsetCenterToMatchBoundedShape = FVector::Zero(),
		FMassByCategory::BMassCategories MyMassClass = FMassByCategory::BMassCategories::MostEnemies);
	static FBSphereParams GenerateSphereBounds(const FVector3d& point, double radius);
	static FBCapParams GenerateCapsuleBounds(const UE::Geometry::FCapsule3d& Capsule);
	static FBCharParams GenerateCharacterBounds(const FVector3d& point, double radius, double extent, double speed);
};

struct BarrageContactEvent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBarrageContactAdded, const BarrageContactEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBarrageContactPersisted, const BarrageContactEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBarrageContactRemoved, const BarrageContactEvent&);
constexpr int ALLOWED_THREADS_FOR_BARRAGE_PHYSICS = 64;
//if we could make a promise about when threads are allocated, we could probably get rid of this
//since the accumulator is in the world subsystem and so gets cleared when the world spins down.
//that would mean that we could add all the threads, then copy the state from the volatile array to a
//fixed read-only hash table during begin play. this is already complicated though, and let's see if we like it
//before we invest more time in it. I had a migraine when I wrote this, by way of explanation --J

	//hi, static actually is per TLU. this has... exotic behavior in our use cases! 
	inline thread_local extern  int32 MyBARRAGEIndex = ALLOWED_THREADS_FOR_BARRAGE_PHYSICS + 1;

	inline thread_local extern  int32 MyWORKERIndex = ALLOWED_THREADS_FOR_BARRAGE_PHYSICS + 1;



UCLASS()
class BARRAGE_API UBarrageDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
	
	friend class FWorldSimOwner;
	friend class UArtilleryLibrary;

public:
	//minimize use of this outside of artillery blueprint library (UArtilleryLibrary)
	static inline UBarrageDispatch* SelfPtr = nullptr;
	constexpr static int OrdinateSeqKey = ORDIN::LastSubstrateKey;
	virtual bool RegistrationImplementation() override;
	void GrantWorkerFeed(int MyThreadIndex);
	static constexpr float TickRateInDelta = 1.0 / HERTZ_OF_BARRAGE;
	int32 ThreadAccTicker = 0;
	TSharedPtr<TransformUpdatesForGameThread> GameTransformPump;
	TSharedPtr<TCircularQueue<BarrageContactEvent>> ContactEventPump;
	 //this value indicates you have none.
	mutable FCriticalSection GrowOnlyAccLock;
	int32 WorkerThreadAccTicker = 0;
	mutable FCriticalSection MultiAccLock;

	// Why would I do it this way? It's fast and easy to debug, and we will probably need to force a thread
	// order for determinism. this ensures there's a call point where we can institute that.
	void GrantClientFeed();
	UBarrageDispatch();
	
	virtual ~UBarrageDispatch() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	
	virtual void SphereCast(double Radius, double Distance, FVector3d CastFrom, FVector3d Direction, TSharedPtr<FHitResult> OutHit, const JPH::BroadPhaseLayerFilter& BroadPhaseFilter, const JPH::ObjectLayerFilter& ObjectFilter, const JPH::BodyFilter& BodiesFilter, uint64_t timestamp = 0);
	virtual void SphereSearch(FBarrageKey ShapeSource, FVector3d Location, double Radius, const JPH::BroadPhaseLayerFilter& BroadPhaseFilter, const JPH::ObjectLayerFilter& ObjectFilter, const JPH::BodyFilter& BodiesFilter, uint32* OutFoundObjectCount, TArray<uint32>& OutFoundObjects);

	virtual void CastRay(FVector3d CastFrom, FVector3d Direction, const JPH::BroadPhaseLayerFilter& BroadPhaseFilter, const JPH::ObjectLayerFilter& ObjectFilter, const JPH::BodyFilter& BodiesFilter, TSharedPtr<FHitResult> OutHit);
	
	//and viola [sic] actually pretty elegant even without type polymorphism by using overloading polymorphism.
	FBLet CreatePrimitive(FBBoxParams& Definition, FSkeletonKey Outkey, uint16 Layer, bool IsSensor = false, bool forceDynamic = false, bool isMovable = true);
	FBLet CreatePrimitive(FBCharParams& Definition, FSkeletonKey Outkey, uint16 Layer);
	FBLet CreatePrimitive(FBSphereParams& Definition, FSkeletonKey OutKey, uint16 Layer, bool IsSensor = false);
	FBLet CreatePrimitive(FBCapParams& Definition, FSkeletonKey OutKey, uint16 Layer, bool IsSensor = false, FMassByCategory::BMassCategories MassClass = FMassByCategory::MostEnemies);
	FBLet CreateProjectile(FBBoxParams& Definition, FSkeletonKey OutKey, uint16_t Layer);
	FBLet LoadComplexStaticMesh(FBTransform& MeshTransform, const UStaticMeshComponent* StaticMeshComponent, FSkeletonKey OutKey, bool IsSensor = false);
	FBLet LoadEnemyHitboxFromStaticMesh(FBTransform& MeshTransform, const UStaticMeshComponent* StaticMeshComponent, FSkeletonKey OutKey, bool IsSensor = false, bool UseRawMeshForCollision = false);
	FBLet GetShapeRef(FBarrageKey Existing) const;
	FBLet GetShapeRef(FSkeletonKey Existing) const;
	void FinalizeReleasePrimitive(FBarrageKey BarrageKey);

	//any non-zero value is the same, effectively, as a nullity for the purposes of any new operation.
	//because we can't control certain aspects of timing and because we may need to roll back, we use tombstoning
	//instead of just reference counting and deleting - this is because cases arise where there MUST be an authoritative
	//single source answer to the alive/dead question for a rigid body, but we still want all the advantages of ref counting
	//and we want to be able to revert that decision for faster rollbacks or for pooling purposes.
	constexpr static uint32 TombstoneInitialMinimum = 9;

	//don't const stuff that causes huge side effects.
	uint32 SuggestTombstone(FBLet Target) 
	{
		if (FBarragePrimitive::IsNotNull(Target))
		{
			Target->tombstone = TombstoneInitialMinimum + TombOffset;
			return Target->tombstone;
		}
		return 1;
		// one indicates that something has no remaining time to live, and is equivalent to finding a nullptr. we return if for tombstone suggestions against tombstoned or null data.
	}

	virtual TStatId GetStatId() const override;
	TSharedPtr<FWorldSimOwner> JoltGameSim;

	//StackUp should be called before StepWorld and from the same thread. anything can be done between them.
	//Returns rather than applies the FBPhysicsInputs that affect Primitives of Types: Character
	//This list may expand. Failure to handle these will result in catastrophic bugs.
	void StackUp();
	bool UpdateCharacters(TSharedPtr<TArray<FBPhysicsInput>> CharacterInputs) const;
	bool UpdateCharacter(FBPhysicsInput& CharacterInput) const;
	
	//ONLY call this from a thread OTHER than gamethread, or you will experience untold sorrow.
	void StepWorld(uint64 Time, uint64_t TickCount);

	//TODO: oh dear I'm doing the same thing as the TransformQueue... Also probably want to check back on this.
	bool BroadcastContactEvents() const;
	
	FOnBarrageContactAdded OnBarrageContactAddedDelegate;
	void HandleContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                        JPH::ContactSettings& ioSettings);
	void HandleContactAdded(BarrageContactEntity Ent1, BarrageContactEntity Ent2);
	void HandleContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::ContactSettings& ioSettings);
	FOnBarrageContactPersisted OnBarrageContactPersistedDelegate;
	void HandleContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
	                            JPH::ContactSettings& ioSettings);
	// REMOVE EVENTS REQUIRE ADDITIONAL SPECIAL HANDLING AS THEY DO NOT HAVE ALL DATA SET
	FOnBarrageContactRemoved OnBarrageContactRemovedDelegate;
	// REMOVE EVENTS REQUIRE ADDITIONAL SPECIAL HANDLING AS THEY DO NOT HAVE ALL DATA SET
	void HandleContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) const;

	FBarrageKey GenerateBarrageKeyFromBodyId(const JPH::BodyID& Input) const;
	FBarrageKey GenerateBarrageKeyFromBodyId(const uint32 RawIndexAndSequenceNumberInput) const;

	FBarrageKey GetBarrageKeyFromFHitResult(TSharedPtr<FHitResult> HitResult) const
	{
		check(HitResult.IsValid());
		return HitResult.Get()->MyItem != JPH::BodyID::cInvalidBodyID ? GenerateBarrageKeyFromBodyId(static_cast<uint32>(HitResult.Get()->MyItem)) : 0;
	}

	JPH::DefaultBroadPhaseLayerFilter GetDefaultBroadPhaseLayerFilter(JPH::ObjectLayer inLayer) const;
	JPH::DefaultObjectLayerFilter GetDefaultLayerFilter(JPH::ObjectLayer inLayer) const;

	static JPH::SpecifiedObjectLayerFilter GetFilterForSpecificObjectLayerOnly(JPH::ObjectLayer inLayer);
	
	JPH::IgnoreSingleBodyFilter GetFilterToIgnoreSingleBody(FBarrageKey ObjectKey) const;
	JPH::IgnoreSingleBodyFilter GetFilterToIgnoreSingleBody(const FBLet& ToIgnore) const;
	
private:
	TSharedPtr<KeyToFBLet> JoltBodyLifecycleMapping;
	TSharedPtr<KeyToKey> TranslationMapping;
	FBLet ManagePointers(FSkeletonKey OutKey, FBarrageKey temp, FBShape form) const;
	uint32 TombOffset = 0; //ticks up by one every world step.
	//this is a little hard to explain. so keys are inserted as 

	//clean tombs must only ever be called from step world which must only ever be called from one thread.
	//this reserves as little memory as possible, but it could be quite a lot (megs) of reserved memory if you expire
	//tons and tons of bodies in one frame. if that's a bottleneck for you, you may wish to shorten the tombstone promise
	//or optimize this for memory better. In general, Barrage and Artillery trade memory for speed and elegance.
	TSharedPtr<TArray<FBLet>> Tombs[TombstoneInitialMinimum + 1];

	void CleanTombs()
	{
		//free tomb at offset - TombstoneInitialMinimum, fulfilling our promised minimum.
		TSharedPtr<TArray<FBLet>>* HoldOpen = Tombs;
		TSharedPtr<TArray<FBLet>> Mausoleum = HoldOpen[(TombOffset) % (TombstoneInitialMinimum + 1)]; //think this math is wrong.
		TSharedPtr<KeyToFBLet> HoldOpenBMap = JoltBodyLifecycleMapping;
		TSharedPtr<KeyToKey> HoldOpenTMap = TranslationMapping;
		if(Mausoleum && !Mausoleum->IsEmpty() && HoldOpenBMap && HoldOpenTMap)
		{
			for (auto Tombstone : *Mausoleum)
			{
				if (Tombstone)
				{
					JoltBodyLifecycleMapping->erase(Tombstone->KeyIntoBarrage);
					TranslationMapping->erase(Tombstone->KeyOutOfBarrage);
				}
			}
		}
		
		Mausoleum = HoldOpen[(TombOffset - TombstoneInitialMinimum) % (TombstoneInitialMinimum + 1)];
		if (Mausoleum)
		{
			Mausoleum->Empty(); //roast 'em lmao.
		}
		TombOffset = (TombOffset + 1) % (TombstoneInitialMinimum + 1);
	}

private:
	std::array<FBPhysicsInput, 32000> InternalSortableSet = {};
	std::array<JPH::BodyID, 8192> Adds;
};
