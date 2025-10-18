// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "EPhysicsLayer.h"
#include "Components/ActorComponent.h"
#include "BarrageAutoBox.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DefaultToInstanced )
class ARTILLERYRUNTIME_API UBarrageAutoBox : public UBarrageColliderBase
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool isMovable = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeX = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeY = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeZ = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DiameterXYZ = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite )
	TEnumAsByte<EBWeightClasses::Type> Weight;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite )
	EPhysicsLayer Layer;
	FMassByCategory MyMassClass;

	UBarrageAutoBox(const FObjectInitializer& ObjectInitializer);
	
	virtual bool RegistrationImplementation() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};

//CONSTRUCTORS
//--------------------
//do not invoke the default constructor unless you have a really good plan. in general, let UE initialize your components.

// Sets default values for this component's properties
inline UBarrageAutoBox::UBarrageAutoBox(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), MyMassClass(Weights::NormalEnemy)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	switch (Weight)
	{
	case EBWeightClasses::NormalEnemy : MyMassClass = Weights::NormalEnemy; break;
	case EBWeightClasses::BigEnemy : MyMassClass = Weights::BigEnemy; break;
	case EBWeightClasses::HugeEnemy : MyMassClass = Weights::HugeEnemy; break;
	default: MyMassClass = FMassByCategory(Weights::NormalEnemy); break;
	}

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

inline bool UBarrageAutoBox::RegistrationImplementation()
{
	if (GetOwner())
	{
		if(MyParentObjectKey ==0)
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
	
		if(!IsReady && MyParentObjectKey != 0) // this could easily be just the !=, but it's better to have the whole idiom in the example
		{
			UPrimitiveComponent* AnyMesh = GetOwner()->GetComponentByClass<UMeshComponent>(); 
			AnyMesh = AnyMesh ? AnyMesh : GetOwner()->GetComponentByClass<UPrimitiveComponent>();
			if(AnyMesh)
			{
				FVector extents = DiameterXYZ.IsNearlyZero() || DiameterXYZ.Length() <= 0.1 ? FVector::ZeroVector : DiameterXYZ;
				if(extents.IsZero())
				{
					FBoxSphereBounds Boxen = AnyMesh->GetLocalBounds();
					if(Boxen.BoxExtent.GetMin() >= 0.01)
					{
						// Multiply by the scale factor, then multiply by 2 since mesh bounds is radius not diameter
						extents = Boxen.BoxExtent * AnyMesh->GetComponentScale() * 2;				
					}
					else
					{
						//I SAID BEHAAAAAAAAAAAVE.
						extents = FVector(1,1,1);
					}
				}

				UBarrageDispatch* Physics =  GetWorld()->GetSubsystem<UBarrageDispatch>();
				FBBoxParams params = FBarrageBounder::GenerateBoxBounds(
					GetOwner()->GetActorLocation(),
					FMath::Max(extents.X, .1),
					FMath::Max(extents.Y, 0.1),
					FMath::Max( extents.Z, 0.1),
					FVector3d(OffsetCenterToMatchBoundedShapeX, OffsetCenterToMatchBoundedShapeY, OffsetCenterToMatchBoundedShapeZ),
					MyMassClass.Category);
				MyBarrageBody = Physics->CreatePrimitive(params, MyParentObjectKey, static_cast<uint16>(Layer), false, false, isMovable);
				if(MyBarrageBody)
				{
					AnyMesh->WakeRigidBody(); 
					IsReady = true;
					AnyMesh->SetSimulatePhysics(false);
				}
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

