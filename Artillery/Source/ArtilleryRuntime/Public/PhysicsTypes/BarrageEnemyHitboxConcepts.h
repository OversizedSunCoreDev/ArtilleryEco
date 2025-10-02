// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryActorControllerConcepts.h"
#include "BarrageColliderBase.h"
#include "BarrageDispatch.h"
#include "SkeletonTypes.h"
#include "KeyCarry.h"
#include "Components/ActorComponent.h"
#include "ArtilleryActorControllerConcepts.h"
#include "ArtilleryDispatch.h"
#include "ArtilleryECSOnlyArtilleryTickable.h"
#include "CoordinateUtils.h"
#include "BarrageEnemyHitboxConcepts.generated.h"

//I'm sorry, I can't stand this semantic.
#define UNSAFE_GET_MESH_PTR auto MeshPtr = StaticMeshRef ? StaticMeshRef : Actor->GetComponentByClass<UStaticMeshComponent>();
// these all are basically actually ticklites that guarantee they will only interact with sim state.
struct FTickHitbox : public FTickECSOnly
{
	FSkeletonKey MyParentObjectKey;
	FVector MyRelativePosition = FVector::ZeroVector;
	explicit FTickHitbox(FSkeletonKey TargetIn, FSkeletonKey Parent, FVector RelPos)
		: FTickECSOnly(TargetIn)
	{
		MyParentObjectKey = Parent;
		MyRelativePosition = RelPos;
	}

	FTickHitbox() = default;

	virtual void ArtilleryTick(uint64_t TicksSoFar) override
	{
		if (ADispatch)
		{
			FBLet ParentPhysicsObject = ADispatch->GetFBLetByObjectKey(MyParentObjectKey, ADispatch->GetShadowNow());
			FBLet PhysicsObject = ADispatch->GetFBLetByObjectKey(Target, ADispatch->GetShadowNow());

			if (ParentPhysicsObject && PhysicsObject)
			{
				auto TempPos = FBarragePrimitive::GetPosition(ParentPhysicsObject);
				
				auto TempRot = FBarragePrimitive::OptimisticGetAbsoluteRotation(ParentPhysicsObject);
				auto Rotated = FVector(TempRot.RotateVector(UE::Math::TVector<float>(MyRelativePosition)) + TempPos);
				FBarragePrimitive::SetPosition(
					Rotated,
					PhysicsObject
				);
			}
		}
	}
};


typedef Ticklites::Ticklite<FTickHitbox> StartHitboxMovement;

//TODO: Document why enemy registration is not needed here.
//a free standing enemy sensor defined by a mesh. mostly superseded by child classes. 
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTILLERYRUNTIME_API UBarrageEnemyHitboxMesh : public UBarrageColliderBase, public IKeyedConstruct
{
	GENERATED_BODY()

public:
	using IKeyedConstruct::IsReady;
	// Sets default values for this component's properties
	UBarrageEnemyHitboxMesh(const FObjectInitializer& ObjectInitializer);
	//determines if this is a fixed subcomponent or not.
	UPROPERTY(EditAnywhere, Category=StaticMeshOwner)
	bool AmIASensor = false;
	UPROPERTY(EditAnywhere,  Category="StaticMeshOwner")
	bool ForceActualMesh = true;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category="StaticMeshOwner")
	UStaticMeshComponent* StaticMeshRef;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshOwner)
	bool IsHitboxStaticMeshVisible = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshOwner)
	bool ShouldIConsumeNavSpace;
	virtual void AttemptRegister() override;
	virtual void SetKeys();
	virtual void BeginPlay() override;
	virtual FSkeletonKey GetMyKey() const override;
	UArtilleryDispatch* ADispatch;
	virtual bool RegistrationImplementation() override;
	//TODO: FACTOR THIS OUT FACTOR THIS OUT FACTOR THIS OUT.
	virtual FSkeletonKey GenerateDependentKey(uint64_t parent, uint32_t localunique)
	{
		auto ret = parent & SKELLY::SFIX_KEYTOMETA;
		ret = (ret << 32) | localunique;
		return FSkeletonKey(ret);
	};
};

//CONSTRUCTORS
//--------------------

