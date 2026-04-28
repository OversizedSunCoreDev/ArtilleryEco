// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim/MegafunkUtilsExampleAnimSubsystem.h"

#include "Anim/MegafunkAnimUtilsTypes.h"
#include "Anim/MegafunkUtilsExampleSkeletalMeshComp.h"
#if WITH_EDITOR
#include "SkinnedAssetCompiler.h"
#endif
#include "Anim/MegafunkUtilsAnimAccessors.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Misc/EngineVersionComparison.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MegafunkUtilsExampleAnimSubsystem)

static FAutoConsoleVariableRef CVaregafunkUtilsExampleEnabled(
	TEXT("MegafunkUtils.ExampleSkeletalMeshManager.Enabled"),
	GbMegafunkUtilsExampleSkeletalMeshManagerEnabled,
	TEXT("If the ExampleSkeletalMeshManager is enabled and ticks managed skeletal meshes in parallel. Enabled by default"),
	ECVF_Default);

static FAutoConsoleVariableRef CVaregafunkUtilsExampleChaosUpdateEnabled(
	TEXT("MegafunkUtils.ExampleSkeletalMeshManager.UpdateChaosBodies"),
	GbMegafunkUtilsExampleSkeletalMeshManagerChaosUpdateEnabled,
	TEXT(
		"If the ExampleSkeletalMeshManager tries to update chaos bodies from bone transforms. Without this physics assets/hitboxies are not updated. Off by default"),
	ECVF_Default);


static FAutoConsoleVariableRef CVarMegafunkUtilsExampleMontageAndAnimNotifyMainThreadCallbackEnabled(
	TEXT("MegafunkUtils.ExampleSkeletalMeshManager.MontageAndAnimNotifyMainThreadCallback"),
	GbMegafunkUtilsExampleMontageAndAnimNotifyMainThreadCallbackEnabled,
	TEXT("If the ExampleSkeletalMeshManager can call anim notifes and montage delegates in a queue for the main thread. On by default"),
	ECVF_Default);


static FAutoConsoleVariableRef CVarMegafunkUtilsExampleAnimInstanceManagerEnabled(
	TEXT("MegafunkUtils.ExampleSkeletalMeshManager.Experimental.AnimInstanceManagerEnabled"),
	GbMegafunkUtilsExampleAnimInstanceManagerEnabled,
	TEXT(""),
	ECVF_Default);

void FMFUtilsAnimManagerTick::UpdateSkeletalMeshExample(FMegafunkUtilsAnimationEvaluationContainer& OutResult,
                                                          UMegafunkUtilsExampleSkeletalMeshComp& Comp,
                                                          const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMegafunkUtilsManagerTick::UpdateSkeletalMeshExample);

	//both may be null for valid if silly reasons. ensures are thus not appropriate.
	UAnimInstance* AnimInstance = Comp.GetAnimInstance();
	if (AnimInstance) {
		return;
	}
	USkeletalMesh* SkeletalMesh = Comp.GetSkeletalMeshAsset();
	if (SkeletalMesh) {
		return;
	}

	MegafunkUtils::Anim::SkeletalMeshComponent_TickUpdateAnyThread(DeltaTime,
																   Comp,
																   *AnimInstance,
																   *SkeletalMesh,
																   OutResult.ComponentSpaceTransforms,
																   OutResult.BoneSpaceTransforms,
																   OutResult.OutRootBoneLocationResult,
																   OutResult.Curve,
																   OutResult.Attributes);

	// @todo we are going to introduce more specific interp support later. for now this is mostly a show of force where we evaluate every frame
	constexpr bool bDoInterp = false;
	if (bDoInterp) {
		// You of course would need previous state
		FMegafunkUtilsAnimationEvaluationContainer OldBones = OutResult;
		const float Alpha = DeltaTime;
		// Comp.MegafunkInterpolateBones(DeltaTime, *Comp.GetSkeletalMeshAsset(),OldBones, OutEvalResult);
		MegafunkUtils::Anim::InterpolateBonesAndAttributes(Alpha, SkeletalMesh, AnimInstance, OutResult, OldBones);
	}
	else {
		//@todo a sad copy... Ideally we don't need to do this
		TRACE_CPUPROFILER_EVENT_SCOPE(CopyBoneTransformsToComponent)
		Comp.GetEditableComponentSpaceTransforms() = OutResult.ComponentSpaceTransforms;

		// Eventually we want to just... directly write them in
		// TArray<FTransform>& MutableBoneTransforms = const_cast<TArray<FTransform>&>(Comp.GetComponentSpaceTransforms());
		// MutableBoneTransforms = OutResult.ComponentSpaceTransforms;
		// Comp.BoneTransformUpdateMethodQueue.Add(EBoneTransformUpdateMethod::AnimationUpdate);

		// This macro helps us get away with accessing this directly without compile warnings (the property is not deprecated, but accessing it directly is protected behind some getters that we don't want to bother with)
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Comp.BoneSpaceTransforms = OutResult.BoneSpaceTransforms;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}


	MegafunkUtils::Anim::SkeletalMeshComponent_ForceFinalizeBoneTransform(Comp);
	// Send render data DIRECTLY. This is a large part of why no main thread step is required (we don't need to fight over a queue for later)
	MegafunkUtils::Anim::SkeletalMeshComponent_PushRenderUpdate(Comp);

	// clear out the AnimEvaluationContext (unsure if required, but clearing out pointers here that could get stale wise)
	auto Clearable = MegafunkUtils::Anim::GetComponentAnimationEvaluationContext(Comp);
	if (Clearable.AnimInstance)
	{
		Clearable.Clear();
	}
}

