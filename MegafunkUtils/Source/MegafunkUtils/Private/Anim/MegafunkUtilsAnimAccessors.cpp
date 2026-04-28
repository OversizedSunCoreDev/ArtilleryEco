// Fill out your copyright notice in the Description page of Project Settings.

#include "Anim/MegafunkUtilsAnimAccessors.h"

#include "AnimNode_ControlRig.h"
#include "Anim/AsyncAnimValidationAndUtils.h"
#include "InterchangeResult.h"
#include "Anim/MegafunkAnimUtilsTypes.h"
#include "MegafunkUtils.h"
#include "MegafunkUtilsLibrary.h"
#include "MegafunkUtilsMacroHelpers.h"
#include "ObjectTrace.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontageEvaluationState.h"
#include "Logging/StructuredLog.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "AnimBlueprintExtension_PropertyAccess.h"
#endif


// A lot of the members and functions we need to mess with not public... Time for some template-assisted cheese
// UAnimInstance
MFUTILS_DEFINE_PRIVATE_ACCESS_TYPE(UAnimInstance, FQueuedRootMotionBlend);

using ExposedFQueuedRootMotionBlendType = UAnimInstance_Private::FQueuedRootMotionBlend;
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, TArray<ExposedFQueuedRootMotionBlendType>, RootMotionBlendQueue);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, TArray<FQueuedMontageBlendingOutEvent>, QueuedMontageBlendingOutEvents);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, TArray<FQueuedMontageBlendedInEvent>, QueuedMontageBlendedInEvents);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, TArray<FQueuedMontageEndedEvent>, QueuedMontageEndedEvents);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, TArray<FQueuedMontageSectionChangedEvent>, QueuedMontageSectionChangedEvents)
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, FAnimMontageInstance*, RootMotionMontageInstance);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, FAnimInstanceProxy*, AnimInstanceProxy);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(UAnimInstance, uint8, bUpdateAnimationEnabled);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(UAnimInstance, FAnimInstanceProxy*, (), CreateAnimInstanceProxy);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(UAnimInstance, void, (float), UpdateMontage);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(UAnimInstance, void, (), UpdateMontageSyncGroup);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(UAnimInstance, void, (), UpdateMontageEvaluationData);

// USkeletalMeshComponent
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(USkeletalMeshComponent, FAnimationEvaluationContext, AnimEvaluationContext);

MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(USkeletalMeshComponent, bool, () const, AreRequiredCurvesUpToDate);

MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(USkeletalMeshComponent, void, (), SendRenderDynamicData_Concurrent);

// FAnimInstanceProxy

MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, TArray<FAnimNotifyEventReference>, ActiveAnimNotifiesSinceLastTick);

using ExposedInertializationRequestDataMapType = TMap<FName, FInertializationRequest>; // TMaps have a comma in them so we alias them
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, ExposedInertializationRequestDataMapType, SlotGroupInertializationRequestDataMap);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, FRootMotionMovementParams, ExtractedRootMotion);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, TSharedPtr<FBoneContainer>, RequiredBones);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, FAnimNode_LinkedInputPose*, DefaultLinkedInstanceInputNode);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimInstanceProxy, USkeleton*, Skeleton);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC_OVERLOAD(FAnimInstanceProxy, TArray<FMontageEvaluationState>&, (), GetMontageEvaluationData, void);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, ExposedInertializationRequestDataMapType&, (), GetSlotGroupInertializationRequestDataMap);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, float, (const FName& SlotName) const, GetSlotNodeGlobalWeight);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (UAnimInstance*), Initialize);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (bool), InitializeRootNode);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (), BindNativeDelegates);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (UAnimInstance* InAnimInstance, float DeltaSeconds), PreUpdate);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (const FAnimNotifyQueue& InAnimNotifyQueue), UpdateActiveAnimNotifiesSinceLastTick);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (), UpdateAnimation);
MFUTILS_DEFINE_PRIVATE_ACCESS_FUNC(FAnimInstanceProxy, void, (UAnimInstance* InAnimInstance) const, PostUpdate);

// FAnimNotifyQueue
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FAnimNotifyQueue, TWeakObjectPtr<UWorld>, World);

#if WITH_EDITOR
// Property access subsystem etc
//@todo this might get nasty enough to merit a second module for editor-only stuff... I would prefer to not add extra modules for as long as I can though
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FPropertyAccessLibrary, TArray<FPropertyAccessCopyBatch>, CopyBatchArray);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FPropertyAccessCopyBatch, TArray<FPropertyAccessCopy>, Copies);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FPropertyAccessCopy, int32, AccessIndex);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FPropertyAccessLibrary, TArray<FPropertyAccessIndirectionChain>, SrcAccesses);
MFUTILS_DEFINE_PRIVATE_ACCESS_MEMBER(FPropertyAccessIndirectionChain, TFieldPath<FProperty>, Property);
#endif


namespace MegafunkUtils::Anim {
	// This helps expose a protected bitfield
	// Since almost everything in here is protected only this seems to work okay
	// I would assume any virtual calls or new members would not work in here. I do not need them here though
	class SkeletalMeshComponentProtectedAccess : public USkeletalMeshComponent {
	public:
		void ForceFinalizeBoneTransform() {
			TRACE_CPUPROFILER_EVENT_SCOPE(SkeletalMeshComponentProtectedAccess::ForceFinalizeBoneTransform);
			//@todo remove the need for FinalizeBoneTransform and bNeedToFlipSpaceBaseBuffers
			// This declares we want to flip the bones buffers
			bNeedToFlipSpaceBaseBuffers = true;
			// swao the game thread and anim instance bone buffers. This is supposed to send a delegate as well but we omit that
			{
				// USkeletalMeshComponent::FinalizeBoneTransform has some multicast delegates that would not be nice to call from threads unless users are very careful (OnBoneTransformsFinalizedMC)
				// we also skip dispatching queued anim events (ConditionallyDispatchQueuedAnimEvents). In the example is this done from a queue afterwards for now
				// By having your own bone arrays this step might not be required 
				USkinnedMeshComponent::FinalizeBoneTransform();
			}

			// @todo simple attached socket examples (visuals only)
			// if (bHasSocketAttachments) {
			// 	UpdateChildTransforms(EUpdateTransformFlags::OnlyUpdateIfUsingSocket);
			// }
		};
	};

	// Yet another cheesy class to expose a protected bitfield, but for anim instances
	class AnimInstanceProxyProtectedAccess : public FAnimInstanceProxy {
	public:
		void RefreshRequiredBones(const TSharedRef<FBoneContainer> InRequiredBonesContainer,
		                          const TArray<FBoneIndexType>& InRequiredBones,
		                          const UE::Anim::FCurveFilterSettings& InCurveFilterSettings,
		                          const TSharedPtr<FSkelMeshRefPoseOverride> InRefPoseOverride,
		                          const UObject& InAsset) {
			TSharedPtr<FBoneContainer>& RequiredBonesRef = MFUTILS_GET_PRIVATE(FAnimInstanceProxy, *this, RequiredBones);


			RequiredBonesRef = InRequiredBonesContainer;

			if (!RequiredBonesRef->IsValid()) {
				RequiredBonesRef->InitializeTo(InRequiredBones, InCurveFilterSettings, InAsset);
				RequiredBonesRef->SetRefPoseOverride(InRefPoseOverride);
			}

			// In order to accept input poses we can optionally initialise the input pose container
			FAnimNode_LinkedInputPose*& DefaultLinkedInstanceInputNodeRef = MFUTILS_GET_PRIVATE(
				FAnimInstanceProxy,
				*this,
				DefaultLinkedInstanceInputNode);
			if (DefaultLinkedInstanceInputNodeRef) {
				DefaultLinkedInstanceInputNodeRef->CachedInputPose.SetBoneContainer(RequiredBonesRef.Get());
				// In the reference example this is allocated but isn't actually initialized, so we mark so here.
				DefaultLinkedInstanceInputNodeRef->bIsCachedInputPoseInitialized = false;
			}

			// Tell anim nodes to update their bone cache...  
			bBoneCachesInvalidated = true;
		};
	};


