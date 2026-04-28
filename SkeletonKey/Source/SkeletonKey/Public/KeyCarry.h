// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "KeyedConcept.h"
#include "SkeletonTypes.h"
#include "TransformDispatch.h"
#include "Runtime/Engine/Classes/Components/ActorComponent.h"
#include "KeyCarry.generated.h"

//this is a simple key-carrier that automatically wires the actorkey up.
//It tries during initialize, and if that fails, it tries again each tick until successful,
//then turns off ticking for itself. Clients should use the Retry_Notify delegate to register
//for notification of success in production code, rather than relying on initialization sequencing.
//Later versions will also set a gameplay tag to indicate that this actor carries a key.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DefaultToInstanced)
class SKELETONKEY_API UKeyCarry : public UActorComponent, public IKeyedConstruct
{
	GENERATED_BODY()
	
protected:
	//We use the blueprint getter to make the keys carried visible in the
	//state tree interface, which has some pretty annoying requirements for
	//being able to interact with user-defined UStructs. Namely, it doesn't
	//correctly pick up functions, just properties. But we don't ever
	//want a direct pull of an object key, in case it's stale.

	
public:
	DECLARE_MULTICAST_DELEGATE(ActorKeyIsReady)
	ActorKeyIsReady Retry_Notify;
	UPROPERTY(BlueprintGetter=GetMyKey, EditAnywhere)
	FSkeletonKey MyObjectKey;
	UFUNCTION(BlueprintGetter)
	virtual FSkeletonKey GetMyKey() const override
	{
		return MyObjectKey;
	}
	
	UKeyCarry(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	{
		// While we init, we do not tick more than once.. This is a "data only" component. gotta be a better way to do this.
		PrimaryComponentTick.bCanEverTick = true;
		bWantsInitializeComponent = true;
	}
	
	virtual bool RegistrationImplementation() override
	{
		if(GetWorld())
		{
			UTransformDispatch* TransformDispatch = GetWorld()->GetSubsystem<UTransformDispatch>();
			AActor* actorRef = GetOwner();
			if(TransformDispatch != nullptr && actorRef != nullptr)
			{
				actorRef->UpdateComponentTransforms();
				UStaticMeshComponent* ActorStaticMesh =  actorRef->GetComponentByClass<UStaticMeshComponent>();
				if(ActorStaticMesh)
				{
					ActorStaticMesh->bEvaluateWorldPositionOffset = true;
					ActorStaticMesh->UpdateInitialEvaluateWorldPositionOffset();
				}
				MyObjectKey = MAKE_ACTORKEY(GetOwner());
				if (MyObjectKey.IsValid())
				{
					TransformDispatch->RegisterObjectToShadowTransform(MyObjectKey ,actorRef);
					actorRef->GetRootComponent()->bNeverNeedsRenderUpdate = false;
					if(Retry_Notify.IsBound())
					{
						Retry_Notify.Broadcast();
					}
					SetComponentTickEnabled(false);
					return true;
				}
			}
		}
		return false;
	}
	
	//will return an invalid object key if it fails.
	static FSkeletonKey KeyOf(AActor* That)
	{
		if(That && That->GetComponentByClass<UKeyCarry>())
		{
			return That->GetComponentByClass<UKeyCarry>()->MyObjectKey;
		}
		return FSkeletonKey();
	}

	virtual void InitializeComponent() override
	{
		
		static int MonotonicKey = 0;
		Super::InitializeComponent();
		if(!HasAnyFlags(RF_ClassDefaultObject) && !IsEditorOnly() && !IsTemplate())
		{
			//disabled for now.
			auto wrld = GetWorld();
			if (wrld)
			{
				wrld->GetSubsystem<UOrdinatePillar>()->REGISTERORDER(++MonotonicKey,1,this);
			}
		}
		IsReady = false;
	}

	//todo: switch this to an artillery tick
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override
	{
		//this shouldn't come up often in practice but late initialization can be an issue.
		if(!IsReady)
		{
			AttemptRegister();
		}
		else
		{
			//shouldn't come up, but in case a refactor makes this case arise:
			SetComponentTickEnabled(false);
		}
	}

	//will return an invalid object key if it fails.
	static FSkeletonKey KeyOf(const UActorComponent* Me)
	{
		if(Me && Me->GetOwner())
		{
			UKeyCarry* ptr = Me->GetOwner()->GetComponentByClass<UKeyCarry>();
			if(ptr)
			{
				return ptr->MyObjectKey;
			}
		}
		return FSkeletonKey();
	}

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();
	}
	
	virtual void BeginDestroy() override
	{
		Super::BeginDestroy();
		if(GetWorld())
		{
			UTransformDispatch* xRef = GetWorld()->GetSubsystem<UTransformDispatch>();
			if(xRef != nullptr)
			{
				xRef->ReleaseKineByKey(MyObjectKey);
			}
		}
	}
};

//UKeyCarry ended up being a much more generic name that I expected.
//SKELETON is a little silly, but it means that the invocation is often SKELETON::Key(this) which at least
//gives you a hint that the SkeletonKey library is involved at a glance.
typedef UKeyCarry SKELETON;
