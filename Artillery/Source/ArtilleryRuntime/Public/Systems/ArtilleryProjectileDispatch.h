﻿// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AInstancedMeshManager.h"
#include "FProjectileDefinitionRow.h"
//look, it's important that you wrap both your typedefs and your lib include in these, and that the lib include always be explicit.
//lbc is a header only lib. this has some pretty stark implications. we probably need to move ALL type defs and ALL
//includes into a Lbc module, isolate them, and compile them.
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include "LibCuckoo/cuckoohash_map.hh"
typedef libcuckoo::cuckoohash_map<FSkeletonKey, TWeakObjectPtr<AInstancedMeshManager>> KeyToItemCuckooMap;
typedef libcuckoo::cuckoohash_map<FSkeletonKey, FGunKey> KeyToGunMap;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END
#include "ArtilleryProjectileDispatch.generated.h"

class UArtilleryDispatch;

/**
 * This is the Artillery subsystem that manages the lifecycle of projectiles using only SkeletonKeys rather than UE5
 * actors. This is done by instancing projectiles through AInstancedMeshManager actors rather than creating an actor
 * per projectile which greatly reduces computational load as all projectiles with the same model can be managed by
 * Artillery + TickLites rather than through the heavy and expensive UE5 Actor system.
 *
 * This does mean that a lot of UE5 default functionality associated with Actors won't and don't work with Artillery
 * Projectiles, but this is intended. Artillery Projectiles should be managed through this subsystem and through the
 * ticklites that are assigned to them (also through this subsystem). This subsystem is responsible for basic default
 * behavior of projectiles that Artillery supports, but additional behavior can be added to Artillery Projectiles by
 * way of attaching custom TickLites to them.
 *
 */
namespace Arty
{
	DECLARE_MULTICAST_DELEGATE(OnArtilleryProjectilesActivated);
}

//todo, switch this over to be an inheritor of the static asset loader? maybe?
UCLASS()
class ARTILLERYRUNTIME_API UArtilleryProjectileDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy
{
	GENERATED_BODY()

	using ICanReady = ITickHeavy;
	
public:
	friend class UArtilleryLibrary;
	static inline UArtilleryProjectileDispatch* SelfPtr = nullptr;
	int DEFAULT_LIFE_OF_PROJECTILE = ArtilleryTickHertz * 20.0; //20 seconds.
	constexpr static int OrdinateSeqKey = ORDIN::E_D_C::ProjectileSystem;
	
	virtual void ArtilleryTick() override;
	virtual bool RegistrationImplementation() override;
	
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	
	int ExpirationCounter = 0;
	
public:
	UArtilleryProjectileDispatch();

protected:
	virtual ~UArtilleryProjectileDispatch() override;
	UDataTable* ProjectileDefinitions;
	TSharedPtr<TSortedMap<int, TArray<FSkeletonKey>>> ExpirationDeadliner;
	TSharedPtr<TMap<FSkeletonKey, TWeakObjectPtr<AInstancedMeshManager>>> ManagerKeyToMeshManagerMapping;
	TSharedPtr<KeyToItemCuckooMap> ProjectileKeyToMeshManagerMapping;
	TSharedPtr<TMap<FName, TWeakObjectPtr<AInstancedMeshManager>>> ProjectileNameToMeshManagerMapping;
	TSharedPtr<TMap<FString, TWeakObjectPtr<AInstancedMeshManager>>> MeshAssetToMeshManagerMapping;
	TSharedPtr<KeyToGunMap> ProjectileToGunMapping;

public:
	virtual void PostInitialize() override;
	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UArtilleryProjectileDispatch, STATGROUP_Tickables);
	}
	
	FProjectileDefinitionRow* GetProjectileDefinitionRow(const FName ProjectileDefinitionId);
	// TODO - Add handling for IsSensor and IsDynamic. We do not currently have anything that uses these flags, so they are not handled by the request router
	FSkeletonKey QueueProjectileInstance(const FName ProjectileDefinitionId, const FGunKey& Gun, const FVector3d& StartLocation, const FVector3d& MuzzleVelocity, const float Scale = 1.0f, Layers::EJoltPhysicsLayer Layer = Layers::PROJECTILE, TArray<FGameplayTag>* TagArray = nullptr);
	//bool KillInOneFrame(FSkeletonKey ProjectileKey);
	FSkeletonKey CreateProjectileInstance(FSkeletonKey ProjectileKey,  FGunKey Gun, const FName ProjectileDefinitionId, const FTransform& WorldTransform, const FVector3d& MuzzleVelocity, const float Scale = 1.0f, const bool IsSensor = true, const bool IsDynamic = false, Layers::EJoltPhysicsLayer Layer = Layers::PROJECTILE, const bool CanExpire = true, const int LifeInTicks = -1);
	bool IsArtilleryProjectile(const FSkeletonKey MaybeProjectile);
	void DeleteProjectile(const FSkeletonKey Target);
	TWeakObjectPtr<AInstancedMeshManager> GetProjectileMeshManagerByManagerKey(const FSkeletonKey ManagerKey);
	TWeakObjectPtr<AInstancedMeshManager> GetProjectileMeshManagerByProjectileKey(const FSkeletonKey ProjectileKey);
	TWeakObjectPtr<USceneComponent> GetSceneComponentForProjectile(const FSkeletonKey ProjectileKey);

	void OnBarrageContactAdded(const BarrageContactEvent& ContactEvent);

private:
	UArtilleryDispatch* MyDispatch;
};