	void SkeletalMeshComponent_ForceFinalizeBoneTransform(USkeletalMeshComponent& InSkeletalMeshComp) {
		static_cast<SkeletalMeshComponentProtectedAccess&>(InSkeletalMeshComp).ForceFinalizeBoneTransform();
	}

	void SkeletalMeshComponent_PushRenderUpdate(USkeletalMeshComponent& InSkeletalMeshComp) {
		TRACE_CPUPROFILER_EVENT_SCOPE(SkeletalMeshComponent_PushRenderUpdate);
		// invalidate then update bounds
		if (InSkeletalMeshComp.bSkipBoundsUpdateWhenInterpolating) {
			ensure(false); // @todo make us skip bounds updates when interpolating.. that sounds very nice
		}
		else {
			TRACE_CPUPROFILER_EVENT_SCOPE(InvalidateCachedBounds());
			// Invalidate local bounds. Bounds updating is a bit scuffed still and probably will need some help...
			InSkeletalMeshComp.InvalidateCachedBounds();
			//@todo Since SendRenderTransform_Concurrent does UpdateBounds why is this needed? Maybe for the first recreate?
			// UpdateBounds(); 
		}

		{
			LLM_SCOPE(ELLMTag::SceneRender);

			if (!InSkeletalMeshComp.IsRegistered()) {
				ensure(false);
				UE_LOG(LogMegafunkUtils, Log, TEXT("Skeletal mesh PushRenderUpdate: (%s) was not registered!"), *InSkeletalMeshComp.GetPathName());
				return;
			}

			if (InSkeletalMeshComp.IsRenderStateDirty()) {
				InSkeletalMeshComp.RecreateRenderState_Concurrent();
			}
			else if (InSkeletalMeshComp.IsRenderStateCreated()) {
				// Do we need to always send this if we haven't moved? Probably not!
				InSkeletalMeshComp.SendRenderTransform_Concurrent();
				MFUTILS_CALL_PRIVATE(USkeletalMeshComponent, InSkeletalMeshComp, SendRenderDynamicData_Concurrent);
			}
		}
	};

	FAnimationEvaluationContext& GetComponentAnimationEvaluationContext(USkeletalMeshComponent& InSkeletalMeshComp) {
		return MFUTILS_GET_PRIVATE(USkeletalMeshComponent, InSkeletalMeshComp, AnimEvaluationContext);
	}

