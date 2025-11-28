#pragma once
#include "SWindow.h"

class PhysicsAssetViewerState;
class UWorld;
class UTexture;
class FViewport;
class FViewportClient;
struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;

class SPhysicsAssetEditorWindow : public SWindow
{
public:
    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // Viewport rendering
    void OnRenderViewport();

    // Accessors
    FViewport* GetViewport() const;
    FViewportClient* GetViewportClient() const;
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
    bool IsFocused() const { return bIsFocused; }

    PhysicsAssetViewerState* GetActiveState() const { return ActiveState; }

    // Render Target
    void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
    void RenderToPreviewRenderTarget();
    ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

private:
    // UI Panels
    void RenderToolbar();
    void RenderViewportPanel();
    void RenderBodyListPanel();
    void RenderSkeletonTreePanel();
    void RenderPropertiesPanel();

    // State
    PhysicsAssetViewerState* ActiveState = nullptr;
    ID3D11Device* Device = nullptr;
    bool bIsOpen = true;
    bool bIsFocused = false;
    bool bInitialPlacementDone = false;

    // Render Target
    ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
    ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
    ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
    ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
    ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
    uint32 PreviewRenderTargetWidth = 0;
    uint32 PreviewRenderTargetHeight = 0;

    void CreateRenderTarget(uint32 Width, uint32 Height);
    void ReleaseRenderTarget();
};
