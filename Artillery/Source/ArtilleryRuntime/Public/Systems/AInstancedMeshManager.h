// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FWorldSimOwner.h"
#include "ArtilleryDispatch.h"
#include "GameFramework/Actor.h"
#include "SwarmKine.h"
#include "EPhysicsLayer.h"
#include <thread>
#include "AInstancedMeshManager.generated.h"

UCLASS()
class ARTILLERYRUNTIME_API AInstancedMeshManager : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Artillery, meta = (AllowPrivateAccess = "true"))
	USwarmKineManager* SwarmKineManager;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Artillery, meta = (AllowPrivateAccess = "true"))
	UArtilleryDispatch* MyDispatch;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Artillery, meta = (AllowPrivateAccess = "true"))
	UTransformDispatch* TransformDispatch;

	uint32 instances_generated;
	bool Usable = false;
	
	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		if (!Usable)
		{
			InitializeManager();
		}
	}
	
	AInstancedMeshManager()
	{
		SwarmKineManager = CreateDefaultSubobject<USwarmKineManager>("SwarmKineManager");
		SwarmKineManager->bDisableCollision = true;
		SwarmKineManager->SetSimulatePhysics(false);
		SetActorEnableCollision(false);
		DisableComponentsSimulatePhysics();
		MyDispatch = nullptr;
		TransformDispatch = nullptr;
	}

	virtual void BeginDestroy() override
	{
		Super::BeginDestroy();// explicitly begin the destruction of the swarmkine manager.
		//one super fun thing is that because all the pointers are weak refs, nothing gets automatically collected
		if(SwarmKineManager)
		{
			SwarmKineManager->ClearInternalFlags(EInternalObjectFlags::Async);
			SwarmKineManager->ConditionalBeginDestroy();	
		}
	}

	ActorKey GetMyKey() const
	{
		return MyKey;
	}

	void SetStaticMesh(UStaticMesh* Mesh)
	{
		SwarmKineManager->SetStaticMesh(Mesh);
	}

	void InitializeManager()
	{
		if(!Usable)
		{
			MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
			TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
			// No Chaos for you!
			SwarmKineManager->SetEnableGravity(false);
			SwarmKineManager->SetSimulatePhysics(false);
			SwarmKineManager->DestroyPhysicsState();

			// Make a key yo
			uint32 keyHash = PointerHash(this);
			UE_LOG(LogTemp, Warning, TEXT("AInstancedMeshManager Parented: %d"), keyHash);
			MyKey = ActorKey(keyHash);
			Usable = true;
		}
	}
	
	FSkeletonKey GenerateNewProjectileKey()
	{
		//You would not believe how much trouble this caused. you just would not believe it.
		//TODO: this is not currently deterministic in some senses. ensure it is not an issue for sim (thread ordering produces diff keys)
		//likely, we want something like an atomic instead of instances generated or we want something that lets us better mark out when and why
		//in a lexical ordering. it's not an easy problem. I think it'll be vastly simplified by breaking skeleton keys into type short,meta short,hash uint
		//that'll let us use meta for ordering in some cases and verity in others.
		std::hash<std::thread::id> hasher;
		uint32 low = HashCombineFast(
				HashCombineFast(GetTypeHash(SwarmKineManager),  FMMM::FastHash32(++instances_generated)),
				F_INeedA::HashDownTo32( hasher(std::this_thread::get_id())));
		uint64 combo = low;
		combo = (combo << 32) + low;
		return FProjectileInstanceKey(combo);
	}

	//TODO: this looks very wrong now.
	UFUNCTION(BlueprintCallable, Category = Instance)
	FSkeletonKey CreateNewInstance(const FTransform& WorldTransform, const FVector& MuzzleVelocity, const EPhysicsLayer Layer, float Scale = 1.0f, bool IsSensor = false, bool IsDynamic = false)
	{
		return CreateNewInstance(WorldTransform, MuzzleVelocity, static_cast<uint16_t>(Layer), Scale, FSkeletonKey(), IsSensor, IsDynamic);
	}
	
	FSkeletonKey CreateNewInstance(const FTransform& WorldTransform, const FVector3d& MuzzleVelocity, const uint16_t Layer, float Scale = 1.0f, FSkeletonKey ExistingKey = FSkeletonKey::Invalid(), bool IsSensor = false, bool IsDynamic = false)
	{
		FSkeletonKey NewInstanceKey = (ExistingKey == FSkeletonKey::Invalid()) ? GenerateNewProjectileKey() : ExistingKey;
		FTransform ScaledTransform(MuzzleVelocity.GetSafeNormal().Rotation(),WorldTransform.GetLocation(), FVector3d(Scale, Scale, Scale));
		FPrimitiveInstanceId NewInstanceId = SwarmKineManager->AddInstanceById(ScaledTransform, true);
		SwarmKineManager->AddToMapDbg(NewInstanceId, NewInstanceKey);

		CreateNewInstanceWithKeyInternal(NewInstanceKey, WorldTransform, MuzzleVelocity, Layer, Scale);
		
		return NewInstanceKey;
	}

	//TODO: this really really really should return a fblet or a kine OR make it impossible to get a FBlet or kine for that scene component.
	//we do not ever want scene components that are managed in two or more ways.
	TWeakObjectPtr<USceneComponent> GetSceneComponentForInstance(const FSkeletonKey InstanceKey)
	{
		return SwarmKineManager->GetSceneComponentForInstance(InstanceKey);
	}

	// This function will eventually become private. Do not call it directly from outside of Artillery itself.
	void CleanupInstance(const FSkeletonKey Target)
	{
		TransformDispatch->ReleaseKineByKey(Target);
		SwarmKineManager->CleanupInstance(Target);
	}

private:
	void CreateNewInstanceWithKeyInternal(FSkeletonKey ProjectileKey, const FTransform& WorldTransform, const FVector3d& MuzzleVelocity, const uint16_t Layer, float Scale) const
	{
		// TODO: can't use the BarrageColliderBase set of types, so in-lining the barrage setup code. Is this what we want long-term?
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		TObjectPtr<UStaticMesh> AnyMesh = SwarmKineManager->GetStaticMesh();
		FBox Boxen = AnyMesh->GetBounds().GetBox();
		FVector extents = Boxen.GetExtent() * 2 * Scale;

		FBBoxParams params = FBarrageBounder::GenerateBoxBounds(WorldTransform.GetLocation(), extents.X, extents.Y, extents.Z,
			FVector3d(0, 0, extents.Z/2));
		FBLet MyBarrageBody = Physics->CreateProjectile(params, ProjectileKey, Layer);

		TransformDispatch->RegisterObjectToShadowTransform(ProjectileKey, SwarmKineManager);
		FBarragePrimitive::SetVelocity(MuzzleVelocity, MyBarrageBody);
		FBarragePrimitive::ApplyRotation(MuzzleVelocity.ToOrientationQuat(), MyBarrageBody);
		FBarragePrimitive::SetGravityFactor(0.f, MyBarrageBody);
		
		MyDispatch->REGISTER_PROJECTILE_FINAL_TICK_RESOLVER(100, ProjectileKey);
	}
	
	ActorKey MyKey;
};