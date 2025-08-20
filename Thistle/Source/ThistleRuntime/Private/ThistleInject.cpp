#include "ThistleInject.h"

#include "ArtilleryBPLibs.h"
#include "ArtilleryDispatch.h"
#include "UFireControlMachine.h"
#include "NavigationSystem.h"
#include "ThistleBehavioralist.h"
#include "UEventLogSystem.h"
#include "Public/GameplayTags.h"

void AThistleInject::OnDeath_Implementation()
{
	FinishDeath();
}

void AThistleInject::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!IsDefaultSubobject())
	{
		LKeyCarry->AttemptRegister();
	}
}

inline FSkeletonKey AThistleInject::GetMyKey() const
{
	return LKeyCarry->GetMyKey();
}

inline bool AThistleInject::RegistrationImplementation()
{
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
		ArtilleryStateMachine->MyDispatch->RegisterVecAttribs(LKeyCarry->GetMyKey(), VectorAttributes);

		IdMapPtr MyRelationships = MakeShareable(new IdentityMap());
		MyRelationships->Add(Ident::Target, MakeShareable(new FConservedAttributeKey()));
		MyRelationships->Add(Ident::EquippedMainGun, MakeShareable(new FConservedAttributeKey()));
		MyRelationships->Add(Ident::Squad, MakeShareable(new FConservedAttributeKey()));
		ArtilleryStateMachine->MyDispatch->RegisterRelationships(LKeyCarry->GetMyKey(), MyRelationships);

		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Move_Needed);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Attack_Available);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Target_Needed);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Rally_PreferSquad);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Enemy);
		ArtilleryStateMachine->MyTags->AddTag(FGameplayTag::RequestGameplayTag("Enemy"));
		
		return true;
	}
	return false;
}

void AThistleInject::FinishDeath()
{
	GetWorld()->GetSubsystem<UEventLogSubsystem>()->LogEvent(E_EventLogType::Died, MyKey);	
	this->Destroy();
}

// Sets default values
AThistleInject::AThistleInject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NavAgentProps.NavWalkingSearchHeightScale = FNavigationSystem::GetDefaultSupportedAgent().NavWalkingSearchHeightScale;
	BarragePhysicsAgent = CreateDefaultSubobject<UBarrageAutoBox>(TEXT("Barrage Physics Agent"));
	SetRootComponent(BarragePhysicsAgent);
	ArtilleryStateMachine = CreateDefaultSubobject<UEnemyMachine>(TEXT("Artillery Enemy Machine"));
	LKeyCarry = CreateDefaultSubobject<UKeyCarry>(TEXT("ActorKeyComponent"));
	LKeyCarry->AttemptRegister();
	this->DisableComponentsSimulatePhysics();
	Attack = FGunKey();
	UMeshComponent* Mesh = GetComponentByClass<UMeshComponent>();
	if (Mesh != nullptr)
	{
		Mesh ->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	}
}

const FNavAgentProperties& AThistleInject::GetNavAgentPropertiesRef() const
{
	return NavAgentProps;
}

// Called when the game starts or when spawned
void AThistleInject::BeginPlay()
{
	Super::BeginPlay();
	if(!IsDefaultSubobject())
	{
		AttemptRegister();
		BarragePhysicsAgent->Register();
		// No Chaos for you
		this->DisableComponentsSimulatePhysics();
		if(EnemyType == Flyer)
		{
			FBarragePrimitive::SetGravityFactor(0, BarragePhysicsAgent->MyBarrageBody);
		}
		
		ArtilleryStateMachine->MyDispatch->REGISTER_ENTITY_FINAL_TICK_RESOLVER(GetMyKey());
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
		Mesh->SetupAttachment(BarragePhysicsAgent);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}
	NavAgentProps.AgentRadius = BarragePhysicsAgent->DiameterXYZ.Size2D()/2;
	NavAgentProps.AgentHeight = BarragePhysicsAgent->DiameterXYZ.Z;
}

void AThistleInject::FireAttack()
{
	if(Attack != DefaultGunKey && Attack.GunInstanceID != 0)
	{
		UArtilleryLibrary::RequestGunFire(Attack);
	}
	else
	{
		bool wedoneyet = false;
		FGunInstanceKey AInstance = FGunInstanceKey(UArtilleryLibrary::K2_GetIdentity(MyKey, FARelatedBy::EquippedMainGun,  wedoneyet));
		if(wedoneyet && AInstance.Obj != 0)
		{
			Attack = FGunKey(GunDefinitionID, AInstance);
			ArtilleryStateMachine->PushGunToFireMapping(Attack);
			FireAttack();
			return;
		}
		FGunKey InstanceThis = FGunKey(GunDefinitionID);
		UArtilleryLibrary::RequestUnboundGun(FARelatedBy::EquippedMainGun, MyKey, InstanceThis);
	}
}

