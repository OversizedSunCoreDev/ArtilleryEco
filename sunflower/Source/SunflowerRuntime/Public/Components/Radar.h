#pragma once

#include "CoreMinimal.h"
#include "CanvasItem.h"
#include "SkeletonTypes.h"
#include "ThistleDispatch.h"

#include "Radar.generated.h"

static constexpr uint8 TEXTURE_BUFFER_SIZE = 2;
static constexpr uint32 TEXTURE_LENGTH = 1024;
static constexpr float MINIMAP_CENTER = TEXTURE_LENGTH / 2;
static const FColor BASE_COLOR = FColor::Black;
static const FColor ENEMY_COLOR = FColor::Red;
static const FColor PLAYER_COLOR = FColor::White;
static constexpr int32 SPRITE_SIDE_LENGTH = 3;

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SUNFLOWERRUNTIME_API URadarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float Radius;

	explicit URadarComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure)
	int GetRadius() const { return Radius; }

	UFUNCTION(BlueprintCallable)
	void SetRadarWidget(UMaterialInstanceDynamic* NewMaterial);
	
	void UpdateMinimapTexture();

private:
	UThistleDispatch* ThistleDispatch;
	TArray<TPair<ActorKey, FVector2d>> ActorsInRange;

	FCanvasBoxItem BoxItem = FCanvasBoxItem(FVector2d(0.f, 0.f), FVector2d(1.f, 1.f));
	UMaterialInstanceDynamic* MinimapMaterialInstance;
	UTextureRenderTarget2D* RenderTarget2D;
};
