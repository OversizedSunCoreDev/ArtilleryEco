#include "BarrageJoltVisualDebugger.h"
#include "PrimitiveSceneProxy.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveDrawingUtils.h"

FPrimitiveSceneProxy* UBarrageJoltVisualDebugger::CreateSceneProxy()
{
	// This subsystem requires a UBarrageDispatch Subsystem to be present and valid.
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}
	
	if(!GetWorld()->HasSubsystem<UBarrageDispatch>())
	{
		return nullptr;
	}

	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	if (!IsValid(BarrageDispatch))
	{
		return nullptr;
	}

	TSharedPtr<FWorldSimOwner> Simulation = BarrageDispatch->JoltGameSim;
	if (!Simulation.IsValid())
	{
		return nullptr;
	}

	// Get the Jolt implementation
	TSharedPtr<JPH::PhysicsSystem> PhysicsSystem = Simulation->physics_system;
	if (!PhysicsSystem.IsValid())
	{
		return nullptr;
	}

	struct FProxy final : public FPrimitiveSceneProxy
	{
		FProxy(const UBarrageJoltVisualDebugger* Component, TSharedPtr<JPH::PhysicsSystem> Simulation)
			: FPrimitiveSceneProxy(Component)
			, PhysicsSystem(Simulation)
		{
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			// Always relevant
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			return Result;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			// Get a lock to get a list of all the bodies in the JOLT simulation
			JPH::BodyIDVector RigidBodies;
			PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, RigidBodies);

			// I don't like this nested loop, but this is rote
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
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
								// C4541 buster
								const JPH::SphereShape* SphereShape = reinterpret_cast<const JPH::SphereShape*>(BodyShape);
								if (SphereShape != nullptr)
								{
									float Radius = CoordinateUtils::JoltToRadius(SphereShape->GetRadius());
									DrawWireSphere(PDI, BodyPosition, FLinearColor::Blue, Radius, 2, SDPG_World, 1.f);
								}
							}
							case JPH::EShapeSubType::Box:
							{
								// C4541 buster
								const JPH::BoxShape* BoxShape = reinterpret_cast<const JPH::BoxShape*>(BodyShape);
								if (BoxShape != nullptr)
								{
									// Jolt boxes are half extents
									JPH::Vec3 HalfExtent = BoxShape->GetHalfExtent();
									FVector Extents = FBarragePrimitive::UpConvertFloatVector(CoordinateUtils::FromJoltCoordinates(HalfExtent)) * 2.f;
									DrawOrientedWireBox(PDI, BodyPosition, BodyRotation.GetRightVector(), BodyRotation.GetForwardVector(), BodyRotation.GetUpVector(), Extents, FLinearColor::Red, SDPG_World, 1.f);
								}
								break;
							}
							default:
								// Unsupported shape type
								break;
							}
						}
					}
				}
			}
		}

		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

	private:
		TSharedPtr<JPH::PhysicsSystem> PhysicsSystem;
	};

	return new FProxy(this, PhysicsSystem);
}
