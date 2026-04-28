// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#if WITH_EDITOR


namespace MegafunkUtils::Anim {
	// Should probably use the thread safe cvar modes but I am going to assume people aren't changing this often
	inline bool GbAsyncAnimInstancesStillRegisterForOnObjectsReinstanced = false;


	MEGAFUNKUTILS_API bool IsAnimInstanceClassConstructorUsingOurThreadSafeVersion(const TNotNull<UClass*> InClass);

	MEGAFUNKUTILS_API void RegisterInstanceClassConstructor_Internal(const TNotNull<UClass*> InClass);

	// Intended for UAnimInstance and native child classes
	template <class AnimClass>
	void InitAnimInstanceThreadedFunc(const FObjectInitializer& Initializer) {
		// On the gamethread or for the CDO we have nothing to change
		if (IsInGameThread()) {
			InternalConstructor<AnimClass>(Initializer);
			return;
		}

		// But for non-gamethread created anim instances we want to skip the branch in there registers to a global static delegate
		const bool bIsRealCDO = Initializer.GetObj()->HasAnyFlags(RF_ClassDefaultObject);
		if (bIsRealCDO) {
			InternalConstructor<AnimClass>(Initializer);
		}
		else {
			Initializer.GetObj()->SetFlags(RF_ClassDefaultObject);
			InternalConstructor<AnimClass>(Initializer);
			Initializer.GetObj()->ClearFlags(RF_ClassDefaultObject);

			// With some extra cheese we can optionally sign up for the delegate on the gamethread with a simple task
			// I am going to say will probably not be safe to listen to unless you are never doing async work during a reinstance 
			if (GbAsyncAnimInstancesStillRegisterForOnObjectsReinstanced)
				if (!Initializer.GetClass()->HasAnyClassFlags(CLASS_Native)) {
					TWeakObjectPtr<UAnimInstance> WeakThis = CastChecked<UAnimInstance>(Initializer.GetObj());
					AsyncTask(ENamedThreads::GameThread,
					          [WeakThis]() {
						          if (UAnimInstance* ValidPtr = WeakThis.Get()) {
							          FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(ValidPtr, &UAnimInstance::HandleObjectsReinstanced);
						          }
					          });
				}
		}
	}

	template <class AnimClass>
	void SetAnimInstanceClassToUseAsyncSafeConstructor(const TNotNull<UClass*> InClass) {
		check(IsInGameThread());
		InClass->ClassConstructor = InitAnimInstanceThreadedFunc<AnimClass>;
		RegisterInstanceClassConstructor_Internal(InClass);
	}
}


#endif
