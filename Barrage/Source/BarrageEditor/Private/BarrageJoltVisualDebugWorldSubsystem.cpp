#include "BarrageJoltVisualDebugWorldSubsystem.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxyDesc.h"
#include "HAL/Platform.h"
THIRD_PARTY_INCLUDES_START

PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING

// jolt internals
#include "Jolt/Core/Memory.h"
#include <Memory/IntraTickThreadblindAlloc.h>

// include all the shapes
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/EmptyShape.h"
#include "Jolt/Physics/Collision/Shape/CompoundShape.h"
JPH_SUPPRESS_WARNINGS

PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY(VLogBarrage);

bool UBarrageJoltVisualDebugWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	// Only enable this subsystem in editor worlds
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game || WorldType == EWorldType::GamePreview;
}

void UBarrageJoltVisualDebugWorldSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UE_CALL_ONCE(JPH::RegisterDefaultAllocator);

	// Create a dummy actor to attach the component to
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = TEXT("BarrageJoltVisualDebuggerActor");
	AActor* DummyActor = GetWorld()->SpawnActor<AActor>(SpawnParams);
	if (IsValid(DummyActor))
	{
		DummyActor->SetFlags(RF_Transient); // We don't want to save this actor
		DebuggerComponent = NewObject<UBarrageJoltVisualDebugger>(DummyActor, TEXT("DebuggerVisualization"), RF_Transient);
		if (IsValid(DebuggerComponent))
		{
			DebuggerComponent->SetVisibility(true);
			DebuggerComponent->RegisterComponent();
		}
	}
}

void UBarrageJoltVisualDebugWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	return;
	// This subsystem requires a UBarrageDispatch Subsystem to be present and valid.
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (!GetWorld()->HasSubsystem<UBarrageDispatch>())
	{
		return;
	}

	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	if (!IsValid(BarrageDispatch) || !BarrageDispatch->IsInitialized())
	{
		return;
	}

	TSharedPtr<FWorldSimOwner> Simulation = BarrageDispatch->JoltGameSim;

	if (!Simulation.IsValid())
	{
		return;
	}

	// Get the Jolt implementation
	TSharedPtr<JPH::PhysicsSystem> PhysicsSystem = Simulation->physics_system;
	if (PhysicsSystem.IsValid())
	{
		// Get a lock to get a list of all the bodies in the JOLT simulation
		JPH::BodyIDVector RigidBodies;
		PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, RigidBodies);
		for (const JPH::BodyID& BodyID : RigidBodies)
		{
			JPH::BodyLockRead BodyReadLock(PhysicsSystem->GetBodyLockInterface(), BodyID);
			if (BodyReadLock.Succeeded())
			{
				const JPH::Body& Body = BodyReadLock.GetBody();
				FVector BodyPosition = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(Body.GetPosition()));
				FQuat BodyRotation = FBarragePrimitive::UpConvertFloatQuat(CoordinateUtils::FromJoltRotation(Body.GetRotation()));

				const FMatrix& LocalToWorld = FMatrix::Identity; // assumption is that the body position is in world space already

				const JPH::Shape* BodyShape = Body.GetShape();
				if (BodyShape == nullptr)
				{
					continue;
				}

				// Since we are not hooking into the debug renderer, we cannot use the 
				// polymorphic interface of JPH::Shape. We need to cast to the specific shape type
				switch (BodyShape->GetSubType())
				{
				case JPH::EShapeSubType::Sphere:
				{
					const JPH::SphereShape* SphereShape = reinterpret_cast<const JPH::SphereShape*>(BodyShape);
					if (SphereShape != nullptr)
					{
						float Radius = CoordinateUtils::JoltToRadius(SphereShape->GetRadius());
						UE_VLOG_SPHERE(this, VLogBarrage, Log, BodyPosition, Radius, FColor::Green, TEXT("Jolt Sphere Body"));
					}
				}
				case JPH::EShapeSubType::Box:
				{
					const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
					if (BoxShape != nullptr)
					{
						// Jolt boxes are half extents
						JPH::Vec3 HalfExtent = BoxShape->GetHalfExtent();
						FVector Extents = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(HalfExtent)) * 2.f;
						UE_VLOG_BOX(this, VLogBarrage, Log, FBox::BuildAABB(BodyPosition, Extents), FColor::Green, TEXT("Jolt Box Body"));
						//DrawOrientedWireBox(PDI, BodyPosition, BodyRotation.GetRightVector(), BodyRotation.GetForwardVector(), BodyRotation.GetUpVector(), Extents, FLinearColor::Green, SDPG_World, 1.f);
					}
					break;
				}
				case JPH::EShapeSubType::Capsule:
				{
					const JPH::CapsuleShape* CapsuleShape = reinterpret_cast<const JPH::CapsuleShape*>(BodyShape);
					if (CapsuleShape != nullptr)
					{
						float Radius = CoordinateUtils::JoltToRadius(CapsuleShape->GetRadius());
						float HalfHeight = CapsuleShape->GetHalfHeightOfCylinder(); //TODO: What is the conversion to UE?
						//DrawWireCapsule(PDI, BodyPosition, BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), FLinearColor::Green, HalfHeight, Radius, 2, SDPG_World, 1.f);
					}
					break;
				}
				case JPH::EShapeSubType::Cylinder:
				{
					const JPH::CylinderShape* CylinderShape = reinterpret_cast<const JPH::CylinderShape*>(BodyShape);
					if (CylinderShape != nullptr)
					{
						float Radius = CoordinateUtils::JoltToRadius(CylinderShape->GetRadius());
						float HalfHeight = CylinderShape->GetHalfHeight(); //TODO: What is the conversion to UE?
						//DrawWireCapsule(PDI, BodyPosition, BodyRotation.GetForwardVector(), BodyRotation.GetRightVector(), BodyRotation.GetUpVector(), FLinearColor::Green, HalfHeight, Radius, 2, SDPG_World, 1.f);
					}
					break;
				}
				case JPH::EShapeSubType::Empty:
				default:
					// Unsupported shape type
					break;
				}
			}
		}
	}
}