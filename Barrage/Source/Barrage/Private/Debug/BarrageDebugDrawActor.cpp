// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#include "Debug/BarrageDebugDrawActor.h"

#include "EngineUtils.h"
#include "Debug/BarrageDebugDraw.h"
#include "Debug/BarrageDebugTriangleSetComponent.h"
#include "Materials/MaterialRenderProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BarrageDebugDrawActor)


ABarrageDebugDrawActor::ABarrageDebugDrawActor() {
	PrimaryActorTick.bCanEverTick = true;
	
	TriDrawer = CreateDefaultSubobject<U3DTriangleSetComponent>("TriangleSetComponent");
	RootComponent = TriDrawer;
}

FVector3f ToFV3dFV3f(const FVector3d& VectorIn) {
	return FVector3f(static_cast<float>(VectorIn.X), static_cast<float>(VectorIn.Y), static_cast<float>(VectorIn.Z));
};

void ABarrageDebugDrawActor::DrawFromQueue() {
	
	FVector ViewLoc;

	ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr) {
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(DrawFromQueue);
	{
		TriDrawer->Clear();

		// TrisToDraw.Reserve(TrisToDrawQueue.Size()); // Moodycamel had an approx size... this does not!!!
		
		TrisToDraw.Reset();

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ABarrageDebugDrawActor::DequeueTris);
			FTriangleWithColor Tri;
			while (TrisToDrawQueue.Dequeue(Tri)) {
				TrisToDraw.Add(Tri);
			}
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(ABarrageDebugDrawActor::TriDrawerPush);

		TriDrawer->MarkRenderStateDirty();
		TriDrawer->ReserveElements(TrisToDraw.Num());
		for (FTriangleWithColor& TriAndColor : TrisToDraw) {
			UE::Geometry::FTriangle3d& TriPoints = TriAndColor.Triangle;

			FVector3f ColorVector{TriAndColor.Color.ReinterpretAsLinear()};
			// TriDrawer->AddElement(ToFV3dFV3f(TriPoints.V[0]), ToFV3dFV3f(TriPoints.V[1]), ToFV3dFV3f(TriPoints.V[2]), ColorVector);
			TriDrawer->AddElement(FVector3f(TriPoints.V[0]), FVector3f(TriPoints.V[1]), FVector3f(TriPoints.V[2]), ColorVector);
		}
	}

	// unreal built-in string flush. This could potentially flush other debug strings so it's not a great setup.
	// FlushDebugStrings(GetWorld());
	FBarrageDebugStringDrawQueueElement StringToDraw;
	while (StringsToDraw.Dequeue(StringToDraw)) {
		
		// Hardcoded value to avoid distance draws beyond 50m... 
		// @todo a bit silly so I will probably replace this eventually
		if (FVector::Dist(ViewLoc, StringToDraw.Location) > 5000.f) {
			continue; 
		}

		DrawDebugString(GetWorld(), StringToDraw.Location, StringToDraw.String, nullptr, StringToDraw.Color, -1, true);
	}
}

void ABarrageDebugDrawActor::BeginPlay() {
	Super::BeginPlay();

	// Yes a hardcoded path is absurdly bad. this is mostly a temporary thing though
	UMaterialInterface* Material = LoadObject<UMaterial>(nullptr, TEXT("/Barrage/DebugRender/M_BarrageDebugDrawTwoSided"));
	if (Material != nullptr) {
		//@todo do we need an instance? I think no?
		UMaterialInstanceDynamic* MatInstance = UMaterialInstanceDynamic::Create(Material, this);
		TriDrawer->SetTriangleMaterial(MatInstance);
		TriDrawer->bBoundsDirty = true;
	}
}


void ABarrageDebugDrawActor::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	
	// @todo This should instead fire when a new frame is complete and be able to avoid flushing when the artillery sim is paused 
	DrawFromQueue();
	TriDrawer->MarkRenderStateDirty();
}
	

void ABarrageDebugDrawActor::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	Super::EndPlay(EndPlayReason);
}


void UBarrageDebugDrawManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
#ifdef JPH_DEBUG_RENDERER
	JPH::BarrageJoltDebugRender* NewJoltDebugRender = new JPH::BarrageJoltDebugRender();
	// Take note that this is a STATIC variable! This means if you want more than one of these you are reponsible for managing them
	JPH::DebugRenderer::sInstance = NewJoltDebugRender;
		
	// We cache a shared ptr to it as well to make sure something can track if it another world changes the static
	DebugRenderPtr = MakeShareable(NewJoltDebugRender);
#endif
}

void UBarrageDebugDrawManagerSubsystem::Deinitialize() {
	Super::Deinitialize();
	// Clear out the static ptr only if it is from our pointer
#if 0
	// @fixme this races the other subsystems. I will have to make this happen after the artillery thread is done
	if (DebugRenderPtr && JPH::DebugRenderer::sInstance == DebugRenderPtr.Get()) {
		JPH::DebugRenderer::sInstance = nullptr;
	}
	
	DebugRenderPtr = nullptr;
#endif
}


void UBarrageDebugDrawManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld) {
	Super::OnWorldBeginPlay(InWorld);
#ifdef JPH_DEBUG_RENDERER
	// Spawn and set the debug drawing actor
	// Ideally this could be done earlier but renderstate is not created early enough in initialize so this is more consistent
	
	// try to find an existing debug draw actor, just in case
	ABarrageDebugDrawActor* DrawActor = nullptr;
	for (TActorIterator<ABarrageDebugDrawActor> It(GetWorld()); It;) {
		DrawActor = *It;
		break;
	}
	
	if (!DrawActor) {
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.bNoFail = true;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.bAllowDuringConstructionScript = true; // Other rendering managers seem to do this... I doubt it matters here though
		SpawnParameters.ObjectFlags = RF_Transient;
		DrawActor = GetWorld()->SpawnActor<ABarrageDebugDrawActor>(SpawnParameters);
	}
	
	if (ensure(DrawActor) && ensure(DebugRenderPtr)) {
		// Tell this debug render about our actor, this is how it pushes triangles to draw currently
		// @todo indirection from actor->component is pointless...
		DebugRenderPtr->DrawActor = DrawActor;
	}
#endif
}

bool UBarrageDebugDrawManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const {
	
	// Only create this subsystem when jolt debug rendering is compiled
#ifdef JPH_DEBUG_RENDERER
	return Super::ShouldCreateSubsystem(Outer);
#else
	return false;
#endif
}

bool UBarrageDebugDrawManagerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const {
	// Currently we don't want to spawn any new actors in editor worlds... That could change though. Having some debug drawing at editor time is 
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
