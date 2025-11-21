// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryActorControllerConcepts.h"
#include "Subsystems/WorldSubsystem.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelPublic.h"
#include "NiagaraWorldManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "SkeletonTypes.h"
#include "KeyedConcept.h"
#include "ORDIN.h"
#include "ParticleRecord.h"

THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include "libcuckoo/cuckoohash_map.hh"
//typedef libcuckoo::cuckoohash_map<FSkeletonKey, ParticleRecord> ParticleCuckoo;
typedef libcuckoo::cuckoohash_map<FSkeletonKey, ParticleRecord> KeyToRecordCuckoo;
typedef TMap<FName, KeyToRecordCuckoo> ParticleCuckoo;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

#include "NiagaraParticleDispatch.generated.h"

/**
 * This is a lightweight subsystem that centralizes spawning and management of Niagara particle systems in an ECS-y
 * manner. Expect the dispatch to take care of threading and particle system lifecycles, and the dispatch can be called
 * off-thread as it will weave it into the main game thread on the client automatically.
 *
 * Uses its own "Kinda-sorta-SkeletonKey-but-not-really" system where it generates ParticleID keys that can be used
 * to then interact with particle systems in the future (like moving it, or stopping it early, or etc)
 *
 * ParticleIDs are thread-safe but are NOT server replicated or reliable outside of the local client. These particles
 * are PURELY cosmetic client-side and should never be depended upon to be the same (even ParticleIDs) across clients
 * or the server.
 */

class UArtilleryDispatch;

enum E_NiagaraVariableType
{
	NONE,
	Position,
};

//template <class VariableType>
struct NiagaraVariableParam
{
	E_NiagaraVariableType Type;
	FName VariableName;
	FVector3d VariableValue;

	static NiagaraVariableParam None()
	{
		return NiagaraVariableParam{NONE, "", FVector3d::Zero()};
	}
};

// THIS IS NOT A SKELETON KEY! DO NOT USE IT AS A SKELETON KEY! DO NOT CONVERT TO A SKELETON KEY! DO NOT SERIALIZE!
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FParticleID
{
	GENERATED_BODY()
	
public:
	FParticleID()
	{
		ParticleId = 0;
	}
	
	FParticleID(uint32_t IdIn)
	{
		ParticleId = IdIn;
	}
	
	//FUN STORY: BLUEPRINT CAN'T USE UINT32 EITHER.
	uint32_t ParticleId;

	friend uint32 GetTypeHash(const FParticleID& Other)
	{
		return GetTypeHash(Other.ParticleId);
	}

	FParticleID& operator++() {
		ParticleId++;
		return *this;
	}
};

static bool operator==(FParticleID const& lhs, FParticleID const& rhs) {
	return lhs.ParticleId == rhs.ParticleId;
}

DECLARE_MULTICAST_DELEGATE(OnNiagaraParticlesActivated);

UCLASS()
class ARTILLERYRUNTIME_API UNiagaraParticleDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy
{
	GENERATED_BODY()

public:
	UNiagaraParticleDispatch(): MyDispatch(nullptr)
	{
		NameToNiagaraSystemMapping = MakeShareable(new TMap<FString, UNiagaraSystem*>());
		ParticleIDToComponentMapping = MakeShareable(new TMap<FParticleID, TWeakObjectPtr<UNiagaraComponent>>());
		ComponentToParticleIDMapping = MakeShareable(new TMap<TWeakObjectPtr<UNiagaraComponent>, FParticleID>());
		BoneKeyToParticleIDMapping = MakeShareable(new TMap<FBoneKey, FParticleID>());
		KeyToParticleParamMapping = MakeShareable(new TMap<FBoneKey, TSharedPtr<TQueue<NiagaraVariableParam>>>());
		ProjectileNameToNDCAsset = MakeShareable(new TMap<FName, ManagementPayload>());
		NameToKeyQueue.Empty();
	}

	static inline UNiagaraParticleDispatch* SelfPtr = nullptr;
	
protected:
	virtual ~UNiagaraParticleDispatch() override;
	FParticleID ParticleIDCounter = FParticleID();
	UArtilleryDispatch* MyDispatch;
	
public:
	//Artillery machinery.
	constexpr static int OrdinateSeqKey = ORDIN::E_D_C::ParticleSystem;
	OnNiagaraParticlesActivated BindToNiagaraParticlesActivated;
	
	virtual bool RegistrationImplementation() override;
	void ArtilleryTick() override;
	void UpdateNDCChannels();
	virtual void Tick(float DeltaTime) override;

	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UNiagaraParticleDispatch, STATGROUP_Tickables);
	}

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	TSharedPtr<TMap<FString, UNiagaraSystem*>> NameToNiagaraSystemMapping;
	TSharedPtr<TMap<FParticleID, TWeakObjectPtr<UNiagaraComponent>>> ParticleIDToComponentMapping;
	TSharedPtr<TMap<TWeakObjectPtr<UNiagaraComponent>, FParticleID>> ComponentToParticleIDMapping;

	// May want to make the value some kind of compacted iterable as we may have more than one particle
	// system on a single component
	TSharedPtr<TMap<FBoneKey, FParticleID>> BoneKeyToParticleIDMapping;
	TSharedPtr<TMap<FBoneKey, TSharedPtr<TQueue<NiagaraVariableParam>>>> KeyToParticleParamMapping;

	// just a reminder, should it be relevant. libcuckoo::hashmap find throws by default.
	// if this is a source of slowdown, you may consider debugging and using the dreadful KeySlink. it's not debugged tho..
	// good luck I guess lol.
	using ManagementPayload = TTuple<TObjectPtr<UNiagaraDataChannelAsset>, TObjectPtr<UNiagaraDataChannelWriter>, TSet<FSkeletonKey>>;
	TSharedPtr<TMap<FName, ManagementPayload>> ProjectileNameToNDCAsset;

	TQueue<TPair<FName, FSkeletonKey>> NameToKeyQueue;

