
#pragma once

#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "MegafunkAnimUtilsTypes.generated.h"


// This is intended to demonstrate how little data we actually need to get out of the anim evaluation steps
// Note these can be stored anywhere we want
// @Todo I will probably remove these in favor of users providing their own bone storage
USTRUCT()
struct FMegafunkUtilsAnimationEvaluationContainer {
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FTransform> ComponentSpaceTransforms;
	
	UPROPERTY()
	TArray<FTransform> BoneSpaceTransforms;
	
	UPROPERTY(VisibleAnywhere)
	FVector OutRootBoneLocationResult = FVector::ZeroVector;
	
	FBlendedHeapCurve Curve;
	UE::Anim::FMeshAttributeContainer Attributes;
};

