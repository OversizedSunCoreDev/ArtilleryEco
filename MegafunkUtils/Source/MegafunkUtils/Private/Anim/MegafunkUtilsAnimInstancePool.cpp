// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim/MegafunkUtilsAnimInstancePool.h"



FMFUtilsAnimInstanceHandle FMFUtilsAnimInstancePool::GetNewAnimInstance(const TSubclassOf<UAnimInstance> InAnimClass) {
	
	// FRWScopeLock ScopeLock(ArrayRWLock, SLT_Write);
	
	if (!FreeElements.IsEmpty()) {
		const int32 ExistingIndex = FreeElements.Pop(EAllowShrinking::No);
		check(Elements.IsValidIndex(ExistingIndex))
		FMFUtilsAnimInstacePoolElement& Element = Elements[ExistingIndex];
		Element.Generation++;
		FMFUtilsAnimInstanceHandle Handle;
		Handle.AnimInstance = Element.AnimInstance;
		Handle.Index = ExistingIndex;
		return Handle;
	}
	
	// we need a new element right now! time for suffering!
	
	UAnimInstance* NewAnimInstance = nullptr;
	USkeletalMeshComponent* Outer = nullptr;
	// NewAnimInstance = MegafunkUtils::Anim::Experimental_ManualAnimInstanceAllocAndInit(Outer, InAnimClass, );
	if (NewAnimInstance) {
		Elements.Add({NewAnimInstance});
		
		// Ideally we wait on the above task (insanely nasty though)
		FMFUtilsAnimInstanceHandle Handle;
		Handle.AnimInstance = NewAnimInstance;
		Handle.Index = Elements.Num() - 1;
		return Handle;
	}
	
	return {};
}

void FMFUtilsAnimInstancePool::ReleaseElement(const int32 InIndex) {
	
	// FRWScopeLock ScopeLock(ArrayRWLock, SLT_Write);
}

void FMFUtilsAnimInstancePool::TickPremptivePooling() {
	TRACE_CPUPROFILER_EVENT_SCOPE(FMFUtilsAnimInstancePool::TickPremptivePooling)
	// FRWScopeLock ScopeLock(ArrayRWLock, SLT_Write);

	
}

void FMFUtilsAnimInstancePool::RequestAddInstance_AnyThread() {
	

}

void FMFUtilsAnimInstancePool::PreAllocateInstances(const TArrayView<TPair<TSubclassOf<UAnimInstance>, int32>> NumPerAnimClass) {
	
	// FRWScopeLock ScopeLock(ArrayRWLock, SLT_Write);

	int32 Total = 0;
	for (auto& [Class, Count] : NumPerAnimClass) {
		Total+=Count;
	}
	
	Elements.Reserve(Total);
	
	for (auto& [Class, Count] : NumPerAnimClass) {
		for (int i = 0; i < Count; ++i) {
			auto ReservedElement = GetNewAnimInstance(Class);
			ReleaseElement(ReservedElement.Index);
		}
	}
}
