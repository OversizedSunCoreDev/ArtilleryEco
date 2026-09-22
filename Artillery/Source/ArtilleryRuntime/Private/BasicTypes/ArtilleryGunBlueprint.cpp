// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#include "ArtilleryGunBlueprint.h"

#include "ArtilleryRuntimeModule.h"
#include "StaticAssetLoader.h"


static TAutoConsoleVariable<bool> CVarReinstanceGunBlueprint(
	TEXT("Artillery.ReinstanceGunBlueprint"),
	true,
	TEXT(""),
	ECVF_Default);

thread_local UArtilleryDispatch* FArtilleryDispatchThreadScope::ArtilleryDispatch = nullptr;
thread_local UBarrageDispatch* FArtilleryDispatchThreadScope::BarrageDispatch = nullptr;
#ifdef JPH_DEBUG_RENDERER
thread_local JPH::DebugRenderer* FArtilleryDispatchThreadScope::DebugRenderer = nullptr;
#endif

FArtilleryDispatchThreadScope::FArtilleryDispatchThreadScope(UArtilleryDispatch* InDispatch)
{
	check(ArtilleryDispatch == nullptr)
	
	ArtilleryDispatch = InDispatch;
	check(ArtilleryDispatch)
	BarrageDispatch = InDispatch->BarrageDispatch;
	check(BarrageDispatch)
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer = JPH::DebugRenderer::sInstance;
#endif
}

FArtilleryDispatchThreadScope::~FArtilleryDispatchThreadScope()
{
	ArtilleryDispatch = nullptr;
	BarrageDispatch = nullptr;
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer = nullptr;
#endif
}

UArtilleryGunBlueprint::UArtilleryGunBlueprint()
{
	// in the editoe we can react to a blueprint being replaced (neat)
	// this is going to be VERY janky imo
#if WITH_EDITOR
	if(!HasAnyFlags(RF_ClassDefaultObject) && !GetClass()->HasAnyClassFlags(CLASS_Native))
	{
		FCoreUObjectDelegates::OnObjectsReinstanced.AddWeakLambda(this, [this](const TMap<UObject*, UObject*>& OldToNewInstanceMap)
		{
			if (!CVarReinstanceGunBlueprint->GetBool())
			{
				return;
			}
			
			bool bWasReinstanced = false;
			UArtilleryGunBlueprint* OldInstance = nullptr;
			// For some reason we check from the new object, so I suppose THIS one is the new object
			
			for(auto& [Old, New] : OldToNewInstanceMap)
			{
				if(New == this)
				{
					bWasReinstanced = true;
					OldInstance = Cast<UArtilleryGunBlueprint>(Old);
					break;
				}
			}
				
			if (bWasReinstanced)
			{
				UE_LOG(LogArtillery, Log, TEXT("Object %s reinstanced! We will not try to replace it!"), *GetName());
				if (UWorld* World = GetWorld())
				{
					auto Dispatch = World->GetSubsystem<UArtilleryDispatch>();
					
					bool bWaitSuccess = 
					Dispatch->ArtilleryAsyncWorldSim.QueueFunctionFromAnyThreadAndWait([this, OldInstance]()
					{
						// The previous instance tracks the guns it had on it! Let's use them to fix replace the old with the new
						FScopeLock Lock(&OldInstance->MyGunInstancesForResinstancingCS);
						for (auto& GunPtr : OldInstance->MyGunInstancesForResinstancing_RequiresCriticalSection)
						{
							if (GunPtr)
							{
								ensure(GunPtr->ArtilleryBlueprintObject.Get() == OldInstance);
								GunPtr->ArtilleryBlueprintObject = TStrongObjectPtr<UArtilleryGunBlueprint>(this);
								this->TrackGunPtr(GunPtr);
							}
						}
						
						OldInstance->MyGunInstancesForResinstancing_RequiresCriticalSection.Reset();

					}, 1.0f);
					
					ensure(bWaitSuccess);
				}
				
			}
		});
	}
#endif
}

int32 UArtilleryGunBlueprint::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	return Super::GetFunctionCallspace(Function, Stack);
}

#if WITH_EDITOR
void UArtilleryGunBlueprint::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	Super::PostCDOCompiled(Context);
	
	// Shameful logging, I mostly wanted to see if I could deduce there being member properties
	// In the future we might try to make a custom UBlueprint to actually fight off (like UAGunBitBP)
	// UE_LOG(LogTemp, Warning, TEXT("PostCDOCompiled %s"), *GetClass()->GetAuthoredName());
	
	for (TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIterationFlags::None); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		// UE_LOG(LogTemp, Warning, TEXT("Property loaded: %s"), *Property->GetAuthoredName());
	}
}

void UArtilleryGunBlueprint::TrackGunPtr(FArtilleryGunBlueprintWrapper* GunPtr)
{
	FScopeLock Lock(&MyGunInstancesForResinstancingCS);
	MyGunInstancesForResinstancing_RequiresCriticalSection.Add(GunPtr);
}

