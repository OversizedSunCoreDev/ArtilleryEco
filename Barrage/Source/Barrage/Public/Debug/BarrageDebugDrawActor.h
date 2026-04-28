// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#pragma once


#include "TriangleTypes.h"
#include "GameFramework/Actor.h"
#include "Containers/CircularQueue.h"
#include "BarrageDebugDrawActor.generated.h"


namespace JPH {
	class BarrageJoltDebugRender;
}

struct FTriangleWithColor {
	UE::Geometry::FTriangle3d Triangle;
	FColor Color;
};


struct FBarrageDebugStringDrawQueueElement {
	FString String = "";
	FVector Location = FVector::ZeroVector;
	FColor Color = FColor::White;
};

// Debug draw actor intended to be a singleton. It needs to be an actor because that's the most simple way to have a rendering comp for now
// This might be NotPlaceable in the future but for now I see no reason it needs to be so restrictive
// Currently the string drawing can manually flush other debug strings so this is super intrusive. It was intended to be the only thing used for debug drawing but this could be improved
UCLASS()
class BARRAGE_API ABarrageDebugDrawActor : public AActor
{
	GENERATED_BODY()

public:
	ABarrageDebugDrawActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void DrawFromQueue();
	
	TMpscQueue<FTriangleWithColor> TrisToDrawQueue;
	
	TMpscQueue<FBarrageDebugStringDrawQueueElement> StringsToDraw;

	TArray<FTriangleWithColor> TrisToDraw;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class U3DTriangleSetComponent> TriDrawer = nullptr;
};


// This currently exists mostly to manage creating a singleton rendering actor for debug drawing. This is not ideal
UCLASS()
class BARRAGE_API UBarrageDebugDrawManagerSubsystem : public UWorldSubsystem 
{	
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// This serves to maintain the BarrageJoltDebugRender if there is a second world that needs to overwrite DebugRenderer::sInstance
	TSharedPtr<JPH::BarrageJoltDebugRender> DebugRenderPtr = nullptr;
};