#include "ThistleInject.h"
#include "ArtilleryBPLibs.h"
#include "ArtilleryDispatch.h"
#include "UFireControlMachine.h"
#include "ThistleBehavioralist.h"
#include "ThistleDispatch.h"
#include "UEventLogSystem.h"
#include "Kismet/KismetMathLibrary.h"
#include "Public/GameplayTags.h"


inline bool AThistleInject::RegistrationImplementation()
{
	Super::RegistrationImplementation();
	if (ArtilleryStateMachine->MyDispatch)
	{
		LKeyCarry->AttemptRegister();

		TMap<AttribKey, double> MyAttributes = TMap<AttribKey, double>();
		MyAttributes.Add(HEALTH, MaxHP);
		MyAttributes.Add(MAXHEALTH, MaxHP);
		MyAttributes.Add(Attr::Shields, MaxShields);
		MyAttributes.Add(Attr::MaxShields, MaxShields);
		MyAttributes.Add(Attr::HealthRechargePerTick, 0);
		MyAttributes.Add(MANA, 1000);
		MyAttributes.Add(MAXMANA, 1000);
		MyAttributes.Add(Attr::ManaRechargePerTick, 10);
		MyAttributes.Add(Attr::ProposedDamage, 5);
		MyKey = ArtilleryStateMachine->CompleteRegistrationWithAILocomotionAndParent(
			MyAttributes, LKeyCarry->GetMyKey());

		//Vectors
		Attr3MapPtr VectorAttributes = MakeShareable(new Attr3Map());
		VectorAttributes->Add(Attr3::AimVector, MakeShareable(new FConservedVector()));
		VectorAttributes->Add(Attr3::FacingVector, MakeShareable(new FConservedVector()));
		ArtilleryStateMachine->MyDispatch->RegisterOrAddVecAttribs(LKeyCarry->GetMyKey(), VectorAttributes);

		IdMapPtr MyRelationships = MakeShareable(new IdentityMap());
		MyRelationships->Add(Ident::Target, MakeShareable(new FConservedAttributeKey()));
		MyRelationships->Add(Ident::EquippedMainGun, MakeShareable(new FConservedAttributeKey()));
		MyRelationships->Add(Ident::Squad, MakeShareable(new FConservedAttributeKey()));
		ArtilleryStateMachine->MyDispatch->RegisterOrAddRelationships(LKeyCarry->GetMyKey(), MyRelationships);

		if (EnableTakeMoveOrder)
		{
			ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Move_Needed);
			ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Move_Possible);
			//turrets and similar should remove this or get bossed around incorrectly.
		}

		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Attack_Available);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Target_Needed);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Orders_Rally_PreferSquad);
		ArtilleryStateMachine->MyTags->AddTag(TAG_Enemy);
		ArtilleryStateMachine->MyTags->AddTag(FGameplayTag::RequestGameplayTag("Enemy"));

		MainGunKine = ManagedRequestingKine(MyMainGun);
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
AThistleInject::AThistleInject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), MainGunKine()
{
	NavAgentProps.NavWalkingSearchHeightScale = FNavigationSystem::GetDefaultSupportedAgent().
		NavWalkingSearchHeightScale;
	Attack = FGunKey();

	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AThistleInject::BeginPlay()
{
	Super::BeginPlay();
	if (EnemyType == Flyer)
	{
		FBarragePrimitive::SetGravityFactor(0, BarragePhysicsAgent->MyBarrageBody);
	}	
	NavAgentProps.AgentRadius = BarragePhysicsAgent->DiameterXYZ.Size2D() / 2;
	NavAgentProps.AgentHeight = BarragePhysicsAgent->DiameterXYZ.Z;
}


void AThistleInject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimRotateMeshComponent();


	if (BarragePhysicsAgent->IsReady && !FBarragePrimitive::IsNotNull(BarragePhysicsAgent->MyBarrageBody))
	{
		this->OnDeath();
	}

}

void AThistleInject::FireAttack()
{
	if (Attack != DefaultGunKey && Attack.GunInstanceID != 0)
	{
		UArtilleryLibrary::RequestGunFire(Attack);
	}
	else
	{
		bool wedoneyet = false;
		FGunInstanceKey AInstance = FGunInstanceKey(
			UArtilleryLibrary::K2_GetIdentity(MyKey, FARelatedBy::EquippedMainGun, wedoneyet));
		if (wedoneyet && AInstance.Obj != 0)
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
	if (MyMainGun)
	{
		bool find = false;
		Attr3Ptr aim = UArtilleryLibrary::implK2_GetAttr3Ptr(GetMyKey(), Attr3::AimVector, find);
		if (find)
		{
			aim->SetCurrentValue(RotateTowards.Vector());
		}
	}
	//TODO move the aim feathering from ThistleAim to this.
	//TODO Add tagss
	return false;
}


bool AThistleInject::EngageNavSystem(FVector3f To)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavData && this && !this->IsActorBeingDestroyed() && !To.ContainsNaN())
		{
			auto Start = FVector3d(FBarragePrimitive::GetPosition(BarragePhysicsAgent->MyBarrageBody));
			auto Finish = FVector3d(To);
			if (!Start.ContainsNaN() && !Finish.ContainsNaN())
			{
				FPathFindingQuery Query(this, *NavData, Start, Finish);
				Query.SetAllowPartialPaths(true);
				Query.SetRequireNavigableEndLocation(true);
				try
				{
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
				catch (...)
				{
					return false;
				}
			}
		}
	}
	return false;
}