void UArtilleryGunBlueprint::RemoveTrackedGunPtr(struct FArtilleryGunBlueprintWrapper* GunPtr)
{
	FScopeLock Lock(&MyGunInstancesForResinstancingCS);
	MyGunInstancesForResinstancing_RequiresCriticalSection.Remove(GunPtr);
}
#endif

FArtilleryGunBlueprintWrapper::~FArtilleryGunBlueprintWrapper()
{
#if WITH_EDITOR
	if (ArtilleryBlueprintObject)
	{
		ArtilleryBlueprintObject->RemoveTrackedGunPtr(this);
	}
#endif
}

FArtilleryGunBlueprintWrapper::FArtilleryGunBlueprintWrapper(const FGunKey& KeyFromDispatch, UArtilleryDispatch* Dispatch) : Super(KeyFromDispatch, Dispatch)
{
	UArtilleryGunBlueprint* SingletonInstance = GetObjectInstanceFromDispatchLoader();
	if (ensure(SingletonInstance))
	{
		ArtilleryBlueprintObject = TStrongObjectPtr<UArtilleryGunBlueprint>(SingletonInstance);
#if WITH_EDITOR
		ArtilleryBlueprintObject->TrackGunPtr(this);
#endif
	}
}

UArtilleryGunBlueprint* FArtilleryGunBlueprintWrapper::GetObjectInstanceFromDispatchLoader()
{
	if (!ensure(MyDispatch))
	{
		return nullptr;
	}

	UGameInstance* GI = MyDispatch->GetWorld()->GetGameInstance();

	UStaticGunLoader* Arsenal = GI->GetSubsystem<UStaticGunLoader>();
	if (ensure(Arsenal))
	{
		// note that this is NOT created on the stack. NEVER CREATE A TObjectPtr ON THE STACK!
		TObjectPtr<class UArtilleryGunBlueprint>* SingletonPtr = Arsenal->BPGunNamesToSingletons.Find(FName(MyGunKey.GunDefinitionID));
		// this failing means you probably didn't add a record in the artillery data table! This will now explode
		if (ensure(SingletonPtr) && ensure(*SingletonPtr))
		{
			return *SingletonPtr;
		}
	}


	return nullptr;
}

void FArtilleryGunBlueprintWrapper::PreFireGun(FArtilleryStates OutcomeStates,
	int DallyFramesToOmit,
	bool RerunDueToReconcile,
	bool VerifiedFrame,
	const EventBufferInfo FireAction)
{
	// FArtilleryGun::PreFireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile, VerifiedFrame, FireAction);
	if (ensure(ArtilleryBlueprintObject))
	{
		ArtilleryBlueprintObject->PreFireGun(MyDispatch, MyGunKey, MyGunKey.GunInstanceID, MyProbableOwner);
	}
}

void FArtilleryGunBlueprintWrapper::FireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit, bool RerunDueToReconcile)
{
	// FArtilleryGun::FireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
	if (ensure(ArtilleryBlueprintObject))
	{
		ArtilleryBlueprintObject->FireGun(MyDispatch, MyGunKey, MyGunKey.GunInstanceID, MyProbableOwner);
	}
}

void FArtilleryGunBlueprintWrapper::PostFireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit, bool RerunDueToReconcile)
{
	// FArtilleryGun::PostFireGun(OutcomeStates, DallyFramesToOmit, RerunDueToReconcile);
	if (ensure(ArtilleryBlueprintObject))
	{
		ArtilleryBlueprintObject->PostFireGun(MyDispatch, MyGunKey, MyGunKey.GunInstanceID, MyProbableOwner);
	}
}

bool FArtilleryGunBlueprintWrapper::Initialize(const FGunKey& KeyFromDispatch,
	const bool MyCodeWillSetGunKey,
	UAGunBitBP* PF,
	UAGunBitBP* PFC,
	UAGunBitBP* F,
	UAGunBitBP* FC,
	UAGunBitBP* PtF,
	UAGunBitBP* PtFc,
	UAGunBitBP* FFC)
{
	// Initialize does some extra stuff that I think we want? unsure
	bool bInit = FArtilleryGun::Initialize(KeyFromDispatch, MyCodeWillSetGunKey, PF, PFC, F, FC, PtF, PtFc, FFC);
	if (ensure(ArtilleryBlueprintObject))
	{
		ArtilleryBlueprintObject->PostFireGun(MyDispatch, MyGunKey, MyGunKey.GunInstanceID, MyProbableOwner);
	}

	return bInit;
}

void FArtilleryGunBlueprintWrapper::ProjectileCollided(const FSkeletonKey ProjectileKey, const FSkeletonKey HitEntity)
{
	FArtilleryGun::ProjectileCollided(ProjectileKey, HitEntity);
	
	ArtilleryBlueprintObject->OnGunProjectileCollided(MyDispatch, MyGunKey, MyGunKey.GunInstanceID, MyProbableOwner, ProjectileKey, HitEntity);
}
