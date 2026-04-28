// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArtilleryRuntimeModule.h"

#include "ArtilleryAnimInstance.h"

#if WITH_EDITOR
#include "Anim/AsyncAnimValidationAndUtils.h"
#endif

#define LOCTEXT_NAMESPACE "FArtilleryRuntimeModule"

void FArtilleryRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module
	
#if WITH_EDITOR
	{
		UClass* AnimInstanceClass = UArtilleryAnimInstance::StaticClass();
		if (ensure(AnimInstanceClass)) {
			MegafunkUtils::Anim::SetAnimInstanceClassToUseAsyncSafeConstructor<UArtilleryAnimInstance>(AnimInstanceClass);
		}
	}
#endif
	
	
}

void FArtilleryRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArtilleryRuntimeModule, ArtilleryRuntime)
