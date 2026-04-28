
#include "MegafunkUtils.h"
#include "Anim/AsyncAnimValidationAndUtils.h"
#include "Anim/MFUtilsAsyncAnimInstance.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMegafunkUtils)


void FMegafunkUtilsModule::StartupModule()
{
#if WITH_EDITOR
	// Credit to Blue Man for showing how to override a class constructor in this way. Without his help here we would have to get really cheesy
	{
		UClass* AnimInstanceClass = UAnimInstance::StaticClass();
		if (ensure(AnimInstanceClass)) {
			MegafunkUtils::Anim::SetAnimInstanceClassToUseAsyncSafeConstructor<UAnimInstance>(AnimInstanceClass);
		}
	}
	
	{
		UClass* MyAnimInstanceClass = UMFUtilsAsyncAnimInstance::StaticClass();
		if (ensure(MyAnimInstanceClass)) {
			MegafunkUtils::Anim::SetAnimInstanceClassToUseAsyncSafeConstructor<UMFUtilsAsyncAnimInstance>(MyAnimInstanceClass);
		}
	}
#endif WITH_EDITOR
}

void FMegafunkUtilsModule::ShutdownModule()
{
}



IMPLEMENT_MODULE(FMegafunkUtilsModule, MegafunkUtils)
