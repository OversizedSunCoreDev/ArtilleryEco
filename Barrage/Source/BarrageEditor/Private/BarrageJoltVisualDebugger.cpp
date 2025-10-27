#include "BarrageJoltVisualDebugger.h"
#include "PrimitiveSceneProxy.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveSceneProxyDesc.h"
#include "Misc/ScopeLock.h"
#include "DynamicMeshBuilder.h"
#include "SceneView.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "BarrageJoltVisualDebuggerSettings.h"
#include "Debug/BarrageDebugRenderProxy.h"

static TAutoConsoleVariable<bool> CVarDebugRenderBarrage(
	TEXT("r.DebugRender.Barrage"), false,
	TEXT("Depends on Engine Show Flag for Collision. Use this to view all Barrage bodies regardless of UE side components."),
	ECVF_RenderThreadSafe);


UBarrageJoltVisualDebugger::UBarrageJoltVisualDebugger()
{
	PrimaryComponentTick.bCanEverTick = true; // Always mark render state dirty for simulation updates.
}

void UBarrageJoltVisualDebugger::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!CVarDebugRenderBarrage.GetValueOnGameThread()) return;
	MarkRenderStateDirty(); // force redraw every frame
}


FBoxSphereBounds UBarrageJoltVisualDebugger::CalcBounds(const FTransform& LocalToWorld) const
{
	// This component is global, so just return a huge bounds
	const FVector BoxExtent(HALF_WORLD_MAX);
	return FBoxSphereBounds(FVector::ZeroVector, BoxExtent, BoxExtent.Size());
}

#if UE_ENABLE_DEBUG_DRAWING
// TODO:
// Nice to haves: editor customized colors, ability to toggle specific shapes, ability to toggle active vs inactive bodies, ability to toggle constraints

FDebugRenderSceneProxy* UBarrageJoltVisualDebugger::CreateDebugSceneProxy()
{
	if (!CVarDebugRenderBarrage.GetValueOnGameThread()) return nullptr;
	// This subsystem requires a UBarrageDispatch Subsystem to be present and valid.
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	if (!GetWorld()->HasSubsystem<UBarrageDispatch>())
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

	struct FEveryBarrageDebugRenderProxy : public FBarrageDebugRenderProxy
	{
		FEveryBarrageDebugRenderProxy(const UPrimitiveComponent* Component, TSharedPtr<JPH::PhysicsSystem> Simulation)
			: FBarrageDebugRenderProxy(Component, Simulation)
		{
			JPH::BodyIDVector RigidBodies;
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_GetBodies);
				Simulation->GetBodies(RigidBodies);
			}
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_BarrageJolt_DBG_BuildCommands);
				ParallelFor(RigidBodies.size(), [this, &RigidBodies](int32 BodyIndex)
					{
						GatherBodyShapeCommands(RigidBodies[BodyIndex]);
					});
			}
		}
	};

	return new FEveryBarrageDebugRenderProxy(this, PhysicsSystem);
}
#endif