bool AThistleInject::MoveToPoint(FVector3f To)
{
	FinalDestination = FVector(To);

	if (EngageNavSystem(To))
	{
		MoveState = EThistleMoveState::Moving;
		SoftFollowMode = false; // This flag is no longer used but we reset it for safety
		return true;
	}

	// If pathfinding fails, go to idle
	MoveState = EThistleMoveState::Idle;
	return false;
}

FVector AThistleInject::Seek(const FVector& Target)
{
	FVector DesiredVelocity = (Target - GetActorLocation()).GetSafeNormal() * MaxWalkSpeed;
	FVector SteeringForce = DesiredVelocity - CurrentVelocity;
	return SteeringForce.GetClampedToMaxSize(MaxForce);
}

void AThistleInject::HandleIdleState()
{
	// Apply braking force if moving
	if (!CurrentVelocity.IsNearlyZero())
	{
		FVector BrakingForce = -CurrentVelocity * 0.8f; // Strong braking
		FBarragePrimitive::ApplyForce(BrakingForce, BarragePhysicsAgent->MyBarrageBody, AIMovement);
	}
}

void AThistleInject::HandleMovingState()
{
	// If the path becomes invalid (blocked by a new obstacle) or we have no path, stop.
	if (!Path.IsValid() || !Path->IsValid())
	{
		MoveState = EThistleMoveState::Idle;
		return;
	}

	if (NextPathIndex >= Path->GetPathPoints().Num())
	{
		MoveState = EThistleMoveState::SlowingDown;
		return;
	}
	const float ArtilleryDeltaTime = 1.0f / Arty::ArtilleryTickHertz;

	CheckStuck(ArtilleryDeltaTime);

	// Path Following
	FVector CurrentLocation = GetActorLocation();
	FVector PathStart = Path->GetPathPoints()[NextPathIndex - 1].Location;
	FVector PathEnd = Path->GetPathPoints()[NextPathIndex].Location;

	// Find the point on the current path segment closest to us
	FVector PointOnPath = FMath::ClosestPointOnSegment(CurrentLocation, PathStart, PathEnd);

	// Look ahead from that point along the path direction
	FVector Target = PointOnPath + (PathEnd - PathStart).GetSafeNormal() * PathLookaheadDistance;

	// Check if we have passed the current waypoint (PathEnd).
	// check if the vector from us to the waypoint is pointing opposite to the path segment's direction.
	FVector ToEndpoint = PathEnd - CurrentLocation;
	FVector SegmentDirection = (PathEnd - PathStart).GetSafeNormal();
	if (FVector::DotProduct(ToEndpoint, SegmentDirection) < 0.f || ToEndpoint.IsNearlyZero(10.f))
	{
		NextPathIndex++;
		if (NextPathIndex >= Path->GetPathPoints().Num())
		{
			MoveState = EThistleMoveState::SlowingDown;
			return;
		}
	}

	FVector SteeringForce = CalculateSteeringForce(Target, false);
		
	//float NavHeight = CompareNavMeshHeight();
	/*FVector PhysVelocity = FVector(FBarragePrimitive::GetVelocity(BarragePhysicsAgent->MyBarrageBody));
	UE_LOG(LogTemp, Warning, TEXT("%s : NavHeight: %f | Steer Height %f. | Velocity %s"), *GetName(), NavHeight, SteeringForce.Z, *PhysVelocity.ToString());*/


	// Rotation before Applying Forces
	FRotator CurrentRot = FQuat(FBarragePrimitive::OptimisticGetAbsoluteRotation(BarragePhysicsAgent->MyBarrageBody)).Rotator();

	FVector PhysPosition = FVector(FBarragePrimitive::GetPosition(BarragePhysicsAgent->MyBarrageBody));	
	FVector SteerCombinedPosition = SteeringForce + PhysPosition;

	//Unstuck bypasses terrain difficulties by setting the position directly without forces for a short duration.
	if (UnStuckTimer > 0.f)
	{
		UnStuckTimer -= ArtilleryDeltaTime;
		FVector SteerPosition = UKismetMathLibrary::VInterpTo(PhysPosition, SteerCombinedPosition, ArtilleryDeltaTime, SteerInterp);
		FBarragePrimitive::SetPosition(SteerPosition, BarragePhysicsAgent->MyBarrageBody);
	}
	else
	{
		FBarragePrimitive::ApplyForce(SteeringForce, BarragePhysicsAgent->MyBarrageBody, AIMovement);
	}


	//Lets just directly set the position instead of applying force in this case.
	/*if (bStuck)
	{		
		FVector StepUp = FVector(PhysPosition.X, PhysPosition.Y, PhysPosition.Z + MaxStepHeight);
		FBarragePrimitive::SetPosition(StepUp, BarragePhysicsAgent->MyBarrageBody);
	}*/
	//else
	//{
	//}



	//Look at rotation from our cosmetic Actor and the added steer (uncorrected z) location.
	FRotator LookRot = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, SteerCombinedPosition);
	LookRot = FRotator(0.f, LookRot.Yaw, 0.f);
	LookRot = UKismetMathLibrary::RInterpTo(GetActorRotation(), LookRot, ArtilleryDeltaTime, RotationSpeed);
	FBarragePrimitive::ApplyRotation(LookRot.Quaternion(), BarragePhysicsAgent->MyBarrageBody);
}

