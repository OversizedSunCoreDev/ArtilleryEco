// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


namespace MegafunkUtils {
	
	void SetWorldTransformFastPathParallelStep(USceneComponent* InComp, const FTransform& InTransform);
	void PushPrimitiveRenderData(USceneComponent& InComp, const FTransform& InTransform);
	
	
	// This is useful to determine if we can skip calling certain BP ufunctions if they would do nothing
	bool IsFunctionImplementedByBP(FName FunctionName, const UClass& Class);
	
}
