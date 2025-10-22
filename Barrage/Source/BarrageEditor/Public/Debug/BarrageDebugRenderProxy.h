#include "CoreMinimal.h"
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawComponent.h"
#include "PrimitiveSceneProxy.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxyDesc.h"
#include "Misc/ScopeLock.h"
#include "DynamicMeshBuilder.h"
#include "SceneView.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "BarrageJoltVisualDebuggerSettings.h"

struct FBarrageDebugRenderProxy : public FDebugRenderSceneProxy
{
	FBarrageDebugRenderProxy(const UPrimitiveComponent* Component, TSharedPtr<JPH::PhysicsSystem> Simulation);
	virtual ~FBarrageDebugRenderProxy();

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual uint32 GetMemoryFootprint(void) const override;
	uint32 GetAllocatedSize(void) const;
	SIZE_T GetTypeHash() const override;

protected:
	// New shapes
	struct DebugChoppedCone
	{
		float TopRadius;
		float BottomRadius;
		float HalfHeight;
		FTransform Transform;

		inline void Draw(FPrimitiveDrawInterface* PDI) const
		{
			const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetCapsuleColliderColor();
			DrawWireChoppedCone(PDI, Transform.GetLocation(), Transform.GetScaledAxis(EAxis::X), Transform.GetScaledAxis(EAxis::Y), Transform.GetScaledAxis(EAxis::Z), Color, HalfHeight, TopRadius, BottomRadius, 8, SDPG_World);
		}
	};

	struct DebugAAB
	{
		FVector Extents;
		FTransform Transform;
		inline void Draw(FPrimitiveDrawInterface* PDI) const
		{
			const auto Color = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderColor();
			const auto Thickness = UBarrageJoltVisualDebuggerSettings::Get().GetBoxColliderLineThickness();
			DrawOrientedWireBox(PDI, Transform.GetLocation(), Transform.GetScaledAxis(EAxis::X), Transform.GetScaledAxis(EAxis::Y), Transform.GetScaledAxis(EAxis::Z), Extents, Color, SDPG_World, Thickness);
		}
	};

	TArray<DebugChoppedCone> ChoppedCones;
	TArray<DebugAAB> AABs;

	void AddInvalidShapePointStar(FTransform Transform);
	void GatherBodyShapeCommands(const JPH::BodyID& BodyID);

private:
	TSharedPtr<JPH::PhysicsSystem> PhysicsSystem;

	FCriticalSection LineLocker;
	FCriticalSection DashedLocker;
	FCriticalSection ArrowLocker;
	FCriticalSection CircleLocker;
	FCriticalSection CylinderLocker;
	FCriticalSection StarLocker;
	FCriticalSection BoxLocker;
	FCriticalSection SphereLocker;
	FCriticalSection TextLocker;
	FCriticalSection ConeLocker;
	FCriticalSection CapsulLocker;
	FCriticalSection CoordinateSystemLocker;
	FCriticalSection MeshLocker;
	FCriticalSection ChoppedConeLocker;
	FCriticalSection AABLocker;

	// Helper recursive function to decompose BodyShapes to a core set of "scalar" shapes that we can draw
	void GatherScalarShapes(const FTransform& JoltLocalToWorld, const JPH::Shape* BodyShape);

};