// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtillerySkeletalMeshDispatch.h"

#include "ArtilleryAnimInstance.h"
#include "ArtilleryDispatch.h"
#include "Anim/MegafunkUtilsAnimAccessors.h"
#include "Misc/EngineVersionComparison.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArtillerySkeletalMeshDispatch)

bool bUseBLKQueue = true;
FAutoConsoleVariableRef CVarUseBLKQueue(TEXT("artillery.anim.UseBLKQueue"),
                                        bUseBLKQueue,
                                        TEXT(
	                                        "enables using experimetnal BLK queue to send bone transforms up to the regular unreal ticking threads"),
                                        ECVF_Default);


bool bPushToRender = true;
FAutoConsoleVariableRef CVarPushToGameThread(TEXT("artillery.anim.PushToRender"),
                                             bPushToRender,
                                             TEXT(
	                                             "enables UArtillerySkeletalMeshDispatch actually changing visual skeletal meshes"),
                                             ECVF_Default);

void UArtillerySkeletalMeshDispatch::CreateAnimInstanceStateForUpdate(const FSkeletonKey InOwnerSkeletonKey,
                                                                      const TSubclassOf<UAnimInstance>
                                                                      InAnimInstanceClass,
                                                                      USkeletalMesh& InSkeletalMesh,
                                                                      TSharedRef<struct FBoneContainer> InRequiredBones,
                                                                      const TArray<FBoneIndexType>&
                                                                      InFillComponentSpaceTransformsRequiredBones,
                                                                      UE::Anim::FCurveFilterSettings
                                                                      InCurveFilterSettings,
                                                                      TSharedPtr<FSkelMeshRefPoseOverride>
                                                                      InRefPoseOverride)
{
	// @todo we might consider non-animating cases or single node/ref pose cases later
	if (!ensure(InAnimInstanceClass))
	{
		return;
	}

	if (!MegafunkUtils::Anim::ValidateAnimInstanceBeingSafeToUseInParallelForExample(InAnimInstanceClass, true))
	{
		ensure(false);
		return;
	}

	USkeleton* Skeleton = InSkeletalMesh.GetSkeleton();


	if (ensure(Skeleton) && ensure(SkeletalMeshComponentOuter))
	{
		// we will probably call Experimental_ComputeRequiredBonesWithoutSkeletalMeshComp
		// Copy to the new bone container
		TSharedRef<FBoneContainer> RequiredBonesCopy = MakeShared<FBoneContainer>();
		*RequiredBonesCopy = *InRequiredBones;

		if (!RequiredBonesCopy->GetSkeletonAsset())
		{
			ensure(false);
			return;
		}

		// We use our own SkeletalMeshComponentOuter for now
		UAnimInstance* NewAnimInstance = MegafunkUtils::Anim::Experimental_ManualAnimInstanceAllocAndInit(
			*SkeletalMeshComponentOuter,
			*InAnimInstanceClass,
			*Skeleton,
			RequiredBonesCopy,
			InCurveFilterSettings,
			InRefPoseOverride);

		if (auto ArtilleryAnimInstance = Cast<UArtilleryAnimInstance>(NewAnimInstance))
		{
			ArtilleryAnimInstance->MyParentObjectKey = InOwnerSkeletonKey;
			if (UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>())
			{
				ArtilleryAnimInstance->MyBarrageBody = BarrageDispatch->GetShapeRef(InOwnerSkeletonKey);
			}
		}


		FArtilleryWIPAnimStateContainer Amim;
		// We already have an anim instance for some reason? 
		if (Amim.AnimInstance)
		{
			ensure(false);
		}

		Amim.AnimInstance = TStrongObjectPtr<UAnimInstance>(NewAnimInstance);
		Amim.SkeletalMesh = TStrongObjectPtr<USkeletalMesh>(&InSkeletalMesh);
		Amim.FillComponentSpaceTransformsRequiredBones = InFillComponentSpaceTransformsRequiredBones;

		// Set some initial values in the eval context
		Amim.EvaluationContext.SkeletalMesh = &InSkeletalMesh;
		Amim.EvaluationContext.AnimInstance = NewAnimInstance;
		Amim.EvaluationContext.ComponentSpaceTransforms.SetNumUninitialized(
			Amim.FillComponentSpaceTransformsRequiredBones.Num());

		FScopeLock ScopeLock(&PendingAnimStateContainersCriticalSection);
		PendingAnimStateContainers.Add({InOwnerSkeletonKey, Amim});
	}
}

