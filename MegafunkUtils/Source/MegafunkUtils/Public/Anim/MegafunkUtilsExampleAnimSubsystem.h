// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MegafunkAnimUtilsTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MegafunkUtilsExampleAnimSubsystem.generated.h"

#define UE_API MEGAFUNKUTILS_API

class UMegafunkUtilsExampleSkeletalMeshComp;
class UMegafunkUtilsExampleAnimSubsystem; // Fwd declared because FMegafunkUtilsManagerTick has a member ptr to them

static bool GbMegafunkUtilsExampleSkeletalMeshManagerEnabled = true;
static bool GbMegafunkUtilsExampleSkeletalMeshManagerChaosUpdateEnabled = false;
static bool GbMegafunkUtilsExampleMontageAndAnimNotifyMainThreadCallbackEnabled = true;
static bool GbMegafunkUtilsExampleAnimInstanceManagerEnabled = false;



USTRUCT()
struct FMFUtilsManagedComponentStateExample {
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMegafunkUtilsExampleSkeletalMeshComp> SkelMeshComponent;
	
	// All of this data is also stored inside of the component, but the idea here is to show it can exist outside of it
	
	UPROPERTY(VisibleAnywhere)
	FMegafunkUtilsAnimationEvaluationContainer EvaluatedAnimState;
};

// An example of using just an anim instance
USTRUCT()
struct FMegafunkUtilsManagedAnimInstanceExample {
	GENERATED_BODY()
	
	// Intended to be created from an async thread
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAnimInstance> AnimInstance;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMesh> SkeletalMesh;
	
	// Todo temp baseline
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshCompBaseline;
	
	FAnimationEvaluationContext EvaluationContext = {};
	
	TArray<FBoneIndexType> FillComponentSpaceTransformsRequiredBones;
	
};

// This is used because this has a ticking phase that lets us place this before physics 
struct FMFUtilsAnimManagerTick : public FTickFunction {
	virtual void ExecuteTick(float DeltaTime,
							 ELevelTick TickType,
							 ENamedThreads::Type CurrentThread,
							 const FGraphEventRef& MyCompletionGraphEvent) override;
	
	// Tick function boilerplate I will probably not actually need to do as intended
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	// There is not much need to advertise a pointer only contained by its own memory to GC
	UMegafunkUtilsExampleAnimSubsystem* Owner = nullptr;
	
	
	static void UpdateSkeletalMeshExample(FMegafunkUtilsAnimationEvaluationContainer& OutResult, UMegafunkUtilsExampleSkeletalMeshComp& Comp, float DeltaTime);
};

/**
 *  You don't need to use this subsystem, but it serves as a simple example
 *  This is not a normal tickable world subsystem because it wants more control over tick ordering in a frame to be pre-physics
 *  UMegafunkUtilsExampleSkeletalMeshComp will automatically register themselves to this 
 *  
 *  We also have a secondary experimental anim-instance only setup which is WIP
 */
UCLASS()
class UE_API UMegafunkUtilsExampleAnimSubsystem : public UWorldSubsystem {
	GENERATED_BODY()

public:
	static UMegafunkUtilsExampleAnimSubsystem* Get(UWorld* World);
	void AddComponent(UMegafunkUtilsExampleSkeletalMeshComp& InSkeletalMeshComp);
	void RemoveComponent(UMegafunkUtilsExampleSkeletalMeshComp& InSkeletalMeshComp);

	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
public:
	
	// @todo replace with FInstanceIdIndexMap or something fancier
	UPROPERTY()
	TArray<FMFUtilsManagedComponentStateExample> ManagedComponentStates;
	
	
	// Tick function that exists mainly to have a specific ticking phase
	FMFUtilsAnimManagerTick ManagerTick;
	
	UPROPERTY()
	TObjectPtr<class AMFUtilsExperimentalAsyncAnimInstanceOwnerActor> AnimInstanceOwnerActor;
	
	AMFUtilsExperimentalAsyncAnimInstanceOwnerActor* GetOrSpawnAnimInstanceOwnerActor();
};

/** 
 * An extremely experimental way to manage a set of anim instances without a skeletal mesh being used directly
 * Anim instances force having a USkeletalMeshComponent outer, and USkeletalMeshComponents force having an actor outer so... 
 */
UCLASS()
class UE_API AMFUtilsExperimentalAsyncAnimInstanceOwnerActor : public AActor {
	GENERATED_BODY()

public:
	AMFUtilsExperimentalAsyncAnimInstanceOwnerActor();

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshAnimInstanceOwner;
	
	UPROPERTY()
	TArray<FMegafunkUtilsManagedAnimInstanceExample> ManagedAnimInstances;
};

#undef UE_API