// Sets default values for this component's properties
inline UBarrageEnemyHitboxMesh::UBarrageEnemyHitboxMesh(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	auto OptimisticSeekMesh = Cast<UStaticMeshComponent>(GetAttachParent());
	if (OptimisticSeekMesh)
	{
		StaticMeshRef = OptimisticSeekMesh;
		StaticMeshRef->SetVisibility(IsHitboxStaticMeshVisible);
		StaticMeshRef->SetSimulatePhysics(false);
		StaticMeshRef->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		StaticMeshRef->SetCanEverAffectNavigation(ShouldIConsumeNavSpace);
	}
	PrimaryComponentTick.bCanEverTick = true;
	MyParentObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

inline void UBarrageEnemyHitboxMesh::AttemptRegister()
{
	
	if (GetOwner())
	{
		if (MyParentObjectKey == 0)
		{
			if (GetOwner())
			{
				// this REQUIRES a keycarry. I'm just done dorking about.
				if (GetOwner()->GetComponentByClass<UKeyCarry>())
				{
					SetKeys();
				}
			}
		}

		if (!IKeyedConstruct::IsReady && MyParentObjectKey != 0)
		// this could easily be just the !=, but it's better to have the whole idiom in the example
		{
			ADispatch = this->GetWorld()->GetSubsystem<UArtilleryDispatch>();
			if (ADispatch)
			{
				AActor* Actor = GetOwner();
				SetTransform(Actor->GetActorTransform());
				auto OptimisticSeekMesh = Cast<UStaticMeshComponent>(GetAttachParent());
				if (OptimisticSeekMesh)
				{
					StaticMeshRef = OptimisticSeekMesh;
					StaticMeshRef->SetVisibility(IsHitboxStaticMeshVisible);
					StaticMeshRef->SetSimulatePhysics(false);
					StaticMeshRef->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
					StaticMeshRef->SetCanEverAffectNavigation(ShouldIConsumeNavSpace);
				}
				IKeyedConstruct::IsReady = RegistrationImplementation();
			}
		}
	}
}


inline void UBarrageEnemyHitboxMesh::SetKeys()
{
	MyParentObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
}

inline void UBarrageEnemyHitboxMesh::BeginPlay()
{
	Super::BeginPlay();
	AttemptRegister();
}

inline FSkeletonKey UBarrageEnemyHitboxMesh::GetMyKey() const
{
	return MyParentObjectKey;
}

//most of this can be factored out once everything is settled, but a surprising amount of the flow control is in flux
//at the moment.
inline bool UBarrageEnemyHitboxMesh::RegistrationImplementation()
{
	AActor* Actor = GetOwner();
	if (Actor)
	{
		UNSAFE_GET_MESH_PTR
		if (MeshPtr)
		{
			// remember, jolt coords are X,Z,Y. BUT we don't want to scale the scale. this breaks our coord guidelines
			// by storing the jolted ver in the params but oh well.
			UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
			MyBarrageBody = Physics->LoadEnemyHitboxFromStaticMesh(Transform, MeshPtr, GetMyKey(), AmIASensor, ForceActualMesh);
		}

		if (MyBarrageBody)
		{
			IKeyedConstruct::IsReady = true;
		}


		if (IKeyedConstruct::IsReady)
		{
			//debuggo.
			//PrimaryComponentTick.SetTickFunctionEnable(false);
			return true;
		}
	}
	return false;
}


//simple hitboxes are an box bb that can be offset from your main enemy body to provide a secondary target.
//it does not add tags or attributes separate from the parent by default, and in fact, it will report hits to the
//parent directly.
//for all but the simplest cases, you'll want to track the relationship using a conserved key attribute
//we do try to embed some of the information in the key itself, but right now, that's not very useful.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTILLERYRUNTIME_API UBarrageSimpleEnemyHitbox : public UBarrageEnemyHitboxMesh
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float OffsetCenterToMatchBoundedShapeZ = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DiameterXYZ = FVector::ZeroVector;

	FVector MyRelativePosition = FVector::ZeroVector;
	FQuat MyRelativeRotation = FQuat::Identity; //TODO
	FSkeletonKey MyKey = FSkeletonKey::Invalid();


	// Sets default values for this component's properties
	UBarrageSimpleEnemyHitbox(const FObjectInitializer& ObjectInitializer);
	virtual void SetKeys() override;
	virtual bool RegistrationImplementation() override;
	virtual FSkeletonKey GetMyKey() const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
};

//CONSTRUCTORS
//--------------------

// Sets default values for this component's properties
inline UBarrageSimpleEnemyHitbox::UBarrageSimpleEnemyHitbox(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	MyRelativePosition = {0,0,0};
	PrimaryComponentTick.bCanEverTick = true;
	MyParentObjectKey = 0;
	bAlwaysCreatePhysicsState = false;
	UPrimitiveComponent::SetNotifyRigidBodyCollision(false);
	bCanEverAffectNavigation = false;
	Super::SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	Super::SetEnableGravity(false);
	Super::SetSimulatePhysics(false);
}

inline void UBarrageSimpleEnemyHitbox::SetKeys()
{
	MyParentObjectKey = GetOwner()->GetComponentByClass<UKeyCarry>()->GetMyKey();
	auto name = GetTypeHash(this->GetName());
	MyKey = GenerateDependentKey(MyParentObjectKey, name);
}