void FMFUtilsAnimManagerTick::ExecuteTick(float DeltaTime,
                                            ELevelTick TickType,
                                            ENamedThreads::Type CurrentThread,
                                            const FGraphEventRef& MyCompletionGraphEvent) {
	// Only do anything when we are enabled
	if (!GbMegafunkUtilsExampleSkeletalMeshManagerEnabled) {
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FMegafunkUtilsManagerTick::ExecuteTick);

	TArray<FMFUtilsManagedComponentStateExample>& States = Owner->ManagedComponentStates;

	if (GbMegafunkUtilsExampleSkeletalMeshManagerChaosUpdateEnabled) {
		FPhysScene* PhysicsScene = Owner->GetWorld()->GetPhysicsScene();
		TRACE_CPUPROFILER_EVENT_SCOPE(MegafunkUtils MarkForPreSimKinematicUpdate)

		// Add these to the list of chaos bodies that need to update. Very sad as it kind of defeats the purpose of course
		for (FMFUtilsManagedComponentStateExample& State : States) {
			if (FBodyInstance* MeshBody = State.SkelMeshComponent->GetBodyInstance()) {
				ECollisionEnabled::Type CollisionEnabled = MeshBody->GetCollisionEnabled();
				if (CollisionEnabled != ECollisionEnabled::PhysicsOnly && CollisionEnabled != ECollisionEnabled::NoCollision) {
					State.SkelMeshComponent->bDeferKinematicBoneUpdate = true;
					ETeleportType Teleport = ETeleportType::None;
					const bool MarkedforUpdate = PhysicsScene->MarkForPreSimKinematicUpdate(State.SkelMeshComponent, Teleport, false);
				}
			}
		}
	}

#if WITH_EDITOR
	// In editor builds skeletal meshes could be still compiling
	TArray<USkinnedAsset*, TInlineAllocator<16>> CompilingMeshAssets;
	for (FMFUtilsManagedComponentStateExample& State : States) {
		if (USkeletalMesh* MeshAsset = State.SkelMeshComponent->GetSkeletalMeshAsset()) {
			if (MeshAsset && MeshAsset->IsCompiling()) {
				CompilingMeshAssets.AddUnique(MeshAsset);
			}
		}
	}

	if (!CompilingMeshAssets.IsEmpty()) {
		// requires being called from the gamethread
		// This will actually wait on the compilation so we are darn sure nothing can get through
		FSkinnedAssetCompilingManager::Get().FinishCompilation(CompilingMeshAssets);
	}
#endif

	// A queue for anim notifies to get read back on the main thread (and montage delegates)
	// This is mostly an example for how you can KEEP main threaded code and use most of the benefits of parallel code
	// In an ideal situation you won't even need a mainthread step and can have notifies that just run in parallel

	// Keep in mind UAnimNotify::MeshContext makes notifying them in a threaded fashion inherently unsafe
	// @todo override UAnimNotify to make a nicer example for how to make them work in parallel
	TMpscQueue<USkeletalMeshComponent*> ComponentsWithAnimNotifiesOrCallbacks;


	/**
	 * A note on threading performance: 
	 * This is a simple but naive implementation because this makes one big assumption: 
	 *	That most anim work will be roughly equal
	 *				
	 * In reality you might have many cheaper elements and a few that are take vastly longer. 
	 * This might make the main thread wait on outliers that take much longer!
	 * In order to make that make better use of thread time you can try to use more tasks instead of just a ParallelFor
	 * or even just do other unrelated work in the main thread and move this ParallelFor to be launched from a task worker
	 */

	ParallelFor(States.Num(),
	            [this, &States, &ComponentsWithAnimNotifiesOrCallbacks, DeltaTime](const int32 Index) {
		            TRACE_CPUPROFILER_EVENT_SCOPE(MegafunkUtils ParallelAnimWork)

		            // @todo make the pre-setup parts only done once per-worker
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 7, 0)
		            FTaskTagScope Scope(ETaskTag::EParallelGameThread);
#else
		            // Optional task tag scope only replaces the tag scope if we aren't already tagged... which is the default in 5.7
		            FOptionalTaskTagScope Scope(ETaskTag::EParallelGameThread);
#endif

#if DO_BLUEPRINT_GUARD
		            if (!Scope.IsCurrentTag(ETaskTag::EGameThread)) {
			            // GInitRunAway is required to be called to call BP from task threads to make sure it resets recursive call tracking
			            // If you see dozens of BP warnings about recursive calls it means someone forgot to call this
			            GInitRunaway();
		            }
#endif

		            // Scoped floating point mode which is for consistency. The engine does this as well
		            FScopedFTZFloatMode FTZ;

		            FMFUtilsManagedComponentStateExample& State = States[Index];
		            UpdateSkeletalMeshExample(State.EvaluatedAnimState, *State.SkelMeshComponent, DeltaTime);


		            if (GbMegafunkUtilsExampleMontageAndAnimNotifyMainThreadCallbackEnabled) {
			            // Queue up anim notifies and montage callbacks
			            if (UAnimInstance* AnimInstance = State.SkelMeshComponent->GetAnimInstance()) {
				            if (MegafunkUtils::Anim::DoesAnimInstanceHaveNotifiesOrDelegateCallbacksToTrigger(*AnimInstance)) {
					            ComponentsWithAnimNotifiesOrCallbacks.Enqueue(States[Index].SkelMeshComponent);
				            }
			            }
		            }
	            });


	if (GbMegafunkUtilsExampleMontageAndAnimNotifyMainThreadCallbackEnabled) {
		TRACE_CPUPROFILER_EVENT_SCOPE(Dispatch Queued Anim Events)

		// Calling this only some of the time is awkward as we might risk not flushing old montage instances
		// If you notice montage instances sticking around too long consider enabling a.Montage.FlushCompletedMontagesOnPlay
		USkeletalMeshComponent* CompWithAnimNotifies;
		while (ComponentsWithAnimNotifiesOrCallbacks.Dequeue(CompWithAnimNotifies)) {
			CompWithAnimNotifies->ConditionallyDispatchQueuedAnimEvents();
		}
	}


	if (GbMegafunkUtilsExampleAnimInstanceManagerEnabled) {
		auto AnimInstanceOwnerActor = Owner->GetOrSpawnAnimInstanceOwnerActor();
		auto& AnimInstances = AnimInstanceOwnerActor->ManagedAnimInstances;
		ParallelFor(AnimInstances.Num(),
		            [&](const int32 Index) {
			            TRACE_CPUPROFILER_EVENT_SCOPE(MegafunkUtils ParallelAnimInstanceTick)

			            // @todo make the pre-setup parts only done once per-worker
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 7, 0)
			            FTaskTagScope Scope(ETaskTag::EParallelGameThread);
#else
			            FOptionalTaskTagScope Scope(ETaskTag::EParallelGameThread);
#endif

#if DO_BLUEPRINT_GUARD
			            if (!Scope.IsCurrentTag(ETaskTag::EGameThread)) {
				            GInitRunaway();
			            }
#endif
		            	
			            FScopedFTZFloatMode FTZ;

		            	
			            FMegafunkUtilsManagedAnimInstanceExample& AnimInstanceElement = AnimInstances[Index];
			            FAnimationEvaluationContext& EvalContext = AnimInstanceElement.EvaluationContext;
		            	
		            	if (EvalContext.ComponentSpaceTransforms.IsEmpty()) {
		            		EvalContext.ComponentSpaceTransforms.SetNumUninitialized(AnimInstanceElement.SkeletalMesh->RefSkeleton.GetNum());
		            	}
		            	
		            	// An absurdly experimental setup where we can CREATE anim instances on another thread. This will remove a huge bottleneck
		            	if (!AnimInstanceElement.AnimInstance) {
		            		
		            		AnimInstanceElement.AnimInstance = MegafunkUtils::Anim::Experimental_ManualAnimInstanceAllocAndInit(
								*AnimInstanceOwnerActor->SkeletalMeshAnimInstanceOwner,
								AnimInstanceElement.SkeletalMeshCompBaseline->AnimClass,
								*AnimInstanceElement.SkeletalMesh->GetSkeleton(),
								AnimInstanceElement.SkeletalMeshCompBaseline->GetSharedRequiredBones().ToSharedRef(),
								AnimInstanceElement.SkeletalMeshCompBaseline->GetCurveFilterSettings(),
								AnimInstanceElement.SkeletalMeshCompBaseline->GetRefPoseOverride()
								);
		            	}
		            	

			            MegafunkUtils::Anim::Experimental_AnimInstanceOnlyFullUpdate(DeltaTime,
			                                                                         true,
			                                                                         EvalContext,
			                                                                         *AnimInstanceElement.AnimInstance,
			                                                                         *AnimInstanceElement.SkeletalMesh,
			                                                                         EvalContext.ComponentSpaceTransforms,
			                                                                         EvalContext.BoneSpaceTransforms,
			                                                                         EvalContext.RootBoneTranslation,
			                                                                         EvalContext.Curve,
			                                                                         EvalContext.CustomAttributes,
			                                                                         AnimInstanceElement.FillComponentSpaceTransformsRequiredBones);
		            });
	}
}

