// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "BoneIndices.h"
#include "Misc/NotNull.h"

class USkeleton;
class USkinnedAsset;
struct FAnimNotifyQueue;

struct FBlendedHeapCurve;
class USkeletalMesh;
struct FAnimInstanceProxy;
class UAnimInstance;
class USkeletalMeshComponent;
struct FAnimationEvaluationContext;
struct FMegafunkUtilsAnimationEvaluationContainer;
// a variety of functions that expose internal anim instance functionality that is normally private. Also includes some validation
// It should be noted that the goal here is to give you some examples and tools without forcing a single way of doing things. You can use all of this or only portions if you need to
namespace MegafunkUtils::Anim {
	MEGAFUNKUTILS_API void SkeletalMeshComponent_ForceFinalizeBoneTransform(USkeletalMeshComponent& InSkeletalMeshComp);
	MEGAFUNKUTILS_API void SkeletalMeshComponent_PushRenderUpdate(USkeletalMeshComponent& InSkeletalMeshComp);

	MEGAFUNKUTILS_API FAnimationEvaluationContext& GetComponentAnimationEvaluationContext(USkeletalMeshComponent& InSkeletalMeshComp);
	MEGAFUNKUTILS_API FAnimInstanceProxy*& AccessAnimInstanceProxyPtrRef(UAnimInstance& InAnimInstance);
	MEGAFUNKUTILS_API FAnimInstanceProxy* CreateAnimInstanceProxy(UAnimInstance& InAnimInstance);

	// Similar to GetAnimInstanceProxy but it doesn't wait on an anim graph completion task from the gamethread
	// Note that this actually will craete a new anim instance proxy in-place if one is needed
	template <typename T = FAnimInstanceProxy>
	T& GetProxyOnAnyThread_Direct(TNotNull<UAnimInstance*> InAnimInstance) {
		FAnimInstanceProxy*& AnimInstanceProxyPtrRef = AccessAnimInstanceProxyPtrRef(*InAnimInstance);
		if (AnimInstanceProxyPtrRef == nullptr) {
			AnimInstanceProxyPtrRef = CreateAnimInstanceProxy(*InAnimInstance);
		}

		return *static_cast<T*>(AnimInstanceProxyPtrRef);
	};


	MEGAFUNKUTILS_API void SkeletalMeshComponent_TickUpdateAnyThread(float DeltaTime,
	                                                                 USkeletalMeshComponent& InSkeletalMeshComp,
	                                                                 UAnimInstance& InAnimInstance,
	                                                                 const USkeletalMesh& InSkeletalMesh,
	                                                                 TArray<FTransform>& OutComponentSpaceTransforms,
	                                                                 TArray<FTransform>& OutBonesSpaceTransforms,
	                                                                 FVector& OutRootBoneLocationResult,
	                                                                 FBlendedHeapCurve& OutCachedCurve,
	                                                                 UE::Anim::FMeshAttributeContainer& OutAttributes);

	MEGAFUNKUTILS_API void AnimInstance_PostUpdateAnimationAnyThread(UAnimInstance& InAnimInstance);

	// Similar to USkeletalMeshComponent::TickAnimation but does not require running on the game thread. 
	// Recalculates curves and bones if needed
	// @todo currently bNeedsValidRootMotion is unused, it normally is used to tell TickAnimInstances we demand full root motion info from the cmc
	MEGAFUNKUTILS_API void SkeletalMeshComponent_TickAnimationAnyThread(USkeletalMeshComponent& InSkeletalMeshComponent,
	                                                                    UAnimInstance& InAnimInstance,
	                                                                    float DeltaSeconds,
	                                                                    bool bNeedsValidRootMotion);

	// Similar to USkeletalMeshComponent::TickAnimInstances but does not require running on the game thread. 
	// This not the true evaluation step but does some setup normally on done from the gamethread
	MEGAFUNKUTILS_API void AnimInstance_TickAnimInstancesOnAnyThread(UAnimInstance& InAnimInstance, float DeltaSeconds);


	// Similar to UAnimInstance::PreUpdateAnimation but does not requiere running on the game thread. 
	// this sets the deltatime in the anim instance and clears anim events
	MEGAFUNKUTILS_API void AnimInstance_PreUpdateAnimationOnAnyThread(UAnimInstance& InAnimInstance, float DeltaSeconds);


	MEGAFUNKUTILS_API void AnimInstance_UpdateMontages(UAnimInstance& InAnimInstance, float DeltaTime);
	MEGAFUNKUTILS_API void AnimInstance_UpdateMontageEvaluationData(UAnimInstance& InAnimInstance);

	MEGAFUNKUTILS_API void AnimInstanceProxy_ClearQueuedAnimEvents(FAnimInstanceProxy& InAnimProxy,
	                                                               FAnimNotifyQueue& InNotifyQueue,
	                                                               const int32 InPredictedLODLevel,
	                                                               const TWeakObjectPtr<UWorld> InWorldWeak,
	                                                               bool bInShouldUpdateActiveAnimNotifiesSinceLastTick = true);

	// This is a WIP attempt to move away from needing a skeletal mesh component to do PerformAnimationProcessing
	MEGAFUNKUTILS_API void PerformAnimationProcessing(TNotNull<const USkeletalMesh*> InSkeletalMesh,
	                                                  TNotNull<UAnimInstance*> InAnimInstance,
	                                                  bool bInDoEvaluation,
	                                                  bool bInForceRefPose,
	                                                  TArray<FTransform>& OutComponentSpaceTransforms,
													  TArray<FTransform>& OutBoneSpaceBoneTransforms,
	                                                  FVector& OutRootBoneTranslation,
	                                                  FBlendedHeapCurve& OutCurve,
	                                                  UE::Anim::FMeshAttributeContainer& OutAttributes,
	                                                  const TArray<FBoneIndexType>& FillComponentSpaceTransformsRequiredBones);