public:
	virtual void PostInitialize() override;

	virtual UNiagaraSystem* GetOrLoadNiagaraSystem(FString NiagaraSystemLocation);

	UFUNCTION(BlueprintCosmetic)
	virtual bool IsStillActive(FParticleID PID);

	UFUNCTION(BlueprintCosmetic)
	virtual FParticleID SpawnFixedNiagaraSystem(
		FString NiagaraSystemLocation,
		FVector Location,
		FRotator Rotation = FRotator::ZeroRotator,
		FVector Scale = FVector(1.f),
		bool bAutoDestroy = true,
		ENCPoolMethod PoolingMethod = ENCPoolMethod::None,
		bool bAutoActivate = true,
		bool bPreCullCheck = true);
	
	UFUNCTION(BlueprintCosmetic)
	virtual FParticleID SpawnAttachedNiagaraSystem(
		FString NiagaraSystemLocation,
		FSkeletonKey AttachToComponentKey,
		FName AttachPointName,
		EAttachLocation::Type LocationType,
		FVector Location = FVector(0.f),
		FRotator Rotation = FRotator(0.f),
		FVector Scale = FVector(1.f),
		bool bAutoDestroy = true,
		ENCPoolMethod PoolingMethod = ENCPoolMethod::AutoRelease,
		bool bAutoActivate = true,
		bool bPreCullCheck = true);

	UFUNCTION(BlueprintCosmetic)
	virtual void DeregisterNiagaraParticleComponent(UNiagaraComponent* NiagaraComponent);

	UFUNCTION(BlueprintCosmetic)
	virtual void Activate(FParticleID PID);
	UFUNCTION(BlueprintCosmetic)
	virtual void Deactivate(FParticleID PID);

	virtual void Activate(FBoneKey BoneKey);
	virtual void Deactivate(FBoneKey BoneKey);

	/// NOTE: Activate/Deactivate Internal functions should only be called on the main Game Thread. ///
	void ActivateInternal(FParticleID PID) const;
	void DeactivateInternal(FParticleID PID) const;

	void QueueParticleSystemParameter(const FBoneKey& Key, const NiagaraVariableParam& Param) const;

	void AddNDCReference(FName Name, TObjectPtr<UNiagaraDataChannelAsset> DataChannelAssetPtr) const;

	bool AssetUsesNDCParticles(FName Name) const
	{
		return ProjectileNameToNDCAsset->Contains(Name);
	}

	TWeakObjectPtr<UNiagaraDataChannelAsset> GetNDCAssetForProjectileDefinition(FName ProjectileDefinitionID) const
	{
		ManagementPayload* Found = ProjectileNameToNDCAsset->Find(ProjectileDefinitionID);
		if (Found == nullptr)
		{
			return nullptr;
		}
		TObjectPtr<UNiagaraDataChannelAsset> NDCAsset = Found->Get<0>();
		return NDCAsset != nullptr ? TWeakObjectPtr<UNiagaraDataChannelAsset>(NDCAsset) : nullptr;
	}

	void RegisterKeyForProcessing(FName ProjectileName, FSkeletonKey ProjectileKey, TWeakObjectPtr<UNiagaraDataChannelAsset> ProjectileNDCAssetPtr)
	{
		NameToKeyQueue.Enqueue(TPair<FName, FSkeletonKey>(ProjectileName, ProjectileKey));
	}

	TQueue<TPair<FName, FSkeletonKey>> KeysToCleanupQueue;
	void CleanupKey(const FSkeletonKey Key, FName ProjectileName = "")
	{
		KeysToCleanupQueue.Enqueue(TPair<FName, FSkeletonKey>(ProjectileName, Key));

		/*ManagementPayload* KeyToRecordMap = ProjectileNameToNDCAsset->Find(ProjectileName);
		if (KeyToRecordMap != nullptr)
		{
			KeyToRecordMap->Get<2>().Remove(Key);
		}
		else
		{
			for (auto it = ProjectileNameToNDCAsset->CreateIterator(); it; ++it)
			{
				it.Value().Get<2>().Remove(Key);
			}
		}*/
	}
};

inline UNiagaraParticleDispatch::~UNiagaraParticleDispatch()
{
	TSharedPtr<TMap<FParticleID, TWeakObjectPtr<UNiagaraComponent>>> Hold = ParticleIDToComponentMapping;
	if(Hold)
	{
		for(TPair<FParticleID, TWeakObjectPtr<UNiagaraComponent>>& x : *Hold.Get())
		{
			if(x.Value.Get())
			{
				x.Value->DestroyInstance();
			}
		}
	}
}
