// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "MFUtilsAsyncAnimInstance.generated.h"



// This is partially an example but I wanted to show a way to reduce some pointless cache misses (I think)

UCLASS()
class MEGAFUNKUTILS_API UMFUtilsAsyncAnimInstance : public UAnimInstance {
	GENERATED_BODY()

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override {
		return &InlineAnimInstanceProxy;
	}

	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override {
		// Normally this would delete. We don't want that here
	}

	FAnimInstanceProxy InlineAnimInstanceProxy;
};
