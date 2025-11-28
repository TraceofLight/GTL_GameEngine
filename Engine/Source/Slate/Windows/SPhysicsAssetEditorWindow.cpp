#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerBootstrap.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "ImGui/imgui.h"
#include <d3d11.h>

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    ReleaseRenderTarget();
    PhysicsAssetViewerBootstrap::DestroyViewerState(ActiveState);
}

bool SPhysicsAssetEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return false;

    Device = InDevice;
    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create ViewerState
    ActiveState = PhysicsAssetViewerBootstrap::CreateViewerState("PhysicsAsset", InWorld, InDevice);
    if (!ActiveState) return false;

    return true;
}

void SPhysicsAssetEditorWindow::OnRender()
{
    if (!bIsOpen) return;

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(GetWidth(), GetHeight()));
        bInitialPlacementDone = true;
    }

    if (ImGui::Begin("Physics Asset Editor", &bIsOpen, Flags))
    {
        bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        RenderToolbar();

        // Main content area with columns
        float ContentHeight = ImGui::GetContentRegionAvail().y;

        ImGui::Columns(2, "MainColumns", true);

        // Left: Viewport
        RenderViewportPanel();

        ImGui::NextColumn();

        // Right: Body List + Properties
        float RightHeight = ContentHeight;

        // Body List (top half)
        ImGui::BeginChild("BodyList", ImVec2(0, RightHeight * 0.5f), true);
        RenderBodyListPanel();
        ImGui::EndChild();

        // Properties (bottom half)
        ImGui::BeginChild("Properties", ImVec2(0, 0), true);
        RenderPropertiesPanel();
        ImGui::EndChild();

        ImGui::Columns(1);
    }
    ImGui::End();
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->World) return;

    ActiveState->World->Tick(DeltaSeconds);
}

void SPhysicsAssetEditorWindow::OnMouseMove(FVector2D MousePos)
{
    // TODO: Forward to viewport
}

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    // TODO: Forward to viewport
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // TODO: Forward to viewport
}

void SPhysicsAssetEditorWindow::OnRenderViewport()
{
    RenderToPreviewRenderTarget();
}

FViewport* SPhysicsAssetEditorWindow::GetViewport() const
{
    return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SPhysicsAssetEditorWindow::GetViewportClient() const
{
    return ActiveState ? ActiveState->Client : nullptr;
}

void SPhysicsAssetEditorWindow::RenderToolbar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load SkeletalMesh...")) { /* TODO */ }
            if (ImGui::MenuItem("Save Physics Asset")) { /* TODO */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ActiveState)
            {
                ImGui::Checkbox("Show Bodies", &ActiveState->bShowBodies);
                ImGui::Checkbox("Show Constraints", &ActiveState->bShowConstraints);
                ImGui::Checkbox("Show Bone Names", &ActiveState->bShowBoneNames);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void SPhysicsAssetEditorWindow::RenderViewportPanel()
{
    ImVec2 AvailSize = ImGui::GetContentRegionAvail();
    uint32 NewWidth = static_cast<uint32>(AvailSize.x);
    uint32 NewHeight = static_cast<uint32>(AvailSize.y);

    if (NewWidth > 0 && NewHeight > 0)
    {
        if (NewWidth != PreviewRenderTargetWidth || NewHeight != PreviewRenderTargetHeight)
        {
            UpdateViewportRenderTarget(NewWidth, NewHeight);
        }

        if (PreviewShaderResourceView)
        {
            ImGui::Image((void*)PreviewShaderResourceView, AvailSize);
        }
    }
}

void SPhysicsAssetEditorWindow::RenderBodyListPanel()
{
    ImGui::Text("Bodies & Constraints");
    ImGui::Separator();

    // TODO: List physics bodies
    ImGui::TextDisabled("(No physics asset loaded)");
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreePanel()
{
    ImGui::Text("Skeleton");
    ImGui::Separator();

    // TODO: Bone hierarchy tree
    ImGui::TextDisabled("(No skeleton loaded)");
}

void SPhysicsAssetEditorWindow::RenderPropertiesPanel()
{
    ImGui::Text("Properties");
    ImGui::Separator();

    // TODO: Selected body/constraint properties
    ImGui::TextDisabled("(Select a body or constraint)");
}

void SPhysicsAssetEditorWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
    if (!Device || Width == 0 || Height == 0) return;

    ReleaseRenderTarget();

    // Create render target texture
    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = Width;
    TexDesc.Height = Height;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
    TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Usage = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &PreviewRenderTargetTexture);
    if (FAILED(hr)) return;

    hr = Device->CreateRenderTargetView(PreviewRenderTargetTexture, nullptr, &PreviewRenderTargetView);
    if (FAILED(hr)) return;

    hr = Device->CreateShaderResourceView(PreviewRenderTargetTexture, nullptr, &PreviewShaderResourceView);
    if (FAILED(hr)) return;

    // Create depth stencil
    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = Width;
    DepthDesc.Height = Height;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = Device->CreateTexture2D(&DepthDesc, nullptr, &PreviewDepthStencilTexture);
    if (FAILED(hr)) return;

    hr = Device->CreateDepthStencilView(PreviewDepthStencilTexture, nullptr, &PreviewDepthStencilView);
    if (FAILED(hr)) return;

    PreviewRenderTargetWidth = Width;
    PreviewRenderTargetHeight = Height;
}

void SPhysicsAssetEditorWindow::ReleaseRenderTarget()
{
    if (PreviewDepthStencilView) { PreviewDepthStencilView->Release(); PreviewDepthStencilView = nullptr; }
    if (PreviewDepthStencilTexture) { PreviewDepthStencilTexture->Release(); PreviewDepthStencilTexture = nullptr; }
    if (PreviewShaderResourceView) { PreviewShaderResourceView->Release(); PreviewShaderResourceView = nullptr; }
    if (PreviewRenderTargetView) { PreviewRenderTargetView->Release(); PreviewRenderTargetView = nullptr; }
    if (PreviewRenderTargetTexture) { PreviewRenderTargetTexture->Release(); PreviewRenderTargetTexture = nullptr; }
    PreviewRenderTargetWidth = 0;
    PreviewRenderTargetHeight = 0;
}

void SPhysicsAssetEditorWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
    CreateRenderTarget(NewWidth, NewHeight);

    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize(0, 0, NewWidth, NewHeight);
    }
}

void SPhysicsAssetEditorWindow::RenderToPreviewRenderTarget()
{
    if (!ActiveState || !ActiveState->Viewport || !PreviewRenderTargetView) return;

    // TODO: Render viewport to render target
}
