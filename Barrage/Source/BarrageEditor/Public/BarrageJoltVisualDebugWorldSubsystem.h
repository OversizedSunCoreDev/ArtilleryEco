#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "BarrageJoltVisualDebugger.h"
#include "BarrageJoltVisualDebugWorldSubsystem.generated.h"

UCLASS()
class BARRAGEEDITOR_API UBarrageJoltVisualDebugWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void PostInitialize() override;

private:
	TObjectPtr<UBarrageJoltVisualDebugger> DebuggerComponent;
};