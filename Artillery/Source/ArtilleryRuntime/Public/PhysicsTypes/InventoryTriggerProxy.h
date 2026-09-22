#pragma once
#include "CoreMinimal.h"
#include "InventoryDispatch.h"
#include "KeyedConcept.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Subsystems/WorldSubsystem.h"
#include "InventoryTriggerProxy.generated.h"

UCLASS(Blueprintable)
class AUInventoryTriggerProxy : public AActor, public IKeyedConstruct
{
	GENERATED_BODY()

public:
	virtual FSkeletonKey GetMyKey() const override;

protected:
	virtual void RegisterDependencies() override;

public:
	AUInventoryTriggerProxy()
	{
		this->DisableComponentsSimulatePhysics();
		UMeshComponent* Mesh = GetComponentByClass<UMeshComponent>();
		if (Mesh != nullptr)
		{
			Mesh ->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		}
	}

protected:
	FSkeletonKey KeyFromTrigger;

public:
	virtual bool RegistrationImplementation() override;
};

inline FSkeletonKey AUInventoryTriggerProxy::GetMyKey() const
{
	return KeyFromTrigger;
}

inline void AUInventoryTriggerProxy::RegisterDependencies()
{
	Super::RegisterDependencies();
}

inline bool AUInventoryTriggerProxy::RegistrationImplementation()
{
	auto MyDispatch =  GetWorld()->GetSubsystem<UArtilleryDispatch>();
	if (MyDispatch)
	{
		//getting this is gonna be a lil more annoying than I thought. will come back
		//KeyFromTrigger 
		auto a = MyDispatch->RequestRouter->CreateTriggerOnVerifiedFrame(this, MyDispatch->GetShadowNow());
		return IKeyedConstruct::RegistrationImplementation() && true;
	}
	
	return false;
}