	FAnimInstanceProxy*& AccessAnimInstanceProxyPtrRef(UAnimInstance& InAnimInstance) {
		return MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, AnimInstanceProxy);
	}

	FAnimInstanceProxy* CreateAnimInstanceProxy(UAnimInstance& InAnimInstance) {
		return MFUTILS_CALL_PRIVATE(UAnimInstance, InAnimInstance, CreateAnimInstanceProxy);
	}


	void SkeletalMeshComponent_TickUpdateAnyThread(float DeltaTime,
	                                               USkeletalMeshComponent& InSkeletalMeshComp,
	                                               UAnimInstance& InAnimInstance,
	                                               const USkeletalMesh& InSkeletalMesh,
	                                               TArray<FTransform>& OutComponentSpaceTransforms,
	                                               TArray<FTransform>& OutBonesSpaceTransforms,
	                                               FVector& OutRootBoneLocationResult,
	                                               FBlendedHeapCurve& OutCachedCurve,
	                                               UE::Anim::FMeshAttributeContainer& OutAttributes) {
		TRACE_CPUPROFILER_EVENT_SCOPE(SkeletalMeshComponent_FullUpdateAnyThread)
#if WITH_EDITOR
		// Do not tick skeletal mesh component if the skeletal mesh asset is compiling or missing outright in the editor
		// I am uncertain if this restriction is important for all usecases but I think it's pretty okay to not do this for now
		if (!InSkeletalMesh.IsValidLowLevel() || InSkeletalMesh.IsCompiling()) {
			return;
		}
#endif

		// Currently we just fix up bone arrays directly... This is of course not going to work if you change LODs at random
		// @todo arbitrary bone array size changing
		if (OutBonesSpaceTransforms.IsEmpty()) {
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			OutBonesSpaceTransforms.SetNum(InSkeletalMesh.GetRefSkeleton().GetNum());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
		if (OutComponentSpaceTransforms.IsEmpty()) {
			OutComponentSpaceTransforms.SetNum(InSkeletalMesh.GetRefSkeleton().GetNum());
		}

		// This block is similar to USkeletalMeshComponent::TickAnimation and typically done from a task thread
		{
			SkeletalMeshComponent_TickAnimationAnyThread(InSkeletalMeshComp, InAnimInstance, DeltaTime, false);
		}


		// Typically done from a task worker
		// This scope is most of USkeletalMeshComponent::ParallelAnimationEvaluation
		{
			// Everything before was all to lead up to doing just this (but without a sad main thread step)
			// @todo replace with our static-style PerformAnimationProcessing
			// @todo handle choosing to evaluate or not evaluate
			InSkeletalMeshComp.PerformAnimationProcessing(&InSkeletalMesh,
			                                              &InAnimInstance,
			                                              true,
			                                              false,
			                                              OutComponentSpaceTransforms,
			                                              OutBonesSpaceTransforms,
			                                              OutRootBoneLocationResult,
			                                              OutCachedCurve,
			                                              OutAttributes);


			//@todo Skip this entirely when not evaluating or interpolating, inline the proxy access
			//@todo we want our own eval context eventually. We do not want to rely on an unreal component for no reason
			FAnimationEvaluationContext& EvalContext = MFUTILS_GET_PRIVATE(USkeletalMeshComponent, InSkeletalMeshComp, AnimEvaluationContext);
			InAnimInstance.UpdateCurvesToEvaluationContext(EvalContext);
		}


		{
			// post anim evaluation code from  UAnimInstance::PostUpdateAnimation
			MegafunkUtils::Anim::AnimInstance_PostUpdateAnimationAnyThread(InAnimInstance);
		}
	}

	void AnimInstance_PostUpdateAnimationAnyThread(UAnimInstance& InAnimInstance) {
		FAnimInstanceProxy& InstanceProxy = MegafunkUtils::Anim::GetProxyOnAnyThread_Direct<FAnimInstanceProxy>(&InAnimInstance);

		// We don't want to bother with 
		// AnimInstanceProxy.FlipBufferWriteIndex();

		MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, InstanceProxy, PostUpdate, &InAnimInstance);


		auto& ProxyExtractedRootMotion = MFUTILS_GET_PRIVATE(FAnimInstanceProxy, InstanceProxy, ExtractedRootMotion);
		if (InstanceProxy.GetExtractedRootMotion().bHasRootMotion) {
			FTransform StartingProxyTransform = InstanceProxy.GetExtractedRootMotion().GetRootMotionTransform();
			StartingProxyTransform.NormalizeRotation();
			ProxyExtractedRootMotion.Accumulate(StartingProxyTransform);
			InstanceProxy.GetExtractedRootMotion().Clear();
		}


		for (const ExposedFQueuedRootMotionBlendType& RootBlendElem : MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, RootMotionBlendQueue)) {
			float SlotWeight = MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, InstanceProxy, GetSlotNodeGlobalWeight, RootBlendElem.SlotName);
			float RootMotionWeight = RootBlendElem.Weight * SlotWeight;
			ProxyExtractedRootMotion.AccumulateWithBlend(RootBlendElem.Transform, RootMotionWeight);
		}

		if (ProxyExtractedRootMotion.bHasRootMotion) {
			ProxyExtractedRootMotion.MakeUpToFullWeight();
		}
	}


	void SkeletalMeshComponent_TickAnimationAnyThread(USkeletalMeshComponent& InSkeletalMeshComponent,
	                                                  UAnimInstance& InAnimInstance,
	                                                  float DeltaSeconds,
	                                                  bool bNeedsValidRootMotion) {
		SkeletalMeshComponent_RecalcBonesAndCurvesIfNeeded(InSkeletalMeshComponent);
		// since we might enqueue some events, set bNeedsQueuedAnimEventsDispatched to true
		// Mainly meaningful if we call ConditionallyDispatchQueuedAnimEvents later
		// Normally we would access it directly like so:
		// MFUTILS_GET_PRIVATE(InSkeletalMeshComponent, bNeedsQueuedAnimEventsDispatched) = true;
		// But it's a bitfield... we would have to get cheesy with bit offsets to write it. Amazingly a function exists for this already!
		InSkeletalMeshComponent.AllowQueuedAnimEventsNextDispatch();

		AnimInstance_TickAnimInstancesOnAnyThread(InAnimInstance, DeltaSeconds);
	}

	void AnimInstance_TickAnimInstancesOnAnyThread(UAnimInstance& InAnimInstance, float DeltaTime) {
		// Similar to TickAnimInstances but only calls UpdateAnimation on ourself for now

		// @todo handle post process anim instance
		//@todo handle the (ShouldOnlyTickMontages || ShouldOnlyTickMontagesAndRefreshBones && !IsAnyMontagePlaying())) case

		// Call pre-update PreUpdateAnimation(..
		AnimInstance_PreUpdateAnimationOnAnyThread(InAnimInstance, DeltaTime);

		// Update montages
		AnimInstance_UpdateMontages(InAnimInstance, DeltaTime);

		// Optional:
		// This step is only needed for property access like objects which force a gamethread step. 
		// This is decidedly unsafe if the object reference can be messed with during work, but this example does work in a parallel for
		MegafunkUtils::Anim::AnimInstance_PreUpdateAnimSubsystems_GameThread(InAnimInstance, DeltaTime);

		//Technically you can call your NativeUpdateAnimation here but I don't really see a big reason to currently
		{
			// A tracing macro here would be a valid concern but I think that is a user problem
			InAnimInstance.NativeUpdateAnimation(DeltaTime);
		}

		// Blueprint's BlueprintUpdateAnimation is NOT SAFE! For that reason I manually validate and warn when I see it implemented
		// You could do a BP callback earlier if desired before the parallel work
		// Left in here commented out partially as an example to make it clear where this normally happens
		// AnimInstance->BlueprintUpdateAnimation(DeltaTime);

		// Optional:
		// Optional post-update for all anim subsystems
		AnimInstance_PostUpdateAnimSubsystems_GameThread(InAnimInstance, DeltaTime);
	}

	void AnimInstance_PreUpdateAnimationOnAnyThread(UAnimInstance& InAnimInstance, float DeltaSeconds) {
		TRACE_CPUPROFILER_EVENT_SCOPE(MegafunkUtils AnimInstancePreUpdateAnimationOnAnyThread)

		// Without this PerformAnimationProcessing will skip calling ParallelUpdateAnimation
		// Arguably it should be a user's concern if this should skip an update
		InAnimInstance.bNeedsUpdate = true;

		FAnimInstanceProxy& AnimInstanceProxy = GetProxyOnAnyThread_Direct<FAnimInstanceProxy>(&InAnimInstance);

		// Substitute for ClearQueuedAnimEvents(true);
		{
			//@todo does this handle LOD that we expect?
			const int32 LODLevel = InAnimInstance.GetLODLevel();
			AnimInstanceProxy_ClearQueuedAnimEvents(AnimInstanceProxy, InAnimInstance.NotifyQueue, LODLevel, InAnimInstance.GetWorld(), true);
		}

		// Reset the root motion blend queue
		MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, RootMotionBlendQueue).Reset();

		// Finally, call PreUpdate on the anim instance directly
		MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, AnimInstanceProxy, PreUpdate, &InAnimInstance, DeltaSeconds);
	}

	void AnimInstance_UpdateMontages(UAnimInstance& InAnimInstance, float DeltaTime) {
		// Advance montages manually UpdateMontage(DeltaSeconds);
		MFUTILS_CALL_PRIVATE_ARGS(UAnimInstance, InAnimInstance, UpdateMontage, DeltaTime);

		// After advancing sync groups are updated
		MFUTILS_CALL_PRIVATE(UAnimInstance, InAnimInstance, UpdateMontageSyncGroup);

		// update evaluation data for the rest of the anim graph
		AnimInstance_UpdateMontageEvaluationData(InAnimInstance);
	}

	void AnimInstance_UpdateMontageEvaluationData(UAnimInstance& InAnimInstance) {
		// Normally we would just directly call this but... It tries to get the game thread proxy! Argh!!
		// MFUTILS_CALL_PRIVATE(InAnimInstance, UpdateMontageEvaluationData);

		// If we are a linked instance we don't want to do anything unless we are the main instance
		if (InAnimInstance.IsUsingMainInstanceMontageEvaluationData()) {
			const USkeletalMeshComponent* OwningComp = InAnimInstance.GetOwningComponent();
			if (OwningComp && OwningComp->GetAnimInstance() != &InAnimInstance) {
				return;
			}
		}

		FAnimInstanceProxy& Proxy = GetProxyOnAnyThread_Direct<FAnimInstanceProxy>(&InAnimInstance);

		TArray<FMontageEvaluationState>& EvaluationData = MFUTILS_CALL_PRIVATE(FAnimInstanceProxy, Proxy, GetMontageEvaluationData);

		EvaluationData.Reset(InAnimInstance.MontageInstances.Num());

		for (FAnimMontageInstance* Instance : InAnimInstance.MontageInstances) {
			// Unreal will just outright ignore anything below 0.00001 weight
			if (Instance->Montage && Instance->GetWeight() > ZERO_ANIMWEIGHT_THRESH) {
				EvaluationData.Add(FMontageEvaluationState(Instance->Montage,
				                                           Instance->GetPosition(),
				                                           Instance->DeltaTimeRecord,
				                                           Instance->bPlaying,
				                                           Instance->IsActive(),
				                                           Instance->GetBlend(),
				                                           Instance->GetActiveBlendProfile(),
				                                           Instance->GetBlendStartAlpha()));
			}
		}


		// Copy the intertialization request map to the ref returned by the getter (I assume to set the main instance to use ours?)
		// Proxy.GetSlotGroupInertializationRequestDataMap() = Proxy.SlotGroupInertializationRequestDataMap;
		MFUTILS_CALL_PRIVATE(FAnimInstanceProxy, Proxy, GetSlotGroupInertializationRequestDataMap) = MFUTILS_GET_PRIVATE(
			FAnimInstanceProxy,
			Proxy,
			SlotGroupInertializationRequestDataMap);

		// Unreal resets the intertialization request data map every frame
		// Proxy.SlotGroupInertializationRequestDataMap.Reset();
		MFUTILS_GET_PRIVATE(FAnimInstanceProxy, Proxy, SlotGroupInertializationRequestDataMap).Reset();
	}

	void AnimInstanceProxy_ClearQueuedAnimEvents(FAnimInstanceProxy& InAnimProxy,
	                                             FAnimNotifyQueue& InNotifyQueue,
	                                             const int32 InPredictedLODLevel,
	                                             const TWeakObjectPtr<UWorld> InWorldWeak,
	                                             bool bInShouldUpdateActiveAnimNotifiesSinceLastTick) {
		// Unreal has the branch here in case you have to call this multiple times without accidentally re-triggering anim notifies...
		// It appears to be called in UCharacterMovementComponent::MoveAutonomous
		if (bInShouldUpdateActiveAnimNotifiesSinceLastTick) {
			// Before resetting notify queue entirely unreal appends the current ones here like so
			// InAnimProxy.UpdateActiveAnimNotifiesSinceLastTick(InNotifyQueue);
			MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, InAnimProxy, UpdateActiveAnimNotifiesSinceLastTick, InNotifyQueue);
		}
		else {
			// InAnimProxy.ActiveAnimNotifiesSinceLastTick.Append(InNotifyQueue.AnimNotifies);
			MFUTILS_GET_PRIVATE(FAnimInstanceProxy, InAnimProxy, ActiveAnimNotifiesSinceLastTick).Append(InNotifyQueue.AnimNotifies);
		}

		// Here we have to get weird... we want to call something like FAnimNotifyQueue::Reset but we have two problems
		// - We don't want to use a skeletal mesh component! (Easily fixed, it's only used for LOD info and a world ptr here
		// - The function is... Not exported! (Oh, cool...) so we do InNotifyQueue.Reset(SkelMeshComponent); manually
		{
			InNotifyQueue.AnimNotifies.Reset();
			InNotifyQueue.UnfilteredMontageAnimNotifies.Reset();

			// According to the code this is what LOD level bones were updated with... I was not aware these even had LODs
			// Either way we don't really have a way of dealing with those as it is now
			InNotifyQueue.PredictedLODLevel = InPredictedLODLevel;

			// Why is there a world pointer in here you ask? Literally just to check if it's a dedicated server. 
			// Why do this for every single anim notify and not just cache the result of GetNetMode? Oh well, anim notifies are fairly infrequent
			MFUTILS_GET_PRIVATE(FAnimNotifyQueue, InNotifyQueue, World) = InWorldWeak;
		}
	}

	void PerformAnimationProcessing(TNotNull<const USkeletalMesh*> InSkeletalMesh,
	                                TNotNull<UAnimInstance*> InAnimInstance,
	                                bool bInDoEvaluation,
	                                bool bInForceRefPose,
	                                TArray<FTransform>& OutComponentSpaceTransforms,
	                                TArray<FTransform>& OutBoneSpaceBoneTransforms,
	                                FVector& OutRootBoneTranslationDelta,
	                                FBlendedHeapCurve& OutCurve,
	                                UE::Anim::FMeshAttributeContainer& OutAttributes,
	                                const TArray<FBoneIndexType>& FillComponentSpaceTransformsRequiredBones) {
		TRACE_CPUPROFILER_EVENT_SCOPE(PerformAnimationProcessing);

		// update anim instance (not sure if this branch makes sense now because we just... always set this to true in my examples)
		if (InAnimInstance->NeedsUpdate()) {
			// UAnimInstance::ParallelUpdateAnimation just calls GetProxyOnAnyThread and UpdateAnimation
			// However,  we don't like GetProxyOnAnyThread as it is tries to complete pending animation tasks on the gamethread
			FAnimInstanceProxy*& AnimInstanceProxy = MFUTILS_GET_PRIVATE(UAnimInstance, *InAnimInstance, AnimInstanceProxy);
			MFUTILS_CALL_PRIVATE(FAnimInstanceProxy, *AnimInstanceProxy, UpdateAnimation);
		}

		// @todo post process anim instance support
		// if(bShouldPostUpdatePostProcessInstance)
		// {
		// 	MyPostProcessAnimInstance->ParallelUpdateAnimation();
		// }

		// Evaluation is optional and also requires to having any bones to update
		if (bInDoEvaluation && OutComponentSpaceTransforms.Num() > 0) {
			FMemMark Mark(FMemStack::Get());
			FCompactPose NewEvaluatedPose;

			UE::Anim::FHeapAttributeContainer NewAttributes;

			// evaluate pure animations, and fill up BoneSpaceTransforms
			// This mimics EvaluateAnimation(...
			FParallelEvaluationData EvaluationData = {OutCurve, NewEvaluatedPose, NewAttributes};
			InAnimInstance->ParallelEvaluateAnimation(bInForceRefPose, InSkeletalMesh, EvaluationData);

			// @todo post process anim instance support (EvaluatePostProcessMeshInstance(...)

			// Finalize the transforms from the evaluation
			// Our goal is to mimic USkeletalMeshComponent::FinalizePoseEvaluationResult
			{
				// Fill out gaps in OutBoneSpaceBoneTransforms with the reference pose if it is valid
				const TArray<FTransform>& RefBonePoseTransforms = InSkeletalMesh->GetRefSkeleton().GetRefBonePose();
				if (NewEvaluatedPose.IsValid() && NewEvaluatedPose.GetNumBones() > 0) {
					// I'm skeptical on if you actually need to call this here every time...
					NewEvaluatedPose.NormalizeRotations();

					// Since the index array is in increasing order it follows the refernece pose index
					// The idea here is to use the reference 
					const auto FillReferencePose = [&OutBoneSpaceBoneTransforms, &RefBonePoseTransforms ](
						const int32 BeginIndex,
						const int32 EndIndex) {
						for (int32 MeshPoseIndex = BeginIndex; MeshPoseIndex < EndIndex; ++MeshPoseIndex) {
							OutBoneSpaceBoneTransforms[MeshPoseIndex] = RefBonePoseTransforms[MeshPoseIndex];
						}
					};

					int32 LastEvaluatedMeshPoseIndex = 0;
					const int32 RefBoneTransformsCountCount = RefBonePoseTransforms.Num();
					OutBoneSpaceBoneTransforms.SetNum(RefBoneTransformsCountCount);
					for (const FCompactPoseBoneIndex BoneIndex : NewEvaluatedPose.ForEachBoneIndex()) {
						const int32 MeshPoseIndex = NewEvaluatedPose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex).GetInt();
						FillReferencePose(LastEvaluatedMeshPoseIndex, MeshPoseIndex);
						OutBoneSpaceBoneTransforms[MeshPoseIndex] = NewEvaluatedPose[BoneIndex];
						LastEvaluatedMeshPoseIndex = MeshPoseIndex + 1;
					}
					FillReferencePose(LastEvaluatedMeshPoseIndex, RefBoneTransformsCountCount);
				}
				else {
					OutBoneSpaceBoneTransforms = RefBonePoseTransforms;
				}

				// The root bone is... The first bone! So we just compare the difference here
				OutRootBoneTranslationDelta = OutBoneSpaceBoneTransforms[0].GetTranslation() - RefBonePoseTransforms[0].GetTranslation();
			}

			if (NewEvaluatedPose.IsValid()) {
				// Copy the attributes into the returned attribute container
				// this mimics FinalizeAttributeEvaluationResults
				OutAttributes.CopyFrom(NewAttributes, NewEvaluatedPose.GetBoneContainer());
			}

			// Now to actually make the component space trnasform in well, component space we now run some matrix math on them
			// Probably the most SIMD-friendly code imaginable so it actually uses ispc::FillComponentSpaceTransforms internally (neat)
			InSkeletalMesh->FillComponentSpaceTransforms(OutBoneSpaceBoneTransforms,
			                                             FillComponentSpaceTransformsRequiredBones,
			                                             OutComponentSpaceTransforms);
		}
	}

	bool DoesAnimInstanceHaveNotifiesOrDelegateCallbacksToTrigger(UAnimInstance& InAnimInstance) {
		// Anim notifies:
		{
			// Note we could arguably use HandleNotify and ShouldTriggerAnimNotifyState to filter on the task thread

			// We check for 
			// - Anything in the NotifyQueue's AnimNotifies (could be states OR new non-state notifies)
			// - Any active states (this might mean a this is firing more often than I would like) 
			if (InAnimInstance.NotifyQueue.AnimNotifies.Num() > 0) {
				return true;
			}

			// ActiveAnimNotifyState could be ending or just in need of their ticking callback
			if (InAnimInstance.ActiveAnimNotifyState.Num() > 0) {
				return true;
			}
		}

		// Montages events:
		{
			// BlendingOut events
			// InAnimInstance.QueuedMontageBlendingOutEvents;
			if (MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, QueuedMontageBlendingOutEvents).Num() > 0) {
				return true;
			}

			//  BlendedIn events
			// InAnimInstance.QueuedMontageBlendedInEvents;
			if (MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, QueuedMontageBlendedInEvents).Num() > 0) {
				return true;
			}

			//  Ended Events 
			// InAnimInstance.QueuedMontageEndedEvents;
			if (MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, QueuedMontageEndedEvents).Num() > 0) {
				return true;
			}

			// Section Changed Events
			// InAnimInstance.QueuedMontageSectionChangedEvents;
			if (MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, QueuedMontageSectionChangedEvents).Num() > 0) {
				return true;
			}
		}

		return false;
	}

	void SkeletalMeshComponent_RecalcBonesAndCurvesIfNeeded(USkeletalMeshComponent& InSkeletalMeshComp) {
		// In here we just update the the RequiredBones array or curves if required
		// Note that RecalcRequiredBones will do RecalcRequiredCurves for you
		if (!InSkeletalMeshComp.bRequiredBonesUpToDate) {
			TRACE_CPUPROFILER_EVENT_SCOPE(RecalcRequiredBones);
			InSkeletalMeshComp.RecalcRequiredBones(InSkeletalMeshComp.GetPredictedLODLevel());
		}
		else if (!MFUTILS_CALL_PRIVATE(USkeletalMeshComponent, InSkeletalMeshComp, AreRequiredCurvesUpToDate)) {
			TRACE_CPUPROFILER_EVENT_SCOPE(RecalcRequiredCurves);
			InSkeletalMeshComp.RecalcRequiredCurves();
		}
	}

	void InterpolateBonesAndAttributes(float Alpha,
	                                   TNotNull<USkinnedAsset*> InSkinnedAsset,
	                                   TNotNull<UAnimInstance*> InAnimInstance,
	                                   FMegafunkUtilsAnimationEvaluationContainer& InOutFromState,
	                                   const FMegafunkUtilsAnimationEvaluationContainer& InInterpToState) {
#if WITH_EDITOR
		if (InSkinnedAsset->IsCompiling()) {
			ensure(false);
			return;
		}
#endif


		// Interpolate the bones transforms and other values like attributes and curves
		TRACE_CPUPROFILER_EVENT_SCOPE(InterpolateBonesAndAttributes);
		// In my experience this tends to be quite cheap but I st

		const TArray<FBoneIndexType>& InstanceRequiredBones = MegafunkUtils::Anim::GetProxyOnAnyThread_Direct(InAnimInstance).GetRequiredBones().
			GetBoneIndicesArray();

		if (InOutFromState.BoneSpaceTransforms.Num() != InInterpToState.BoneSpaceTransforms.Num()) {
			InOutFromState.BoneSpaceTransforms.SetNum(InInterpToState.BoneSpaceTransforms.Num());
		}

		if (!InInterpToState.BoneSpaceTransforms.IsEmpty()) {
			FAnimationRuntime::LerpBoneTransforms(InOutFromState.BoneSpaceTransforms,
			                                      InInterpToState.BoneSpaceTransforms,
			                                      Alpha,
			                                      InstanceRequiredBones);

			InSkinnedAsset->FillComponentSpaceTransforms(InOutFromState.BoneSpaceTransforms,
			                                             InstanceRequiredBones,
			                                             InOutFromState.ComponentSpaceTransforms);
		}

		// Interpolate curves
		InOutFromState.Curve.LerpTo(InInterpToState.Curve, Alpha);
		// Interpolate attributes  
		UE::Anim::Attributes::InterpolateAttributes(InOutFromState.Attributes, InInterpToState.Attributes, Alpha);
	}


	void AnimInstance_PreUpdateAnimSubsystems_GameThread(UAnimInstance& InAnimInstance, float DeltaSeconds) {
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(InAnimInstance.GetClass())) {
			AnimBlueprintClass->ForEachSubsystem(&InAnimInstance,
			                                     [&](const FAnimSubsystemInstanceContext& InContext) {
				                                     FAnimSubsystemUpdateContext Context(InContext, &InAnimInstance, DeltaSeconds);
				                                     // PRE update
				                                     InContext.Subsystem.OnPreUpdate_GameThread(Context);
				                                     return EAnimSubsystemEnumeration::Continue;
			                                     });
		}
	}

	void AnimInstance_PostUpdateAnimSubsystems_GameThread(UAnimInstance& InAnimInstance, float DeltaSeconds) {
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(InAnimInstance.GetClass())) {
			AnimBlueprintClass->ForEachSubsystem(&InAnimInstance,
			                                     [&](const FAnimSubsystemInstanceContext& InContext) {
				                                     FAnimSubsystemUpdateContext Context(InContext, &InAnimInstance, DeltaSeconds);
				                                     // POST update
				                                     InContext.Subsystem.OnPostUpdate_GameThread(Context);
				                                     return EAnimSubsystemEnumeration::Continue;
			                                     });
		}
	}

	UAnimInstance* Experimental_ManualAnimInstanceAllocAndInit(USkeletalMeshComponent& InOuterSkelMesh,
	                                                           const TSubclassOf<UAnimInstance> InAnimInstanceClass,
	                                                           USkeleton& InSkeleton,
	                                                           const TSharedRef<FBoneContainer> InRequiredBonesContainer,
	                                                           const UE::Anim::FCurveFilterSettings& InCurveFilterSettings,
	                                                           const TSharedPtr<FSkelMeshRefPoseOverride> InRefPoseOverride) {
		TRACE_CPUPROFILER_EVENT_SCOPE(Experimental_AnimInstanceInit);

		if (!ensure(InAnimInstanceClass)) {
			return nullptr;
		}

		// Not clue if I want to defer this or not
		const bool bInDeferRootNodeInitialization = true;


		// An insane bodge: we want to skip this signing up for reinstancing. 
		// We implement a custom constructor when invoking this from the editor that fixes this signing up for a global delegate unsafely
		UAnimInstance* NewAnimInstance;
		{
			FGCScopeGuard GCGuard; //You of course cannot create a new object safely while GC is running, shocker
			NewAnimInstance = NewObject<UAnimInstance>(&InOuterSkelMesh, InAnimInstanceClass, NAME_None);
		}

		if (!NewAnimInstance)
		{
			return nullptr;
		}
		
		// The regular unreal case resets linked anim instances here, but we don't use those.

		// Now for the hard part... we want to do UAnimInstance::InitializeAnimation but it has many gamethread access calls
		{
			FScopeCycleCounterUObject ContextScope(NewAnimInstance);
			LLM_SCOPE(ELLMTag::Animation);

			// we don't need to call UninitializeAnimation for now
			// NewAnimInstance->UninitializeAnimation();

			// we want to do NewAnimInstance->bUninitialized = false;
			// UAnimInstance::bUninitialized is a bitfield with the layout of...
			struct BitFieldCheese
			{
				uint8 bUpdateAnimationEnabled;
				uint8 bQueueMontageEvents : 1;
				uint8 bUninitialized : 1;
			};

			// So because epic forgot to make bUpdateAnimationEnabled a bitfield we can piggyback off of it as a pointer and bonk bUninitialized
			// As a note... we also don't really NEED this do we? It seems mostly for montages which are not required by my usecase

			uint8* PtrToUpdateAnimEnabled = &MFUTILS_GET_PRIVATE(UAnimInstance, *NewAnimInstance, bUpdateAnimationEnabled);
			BitFieldCheese* CheesePointer = reinterpret_cast<BitFieldCheese*>(PtrToUpdateAnimEnabled);

			CheesePointer->bUninitialized = false;
			ensure(NewAnimInstance->IsInitialized());
			// If this ensure fired we failed to write the intended memory! Very bad news all things considered...


			TRACE_OBJECT_LIFETIME_BEGIN(NewAnimInstance);

			// This skeleton had better be initialized... 
			// @todo we might need to use async access calls?

			NewAnimInstance->CurrentSkeleton = &InSkeleton;
			
#if WITH_EDITOR
			// in editor reset the snapshot buffer
			if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(NewAnimInstance->GetClass()))
			{
				NewAnimInstance->LifeTimer = 0.0;
				NewAnimInstance->CurrentLifeTimerScrubPosition = 0.0;

				if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Cast<UAnimBlueprintGeneratedClass>(AnimBlueprintClass)->ClassGeneratedBy))
				{
					if (Blueprint->GetObjectBeingDebugged() == NewAnimInstance)
					{
						Cast<UAnimBlueprintGeneratedClass>(AnimBlueprintClass)->GetAnimBlueprintDebugData().ResetSnapshotBuffer();
					}
				}
			}