FString FMFUtilsAnimManagerTick::DiagnosticMessage() {
	return "FMegafunkUtilsManagerTick tick diagnostic messsage!";
}

FName FMFUtilsAnimManagerTick::DiagnosticContext(bool bDetailed) {
	return "FMegafunkUtilsManagerTick DiagnosticContext!";
}


UMegafunkUtilsExampleAnimSubsystem* UMegafunkUtilsExampleAnimSubsystem::Get(UWorld* World) {
	return World->GetSubsystem<UMegafunkUtilsExampleAnimSubsystem>();
}

void UMegafunkUtilsExampleAnimSubsystem::AddComponent(UMegafunkUtilsExampleSkeletalMeshComp& InSkeletalMeshComp) {
	ManagedComponentStates.Add({.SkelMeshComponent = &InSkeletalMeshComp});
	
	if (GbMegafunkUtilsExampleAnimInstanceManagerEnabled) {
		if (USkeletalMesh* SkeletalMesh = InSkeletalMeshComp.GetSkeletalMeshAsset()) {
			USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
			AMFUtilsExperimentalAsyncAnimInstanceOwnerActor* AnimInstanceOwner = GetOrSpawnAnimInstanceOwnerActor();
			if (ensure(Skeleton) && ensure(AnimInstanceOwner)) {
				AnimInstanceOwner->ManagedAnimInstances.Add({
					nullptr, SkeletalMesh, &InSkeletalMeshComp, {}, InSkeletalMeshComp.FillComponentSpaceTransformsRequiredBones
				});
			}
		}
	}
}

