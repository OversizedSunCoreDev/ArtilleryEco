// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkeletonTypes.h"
#include "TransformDispatch.h"
#include "KeyCarry.h"
#include "PlayerKine.h"
#include "Runtime/Engine/Classes/Components/ActorComponent.h"
#include "PlayerKeyCarry.generated.h"

//this is a simple key-carrier that automatically wires the player up.
//It tries during initialize, and if that fails, it tries again each tick until successful,
//then turns off ticking for itself. Clients should use the Retry_Notify delegate to register
//for notification of success in production code, rather than relying on initialization sequencing.
//Later versions will also set a gameplay tag to indicate that this actor carries a key.
//TODO: integrate with the ORDIN system.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DefaultToInstanced)
class ARTILLERYRUNTIME_API UPlayerKeyCarry : public UKeyCarry
{
	GENERATED_BODY()

public:
	FSkeletonKey MyControllerIfAny;
	
	DECLARE_MULTICAST_DELEGATE(ActorKeyIsReady)
	ActorKeyIsReady Retry_Notify;
	
	UPlayerKeyCarry(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	{
		// While we init, we do not tick more than once.. This is a "data only" component. gotta be a better way to do this.
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();
	}

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();
		auto wrld = GetWorld();
		if (wrld)
		{
			wrld->GetSubsystem<UOrdinatePillar>()->REGISTERORDER(static_cast<int>(PlayerKey::CABLE),0,this);
		}
	}

	virtual void PostLoad() override
	{
		Super::PostLoad();
	}

	virtual bool RegistrationImplementation() override
	{
		if(GetWorld())
		{
			if(UTransformDispatch* xRef = GetWorld()->GetSubsystem<UTransformDispatch>())
			{
				// ReSharper disable once CppUE4CodingStandardNamingViolationWarning
				// The function of the code becomes inobvious.
				AActor* actorRef = GetOwner();
				if(xRef != nullptr && actorRef !=nullptr)
				{
					actorRef->UpdateComponentTransforms();
					if(actorRef)
					{
						//I think this will end up diverging more than usual.
						uint32 val = PointerHash(GetOwner());
						ActorKey TopLevelActorKey = ActorKey(val);
						MyObjectKey = TopLevelActorKey;
						if (MyObjectKey.IsValid())
						{						
							UE_LOG(LogTemp, Warning, TEXT("PKC Parented: %d"), val);
							xRef->RegisterObjectToShadowTransform<PlayerKine, ActorKey, TObjectPtr<AActor> >(MyObjectKey ,actorRef);
							if(Retry_Notify.IsBound())
							{
								Retry_Notify.Broadcast();
							}
							SetComponentTickEnabled(false);
							return true;
						}
					}
				}
			}
		}
		return false;
	}
	
	//will return an invalid object key if it fails.
	static FSkeletonKey KeyOfPlayer(AActor* That)
	{
		if(That)
		{
			UPlayerKeyCarry* ThatKeyCarry = That->GetComponentByClass<UPlayerKeyCarry>();
			return ThatKeyCarry ? ThatKeyCarry->MyObjectKey : FSkeletonKey();
		}
		return FSkeletonKey();
	}
};
