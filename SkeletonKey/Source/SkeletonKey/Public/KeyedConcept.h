#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "UObject/ScriptMacros.h"
#include "SkeletonTypes.h"
#include "KeyedConcept.generated.h"

UINTERFACE(NotBlueprintable)
class SKELETONKEY_API UCanReady : public UInterface
{
	GENERATED_UINTERFACE_BODY()

	
};


inline UCanReady::UCanReady(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}



class SKELETONKEY_API ICanReady 
{
	GENERATED_IINTERFACE_BODY()
	
	//this generalizes the latch-forward design we use in the keycarry and elsewhere. It's turned out to not suck.
	bool IsReady = false;

	//Generally, you don't need to override this, but it's virtual just in case. the default behavior is what we've found effective.
	virtual void AttemptRegister()
	{
		if (!IsReady)
		{
			IsReady = RegistrationImplementation();
		}
		
	};
	// override this:
	virtual bool RegistrationImplementation() { return false; };
};

UINTERFACE(NotBlueprintable)
class SKELETONKEY_API UKeyedConstruct : public UCanReady
{
	GENERATED_UINTERFACE_BODY()

	
};


inline UKeyedConstruct::UKeyedConstruct(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}



class SKELETONKEY_API IKeyedConstruct : public ICanReady
{
	GENERATED_IINTERFACE_BODY()
	
	virtual FSkeletonKey GetMyKey() const
	{
		UE_LOG(
	LogTemp,
	Error,
	TEXT("Unexpected call to unimplemented GetMyKey method. While not necessarily fatal, this is always worth checking."));
		return FSkeletonKey();
	}
};

struct WorldRecord
{
	bool IsReady = false;
	bool IsForbidden = false;
	bool IsSubstrate = false;
	
	bool SubsystemsReady = true;//starts true!
	
};


//tag interface marking out that this is a system, pillar, entity, or external that provides facts about keys or manages keys
UINTERFACE(NotBlueprintable)
class USkeletonLord : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};


inline USkeletonLord::USkeletonLord(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#define __GetWorldRecordBinding  std::shared_ptr<WorldRecord> ___LIVING = MyWorldState.expired() || this == nullptr ? nullptr : MyWorldState.lock();
#define __IsWorldRecordFullyReady  __GetWorldRecordBinding GEngine && ___LIVING != nullptr && ___LIVING->IsReady == true && ___LIVING->IsForbidden == false
//This is a much weaker guarantee
#define __AreArtillerySubsystemsFullyUp  __GetWorldRecordBinding GEngine && ___LIVING != nullptr && ___LIVING->SubsystemsReady == true && ___LIVING->IsForbidden == false
#define __AreArtillerySubsystemsComingUpSafely  __GetWorldRecordBinding GEngine && ___LIVING != nullptr && ___LIVING->SubsystemsReady == true

//A Key Binder, Keyless Itself (These are used ONLY for subsystems or other ECS pillars that are system preconditions)
class ISkeletonLord
{
	GENERATED_IINTERFACE_BODY()
	std::weak_ptr<WorldRecord> MyWorldState = std::weak_ptr<WorldRecord>();
};