#include "Components/ExedreFastRenderTarget.h"


UExedreWidgetRenderTarget::UExedreWidgetRenderTarget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), DefaultTexture(nullptr)
{
	ImageBrush = FSlateBrush();
	ImageBrush.SetResourceObject(DefaultTexture);

	RenderingMaterial = nullptr;
}

void UExedreWidgetRenderTarget::SetRenderMaterial( UMaterialInterface* Material , FString Path)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Texture(*Path);
	DefaultTexture = Texture.Object;
	if( Material != nullptr && RenderingMaterial != Material )
	{
		// Store new reference
		RenderingMaterial = Material;

		// Updating internal rendering brush
		ImageBrush.SetResourceObject(Material);
	}
}


TSharedRef<SWidget> UExedreWidgetRenderTarget::RebuildWidget()
{
	if( !WidgetParent.IsValid() )
	{
		WidgetParent = SNew(SImage).Image( &ImageBrush );
	}

	return WidgetParent.ToSharedRef();
}


void UExedreWidgetRenderTarget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	WidgetParent.Reset();
}