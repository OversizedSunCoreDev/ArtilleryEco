// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim/MegafunkUtilsExampleSkeletalMeshComp.h"

#include "Anim/MegafunkUtilsAnimAccessors.h"
#include "Anim/MegafunkUtilsExampleAnimSubsystem.h"
#if WITH_EDITOR
#include "SkinnedAssetCompiler.h"
#endif
#include UE_INLINE_GENERATED_CPP_BY_NAME(MegafunkUtilsExampleSkeletalMeshComp)

UMegafunkUtilsExampleSkeletalMeshComp::UMegafunkUtilsExampleSkeletalMeshComp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {
	
}



bool UMegafunkUtilsExampleSkeletalMeshComp::ShouldCreatePhysicsState() const {
	// Currently I do not want to bother with async primitive updates
	// But I will give it a shot...
	return Super::ShouldCreatePhysicsState();
}

void UMegafunkUtilsExampleSkeletalMeshComp::InitializeComponent() {
#if WITH_EDITOR
	// Non-game worlds should enable animation as they tend to be things like previews, debug views etc which we currently do not want to intefere with
	if (!GetWorld()->IsGameWorld()) {
		// bEnableAnimation = true;
	}
#endif
	if (!ShouldTickWithExampleSubsystem()) {
		// bEnableAnimation = true;
	}
	
	Super::InitializeComponent();
}

void UMegafunkUtilsExampleSkeletalMeshComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	// don't tick in-game! We allow this for editor worlds though
	if (ShouldTickWithExampleSubsystem()) {
		if (GetWorld()->IsGameWorld()) {
			ensure(false);
			return;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMegafunkUtilsExampleSkeletalMeshComp::BeginPlay() {
	Super::BeginPlay();
	
	// This should never tick in game worlds. we want to manually tick this!
	UWorld* World = GetWorld();

	if (ShouldTickWithExampleSubsystem()) {
		
		// Stop ticking normally as we are going to want to tick with a subsystem
		if (World->IsGameWorld()) {
			SetComponentTickEnabled(false);
		}
		
#if !UE_BUILD_SHIPPING
		if (UAnimInstance* AnimInstance = GetAnimInstance()) {
			// @todo we need to figure out a better "testing" setup for validation
			// we want to avoid spam checking these and maybe catch the instance being swapped out
			MegafunkUtils::Anim::ValidateAnimInstanceBeingSafeToUseInParallelForExample(AnimInstance->GetClass());
		}
#endif
		
		UMegafunkUtilsExampleAnimSubsystem* SubSystem = UMegafunkUtilsExampleAnimSubsystem::Get(World);
		if (ensure(SubSystem)) {
			
			SubSystem->AddComponent(*this);
			bDidRegisterWithExampleSubsystem = true;
		}
	}
}

void UMegafunkUtilsExampleSkeletalMeshComp::EndPlay(EEndPlayReason::Type Reason) {
	Super::EndPlay(Reason);

	if (bDidRegisterWithExampleSubsystem) {
		if (UMegafunkUtilsExampleAnimSubsystem* SubSystem = UMegafunkUtilsExampleAnimSubsystem::Get(GetWorld())) {
			SubSystem->RemoveComponent(*this);
		}
	}
}

FBoxSphereBounds UMegafunkUtilsExampleSkeletalMeshComp::CalcBounds(const FTransform& LocalToWorld) const {
	// @todo use a smarter way to calculate bounds
	return Super::CalcBounds(LocalToWorld);
}

bool UMegafunkUtilsExampleSkeletalMeshComp::ShouldTickWithExampleSubsystem() const {
	return bRegisterWithExampleWorldSubsystem && GbMegafunkUtilsExampleSkeletalMeshManagerEnabled;
}