	// We want to be able to sus out if there are any interesting main thread callbacks to fire
	// Not a part of normal animation ticking flow, this is mostly to support deciding to queue up events from task threads
	// I use this currently to figure out if calling ConditionallyDispatchQueuedAnimEvents in the future is useful
	// One could argue we should check not only if there were any events, but also if anything listens as well
	MEGAFUNKUTILS_API bool DoesAnimInstanceHaveNotifiesOrDelegateCallbacksToTrigger(UAnimInstance& InAnimInstance);


	MEGAFUNKUTILS_API void SkeletalMeshComponent_RecalcBonesAndCurvesIfNeeded(USkeletalMeshComponent& InSkeletalMeshComp);

	// Inerpolates bone state and attributes. 
	// I'm not super happy with this function as it forces users to pass in a special struct
	// The eventual goal will be to make this not require FMegafunkUtilsAnimationEvaluationContainer
	MEGAFUNKUTILS_API void InterpolateBonesAndAttributes(float Alpha,
	                                                     TNotNull<USkinnedAsset*> InSkinnedAsset,
	                                                     TNotNull<UAnimInstance*> InAnimInstance,
	                                                     FMegafunkUtilsAnimationEvaluationContainer& InOutFromState,
	                                                     const FMegafunkUtilsAnimationEvaluationContainer& InInterpToState);


	/** 
	 * Some internal systems like property access (FAnimSubsystem_PropertyAccess) can be useful to call
	 * if you have any property accessors that are forced to be "gamethread" only for various reasons (an object ref etc)
	 * You can be evil and just call them from an async thread but this is at your own risk (doing it from a simple parallelfor from the GT would probably be okay though?)
	 * 
	 * OnPreUpdate_WorkerThread and OnPostUpdate_WorkerThread are called by when FAnimInstanceProxy::UpdateAnimation is invoked so we only need to worry about these ones currently
	 */
	MEGAFUNKUTILS_API void AnimInstance_PreUpdateAnimSubsystems_GameThread(UAnimInstance& InAnimInstance, float DeltaSeconds);
	MEGAFUNKUTILS_API void AnimInstance_PostUpdateAnimSubsystems_GameThread(UAnimInstance& InAnimInstance, float DeltaSeconds);


	/**
	 * Ultra-experimental async anim instance init
	 * Note: Because UAnimInstance has `Within=SkeletalMeshComponent` it must have a skeletal mesh component as an outer.
	 * In my case I just make a simple dummy one owned by the same world to provide it with... something
	 *
	 */
	MEGAFUNKUTILS_API UAnimInstance* Experimental_ManualAnimInstanceAllocAndInit(USkeletalMeshComponent& InOuterSkelMesh,
	                                                                          const TSubclassOf<UAnimInstance> InAnimInstanceClass,
	                                                                          USkeleton& InSkeleton,
	                                                                          const TSharedRef<FBoneContainer> InRequiredBonesContainer,
	                                                                          const UE::Anim::FCurveFilterSettings& InCurveFilterSettings,
	                                                                          const TSharedPtr<FSkelMeshRefPoseOverride> InRefPoseOverride);
	
	MEGAFUNKUTILS_API void Experimental_AnimInstanceOnlyFullUpdate(const float DeltaTime,
	                                                     bool bInDoEvaluation,
	                                                     FAnimationEvaluationContext& InEvalContext,
	                                                     UAnimInstance& InAnimInstance,
	                                                     const USkeletalMesh& InSkeletalMesh,
	                                                     TArray<FTransform>& OutComponentSpaceTransforms,
														 TArray<FTransform>& OutBonesSpaceTransforms,
	                                                     FVector& OutRootBoneLocationResult,
	                                                     FBlendedHeapCurve& OutCachedCurve,
	                                                     UE::Anim::FMeshAttributeContainer& OutAttributes,
	                                                     const TArray<FBoneIndexType>& FillComponentSpaceTransformsRequiredBones);

	
	MEGAFUNKUTILS_API void Experimental_ComputeRequiredBonesWithoutSkeletalMeshComp(TArray<FBoneIndexType>& InOutRequiredBones,
	                                                         TArray<FBoneIndexType>& InOutFillComponentSpaceTransformsRequiredBones,
	                                                         int32 InSkeletonLODIndex,
	                                                         bool bInIgnorePhysicsAsset);
	
	
	/**
	 * Tries to sus out things that my functions don't support in an anim graph. You can ignore this if you want but it will catch unintended setups for the example cases at least
	 * Arguably should be editor only and a lot of the checking can only run in the editor
	 * @param  bValidateAsycSpawn if we check for potential issues from creating+initializing the anim instance off of the game thread
	 * @return If we found something that would outright not work given the input parameters. You can choose to use this or not.
	 * 
	 * @todo split this into more separate funcs or add more params
	 */
	MEGAFUNKUTILS_API bool ValidateAnimInstanceBeingSafeToUseInParallelForExample(const TSubclassOf<UAnimInstance> InAnimInstanceClass, bool bValidateAsycSpawn = false, bool bLogErrors = true);
}