bool AThistleInject::RotateMainGun(FRotator RotateTowards, ERelativeTransformSpace OperatingSpace)
{
	if(MyMainGun)
	{
		bool find = false;
		Attr3Ptr aim = UArtilleryLibrary::implK2_GetAttr3Ptr(GetMyKey(), Attr3::AimVector, find);
		if (find)
		{
			aim->SetCurrentValue(RotateTowards.Vector());
		}
	}
	//TODO move the aim feathering from ThistleAim to this.
	//TODO Add tags
	return false;
}

void AThistleInject::AddForce(FVector3f Force)
{
	FVector3d FinalForce(Force.X, Force.Y, disableZAxis ? 0 : Force.Z);
	FBarragePrimitive::ApplyForce(FinalForce, BarragePhysicsAgent->MyBarrageBody, AIMovement);
}

bool AThistleInject::MoveToPoint(FVector3f To)
{
	FinalDestination = To;
	bool AreWeBarraging = false;
	UArtilleryLibrary::implK2_GetLocation(GetMyKey(), AreWeBarraging);
	// Immediately stuff this into the AILocomotionBuffer because I need something that just *works*
	// arms, body, legs, flesh, skin, bones, sinew
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if(NavSys)
	{
		ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavData && this && !this->IsActorBeingDestroyed() && !To.ContainsNaN())
		{
			auto Start = FVector3d(FBarragePrimitive::GetPosition(BarragePhysicsAgent->MyBarrageBody));
			auto Finish = FVector3d(To);
			if (!Start.ContainsNaN() && !Finish.ContainsNaN())
			{
				FPathFindingQuery Query(this, *NavData, Start, Finish);
			
				FPathFindingResult Result = NavSys->FindPathSync(Query);
				if (Result.IsSuccessful())
				{
					// I KNOW THIS LOOKS DUMB BUT ONE IS POINTER CHECK AND OTHER IS PATH VALIDITY CHECK (lol.)
					if (Path.IsValid() && Path->IsValid())
					{
						Path = Result.Path;
						Path->EnableRecalculationOnInvalidation(false);
						Path->SetIgnoreInvalidation(true);
					}
					else // TODO: flying nav doesn't work with navmesh so just set a dumb path for now
					{
						TArray<FVector> PathPoints;
						PathPoints.Add(Query.StartLocation);
						PathPoints.Add(FVector3d(To));
						Path = MakeShareable<FNavigationPath>(new FNavigationPath(PathPoints, this));
						Path->SetNavigationDataUsed(NavData);
					}

					// First path point is start location, so we skip it since we're already there
					NextPathIndex = 1;
					// Path->DebugDraw(NavData, FColor::Red, nullptr, /*bPersistent=*/false, 5.f);
					return true;
				}
			}
		}
	}
		
	return false;
}

