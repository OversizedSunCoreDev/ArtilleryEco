#include "Components/Radar.h"

#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"

//TODO: we might be able to use something like " if constexpr(std::is_constructible<T, FObjectInitializer&>{}) "
//to force inheritors to implement the correct form of constructor. 
URadarComponent::URadarComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	Radius = 1.f;
	ActorsInRange.Reserve(500);
	MinimapMaterialInstance = nullptr;
	RenderTarget2D = nullptr;
	ThistleDispatch = nullptr;
	BoxItem.LineThickness = 1.f;
	BoxItem.SetColor(FLinearColor::Red);
}

void URadarComponent::BeginPlay()
{
	Super::BeginPlay();
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	RenderTarget2D = UKismetRenderingLibrary::CreateRenderTarget2D(this, TEXTURE_LENGTH, TEXTURE_LENGTH, RTF_R8);
	ThistleDispatch = GetWorld()->GetSubsystem<UThistleDispatch>();
}

void URadarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void URadarComponent::OnRegister()
{
	Super::OnRegister();
}

void URadarComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Make the box we'll use to search the Thistle quadtree
	FVector PlayerLocation = GetOwner()->GetActorLocation();
	FVector2d TwoDCenter = FVector2d(PlayerLocation.X, PlayerLocation.Y);
	FBox2d RadarBox = FBox2d(TwoDCenter - Radius, TwoDCenter + Radius);
	 
	ActorsInRange.Empty();
	if (!ThistleDispatch->QuadTreeMaintenance)
	{
		ThistleDispatch->QuadTreeForDistance.Get()->GetElements(RadarBox, ActorsInRange);
	}

	if (MinimapMaterialInstance != nullptr)
	{
		UpdateMinimapTexture();
		MinimapMaterialInstance->SetScalarParameterValue(FName("RotationAngle"), GetOwner()->GetActorRotation().Yaw);
	}
}

// TODO account for different texture sizes
inline void URadarComponent::UpdateMinimapTexture()
{
	UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget2D);
	UCanvas* Canvas;
	FVector2D CanvasToRenderTargetSize;
	FDrawToRenderTargetContext RenderTargetContext;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RenderTarget2D, Canvas, CanvasToRenderTargetSize, RenderTargetContext);
	RenderTargetContext.RenderTarget->Filter = TF_Nearest;
	RenderTargetContext.RenderTarget->bAutoGenerateMips = false;
	FVector2d PlayerLocation(GetOwner()->GetActorLocation());
	const double MinimapScalar = Radius / static_cast<float>(TEXTURE_LENGTH);
	for (TPair KeyLocPair : ActorsInRange)
	{
		// Normalize the position to the minimap's coordinates with player at center
		FVector2d Direction = PlayerLocation - KeyLocPair.Value;
		float DirectionLength = Direction.Length() / MinimapScalar;
		Direction = Direction.GetSafeNormal() * DirectionLength;
		Direction = Direction + MINIMAP_CENTER;
		Canvas->DrawItem(BoxItem, FVector2d(FMath::RoundToInt32(Direction.X), FMath::RoundToInt32(Direction.Y)));
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, RenderTargetContext);
}

inline void URadarComponent::SetRadarWidget(UMaterialInstanceDynamic* NewMaterial)
{
	MinimapMaterialInstance = NewMaterial;
	if (MinimapMaterialInstance != nullptr)
	{
		MinimapMaterialInstance->SetTextureParameterValue(FName("RenderTarget"), RenderTarget2D);
	}
}

