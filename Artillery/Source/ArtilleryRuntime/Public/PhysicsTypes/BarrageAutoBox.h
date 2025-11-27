// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "EPhysicsLayer.h"
#include "Components/ActorComponent.h"
#include "BarrageAutoBox.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DefaultToInstanced )
class ARTILLERYRUNTIME_API UBarrageAutoBox : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool isMovable = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeX = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeY = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeZ = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DiameterXYZ = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite )
	TEnumAsByte<EBWeightClasses::Type> Weight;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite )
	EPhysicsLayer Layer;
	FMassByCategory MyMassClass;

	UBarrageAutoBox(const FObjectInitializer& ObjectInitializer);
	
	virtual bool RegistrationImplementation() override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};