void UArtillerySkeletalMeshDispatch::UnregisterAnimInstanceStateForUpdate(const ActorKey& ActorKey)
{
	FScopeLock ScopeLock(&PendingAnimStateContainersCriticalSection);
	PendingAnimStateContainers.RemoveAll([&](const TPair<FSkeletonKey, FArtilleryWIPAnimStateContainer>& ToFind)
	{
		return ToFind.Key == ActorKey;
	});

	PendingRemovalAnimStateContainers.Add(ActorKey);
}

void UArtillerySkeletalMeshDispatch::ArtilleryTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArtillerySkeletalMeshDispatch::ArtilleryTick)


	{
		FScopeLock ScopeLock(&PendingAnimStateContainersCriticalSection);
		// Handle removals and additions from other threads
		for (FSkeletonKey& ToRemove : PendingRemovalAnimStateContainers)
		{
			AnimStateContainers.Remove(ToRemove);
		}
		PendingRemovalAnimStateContainers.Reset(PendingRemovalAnimStateContainers.Num());


		for (auto& [Owner, Anim] : PendingAnimStateContainers)
		{
			AnimStateContainers.Add(Owner, MoveTemp(Anim));
		}
		PendingAnimStateContainers.Reset(PendingAnimStateContainers.Num());
	}
	
	
	// Currently we just use a simple task setup to tick these
	// Launch some tasks that evaluate anim instances
	
	for (auto& [Owner, Anim] : AnimStateContainers)
	{
		static constexpr float DeltaTime = 1.0f / HERTZ_OF_BARRAGE;
		Anim.TaskHandle = UE::Tasks::Launch(TEXT("Artillery Anim instance update"),
		                                    [&]
		                                    {
			                                    MegafunkUtils::Anim::Experimental_AnimInstanceOnlyFullUpdate(DeltaTime,
				                                    true,
				                                    Anim.EvaluationContext,
				                                    *Anim.AnimInstance,
				                                    *Anim.SkeletalMesh,
				                                    Anim.EvaluationContext.ComponentSpaceTransforms,
				                                    Anim.EvaluationContext.BoneSpaceTransforms,
				                                    Anim.EvaluationContext.RootBoneTranslation,
				                                    Anim.EvaluationContext.Curve,
				                                    Anim.EvaluationContext.CustomAttributes,
				                                    Anim.FillComponentSpaceTransformsRequiredBones);


			                                    if (!Anim.EvaluationContext.ComponentSpaceTransforms.IsEmpty())
			                                    {
				                                    if (bUseBLKQueue)
				                                    {
					                                    FTransform& TransformData = *Anim.EvaluationContext.
						                                    ComponentSpaceTransforms.GetData();
					                                    const int32 Num = Anim.EvaluationContext.
					                                                           ComponentSpaceTransforms.Num();

					                                    thread_local BLK::WorkerStateBundle ThreadStateBundle;
					                                    SkeletonBLKRing.UpdateBufferAssignment(ThreadStateBundle);
					                                    SkeletonBLKRing.AddBones(
						                                    Owner, &TransformData, Num, ThreadStateBundle,
						                                    BLKFrameCounter);
				                                    }
			                                    }
		                                    });
	}

	// Try to help with the tasks we just sent out (A bit silly but these can be unbalanced so a parallel for is not quite as nice)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TryRetractAndExecute tasks)
		for (auto& [Owner, Anim] : AnimStateContainers) {
			Anim.TaskHandle.TryRetractAndExecute();
		}
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Wait on tasks)
		for (auto& [Owner, Anim] : AnimStateContainers) {
			Anim.TaskHandle.Wait();
		}
	}
	
	//We have waited until all tasks are done. We can now declare that tick complete.
	BLKFrameCounter++;
}