void AThistleInject::LocomotionStateMachine()
{
	// I KNOW THIS LOOKS DUMB BUT ONE IS POINTER CHECK AND OTHER IS PATH VALIDITY CHECK (lol.)
	FBLet selfpin = BarragePhysicsAgent->MyBarrageBody;
	if(this && this->MyKey != 0 && Path.IsValid() && selfpin)
	{
		UBarrageDispatch* Physics = UBarrageDispatch::SelfPtr;
		// Perform a downwards cast towards ground to project the target location to the ground
		const JPH::DefaultBroadPhaseLayerFilter LevelGeoBroadPhaseFilter = Physics->GetDefaultBroadPhaseLayerFilter(Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY);
		const JPH::DefaultObjectLayerFilter LevelGeoObjectLayerFilter = Physics->GetDefaultLayerFilter(Layers::CAST_QUERY_LEVEL_GEOMETRY_ONLY);
		const JPH::IgnoreSingleBodyFilter BodyFilter = Physics->GetFilterToIgnoreSingleBody(BarragePhysicsAgent->MyBarrageBody);
		
		TSharedPtr<FHitResult> HitResult = MakeShared<FHitResult>();
		Physics->SphereCast(0.05f, 200.0f, static_cast<FVector3d>(BarragePhysicsAgent->MyBarrageBody->GetCentroidPossiblyStale(BarragePhysicsAgent->MyBarrageBody)), FVector::DownVector, HitResult, LevelGeoBroadPhaseFilter, LevelGeoObjectLayerFilter, BodyFilter);
		bool groundful = HitResult && HitResult->Distance < 30;
		if (EnemyType == Ground && groundful && !ArtilleryStateMachine->MyTags->HasTag(TAG_Orders_Move_Break) && Path && Path->IsReady() && (Path->GetPathPoints()).Num() >= NextPathIndex)
		{
			FVector3f Destination = FVector3f(Path->GetDestinationLocation());
			FVector3f NextWaypoint = FVector3f(Path->GetPathPoints()[NextPathIndex].Location);
		
			FVector3f CurrentPos = FVector3f(GetActorLocation());
			LastTickPosition = CurrentPos;

			FVector3f CurrentVelocity = FBarragePrimitive::GetVelocity(BarragePhysicsAgent->MyBarrageBody);
			double EasingDistance = StoppingTime * CurrentVelocity.Length();
			
			if (FVector3f(CurrentPos.X, CurrentPos.Y, EnemyType == Ground ? 0 : CurrentPos.Z).Equals(FVector3f(Destination.X, Destination.Y, EnemyType == Ground ? 0 : Destination.Z), 10.0))
			{
				// Hard stop
				if (!CurrentVelocity.IsNearlyZero()) {
					// Put the Z back
					FBarragePrimitive::SetVelocity(FBarragePrimitive::UpConvertFloatVector(CurrentVelocity * 0.5f), BarragePhysicsAgent->MyBarrageBody);
				}
				Idle = true;
			
				//TODO Add tags
				return;
			}

			//UE_LOG(LogTemp, Warning, TEXT("Current: %f %f Waypoint: %f %f"), CurrentPos.X, CurrentPos.Y, NextWaypoint.X, NextWaypoint.Y);
			if (FVector3f(CurrentPos.X, CurrentPos.Y, EnemyType == Ground ? 0 : CurrentPos.Z).Equals(FVector3f(NextWaypoint.X, NextWaypoint.Y, EnemyType == Ground ? 0 : NextWaypoint.Z), 10.0))
			{
				NextPathIndex++;
				NextWaypoint = FVector3f(Path->GetPathPoints()[NextPathIndex].Location);
			}

			//TODO Add tags
			Idle = false;
		
			FVector3f DirectionOfMovement = NextWaypoint - CurrentPos;
			// 2D projected direction of movement (parallel to ground)
			// Accelerate towards next waypoint if still at least 0.5 * StoppingTime (s) away from final destination or next waypoint is not final destination
			if ((CurrentPos - Destination).Length() > EasingDistance)
			{
				FVector3f NewVelocityAfterAcceleration = (CurrentVelocity + UBarrageDispatch::TickRateInDelta * Acceleration * DirectionOfMovement).GetClampedToMaxSize(MaxWalkSpeed);

				// Rotate towards destination
				FBarragePrimitive::ApplyRotation(FBarragePrimitive::UpConvertFloatQuat(NewVelocityAfterAcceleration.ToOrientationQuat()), BarragePhysicsAgent->MyBarrageBody);
				FBarragePrimitive::SetVelocity(FBarragePrimitive::UpConvertFloatVector(NewVelocityAfterAcceleration), BarragePhysicsAgent->MyBarrageBody);
			}
			else // Otherwise, start stopping
			{
				FVector3f NewVelocityAfterDeceleration = (CurrentVelocity * 0.8f).GetClampedToMaxSize(MaxWalkSpeed);

				// Rotate towards destination
				FBarragePrimitive::ApplyRotation(FBarragePrimitive::UpConvertFloatQuat(NewVelocityAfterDeceleration.ToOrientationQuat()), BarragePhysicsAgent->MyBarrageBody);
				FBarragePrimitive::SetVelocity(FBarragePrimitive::UpConvertFloatVector(NewVelocityAfterDeceleration), BarragePhysicsAgent->MyBarrageBody);
			}
		}
		else if (EnemyType == Ground)
		{
			FBarragePrimitive::ApplyForce({0, 0, -20000 / HERTZ_OF_BARRAGE}, BarragePhysicsAgent->MyBarrageBody, PhysicsInputType::OtherForce);
		}
	}
}

// Called every frame
void AThistleInject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	bool find = false;
	Attr3Ptr aim = UArtilleryLibrary::implK2_GetAttr3Ptr(GetMyKey(), Attr3::AimVector, find);
	if (find)
	{
		MyMainGun->MoveComponent(FVector::ZeroVector, aim->CurrentValue.Rotation(), false);
	}
	
	if(BarragePhysicsAgent->IsReady && !FBarragePrimitive::IsNotNull(BarragePhysicsAgent->MyBarrageBody))
	{
		this->OnDeath();
	}
}

float AThistleInject::GetHealth()
{
	return ArtilleryStateMachine->MyDispatch->GetAttrib(GetMyKey(), HEALTH)->GetCurrentValue();
}
