// Fill out your copyright notice in the Description page of Project Settings.


#include "Anim/AsyncAnimValidationAndUtils.h"

#if WITH_EDITOR

FAutoConsoleVariableRef CVarAsyncAnimInstancesStillRegisterForOnObjectsReinstanced(

	TEXT("MegafunkUtils.Anim.AsyncAnimInstancesStillRegisterForOnObjectsReinstanced"),
	MegafunkUtils::Anim::GbAsyncAnimInstancesStillRegisterForOnObjectsReinstanced,
	TEXT(""),
	ECVF_Default);

TSet<TObjectKey<UClass>> RegisteredNativeAnimInstanceClasses;

bool MegafunkUtils::Anim::IsAnimInstanceClassConstructorUsingOurThreadSafeVersion(const TNotNull<UClass*> InClass) {
	if (!ensure(InClass->IsChildOf<UAnimInstance>())) {
		return false;
	}

	// We don't care about BP classes, do we?
	UClass* NativeClass = GetParentNativeClass(InClass);
	return RegisteredNativeAnimInstanceClasses.Contains(NativeClass);
}

void MegafunkUtils::Anim::RegisterInstanceClassConstructor_Internal(const TNotNull<UClass*> InClass) {
	UClass* NativeClass = GetParentNativeClass(InClass);
	RegisteredNativeAnimInstanceClasses.Add(NativeClass);
}

#endif
