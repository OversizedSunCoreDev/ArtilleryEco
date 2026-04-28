// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ArtilleryActorControllerConcepts.h"
#include "BLK.h"
#include "KeyedConcept.h"
#include "ORDIN.h"
#include "TransformDispatch.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArtillerySkeletalMeshDispatch.generated.h"

namespace BLK {
	class BLKRing;
	struct WorkerStateBundle;
}


// Intended to be created and managed from an async thread
// If you leave this alive with a reference in the async thread unreal will fail to GC the unloading world and assert. Do not do that.
struct FArtilleryWIPAnimStateContainer {
	
	TStrongObjectPtr<UAnimInstance> AnimInstance;
	TStrongObjectPtr<USkeletalMesh> SkeletalMesh;

	FAnimationEvaluationContext EvaluationContext = {};

	TArray<FBoneIndexType> FillComponentSpaceTransformsRequiredBones;
	
	UE::Tasks::TTask<void> TaskHandle;
};

struct FSkeletonAnimResultQueueElement
{
	FSkeletonKey OwnerKey;
	// currently world origin component space
	TArray<FTransform, TInlineAllocator<128>> BoneTransforms;
};

class UArtilleryDispatch;
/**
 * Skeletal animation done entirely from inside artillery
 * Intended to also send results to main thread rendering representatives (kines ideally in the future)
 */
UCLASS()
class ARTILLERYRUNTIME_API UArtillerySkeletalMeshDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy 
{
	GENERATED_BODY()
public:

	// Creates an anim instance in the artillery thread and updates it in artillery tick
	void CreateAnimInstanceStateForUpdate(const FSkeletonKey InOwnerSkeletonKey,
	                                        const TSubclassOf<UAnimInstance> InAnimInstanceClass,
	                                        USkeletalMesh& InSkeletalMesh,
	                                        TSharedRef<struct FBoneContainer> InRequiredBones,
	                                        const TArray<FBoneIndexType>& InFillComponentSpaceTransformsRequiredBones,
	                                        UE::Anim::FCurveFilterSettings InCurveFilterSettings,
	                                        TSharedPtr<FSkelMeshRefPoseOverride> InRefPoseOverride = nullptr);

	void UnregisterAnimInstanceStateForUpdate(const ActorKey& ActorKey);

	
	virtual void ArtilleryTick() override;
	virtual bool RegistrationImplementation() override;

	constexpr static int32 OrdinateSeqKey = ORDIN::E_D_C::SkeletalAnimationSystem;

protected:
	
	// Special lock-free queue to forward skeleteal mesh data to the game thread
	BLK::BLKRing SkeletonBLKRing;
	uint64 BLKFrameCounter = 0;
	static constexpr int32 ConsumerCount = 8;
	UE::Tasks::TTask<void> ConsumerTaskHandles[ConsumerCount] = {};
	// Artillery thread only handle into the BLKRing
	BLK::WorkerStateBundle ThreadStateBundleArtilleryThread;

	
	// Artillery thread only (we create objects on other threads in here, it can't use normal GC afaik but I can test some things)
	TMap<FSkeletonKey, FArtilleryWIPAnimStateContainer> AnimStateContainers;
	
	// Queued from artillery and intended to be read for each render udpate 
	UE::TConsumeAllMpmcQueue<FSkeletonAnimResultQueueElement> AnimationEvaluationResultQueue;
	
	
	// Why? Because we have multiple threads constantly trying to fight over these, I will use a normal queue soon I think
	FCriticalSection PendingAnimStateContainersCriticalSection;
	// Accessible from any thread under PendingAnimStateContainersCriticalSection
	TArray<TPair<FSkeletonKey, FArtilleryWIPAnimStateContainer>> PendingAnimStateContainers;
	// Accessible from any thread under PendingAnimStateContainersCriticalSection
	TArray<FSkeletonKey> PendingRemovalAnimStateContainers;
	
	
	// Why? Because every single anim instance has Within=SkeletalMeshComponent so they MUST use one as an outer! Very frustrating..
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponentOuter;

	// Gamethread skeletal mesh components we have assumed manual control over
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> ManagedRenderComponents;
	
public:
	UPROPERTY()
	TObjectPtr<UArtilleryDispatch> ArtilleryDispatch;
	UPROPERTY()
	TObjectPtr<UTransformDispatch> TransformDispatch;
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UArtillerySkeletalMeshDispatch, STATGROUP_Tickables); }

protected:
	// We only want game worlds
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override {
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	};
};
