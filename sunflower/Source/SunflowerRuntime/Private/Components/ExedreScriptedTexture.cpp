
#include "Components/ExedreScriptedTexture.h"


// Constructor
UExedreScriptedTexture::UExedreScriptedTexture(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ScriptedTexture = nullptr;
}

// Begin play, setup of the Slate virtual window
void UExedreScriptedTexture::BeginPlay()
{
    Super::BeginPlay();

    if( FSlateApplication::IsInitialized() )
    {
        SlateWindow = SNew(SVirtualWindow).Size( FVector2D(256.0,256.0) );
        SlateGrid   = MakeShareable( new FHittestGrid() );
    }

    check( SlateWindow.IsValid() );
}


// Cleanup any Slate references when the component is being destroyed
void UExedreScriptedTexture::OnUnregister()
{
    Super::OnUnregister();

    if( SlateGrid.IsValid() )
    {
        SlateGrid.Reset();
    }

    if ( SlateWindow.IsValid() )
    {
        if( FSlateApplication::IsInitialized() )
        {
            FSlateApplication::Get().UnregisterVirtualWindow( SlateWindow.ToSharedRef() );
        }

        SlateWindow.Reset();
    }

    ScriptedTexture = nullptr;
    RenderingWidget = nullptr;
}


// Create the Render Target resource and the User Widget for rendering
void UExedreScriptedTexture::Init()
{
    // Create widget to render into RTT
    // Load a class from a blueprint object,
    // Don't forget to add "_C" at the end to get the class
    FString Path = "WidgetBlueprint'/Game/UI/UMG_RenderMaterial.UMG_RenderMaterial_C'";
    TSubclassOf<UUserWidget> ClassWidget = LoadClass<UUserWidget>(nullptr, *Path);

    RenderingWidget = CreateWidget<UUserWidget>( GetWorld(), ClassWidget );

    // Create render target resource
    FString Name = GetName() + "_ScriptTxt";
    ScriptedTexture = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass(), *Name);
    check( ScriptedTexture );

    ScriptedTexture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
    ScriptedTexture->SizeX      = 256;
    ScriptedTexture->SizeY      = 256;
    ScriptedTexture->ClearColor = FLinearColor::Transparent;

    ScriptedTexture->UpdateResource();

    // Slate setup
    Renderer = new FWidgetRenderer(false, true); //bool bUseGammaCorrection, bool bInClearTarget

    if( FSlateApplication::IsInitialized() )
    {
        FSlateApplication::Get().RegisterVirtualWindow( SlateWindow.ToSharedRef() );
    }

    UpdateSlateWindow();
}


// Setup the Slate window with the widget
void UExedreScriptedTexture::UpdateSlateWindow()
{
    SlateWindow->SetContent( RenderingWidget->TakeWidget() );
    SlateWindow->Resize({256.0f, 256.0f});
    SlateGeometry = FGeometry::MakeRoot( FVector2D( 256, 256 ), FSlateLayoutTransform(1.0f));
}


// Render/Draw the texture
void UExedreScriptedTexture::Render( float DeltaTime )
{

    //void FWidgetRenderer::DrawWindow(
    // FRenderTarget* RenderTarget,
    // FHittestGrid& HitTestGrid,
    // TSharedRef<SWindow> Window,
    // float Scale,
    // FVector2D DrawSize,
    // float DeltaTime,
    // bool bDeferRenderTargetUpdate)
    //  void DrawWindow(FRenderTarget* RenderTarget, FHittestGrid& HitTestGrid, TSharedRef<SWindow> Window, float Scale, FVector2D DrawSize, float DeltaTime, bool bDeferRenderTargetUpdate = false) (in class FWidgetRenderer)
    FRenderTarget* bind = (ScriptedTexture->GameThread_GetRenderTargetResource());
    Renderer->DrawWindow(
        bind, *SlateGrid.Get(), SlateWindow.ToSharedRef(), 1.0f, SlateGeometry.Size, DeltaTime
    );

    // Generate the MipMaps if needed
    // ScriptedTexture->UpdateResourceImmediate( false );
}


// Resize the render target and update the Slate window
// Note: the UpdateSlateWindow() use an hardcoded size
// so be sure to adjust the code to pass the right size
// to the window as well.
void UExedreScriptedTexture::Resize( FIntPoint& NewSize )
{
    if( ScriptedTexture != nullptr )
    {
        // Resizes the render target without recreating 
        // the FTextureResource. It might crash if you are 
        // using MipMaps because of an engine bug, in that 
        // case use UpdateResource() instead.
        // This issue should be fixed with UE4 4.26.
        ScriptedTexture->ResizeTarget( NewSize.X, NewSize.Y );

        // Recreate the Slate window used for rendering (since the size changed)
        UpdateSlateWindow();
    }
}