bool AThistleInject::CheckStuck(float DeltaSeconds)
{
	
	FVector PhysVelocity = FVector(FBarragePrimitive::GetVelocity(BarragePhysicsAgent->MyBarrageBody));
	// Check if we are trying to move but our horizontal speed is very low, indicating we might be stuck.
	if (PhysVelocity.Size2D() < StuckVelocityThreshold)
	{
		StuckTimer += DeltaSeconds;
		if (StuckTimer >= StuckTimeThreshold)
		{
			StuckTimer = 0.f;
			UnStuckTimer = 0.1;
			return true;
		}
	}
	else
	{
		// not stuck.
		StuckTimer = 0.f;
		MinStuckVelocity = 0.f;
	}
	return false;
}

float AThistleInject::CompareNavMeshHeight()
{
	// NavMesh height check
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation ProjectedLocation;

		//FVector ForwardOffset = GetActorLocation() + GetActorForwardVector() * NavmeshForwardProject;
		FVector CurrentLocation = FVector(FBarragePrimitive::GetPosition(BarragePhysicsAgent->MyBarrageBody));


		// Project the AIs current XY position onto the navmesh to find the correct Z height
		if (NavSys->ProjectPointToNavigation(CurrentLocation, ProjectedLocation, NavmeshProjectionSearchExtent))
		{	
			return ProjectedLocation.Location.Z;
		}
	}
	return -1.f;
}

void AThistleInject::HandleSlowingDownState()
{
	if (FVector::DistSquared(GetActorLocation(), FinalDestination) < FMath::Square(10.0f) || CurrentVelocity.Length() < 10.0f)
	{
		MoveState = EThistleMoveState::Idle;
		OnArrivalAtDestination.Broadcast();
		Path.Reset();
		return;
	}
	FVector SteeringForce = CalculateSteeringForce(FinalDestination, true);
	FBarragePrimitive::ApplyForce(SteeringForce, BarragePhysicsAgent->MyBarrageBody, AIMovement);
}

void AThistleInject::LocomotionStateMachine()
{
	bool bLocalDebug = false;
	if (!BarragePhysicsAgent || !BarragePhysicsAgent->MyBarrageBody)
	{
		return;
	}

	//const float ArtilleryDeltaTime = 1.0f / Arty::ArtilleryTickHertz;

	/*if (StepUpCooldownTimer > 0.0f)
	{		
		StepUpCooldownTimer -= ArtilleryDeltaTime;
	}*/

	if (MoveState != EThistleMoveState::Moving) 
	{
		StuckTimer = 0.0f;
	}

	switch (MoveState)
	{
	case EThistleMoveState::Idle:
		if (bLocalDebug) UE_LOG(LogTemp, Warning, TEXT("%s : Idle."), *GetName());
		HandleIdleState();
		break;
	case EThistleMoveState::Moving:
		if (bLocalDebug) UE_LOG(LogTemp, Warning, TEXT("%s : Moving."), *GetName());
		HandleMovingState();
		break;
	case EThistleMoveState::SlowingDown:
		if (bLocalDebug) UE_LOG(LogTemp, Warning, TEXT("%s : Slow."), *GetName());
		HandleSlowingDownState();
		break;
	}

	//if (CurrentVelocity.SizeSquared() > FMath::Square(MaxWalkSpeed))
	//{
	//	FVector ClampedVelocity = CurrentVelocity.GetSafeNormal() * MaxWalkSpeed;
	//	FBarragePrimitive::SetVelocity(ClampedVelocity, BarragePhysicsAgent->MyBarrageBody);
	//	CurrentVelocity = ClampedVelocity; // Update our local copy
	//}
}