bool UArtillerySkeletalMeshDispatch::RegistrationImplementation()
{
	check(ArtilleryDispatch);
	ArtilleryDispatch->SetSkeletalMeshDispatch(this);

	return true;
}

void UArtillerySkeletalMeshDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ArtilleryDispatch = Collection.InitializeDependency<UArtilleryDispatch>();

	TransformDispatch = Collection.InitializeDependency<UTransformDispatch>();

	Collection.InitializeDependency<UOrdinatePillar>()->REGISTERLORD(OrdinateSeqKey, this, this);
}

void UArtillerySkeletalMeshDispatch::Deinitialize()
{
	Super::Deinitialize();
	// Now... we wait (and manually clear out the task handle)
	for (auto& [Owner, Anim] : AnimStateContainers)
	{
		Anim.TaskHandle.Wait();
		Anim.TaskHandle = UE::Tasks::TTask<void>();
	}
	// We want strong pointers to be manually removed before GC runs
	AnimStateContainers.Reset();
	PendingAnimStateContainers.Reset();
	SkeletonBLKRing.Reset(); //this increments the generation, invalidating any existing tasks as soon as possible 
	//but should generally not be called while the BLK ring is running. pretty sure it's safe but I've been pretty sure of a lot of stuff.
}

void UArtillerySkeletalMeshDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);


	// Because we need a valid USkeletalMeshComponent outer for these objects we must provide one.
	// Spawning actor component async does not seem very fun so I am opting to just provide one that all of them share for now
	// I will probably need to make sure this never renders/ticks etc

	FActorSpawnParameters SpawnParams;
	SpawnParams.bNoFail = true;
	SpawnParams.ObjectFlags = RF_Transient;
	SpawnParams.Name = MakeUniqueObjectName(&InWorld, AActor::StaticClass(),
	                                        "ArtillerySkeletalMeshDispatch Owner Actor");
	auto OwnerActor = InWorld.SpawnActor<AActor>(SpawnParams);

	SkeletalMeshComponentOuter = NewObject<USkeletalMeshComponent>(OwnerActor);
	// We want to make darn sure it can never effect physics/navigation. Some may be redundant
	SkeletalMeshComponentOuter->bAlwaysCreatePhysicsState = false;
	SkeletalMeshComponentOuter->SetComponentTickEnabled(false);
	SkeletalMeshComponentOuter->SetHiddenInGame(true);
	SkeletalMeshComponentOuter->SetCollisionEnabled(ECollisionEnabled::NoCollision);


	OwnerActor->SetRootComponent(SkeletalMeshComponentOuter);
	SkeletalMeshComponentOuter->RegisterComponentWithWorld(&InWorld);

	// This lets us see the object in the editor
	OwnerActor->AddInstanceComponent(SkeletalMeshComponentOuter);
}

void UArtillerySkeletalMeshDispatch::OnWorldEndPlay(UWorld& InWorld)
{
	Super::OnWorldEndPlay(InWorld);
}