inline bool UBarrageSimpleEnemyHitbox::RegistrationImplementation()
{
	AActor* Actor = GetOwner();
	SetTransform(Actor->GetActorTransform());
	auto ActorCenter = Actor->GetActorLocation();
	auto rotator = Actor->GetActorRotation().Quaternion();
	UNSAFE_GET_MESH_PTR
	if (MeshPtr)
	{
		FVector extents = DiameterXYZ.IsNearlyZero() || DiameterXYZ.Length() <= 0.1 ? FVector(1, 1, 1) : DiameterXYZ;

		// remember, jolt coords are X,Z,Y. BUT we don't want to scale the scale. this breaks our coord guidelines
		// by storing the jolted ver in the params but oh well.
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		if (Physics && ADispatch)
		{
			MyRelativePosition = {
				OffsetCenterToMatchBoundedShapeX, OffsetCenterToMatchBoundedShapeY, OffsetCenterToMatchBoundedShapeZ
			};
			FBBoxParams params = FBarrageBounder::GenerateBoxBounds(
				rotator.RotateVector(MyRelativePosition) + ActorCenter,
				FMath::Max(extents.X, .1),
				FMath::Max(extents.Y, 0.1),
				FMath::Max(extents.Z, 0.1),
				FVector3d(OffsetCenterToMatchBoundedShapeX, OffsetCenterToMatchBoundedShapeY,
				          OffsetCenterToMatchBoundedShapeZ), FMassByCategory::MostEnemies);
			MyBarrageBody = Physics->CreatePrimitive(params, GetMyKey(), Layers::EJoltPhysicsLayer::ENEMYHITBOX, AmIASensor,
			                                         true, true);
			if (MyBarrageBody)
			{
				IKeyedConstruct::IsReady = true;
				FBarragePrimitive::ApplyRotation(rotator, MyBarrageBody);
			}
		}
	}

	if (IKeyedConstruct::IsReady)
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		FTickHitbox temp = FTickHitbox(GetMyKey(), MyParentObjectKey, {0,0,0}); //This starts a ticklite that lives as long as the key of the collider.
		//these colliders can actually have different lifespans compared to the parent entity
		this->ADispatch->RequestAddTicklite(MakeShareable(new StartHitboxMovement(temp)), Early);
		return true;
	}
	return false;
}

inline FSkeletonKey UBarrageSimpleEnemyHitbox::GetMyKey() const
{
	return MyKey;
}

inline void UBarrageSimpleEnemyHitbox::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	auto store = FBarragePrimitive::GetLocalBounds(MyBarrageBody);
	auto pos =  this->GetActorPositionForRenderer();
	DrawDebugLine(ADispatch->GetWorld(), pos,  FVector(FBarragePrimitive::GetPosition(MyBarrageBody)), FColor::Red, true, 5.f);
	DrawDebugLine(ADispatch->GetWorld(), pos,  pos + store.Key, FColor::Green, false, 5.f);
	DrawDebugLine(ADispatch->GetWorld(), pos,  pos + store.Value, FColor::Blue, false, 5.f);
}

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARTILLERYRUNTIME_API UBarrageDependentEnemyHitbox : public UBarrageSimpleEnemyHitbox
{
	GENERATED_BODY()

public:
	//determines if this is a fixed subcomponent or not.
	UPROPERTY(EditAnywhere, Category=StaticMeshOwner)
	bool CanThisMoveIndependentOfTheParent = false;
	// Sets default values for this component's properties
	UBarrageDependentEnemyHitbox(const FObjectInitializer& ObjectInitializer);
	virtual bool RegistrationImplementation() override;
};

//CONSTRUCTORS
//--------------------

// Sets default values for this component's properties
inline UBarrageDependentEnemyHitbox::UBarrageDependentEnemyHitbox(const FObjectInitializer& ObjectInitializer) : Super(
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

inline bool UBarrageDependentEnemyHitbox::RegistrationImplementation()
{
	AActor* Actor = GetOwner();
	UNSAFE_GET_MESH_PTR
	if (MeshPtr)
	{
		FTransform AbsToChaos = MeshPtr->GetComponentTransform(); //screwed up by using the rel transform, forgetting that rel transforms are modified by all hierarchy members. so uh. this meant that was of by around 8x in one test case, and 100x in another.
		SetTransform(AbsToChaos);
		MyRelativePosition = AbsToChaos.GetLocation() - Actor->GetTransform().GetLocation(); //under the hood, grabs transform's translation
		MyRelativeRotation = AbsToChaos.GetRotation(); //does this double apply it?
		// remember, jolt coords are X,Z,Y. BUT we don't want to scale the scale. this breaks our coord guidelines
		// by storing the jolted ver in the params but oh well.
		UBarrageDispatch* Physics = GetWorld()->GetSubsystem<UBarrageDispatch>();
		MyBarrageBody = Physics->LoadEnemyHitboxFromStaticMesh(Transform, MeshPtr, GetMyKey(), AmIASensor, ForceActualMesh);
	}

	if (MyBarrageBody)
	{
		IKeyedConstruct::IsReady = true;
	}


	if (IKeyedConstruct::IsReady)
	{
		FTickHitbox TickLaunchable = FTickHitbox(GetMyKey(), MyParentObjectKey, MyRelativePosition); //this semantic STILL sucks. gotta fix it.
		this->ADispatch->RequestAddTicklite(MakeShareable(new StartHitboxMovement(TickLaunchable)), Early);
		return true;
	}
	return false;
}
