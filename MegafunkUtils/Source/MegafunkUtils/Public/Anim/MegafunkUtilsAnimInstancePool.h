// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MegafunkUtilsAnimInstancePool.generated.h"



// Used so we can quickly remove instnaces from the array
struct  FMFUtilsAnimInstanceHandle {
	UAnimInstance* AnimInstance = nullptr;
	int32 Index = 0;
};


USTRUCT()
struct MEGAFUNKUTILS_API FMFUtilsAnimInstacePoolElement {
	GENERATED_BODY()
	
	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance;
	
	// This is so we can figure out how many times it was used (not sure if useful or not yet)
	UPROPERTY()
	int32 Generation = 0;
};

// We want
// - To pre-empt making new N new instances to prepare 
//
USTRUCT()
struct MEGAFUNKUTILS_API FMFUtilsAnimInstancePool  {
	GENERATED_BODY()
	
	// Either spawns or 
	FMFUtilsAnimInstanceHandle GetNewAnimInstance(const TSubclassOf<UAnimInstance> InAnimClass);
	
	
	
	void ReleaseElement(const int32 InIndex);
	
	// Intended to be called each frame
	void TickPremptivePooling();
	
	
	// Welp! We ran out! Time for suffering
	void RequestAddInstance_AnyThread();

	
	void PreAllocateInstances(const TArrayView<TPair<TSubclassOf<UAnimInstance>, int32>> NumPerAnimClass);

protected:
	// FRWLock ArrayRWLock;
	
	UPROPERTY()
	TArray<FMFUtilsAnimInstacePoolElement> Elements;
	
	TArray<int32, TInlineAllocator<128>> FreeElements;
};
