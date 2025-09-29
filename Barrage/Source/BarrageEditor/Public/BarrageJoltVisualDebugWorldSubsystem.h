#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "BarrageJoltVisualDebugger.h"
#include "VisualLogger/VisualLogger.h"
#include "BarrageJoltVisualDebugWorldSubsystem.generated.h"

BARRAGEEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(VLogBarrage, Display, All);

UCLASS()
class BARRAGEEDITOR_API UBarrageJoltVisualDebugWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void PostInitialize() override;
	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBarrageJoltVisualDebugWorldSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return false; }

private:
	TObjectPtr<UBarrageJoltVisualDebugger> DebuggerComponent;
};