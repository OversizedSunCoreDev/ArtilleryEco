//see https://www.froyok.fr/blog/2020-06-render-target-performances/#unfortunately_canvas_doesnt_work
//From the inimitable Froyok
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Input/HittestGrid.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SVirtualWindow.h"
#include "ExedreScriptedTexture.generated.h"

UCLASS()
class SUNFLOWERRUNTIME_API UExedreScriptedTexture : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	public:
	virtual void Init();

	void Render( float DeltaTime = 0.0f );

	void Resize( FIntPoint& NewSize );

	virtual void BeginPlay() override;

protected:
	virtual void OnUnregister() override;

private:
	// The cached window containing the rendering widget
	TSharedPtr<SVirtualWindow>  SlateWindow;
	TSharedPtr<FHittestGrid>    SlateGrid;
	FGeometry SlateGeometry;

	void UpdateSlateWindow();

	UPROPERTY(transient)
	UTextureRenderTarget2D* ScriptedTexture;

	UPROPERTY(transient)
	UUserWidget* RenderingWidget;

	FWidgetRenderer* Renderer;
};