// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "Components/ActorComponent.h"
#include "BarrageBoxComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTILLERYRUNTIME_API UBarrageBoxComponent : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int XDiam = 30;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int YDiam = 30;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	int ZDiam = 20;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeX = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeY = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeZ = 0;
	
	UBarrageBoxComponent(const FObjectInitializer& ObjectInitializer);
	virtual bool RegistrationImplementation() override;
};

//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
inline UBarrageBoxComponent::UBarrageBoxComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	MyParentObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

//KEY REGISTER, initializer, and failover.
//----------------------------------

inline bool UBarrageBoxComponent::RegistrationImplementation()
{
	if(MyParentObjectKey == 0)
	{
		if(GetOwner())
		{
			if(GetOwner()->GetComponentByClass<UKeyCarry>())
			{
				MyParentObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
			}

			if(MyParentObjectKey == 0)
			{
				uint32 val = PointerHash(GetOwner());
				MyParentObjectKey = ActorKey(val);
			}
		}
	}
	
	if(!IsReady && MyParentObjectKey != 0) // this could easily be just the !=, but it's better to have the whole idiom in the example
	{
		UBarrageDispatch* Physics =  GetWorld()->GetSubsystem<UBarrageDispatch>();
		FBBoxParams params = FBarrageBounder::GenerateBoxBounds(
			GetOwner()->GetActorLocation(),
			XDiam,
			YDiam,
			ZDiam,
			FVector3d(OffsetCenterToMatchBoundedShapeX, OffsetCenterToMatchBoundedShapeY, OffsetCenterToMatchBoundedShapeZ));
		MyBarrageBody = Physics->CreatePrimitive(params, MyParentObjectKey, Layers::MOVING);
		if(MyBarrageBody)
		{
			IsReady = true;
		}
	}
	
	if(IsReady)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return true;
	}
	return false;
}
