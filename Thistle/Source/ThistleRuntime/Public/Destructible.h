#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "GenericTeamAgentInterface.h"
#include "ArtilleryRuntime/Public/Systems/ArtilleryDispatch.h"
#include "FMockArtilleryGun.h"
#include "GameplayTags.h"
#include "UEnemyMachine.h"
#include "UEventLogSystem.h"
#include "PhysicsTypes/BarrageAutoBox.h"
#include "PhysicsTypes/BarrageAutoCap.h"

#include "Destructible.generated.h"


UCLASS()
class THISTLERUNTIME_API ADestructible : public AActor, public IGenericTeamAgentInterface, public IKeyedConstruct
{
	GENERATED_BODY()
public:
	//You are expected to call finish dying on your death being ready to tidy up.
	UFUNCTION(BlueprintNativeEvent,  Category = "Thistle")
	void OnDeath();
	void OnDeath_Implementation()
	{
		//this likely doesn't help dash heal
		ArtilleryStateMachine->MyDispatch->AddTagToEntity(GetMyKey(), TAG_Entity_IsDead_True);
		FinishDeath();
	}
	UFUNCTION(BlueprintCallable,  Category = "Thistle")
	virtual void FinishDeath()
	{
		GetWorld()->GetSubsystem<UEventLogSubsystem>()->LogEvent(E_EventLogType::Died, MyKey);	
		this->Destroy();
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thistle", meta = (AllowPrivateAccess = "true"))	
	bool AffectedByGravity = true;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Thistle", meta = (AllowPrivateAccess = "true"))
	UBarrageColliderBase* BarragePhysicsAgent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thistle", meta = (AllowPrivateAccess = "true"))	
	UKeyCarry* LKeyCarry;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Thistle", meta = (AllowPrivateAccess = "true"))
	UEnemyMachine* ArtilleryStateMachine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Barrage)
	bool disableZAxis = true;
	
	virtual inline FSkeletonKey GetMyKey() const override
	{
		return LKeyCarry->GetMyKey();
	}
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void AddForce(FVector3f Force)
	{
		FVector3d FinalForce(Force.X, Force.Y, disableZAxis ? 0 : Force.Z);
		FBarragePrimitive::ApplyForce(FinalForce, BarragePhysicsAgent->MyBarrageBody, AIMovement);
	}

	ADestructible(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	{
		BarragePhysicsAgent = CreateDefaultSubobject<UBarrageColliderBase>(TEXT("Barrage Physics Agent"));
		SetRootComponent(BarragePhysicsAgent);
		ArtilleryStateMachine = CreateDefaultSubobject<UEnemyMachine>(TEXT("Artillery Enemy Machine"));
		LKeyCarry = CreateDefaultSubobject<UKeyCarry>(TEXT("ActorKeyComponent"));
		LKeyCarry->AttemptRegister();
		this->DisableComponentsSimulatePhysics();
		UMeshComponent* Mesh = GetComponentByClass<UMeshComponent>();
		if (Mesh != nullptr)
		{
			Mesh ->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		}
	}
	virtual void BeginPlay() override
	{
		Super::BeginPlay();
		if(!IsDefaultSubobject())
		{
			AttemptRegister();
			BarragePhysicsAgent->RegistrationImplementation();
			// No Chaos for you
			this->DisableComponentsSimulatePhysics();
			ArtilleryStateMachine->MyDispatch->REGISTER_ENTITY_FINAL_TICK_RESOLVER(GetMyKey());
		}
		
		if(AffectedByGravity == false)
		{
			FBarragePrimitive::SetGravityFactor(0, BarragePhysicsAgent->MyBarrageBody);
		}
		
		UMeshComponent* Mesh = GetComponentByClass<UMeshComponent>();
		if (Mesh != nullptr)
		{
			Mesh->AlwaysLoadOnClient = true;
			Mesh->AlwaysLoadOnServer = true;
			Mesh->bOwnerNoSee = false;
			Mesh->bCastDynamicShadow = true;
			Mesh->bAffectDynamicIndirectLighting = true;
			Mesh->SetSimulatePhysics(false);
			Mesh->bAlwaysCreatePhysicsState = false;
			Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
			Mesh->SetGenerateOverlapEvents(false);
			Mesh->SetCanEverAffectNavigation(false);
		}
	}

	virtual void PostInitializeComponents() override
	{
		Super::PostInitializeComponents();
		if (!IsDefaultSubobject() && LKeyCarry)
		{
			LKeyCarry->AttemptRegister();
		}
	}
	
	UFUNCTION(BlueprintGetter)
	virtual bool RegistrationImplementation() override
	{
		
		if(!ArtilleryStateMachine->MyDispatch)
		{
			ArtilleryStateMachine->MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
			ArtilleryStateMachine->TransformDispatch =  GetWorld()->GetSubsystem<UTransformDispatch>();
		}
		
		if(ArtilleryStateMachine->MyDispatch)
		{
			LKeyCarry->AttemptRegister();
			// FArtilleryRunAILocomotionFromDispatch Inbound;
			// Inbound.BindUObject(this, &AThistleInject::LocomotionStateMachine); // this looks like it's not used...
			TMap<AttribKey, double> MyAttributes = TMap<AttribKey, double>();
			MyAttributes.Add(HEALTH, 1000);
			MyAttributes.Add(MAXHEALTH, 100);
			MyAttributes.Add(Attr::HealthRechargePerTick, 0);
			MyAttributes.Add(MANA, 1000);
			MyAttributes.Add(MAXMANA, 1000);
			MyAttributes.Add(Attr::ManaRechargePerTick, 10);
			MyAttributes.Add(Attr::ProposedDamage, 5);
			MyKey = ArtilleryStateMachine->CompleteRegistrationWithAILocomotionAndParent( MyAttributes, LKeyCarry->GetMyKey());

			//Vectors
			Attr3MapPtr VectorAttributes = MakeShareable(new Attr3Map());
			VectorAttributes->Add(Attr3::AimVector, MakeShareable(new FConservedVector()));
			VectorAttributes->Add(Attr3::FacingVector, MakeShareable(new FConservedVector()));
			ArtilleryStateMachine->MyDispatch->RegisterOrAddVecAttribs(LKeyCarry->GetMyKey(), VectorAttributes);

			IdMapPtr MyRelationships = MakeShareable(new IdentityMap());
			ArtilleryStateMachine->MyDispatch->RegisterOrAddRelationships(LKeyCarry->GetMyKey(), MyRelationships);

			ArtilleryStateMachine->MyTags->AddTag(TAG_Enemy);
			ArtilleryStateMachine->MyTags->AddTag(FGameplayTag::RequestGameplayTag("Enemy"));
		
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Attributes, meta=(BlueprintThreadSafe))
	virtual FVector GetBarragePhysicsVelocity()
	{
		return FVector(FBarragePrimitive::GetVelocity(BarragePhysicsAgent->MyBarrageBody));
	}

protected:
	UFUNCTION(BlueprintCallable, Category = Attributes, meta=(BlueprintThreadSafe))
	virtual float GetHealth()
	{
		if (ArtilleryStateMachine->MyDispatch)
		{		
			auto HealthAttr = ArtilleryStateMachine->MyDispatch->GetAttrib(GetMyKey(), HEALTH);
			if (HealthAttr)
			{
				return HealthAttr->GetCurrentValue();
			}
			return 0.f;
		}
		return 0.f;
	}



	FSkeletonKey MyKey;
};