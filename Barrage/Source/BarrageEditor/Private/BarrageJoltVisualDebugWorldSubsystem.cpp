#include "BarrageJoltVisualDebugWorldSubsystem.h"

bool UBarrageJoltVisualDebugWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	// Only enable this subsystem in editor worlds
	return WorldType == EWorldType::Editor || WorldType == EWorldType::PIE || WorldType == EWorldType::Game || WorldType == EWorldType::GamePreview;
}

void UBarrageJoltVisualDebugWorldSubsystem::PostInitialize()
{
	Super::PostInitialize();

	// Create a dummy actor to attach the component to
	AActor* DummyActor = GetWorld()->SpawnActor<AActor>();
	DebuggerComponent = NewObject<UBarrageJoltVisualDebugger>(DummyActor);
	if (DebuggerComponent)
	{
		DummyActor->AddInstanceComponent(DebuggerComponent);
		DebuggerComponent->RegisterComponent();
		DebuggerComponent->AttachToComponent(DummyActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DebuggerComponent->SetVisibility(true);
#if WITH_EDITORONLY_DATA
		DebuggerComponent->SetIsVisualizationComponent(true);
#endif
	}
}