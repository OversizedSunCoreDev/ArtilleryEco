// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtilleryAnimInstance.h"

#include "FBarragePrimitive.h"
#include "PhysicsTypes/BarrageColliderBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArtilleryAnimInstance)

void UArtilleryAnimInstance::NativeInitializeAnimation() 
{
	Super::NativeInitializeAnimation();
}

FVector UArtilleryAnimInstance::GetArtilleryOwnerVelocity() const 
{
	if (MyBarrageBody)
	{
		const FVector3f Velocity = FBarragePrimitive::GetVelocity(MyBarrageBody);
		return FVector(Velocity);
	}
	
	return FVector::ZeroVector;
}
