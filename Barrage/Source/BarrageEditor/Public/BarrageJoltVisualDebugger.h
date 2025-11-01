#pragma once

#include "CoreMinimal.h"
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawComponent.h"
#include "BarrageJoltVisualDebugger.generated.h"

/**
* Convenience component to visualize Jolt physics in the editor. Rather than
* having to use a render queue and raw draw to RHI, we can use a lot of the
* built-in UE rendering functionality high level functions like Draw... etc.
* Reasons we do not use the FVisualDebugComponent class directly:
* - Requires an Actor to attach to, which is inconvenient for rendering an
*   the entire physics scene regardless of actors.
* - Requires an object to be selected, not appropriate for a global debugger.
**/

UCLASS(editinlinenew, ClassGroup = Debug)
class BARRAGEEDITOR_API UBarrageJoltVisualDebugger : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UBarrageJoltVisualDebugger();

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
#endif
};