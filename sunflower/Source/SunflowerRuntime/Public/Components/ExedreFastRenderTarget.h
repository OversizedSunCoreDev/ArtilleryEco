#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExedreFastRenderTarget.generated.h"

UCLASS()
class SUNFLOWERRUNTIME_API UExedreWidgetRenderTarget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

	public:
	void SetRenderMaterial( UMaterialInterface* Material, FString Path = "");

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SWidget> WidgetParent;

	UPROPERTY(transient)
	UMaterialInterface* RenderingMaterial;

	UPROPERTY(transient)
	FSlateBrush ImageBrush;

	UPROPERTY(transient)
	UTexture2D* DefaultTexture;
};