void UArtillerySkeletalMeshDispatch::Tick(float DeltaTime)
{
	// Game thread tick

	Super::Tick(DeltaTime);
	//Shipping any update twice can cause explosions.
	SkeletonBLKRing.ResetConsumersOnly_NoGenerationIncrement();
	SkeletonBLKRing.UpdateConsumersAtStartOfTick(ConsumerCount);
	auto LastKnownFinishedTick =  BLKFrameCounter - 1;//this increments ONLY when a frame is finished, so we know that counter-1 is completed.
	if (LastKnownFinishedTick > 0)
	{
		ParallelFor(ConsumerCount, [&](int32 BodyIndex)
		{
	#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 7, 0)
					FTaskTagScope Scope(ETaskTag::EParallelGameThread);
	#else
					FOptionalTaskTagScope Scope(ETaskTag::EParallelGameThread);
	#endif
			if (bUseBLKQueue)
			{
				// Game thread only handle into the BLKRing
			
				BLK::WorkerStateBundle ThreadStateBundleGameThread; // parallel for doesn't promise we'll be on threads, so we can't use thread local.
				 volatile bool DidThisChange = SkeletonBLKRing.UpdateConsumerModulo(ThreadStateBundleGameThread);

				BLK::RecordFetchState BoneRecord = SkeletonBLKRing.GetMyNextRecord(
					ThreadStateBundleGameThread, LastKnownFinishedTick);
				while (BoneRecord.second.has_value() && BoneRecord.first != -1)
				{
					BLK::TransientQueuedDataRange NoWarrantiesBoneView = SkeletonBLKRing.
						GetBoneIterator(
							BoneRecord, ThreadStateBundleGameThread);
					auto Rec = BoneRecord.second.value_or(BLK::FBoneArrayRecord()).key;
					auto ptr = TransformDispatch->GetAActorByObjectKey(Rec);
					auto ActorPtr = ptr.IsValid() ? ptr.Pin() : nullptr;
					if (ActorPtr)
						//@Megafunk, the above is the normal idiom, but is this correct in UEWorld?
					{
						if (auto SkeletalMeshComponent = ActorPtr->FindComponentByClass<
							USkeletalMeshComponent>())
						{
							// Disable the normal component tick (temporary, ideally it would not be set in the first place!)
							if (SkeletalMeshComponent->IsComponentTickEnabled())
							{
								// Becuase we are not on the gamethread we have to get weird... this is definitely not ideal
								auto WeakSkeletalMeshComponent = TWeakObjectPtr<USkeletalMeshComponent>(SkeletalMeshComponent);
								AsyncTask(ENamedThreads::GameThread, [WeakSkeletalMeshComponent]() {
									
									if (auto GameThreadPtr = WeakSkeletalMeshComponent.Get()) {
										GameThreadPtr->SetComponentTickEnabled(false);
									}
								});
							}

							auto& BoundBoneArray = SkeletalMeshComponent->
								GetEditableComponentSpaceTransforms();
							//records and bone sets are queued "side by side" on two queues. For the current record,
							//we have gotten a view over the transmitted bone set. we will now copy directly from this view
							//into the editable transform array of the specified skeleton.
							if (bPushToRender && !BoundBoneArray.IsEmpty())
							{
								//in a just and righteous world, these would always be the same. Who knows! maybe they will be.
								//I however am not going to count on it. <3
								volatile auto HowManyBonesYouActuallyHave = BoundBoneArray.Num(); //This gets optimized out unless you add volatile
								//And you are gonna want it for debug purposes.
								auto HowManyBonesYouGotTransformsFor = NoWarrantiesBoneView.
									Num;

								//we assume bone order is the same because we wrote it in from the same data source. if this is an unsafe assumption,
								//we need to make some changes to force order.
								for (int i = 0;
									 i < HowManyBonesYouGotTransformsFor
									 &&
									 i < HowManyBonesYouActuallyHave;
									 ++i)
								{
									auto found = SkeletonBLKRing.IterateRange(
										NoWarrantiesBoneView);
									if (found)
									{
										//this... does the right thing, right? Like we aren't causing a spare copy, right?
										BoundBoneArray[i] = *found;
										//the copy actually happens here. I'll be honest, it makes me a bit nervous.
									}
									else
									{
										UE_LOG(LogTemp, Log,
											   TEXT(
												   "Why is the queued set of bones for %s too short?"
											   ),
											   *SkeletalMeshComponent->GetOwner()->
											   GetActorNameOrLabel());
									}
								}
							
								MegafunkUtils::Anim::SkeletalMeshComponent_ForceFinalizeBoneTransform(
									*SkeletalMeshComponent);
								// Send render data DIRECTLY. This is a large part of why no main thread step is required
								MegafunkUtils::Anim::SkeletalMeshComponent_PushRenderUpdate(
									*SkeletalMeshComponent);
							}
							else
							{
								UE_LOG(LogTemp, Log,
									   TEXT("Why is the bone array for %s empty?"),
									   *SkeletalMeshComponent->GetOwner()->
									   GetActorNameOrLabel());
							}
						}
					}

					BoneRecord = SkeletonBLKRing.
						GetMyNextRecord(ThreadStateBundleGameThread, LastKnownFinishedTick);
				}
			}
		}, false, true);
	}
}
