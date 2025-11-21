// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "Components/ActorComponent.h"
#include "BarrageStaticAutoMesh.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARTILLERYRUNTIME_API UBarrageStaticAutoMesh : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBarrageStaticAutoMesh(const FObjectInitializer& ObjectInitializer);
	virtual bool RegistrationImplementation() override;
};

//CONSTRUCTORS
//--------------------

// Sets default values for this component's properties
inline UBarrageStaticAutoMesh::UBarrageStaticAutoMesh(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
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

inline bool UBarrageStaticAutoMesh::RegistrationImplementation()
{
	if (GetOwner())
	{
		if(MyParentObjectKey ==0 )
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
			AActor* Actor = GetOwner();
			SetTransform(Actor->GetActorTransform());
		
			UStaticMeshComponent* MeshPtr = Actor->GetComponentByClass<UStaticMeshComponent>();
			if(MeshPtr)
			{
				// remember, jolt coords are X,Z,Y. BUT we don't want to scale the scale. this breaks our coord guidelines
				// by storing the jolted ver in the params but oh well.
				UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
				MyBarrageBody = Physics->LoadComplexStaticMesh(Transform, MeshPtr, MyParentObjectKey);
			}
			
			if(MyBarrageBody)
			{
				IsReady = true;
			}
		}
	}
	
	if(IsReady)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return true;
	}
	return false;
}