#include "PhysicsTypes/BarrageStaticAutoMesh.h"

#include "FWorldSimOwner.h"
#include "Landscape.h"
#include "Conversion/BarrageUnrealSkeletonToJoltRagdollConversion.h"

//CONSTRUCTORS
//--------------------

// Sets default values for this component's properties
UBarrageStaticAutoMesh::UBarrageStaticAutoMesh(const FObjectInitializer& ObjectInitializer) : Super(
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

bool UBarrageStaticAutoMesh::RegistrationImplementation()
{
	if (GetOwner())
	{
		if (MyParentObjectKey == 0)
		{
			if (GetOwner())
			{
				if (GetOwner()->GetComponentByClass<UKeyCarry>())
				{
					MyParentObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
				}

				if (MyParentObjectKey == 0)
				{
					uint32 val = PointerHash(GetOwner());
					MyParentObjectKey = ActorKey(val);
				}
			}
		}

		if (!IsReady && MyParentObjectKey != 0) // this could easily be just the !=, but it's better to have the whole idiom in the example
		{
			AActor* Actor = GetOwner();
			SetTransform(Actor->GetActorTransform());

			UStaticMeshComponent* MeshPtr = Actor->GetComponentByClass<UStaticMeshComponent>();
			if (MeshPtr)
			{
				// remember, jolt coords are X,Z,Y. BUT we don't want to scale the scale. this breaks our coord guidelines
				// by storing the jolted ver in the params but oh well.
				UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
				MyBarrageBody = Physics->LoadComplexStaticMesh(Transform, MeshPtr, MyParentObjectKey);
			}

			if (MyBarrageBody)
			{
				IsReady = true;
			}


			if (USkeletalMeshComponent* SkeletalMeshComp = Actor->GetComponentByClass<USkeletalMeshComponent>()) {
				auto SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
				auto PhysicsAsset = SkeletalMeshComp->GetPhysicsAsset();
				if (SkeletalMesh && PhysicsAsset) {
					UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
					if (ensure(Physics->JoltGameSim) && ensure(Physics->JoltGameSim->physics_system)) {
						auto Ragdoll = Barrage::Conversion::ExampleCreateJoltRagdollFromUnrealSkeleton(
							*Physics->JoltGameSim->physics_system,
							SkeletalMeshComp,
							SkeletalMesh->GetSkeleton(),
							PhysicsAsset);
					}
				}
			}
			
			if (ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(Actor))
			{
				UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
				Physics->CreateHeightfieldLandscapeMesh(Landscape);
			}
		}
	}

	if (IsReady)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return true;
	}
	return false;
}