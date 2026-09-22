// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ArtilleryBPLibs.h"
#include "FBarrageKey.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TickliteLibrary.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FTickliteHitDelegate, const FVector&, RayStart, const FHitResult&, HitResultFromTicklite);
/**
 * Mostly intended to be called from blueprint as it uses the thread-local singleton
 */
UCLASS()
class ARTILLERYRUNTIME_API UTickliteLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintCallable, meta = (ExpandEnumAsExecs = "ReturnValue"), Category="Artillery|TickLites")
	static EArtilleryFindResult K2_SphereCastTickLiteFromGun(const FGunKey& Owner, FSkeletonKey BarrageShapeOwner, float Length, const FVector& Location, const FVector& Direction, const FTickliteHitDelegate& Delegate);
	
};
