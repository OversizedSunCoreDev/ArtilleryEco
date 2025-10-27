#pragma once

#include "CoreMinimal.h"
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawComponent.h"
#include "FBarragePrimitive.h"
#include "BarrageDebugComponent.generated.h"

/*
* Attach to a component that will have a Barrage body to visualize its debug info.
* Set the barrage body ID on this component to have it render debug info for that body.
*/
UCLASS(ClassGroup = Debug, meta = (BlueprintSpawnableComponent))
class BARRAGEEDITOR_API UBarrageDebugComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

protected:
#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
#endif

	FBLet TargetBodyToVisualize;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawOnlyIfSelected;

public:
	UBarrageDebugComponent();

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	void SetTargetBody(FBLet InBodyToVisualize) { TargetBodyToVisualize = InBodyToVisualize; }
};

