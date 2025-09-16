// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "Components/ActorComponent.h"
#include "BarrageBoxComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTILLERYRUNTIME_API UBarrageBoxComponent : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int XDiam = 30;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int YDiam = 30;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int ZDiam = 20;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeX = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeY = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeZ = 0;
	
	UBarrageBoxComponent(const FObjectInitializer& ObjectInitializer);
	virtual void Register() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