void UMegafunkUtilsExampleAnimSubsystem::RemoveComponent(UMegafunkUtilsExampleSkeletalMeshComp& InSkeletalMeshComp) {
	for (int32 i = ManagedComponentStates.Num() - 1; i >= 0; --i) {
		if (ManagedComponentStates[i].SkelMeshComponent == &InSkeletalMeshComp) {
			ManagedComponentStates.RemoveAtSwap(i);
		}
	}
}

void UMegafunkUtilsExampleAnimSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);

	// This isn't a regular tickable world subsystem so I can have a ticking phase in relation to normal actor ticks
	// Animations ticking at the time we want is critical if you care about the their transforms for gameplay purposes. 
	// Otherwise, it might be fine to put this where you want
	ManagerTick.TickGroup = TG_PrePhysics; //Currently this is pre physics to get in front of deferred kinematic bone updates and other gaemaply code
	ManagerTick.bCanEverTick = false;
	ManagerTick.bStartWithTickEnabled = true;
	ManagerTick.bAllowTickBatching = true;

#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 7, 0)
	ManagerTick.bRunTransactionally = false; // I feel like random transactional memory calls are not interesting to us here
#endif

	// Does not register by default
	ManagerTick.SetTickFunctionEnable(true);
	ManagerTick.Owner = this;
}

void UMegafunkUtilsExampleAnimSubsystem::Deinitialize() {
	Super::Deinitialize();
	ManagerTick.UnRegisterTickFunction();
}

void UMegafunkUtilsExampleAnimSubsystem::OnWorldBeginPlay(UWorld& InWorld) {
	Super::OnWorldBeginPlay(InWorld);

	// Should we do this earlier? unsure
	ManagerTick.RegisterTickFunction(InWorld.PersistentLevel);
}


bool UMegafunkUtilsExampleAnimSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const {
	// Game worlds only for now
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AMFUtilsExperimentalAsyncAnimInstanceOwnerActor* UMegafunkUtilsExampleAnimSubsystem::GetOrSpawnAnimInstanceOwnerActor() {
	if (AnimInstanceOwnerActor) {
		return AnimInstanceOwnerActor;
	}
	
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.bNoFail = true;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags = RF_Transient;
	
	AnimInstanceOwnerActor = GetWorld()->SpawnActor<AMFUtilsExperimentalAsyncAnimInstanceOwnerActor>(SpawnParameters);
	
	return AnimInstanceOwnerActor;
}

AMFUtilsExperimentalAsyncAnimInstanceOwnerActor::AMFUtilsExperimentalAsyncAnimInstanceOwnerActor() {
	
	SkeletalMeshAnimInstanceOwner = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshAnimInstanceOwner");
}

