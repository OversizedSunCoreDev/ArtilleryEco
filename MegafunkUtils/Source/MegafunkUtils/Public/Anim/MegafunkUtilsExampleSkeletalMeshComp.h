// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Components/SkeletalMeshComponent.h"
#include "MegafunkUtilsExampleSkeletalMeshComp.generated.h"


struct FMegafunkUtilsAnimationEvaluationContainer;
/**
 *  Example component that signs up to be ticked in parallel by an example subsystem
 *  Note that currently some initialization and teardown will still require a mainthread step currently
 *  The most important part of this aside from just registering to the example subsystem is some validation and considering editor preview worlds
 */
UCLASS(meta=(BlueprintSpawnableComponent))
class MEGAFUNKUTILS_API UMegafunkUtilsExampleSkeletalMeshComp : public USkeletalMeshComponent {
	GENERATED_BODY()

public:
	UMegafunkUtilsExampleSkeletalMeshComp(const FObjectInitializer& ObjectInitializer);

protected:
	// Overrides:
	virtual bool ShouldCreatePhysicsState() const override;
public:
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	bool ShouldTickWithExampleSubsystem() const;
	
	// If this will register itself with the UMegafunkUtilsAnimSubsystemExample. You don't need to use that if you want to replace it!
	UPROPERTY(EditAnywhere)
	uint8 bRegisterWithExampleWorldSubsystem : 1 = true;
	
	uint8 bDidRegisterWithExampleSubsystem : 1 = false;
};