#endif

			FAnimInstanceProxy& AnimProxy = GetProxyOnAnyThread_Direct(NewAnimInstance);

			// recalc required bones for init
			// unfortunately yet again AnimProxy.RecalcRequiredBones(SkelMeshComp, CurrentSkeleton); uses gamethread access. We must replace it manually
			// We also need to pass in the skeletal mesh reqruied bones more... indirectly
			{
				//@todo what is the distinction between requiredbones container and the RequiredBones field of a skeletal mesh component?
				static_cast<AnimInstanceProxyProtectedAccess&>(AnimProxy).RefreshRequiredBones(
					InRequiredBonesContainer,
					InRequiredBonesContainer->GetBoneIndicesArray(),
					InCurveFilterSettings,
					InRefPoseOverride,
					InSkeleton);
			}


			MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, AnimProxy, Initialize, NewAnimInstance);

			// If the skeleton ptr was not set earlier we should just use ours
			USkeleton*& InstanceSkeletonPtr = MFUTILS_GET_PRIVATE(FAnimInstanceProxy, AnimProxy, Skeleton);
			if (!InstanceSkeletonPtr)
			{
				InstanceSkeletonPtr = &InSkeleton;
			}


			{
#if DO_CHECK
				// This is mainly for linked layer validation.... I do not feel like exposing this bitfield for no reason then
				// FGuardValue_Bitfield(NewAnimInstance->bInitializing, true);
#endif

				NewAnimInstance->NativeInitializeAnimation();
				NewAnimInstance->BlueprintInitializeAnimation();
			}

			// @fixme aburd fake gamethread scope to try to fight engine checks
			MFUTILS_CALL_PRIVATE_ARGS(FAnimInstanceProxy, AnimProxy, InitializeRootNode, bInDeferRootNodeInitialization);

			// After init unreal binds native delegates. This appears to be how state transitions work?
			// AnimProxy.BindNativeDelegates();
			MFUTILS_CALL_PRIVATE(FAnimInstanceProxy, AnimProxy, BindNativeDelegates);

			// @todo linked layers are not support because this codepath will 
			// NewAnimInstance->InitializeGroupedLayers(bInDeferRootNodeInitialization);

			NewAnimInstance->NativeLinkedAnimationLayersInitialized();
			NewAnimInstance->BlueprintLinkedAnimationLayersInitialized();
		}

		// Normtally we call these on a new anim instance crated at runtime but we do not need this
		// NewAnimScriptInstance->NativeBeginPlay();
		// NewAnimScriptInstance->BlueprintBeginPlay();

		return NewAnimInstance;
		
	}



	void Experimental_AnimInstanceOnlyFullUpdate(const float DeltaTime,
	                                             bool bInDoEvaluation,
	                                             FAnimationEvaluationContext& InEvalContext,
	                                             UAnimInstance& InAnimInstance,
	                                             const USkeletalMesh& InSkeletalMesh,
	                                             TArray<FTransform>& OutComponentSpaceTransforms,
	                                             TArray<FTransform>& OutBonesSpaceTransforms,
	                                             FVector& OutRootBoneLocationResult,
	                                             FBlendedHeapCurve& OutCachedCurve,
	                                             UE::Anim::FMeshAttributeContainer& OutAttributes,
	                                             const TArray<FBoneIndexType>& FillComponentSpaceTransformsRequiredBones) {
		// This block is similar to USkeletalMeshComponent::TickAnimation and typically done from a task thread
		// We don't care about skeletal mesh specific pieces here so we 
		{
			// SkeletalMeshComponent_RecalcBonesAndCurvesIfNeeded(InSkeletalMeshComponent);

			AnimInstance_TickAnimInstancesOnAnyThread(InAnimInstance, DeltaTime);
		}


		constexpr bool bInForceRefPose = false;

		TRACE_CPUPROFILER_EVENT_SCOPE(Experimental_AnimInstanceOnly PerformAnimationProcessing);

		FAnimInstanceProxy*& AnimInstanceProxy = MFUTILS_GET_PRIVATE(UAnimInstance, InAnimInstance, AnimInstanceProxy);
		
		// In a small departure from the normal setup we need to forcibly set the skeleton here
		// Because we might not have a normal skeletal mesh comp owner (hte idea here is to let you use this without a normal skeletal mesh component)
		// @todo arguably htis should be in PreUpdate steps
		
		// a shameful const cast but it is the least absurd hack in this file
		MFUTILS_GET_PRIVATE(FAnimInstanceProxy, *AnimInstanceProxy, Skeleton) = const_cast<USkeleton*>(InSkeletalMesh.GetSkeleton());
		
		// update anim instance (not sure if this branch makes sense now because we just... always set this to true in my examples)
		if (InAnimInstance.NeedsUpdate()) {
			// UAnimInstance::ParallelUpdateAnimation just calls GetProxyOnAnyThread and UpdateAnimation
			// However,  we don't like GetProxyOnAnyThread as it is tries to complete pending animation tasks on the gamethread
			MFUTILS_CALL_PRIVATE(FAnimInstanceProxy, *AnimInstanceProxy, UpdateAnimation);
		}

		// @todo post process anim instance support
		// if(bShouldPostUpdatePostProcessInstance)
		// {
		// 	MyPostProcessAnimInstance->ParallelUpdateAnimation();
		// }

		// Evaluation is optional and also requires to having any bones to update
		if (bInDoEvaluation && OutComponentSpaceTransforms.Num() > 0) {
			// Artilly-specific validation
			if (!ensure(AnimInstanceProxy->GetRequiredBones().IsValid())) {
				return;
			}


			// FCompactPose uses a stack allocator that we make in a scope here... perhaps wasteful?
			FMemMark Mark(FMemStack::Get());
			FCompactPose NewEvaluatedPose;

			UE::Anim::FHeapAttributeContainer NewAttributes;

			// evaluate pure animations, and fill up BoneSpaceTransforms
			// This mimics EvaluateAnimation(...
			FParallelEvaluationData EvaluationData = {OutCachedCurve, NewEvaluatedPose, NewAttributes};
			InAnimInstance.ParallelEvaluateAnimation(bInForceRefPose, &InSkeletalMesh, EvaluationData);

			// @todo post process anim instance support (EvaluatePostProcessMeshInstance(...)

			// Finalize the transforms from the evaluation
			// Our goal is to mimic USkeletalMeshComponent::FinalizePoseEvaluationResult
			{
				// Fill out gaps in OutBoneSpaceBoneTransforms with the reference pose if it is valid
				const TArray<FTransform>& RefBonePoseTransforms = InSkeletalMesh.GetRefSkeleton().GetRefBonePose();
				if (NewEvaluatedPose.IsValid() && NewEvaluatedPose.GetNumBones() > 0) {
					// I'm skeptical on if you actually need to call this here every time...
					NewEvaluatedPose.NormalizeRotations();

					// Since the index array is in increasing order it follows the refernece pose index
					// The idea here is to use the reference 
					const auto FillReferencePose = [&OutBonesSpaceTransforms, &RefBonePoseTransforms ](const int32 BeginIndex, const int32 EndIndex) {
						for (int32 MeshPoseIndex = BeginIndex; MeshPoseIndex < EndIndex; ++MeshPoseIndex) {
							OutBonesSpaceTransforms[MeshPoseIndex] = RefBonePoseTransforms[MeshPoseIndex];
						}
					};

					int32 LastEvaluatedMeshPoseIndex = 0;
					const int32 RefBoneTransformsCountCount = RefBonePoseTransforms.Num();
					OutBonesSpaceTransforms.SetNum(RefBoneTransformsCountCount);
					for (const FCompactPoseBoneIndex BoneIndex : NewEvaluatedPose.ForEachBoneIndex()) {
						const int32 MeshPoseIndex = NewEvaluatedPose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex).GetInt();
						FillReferencePose(LastEvaluatedMeshPoseIndex, MeshPoseIndex);
						OutBonesSpaceTransforms[MeshPoseIndex] = NewEvaluatedPose[BoneIndex];
						LastEvaluatedMeshPoseIndex = MeshPoseIndex + 1;
					}
					FillReferencePose(LastEvaluatedMeshPoseIndex, RefBoneTransformsCountCount);
				}
				else {
					OutBonesSpaceTransforms = RefBonePoseTransforms;
				}

				// The root bone is... The first bone! So we just compare the difference here
				OutRootBoneLocationResult = OutBonesSpaceTransforms[0].GetTranslation() - RefBonePoseTransforms[0].GetTranslation();
			}

			if (NewEvaluatedPose.IsValid()) {
				// Copy the attributes into the returned attribute container
				// this mimics FinalizeAttributeEvaluationResults
				OutAttributes.CopyFrom(NewAttributes, NewEvaluatedPose.GetBoneContainer());
			}

			// Now to actually make the component space trnasform in well, component space we now run some matrix math on them
			// Probably the most SIMD-friendly code imaginable so it actually uses ispc::FillComponentSpaceTransforms internally (neat)
			InSkeletalMesh.FillComponentSpaceTransforms(OutBonesSpaceTransforms,
			                                            FillComponentSpaceTransformsRequiredBones,
			                                            OutComponentSpaceTransforms);
		}


		// InEvalContext.
		InAnimInstance.UpdateCurvesToEvaluationContext(InEvalContext);

		{
			// post anim evaluation code from  UAnimInstance::PostUpdateAnimation
			MegafunkUtils::Anim::AnimInstance_PostUpdateAnimationAnyThread(InAnimInstance);
		}
	}
	
	void Experimental_ComputeRequiredBonesWithoutSkeletalMeshComp(
		const USkeletalMesh& InSkeletalMesh,
		TArray<FBoneIndexType>& InOutRequiredBones,
															 TArray<FBoneIndexType>& InOutFillComponentSpaceTransformsRequiredBones,
															 int32 InSkeletonLODIndex,
															 bool bInIgnorePhysicsAsset)
	{
		// This is mostly USkeletalMeshComponent::ComputeRequiredBones but it skips the physics asset step
		InOutRequiredBones.Reset();
		InOutFillComponentSpaceTransformsRequiredBones.Reset();
		
		FSkeletalMeshRenderData* RenderData = InSkeletalMesh.GetResourceForRendering();
		if (!RenderData)
		{
			// No render data! We are cooked...
			return;
		}
		
		if (RenderData->LODRenderData.IsEmpty())
		{
			// We want LOD data!
			return;
		}
		
		// We can't use an index higher than the actual lods of ocurse
		InSkeletonLODIndex = FMath::Clamp(InSkeletonLODIndex, 0 , RenderData->LODRenderData.Num() - 1);
		
		FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[InSkeletonLODIndex];
		InOutRequiredBones = LODData.RequiredBones;

		const FReferenceSkeleton& RefSkeleton = InSkeletalMesh.RefSkeleton;
		
		// Append virutal bones
		USkinnedMeshComponent::MergeInBoneIndexArrays(InOutRequiredBones, RefSkeleton.GetRequiredVirtualBones());

		
		// Appent bones that are required for sockets 
		TArray<FBoneIndexType> NeededBonesForFillComponentSpaceTransforms;
		USkinnedMeshComponent::GetSocketRequiredBones(&InSkeletalMesh, InOutRequiredBones, NeededBonesForFillComponentSpaceTransforms);
		
		
		FAnimationRuntime::EnsureParentsPresent(InOutRequiredBones, RefSkeleton);
		InOutRequiredBones.Sort();
		
		// Now that we have the required bones, we fill in InOutFillComponentSpaceTransformsRequiredBones by combing them 
		InOutFillComponentSpaceTransformsRequiredBones.Reset(InOutRequiredBones.Num() + NeededBonesForFillComponentSpaceTransforms.Num());
		InOutFillComponentSpaceTransformsRequiredBones = InOutRequiredBones;

		NeededBonesForFillComponentSpaceTransforms.Sort();
		USkinnedMeshComponent::MergeInBoneIndexArrays(InOutFillComponentSpaceTransformsRequiredBones, NeededBonesForFillComponentSpaceTransforms);
		FAnimationRuntime::EnsureParentsPresent(InOutFillComponentSpaceTransformsRequiredBones, RefSkeleton);
	}


	bool ValidateAnimInstanceBeingSafeToUseInParallelForExample(const TSubclassOf<UAnimInstance> InAnimInstanceClass,
	                                                            bool bValidateAsycSpawn,
	                                                            bool bLogErrors) {
		if (!ensure(InAnimInstanceClass.Get())) {
			return false;
		}
		
		bool bAnyErrors = false;

		// per-anim-update bp functions that work on the main thread only are generally unsafe
		auto ErrorIfBPFunctionImplemented = [&](const FName FuncName) {
			if (MegafunkUtils::IsFunctionImplementedByBP(FuncName, *InAnimInstanceClass.Get())) {
				UE_CLOGFMT(bLogErrors,
				           LogMegafunkUtils,
				           Error,
				           "Parallel anim ticking is not safe for anim instance class {class} because it implememnts {FuncName}"
				           "Please use BlueprintThreadSafeUpdateAnimation or in-anim graph bp calls if you need BP functionality for each anim update",
				           InAnimInstanceClass->GetName(),
				           FuncName);
				return false;
			}

			return true;
		};

		ErrorIfBPFunctionImplemented(GET_FUNCTION_NAME_CHECKED(UAnimInstance, BlueprintUpdateAnimation));
		ErrorIfBPFunctionImplemented(GET_FUNCTION_NAME_CHECKED(UAnimInstance, BlueprintPostEvaluateAnimation));
		
		
		
		IAnimClassInterface* AnimClassInterface = IAnimClassInterface::GetFromClass(InAnimInstanceClass);
		
		
		// For async spawn we want to complain about some specific anim nodes. This will not be an exhaustive list
		if (bValidateAsycSpawn) {
			auto AnimInstanceCDO = InAnimInstanceClass.GetDefaultObject();
			if (ensure(AnimClassInterface)) {
				
				TArray<UScriptStruct*, TFixedAllocator<1>> DisallowedAnimNodes;
				DisallowedAnimNodes = {FAnimNode_ControlRig::StaticStruct()};
				
				auto IsDisallowedNode = [&](const FStructProperty* NodeProperty) {
					for (UScriptStruct* DisallowedAnimNode : DisallowedAnimNodes) {
						if (NodeProperty->Struct->IsChildOf(DisallowedAnimNode)) {
							UE_CLOGFMT(bLogErrors,
							   LogMegafunkUtils,
							   Error,
							   "Parallel anim instance spawning is not safe for anim instance class {class} because it uses disallowed anim node {nodename}",
							   InAnimInstanceClass->GetName(),
							   NodeProperty->Struct.GetName());
							return true;
						}
					}
					
					return false;
				};
				
				// for (const FStructProperty* NodeProperty : AnimClassInterface->GetInitializationNodeProperties())
				// {
				// 	bAnyErrors |= IsDisallowedNode(NodeProperty);
				// }			
				
				for (const FStructProperty* NodeProperty : AnimClassInterface->GetAnimNodeProperties())
				{
					// FAnimNode_Base* AnimNode = NodeProperty->ContainerPtrToValuePtr<FAnimNode_Base>(AnimInstanceCDO);
					bAnyErrors |= IsDisallowedNode(NodeProperty);
				}
			}
		}

#if WITH_EDITOR
		// Only editor builds need to care about this wacky constructor overloading fun
		if (bValidateAsycSpawn) {
			
			const bool bIsConstructorSafe = IsAnimInstanceClassConstructorUsingOurThreadSafeVersion(InAnimInstanceClass);
			ensureMsgf(bIsConstructorSafe, TEXT("Anim Class %s is not safe to spawn async in the editor! "
									   "Please use our hacky SetAnimInstanceClassToUseAsyncSafeConstructor template for this class on module startup"), *InAnimInstanceClass->GetName());
		}

		// Even if the BP uses the correct BP thread safe anim update, some property accesses can be done in a batch on the gamethread
		// determine if FAnimSubsystem_PropertyAccess::OnPostUpdate_GameThread or FAnimSubsystem_PropertyAccess::OnPreUpdate_GameThread would do anything if called
		// This is visible in each property access BP node in the editor... Might make a better validation message later for compilation if there is demand for it

		const bool bIsAnimBlueprint = InAnimInstanceClass->ClassGeneratedBy ? InAnimInstanceClass->ClassGeneratedBy->IsA<UAnimBlueprint>() : false;
		if (bIsAnimBlueprint && AnimClassInterface) {
			const FAnimSubsystem_PropertyAccess& PropertyAccessSubsystem = AnimClassInterface->GetSubsystem<FAnimSubsystem_PropertyAccess>();

			const FPropertyAccessLibrary& Library = PropertyAccessSubsystem.GetLibrary();
			const TArray<FPropertyAccessCopyBatch>& CopyBatches = MFUTILS_GET_PRIVATE(FPropertyAccessLibrary, Library, CopyBatchArray);

			auto CheckBatchByEnumID = [&](EAnimPropertyAccessCallSite CallSiteEnum) {
				const PropertyAccess::FCopyBatchId CopyBatchIDGameThread_PreEventGraph = PropertyAccess::FCopyBatchId((int32)CallSiteEnum);

				if (!CopyBatches.IsValidIndex(CopyBatchIDGameThread_PreEventGraph.Id)) {
					return;
				}

				FString AccessDebugString = "";
				const TArray<FPropertyAccessCopy>& Batch = MFUTILS_GET_PRIVATE(FPropertyAccessCopyBatch,
				                                                               CopyBatches[CopyBatchIDGameThread_PreEventGraph.Id],
				                                                               Copies);
				for (const FPropertyAccessCopy& PropertyAccessCopy : Batch) {
					const int32 AccessIndex = MFUTILS_GET_PRIVATE(FPropertyAccessCopy, PropertyAccessCopy, AccessIndex);
					const TArray<FPropertyAccessIndirectionChain>& SrcAccesses = MFUTILS_GET_PRIVATE(FPropertyAccessLibrary, Library, SrcAccesses);
					if (SrcAccesses.IsValidIndex(AccessIndex)) {
						if (FProperty* SourceProperty = MFUTILS_GET_PRIVATE(FPropertyAccessIndirectionChain, SrcAccesses[AccessIndex], Property).
							Get()) {
							AccessDebugString = SourceProperty->GetFullGroupName(true);
						}
						else {
							AccessDebugString = "unknown";
						}
					}
				}

				if (bLogErrors) {
					// This is not actually a reason to outright prevent ticking... kind of
					// bAnyErrors |= true;
					const FString EnumString = StaticEnum<EAnimPropertyAccessCallSite>()->GetAuthoredNameStringByValue((int64)CallSiteEnum);

					UE_LOGFMT(LogMegafunkUtils,
					          Warning,
					          "Anim instance class {class} might be partially unsafe to tick in a threaded way because property access {access} uses the game thread ({enum})"
					          "It is possible that you could call this manually safely by using AnimInstancePreUpdateAnimSubsystems_GameThread (and the Post version)"
					          "Generally this is the result of using object reference based property access nodes in the graph",
					          InAnimInstanceClass->GetName(),
					          AccessDebugString,
					          EnumString);
				}
			};

			CheckBatchByEnumID(EAnimPropertyAccessCallSite::GameThread_Batched_PreEventGraph);
			CheckBatchByEnumID(EAnimPropertyAccessCallSite::GameThread_Batched_PostEventGraph);
		}
#endif

		if (bAnyErrors) {
			return false;
		}

		return true;
	}
}
