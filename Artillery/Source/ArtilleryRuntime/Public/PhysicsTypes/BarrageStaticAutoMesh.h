// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "Components/ActorComponent.h"
#include "BarrageStaticAutoMesh.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTILLERYRUNTIME_API UBarrageStaticAutoMesh : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBarrageStaticAutoMesh(const FObjectInitializer& ObjectInitializer);
	virtual bool RegistrationImplementation() override;
};