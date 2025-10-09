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