void AThistleInject::OnPhysicsCollision(const BarrageContactEvent ContactEvent)
{
	//Disabled
}

FVector AThistleInject::CalculateSteeringForce(const FVector& Target, bool bUseArrival)
{
	FVector ToTarget = Target - GetActorLocation();
	double Distance = ToTarget.Length();

	double RampedSpeed = MaxWalkSpeed;
	if (bUseArrival && Distance < ArrivalRadius)
	{
		// Scale speed based on distance as we enter the arrival radius
		RampedSpeed = MaxWalkSpeed * (Distance / ArrivalRadius);
	}

	FVector DesiredVelocity = ToTarget.GetSafeNormal() * RampedSpeed;
	FVector SteeringForce = DesiredVelocity - FVector(FBarragePrimitive::GetVelocity(BarragePhysicsAgent->MyBarrageBody));	

	return SteeringForce.GetClampedToMaxSize(MaxForce);
}

//void AThistleInject::OnPathfindingComplete(uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr FoundPath)
//{
//	if (Result == ENavigationQueryResult::Success && FoundPath.IsValid() && FoundPath->GetPathPoints().Num() > 1)
//	{
//		Path = FoundPath;
//		NextPathIndex = 1; // Start at the first waypoint (index 0 is our start location)
//		Path->EnableRecalculationOnInvalidation(true);
//	}
//	else
//	{
//		// Path generation failed or resulted in an empty path. Go to idle.
//		UE_LOG(LogTemp, Warning, TEXT("AThistleInject::OnPathfindingComplete: Pathfinding failed for %s."), *GetName());
//		Path.Reset();
//		MoveState = EThistleMoveState::Idle;
//	}
//}

void AThistleInject::UpdatePathAfterDisplacement()
{
	if (!Path.IsValid() || Path->GetPathPoints().Num() <= 1)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	float MinDistanceSq = -1.0f;
	int32 BestPathIndex = -1;

	// Iterate through the remaining path segments to find the one we are closest to, start from the segment we were previously on.
	for (int32 i = FMath::Max(1, NextPathIndex); i < Path->GetPathPoints().Num(); ++i)
	{
		const FVector PathStart = Path->GetPathPoints()[i - 1].Location;
		const FVector PathEnd = Path->GetPathPoints()[i].Location;

		const FVector ClosestPointOnSegment = FMath::ClosestPointOnSegment(CurrentLocation, PathStart, PathEnd);
		const float DistanceSq = FVector::DistSquared(CurrentLocation, ClosestPointOnSegment);

		if (BestPathIndex == -1 || DistanceSq < MinDistanceSq)
		{
			MinDistanceSq = DistanceSq;
			BestPathIndex = i;
		}
	}

	if (BestPathIndex != -1)
	{
		// Found a new best segment. Set our next target waypoint to the end of that segment.
		NextPathIndex = BestPathIndex;
	}
}

void AThistleInject::AimRotateMeshComponent()
{
	bool find = false;
	const Attr3Ptr aim = UArtilleryLibrary::implK2_GetAttr3Ptr(GetMyKey(), Attr3::AimVector, find);
	if (find && MyMainGun != nullptr)
	{
		const FVector TargetWorldDirection = aim->CurrentValue;

		// Get the parent component transform in the coordinate system the relative rotation will be based on.
		const USceneComponent* ParentComp = MyMainGun->GetAttachParent();
		if (ParentComp)
		{
			// Transform the world target direction into the parent local space.
			const FTransform ParentWorldTransform = ParentComp->GetComponentTransform();
			const FVector TargetLocalDirection = ParentWorldTransform.InverseTransformVectorNoScale(TargetWorldDirection);
			FVector MeshLocalForwardVector = FVector::XAxisVector;
			FQuat RelativeQuat;

			// Large enemies have inconsistently setup art assets - this attempts to correct it.
			if (bRotationCorrection)
			{
				MeshLocalForwardVector = FVector::YAxisVector;
				// Align mesh forward vector with target direction in local space as a rotation.
				RelativeQuat = FQuat::FindBetweenVectors(MeshLocalForwardVector, TargetLocalDirection.GetSafeNormal());
				FRotator AimRot = RelativeQuat.Rotator();
				AimRot = FRotator(0, AimRot.Yaw, 0);
				MyMainGun->SetRelativeRotation(AimRot);
			}
			else
			{
				// Align mesh forward vector with target direction in local space as a rotation.				
				RelativeQuat = FQuat::FindBetweenVectors(MeshLocalForwardVector, TargetLocalDirection.GetSafeNormal());

				FRotator AimRot = RelativeQuat.Rotator();
				AimRot = FRotator(0, AimRot.Yaw, 0);
				MyMainGun->SetRelativeRotation(AimRot);
			}
		}
	}
}