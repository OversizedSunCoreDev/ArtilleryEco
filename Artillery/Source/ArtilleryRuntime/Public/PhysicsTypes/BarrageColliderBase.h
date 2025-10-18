// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "FBarragePrimitive.h"
#include "Components/ActorComponent.h"
#include "BarrageColliderBase.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBarrageColliderBase : public UPrimitiveComponent, public ICanReady
{
	GENERATED_BODY()

public:
	FBLet MyBarrageBody = nullptr;
	FSkeletonKey MyParentObjectKey;
	
	// Sets default values for this component's properties
	UBarrageColliderBase(const FObjectInitializer& ObjectInitializer);
	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeforeBeginPlay(FSkeletonKey TransformOwner);
	//Colliders must override this.
	virtual bool RegistrationImplementation() override;

	virtual void OnDestroyPhysicsState() override;
	
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetTransform(const FTransform& NewTransform);

protected:
	FBTransform Transform;
};

//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
inline UBarrageColliderBase::UBarrageColliderBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	MyParentObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

//---------------------------------

inline void UBarrageColliderBase::InitializeComponent()
{
	Super::InitializeComponent();
}

//SETTER: Unused example of how you might set up a registration for an arbitrary key.
inline void UBarrageColliderBase::BeforeBeginPlay(FSkeletonKey TransformOwner)
{
	MyParentObjectKey = TransformOwner;
}

//Colliders must override this.
inline bool UBarrageColliderBase::RegistrationImplementation()
{
	PrimaryComponentTick.SetTickFunctionEnable(false);
	return true;
}

inline void UBarrageColliderBase::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AttemptRegister(); // ...
}

// Called when the game starts
inline void UBarrageColliderBase::BeginPlay()
{
	Super::BeginPlay();
	AttemptRegister();
}

//TOMBSTONERS

inline void UBarrageColliderBase::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();
	if (GetWorld())
	{
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics && MyBarrageBody)
		{
			Physics->SuggestTombstone(MyBarrageBody);
			MyBarrageBody.Reset();
		}
	}
}

inline void UBarrageColliderBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (GetWorld())
	{
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics && MyBarrageBody)
		{
			Physics->SuggestTombstone(MyBarrageBody);
			MyBarrageBody.Reset();
		}
	}
}

inline void UBarrageColliderBase::SetTransform(const FTransform& NewTransform)
{
	Transform.SetTransform(NewTransform);
}
