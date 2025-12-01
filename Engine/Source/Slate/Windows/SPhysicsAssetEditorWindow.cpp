#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerBootstrap.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAssetUtils.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Editor/PlatformProcess.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "USlateManager.h"
#include "Renderer.h"
#include "RenderManager.h"
#include "ImGui/imgui.h"
#include <d3d11.h>
#include <functional>
#include <algorithm>

// ============================================================================
// SPhysicsAssetEditorWindow
// ============================================================================

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    ViewportRect = FRect(0, 0, 0, 0);
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    ReleaseRenderTarget();

    // 스플리터 정리
    if (MainSplitter)
    {
        delete MainSplitter;
        MainSplitter = nullptr;
    }

    PhysicsAssetViewerBootstrap::DestroyViewerState(ActiveState);
}

bool SPhysicsAssetEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice, bool bEmbedded)
{
    if (!InDevice) return false;

    Device = InDevice;
    World = InWorld;
    bIsEmbeddedMode = bEmbedded;
    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create ViewerState
    ActiveState = PhysicsAssetViewerBootstrap::CreateViewerState("PhysicsAsset", InWorld, InDevice);
    if (!ActiveState) return false;

    // 패널 생성
    ToolbarPanel = new SPhysicsAssetToolbarPanel(this);
    ViewportPanelWidget = new SPhysicsAssetViewportPanel(this);
    BodyListPanel = new SPhysicsAssetBodyListPanel(this);
    PropertiesPanelWidget = new SPhysicsAssetPropertiesPanel(this);

    // 스플리터 계층 구조 생성
    // 우측: BodyList(상) | Properties(하)
    RightSplitter = new SSplitterV();
    RightSplitter->SetSplitRatio(0.5f);
    RightSplitter->SideLT = BodyListPanel;
    RightSplitter->SideRB = PropertiesPanelWidget;

    // 메인: Viewport(좌) | Right(우)
    MainSplitter = new SSplitterH();
    MainSplitter->SetSplitRatio(0.65f);
    MainSplitter->SideLT = ViewportPanelWidget;
    MainSplitter->SideRB = RightSplitter;

    // 스플리터 초기 Rect 설정
    MainSplitter->SetRect(StartX, StartY, StartX + Width, StartY + Height);

    bIsOpen = true;

    return true;
}

void SPhysicsAssetEditorWindow::OnRender()
{
    if (!bIsOpen) return;

    // Embedded 모드에서는 OnRender가 호출되지 않음 (RenderEmbedded 사용)
    if (bIsEmbeddedMode) return;

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

// ============================================================================
// RenderEmbedded - DynamicEditorWindow 내장 모드용 렌더링
// ============================================================================

void SPhysicsAssetEditorWindow::RenderEmbedded(const FRect& ContentRect)
{
    if (!ActiveState)
    {
        return;
    }

    // SSplitter에 영역 설정 및 렌더링 (탭 바는 DynamicEditorWindow에서 관리)
    if (MainSplitter)
    {
        MainSplitter->SetRect(ContentRect.Left, ContentRect.Top, ContentRect.Right, ContentRect.Bottom);
        MainSplitter->OnRender();

        // 패널 Rect 캐시 (입력 처리용)
        if (MainSplitter->GetLeftOrTop())
        {
            ViewportRect = MainSplitter->GetLeftOrTop()->GetRect();
        }
    }

    // 패널 윈도우들을 앞으로 가져오기 (DynamicEditorWindow 뒤에 가려지는 문제 해결)
    if (!SLATE.IsContentBrowserVisible())
    {
        ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##PhysicsAssetViewport");
        ImGuiWindow* BodyListWin = ImGui::FindWindowByName("Bodies##PhysicsAssetBodyList");
        ImGuiWindow* PropertiesWin = ImGui::FindWindowByName("Properties##PhysicsAssetProperties");

        if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
        if (BodyListWin) ImGui::BringWindowToDisplayFront(BodyListWin);
        if (PropertiesWin) ImGui::BringWindowToDisplayFront(PropertiesWin);
    }

    // 팝업들도 패널 위로 가져오기
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g && g->OpenPopupStack.Size > 0)
    {
        for (int i = 0; i < g->OpenPopupStack.Size; ++i)
        {
            ImGuiPopupData& PopupData = g->OpenPopupStack[i];
            if (PopupData.Window)
            {
                ImGui::BringWindowToDisplayFront(PopupData.Window);
            }
        }
    }
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState) return;

    // ViewportClient Tick (카메라 입력 처리 - WASD, 마우스 드래그 등)
    if (ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }

    // World Tick (기즈모 등 액터 업데이트)
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);
    }
}

void SPhysicsAssetEditorWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport)
    {
        return;
    }

    // 팝업/모달이 열려있으면 뷰포트 입력 무시
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    // 뷰포트 영역 가져오기 (ViewportRect는 RenderEmbedded에서 설정됨)
    FRect VPRect = ViewportRect;
    // ContentRect가 있으면 더 정확한 영역 사용
    if (ViewportPanelWidget && ViewportPanelWidget->ContentRect.GetWidth() > 0)
    {
        VPRect = ViewportPanelWidget->ContentRect;
    }

    // 기즈모 드래그 중인지 확인
    AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
    bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

    // 기즈모 드래그 중이거나 뷰포트 영역 안에 있을 때 마우스 이벤트 전달
    if (bGizmoDragging || VPRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);

        // 기즈모 호버링/드래그 인터랙션 처리
        if (Gizmo && ActiveState->Client)
        {
            ACameraActor* Camera = ActiveState->Client->GetCamera();
            if (Camera)
            {
                Gizmo->ProcessGizmoInteraction(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
            }
        }
    }

    // 스플리터에도 전달 (리사이즈용)
    if (MainSplitter)
    {
        MainSplitter->OnMouseMove(MousePos);
    }
}

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport)
    {
        return;
    }

    // 팝업/모달이 열려있으면 뷰포트 입력 무시
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    // 뷰포트 영역 가져오기 (ViewportRect는 RenderEmbedded에서 설정됨)
    FRect VPRect = ViewportRect;
    if (ViewportPanelWidget && ViewportPanelWidget->ContentRect.GetWidth() > 0)
    {
        VPRect = ViewportPanelWidget->ContentRect;
    }
    bool bInViewport = VPRect.Contains(MousePos);

    if (bInViewport)
    {
        FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);

        // 뷰포트에 마우스 다운 전달
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // 뷰포트 내 클릭은 스플리터에 전달하지 않음
        return;
    }

    // 뷰포트 밖: 스플리터에만 전달 (리사이즈용)
    if (MainSplitter)
    {
        MainSplitter->OnMouseDown(MousePos, Button);
    }
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport)
    {
        return;
    }

    // 뷰포트 영역 가져오기 (ViewportRect는 RenderEmbedded에서 설정됨)
    FRect VPRect = ViewportRect;
    if (ViewportPanelWidget && ViewportPanelWidget->ContentRect.GetWidth() > 0)
    {
        VPRect = ViewportPanelWidget->ContentRect;
    }

    // 기즈모 드래그 중이면 항상 처리
    AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
    bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

    if (bGizmoDragging || VPRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }

    // 스플리터에도 전달
    if (MainSplitter)
    {
        MainSplitter->OnMouseUp(MousePos, Button);
    }
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

void SPhysicsAssetEditorWindow::LoadSkeletalMesh(const FString& Path)
{
    if (Path.empty() || !ActiveState)
    {
        return;
    }

    // SkeletalMesh 로드
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (!Mesh)
    {
        return;
    }

    // PreviewActor에 메시 설정
    if (ActiveState->PreviewActor)
    {
        ActiveState->PreviewActor->SetSkeletalMesh(Path);

        // 메시 가시성 설정
        if (auto* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            SkelComp->SetVisibility(ActiveState->bShowMesh);
        }
    }

    // State 업데이트
    ActiveState->CurrentMesh = Mesh;
    ActiveState->LoadedMeshPath = Path;

    // 기존 Physics Asset이 있으면 유지, 없으면 새로 생성
    if (!ActiveState->PhysicsAsset)
    {
        // 새 Physics Asset 생성
        ActiveState->PhysicsAsset = NewObject<UPhysicsAsset>();
        ActiveState->PhysicsAsset->SourceSkeletalPath = Path;
    }

    // 선택 상태 초기화
    ActiveState->ClearSelection();
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset(const FString& Path)
{
    if (Path.empty() || !ActiveState)
    {
        return;
    }

    // TODO: Physics Asset 직렬화/역직렬화 구현 필요
    // 현재는 Physics Asset 파일 로드가 구현되지 않음
    // PhysicsAsset은 UResourceBase를 상속하지 않으므로 별도 로드 로직 필요

    // 임시: 새 Physics Asset 생성
    UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>();
    if (!PhysAsset)
    {
        return;
    }

    // State 업데이트
    ActiveState->PhysicsAsset = PhysAsset;
    ActiveState->LoadedPhysicsAssetPath = Path;

    // 선택 상태 초기화
    ActiveState->ClearSelection();
}

void SPhysicsAssetEditorWindow::RenderToolbar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load SkeletalMesh..."))
            {
                std::filesystem::path FilePath = FPlatformProcess::OpenLoadFileDialog(
                    L"Data",
                    L".fbx",
                    L"SkeletalMesh Files (*.fbx)"
                );
                if (!FilePath.empty())
                {
                    FString PathStr = FilePath.string();
                    LoadSkeletalMesh(PathStr);
                }
            }

            if (ImGui::MenuItem("Load Physics Asset..."))
            {
                std::filesystem::path FilePath = FPlatformProcess::OpenLoadFileDialog(
                    L"Data",
                    L".physicsasset",
                    L"Physics Asset Files (*.physicsasset)"
                );
                if (!FilePath.empty())
                {
                    FString PathStr = FilePath.string();
                    LoadPhysicsAsset(PathStr);
                }
            }

            ImGui::Separator();

            // Save는 SkeletalMesh가 로드되어 있어야 활성화
            bool bCanSave = ActiveState && ActiveState->CurrentMesh && ActiveState->PhysicsAsset;
            if (ImGui::MenuItem("Save Physics Asset", nullptr, false, bCanSave))
            {
                // TODO: 저장 로직 구현
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ActiveState)
            {
                ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh);
                ImGui::Checkbox("Show Bodies", &ActiveState->bShowBodies);
                ImGui::Checkbox("Show Constraints", &ActiveState->bShowConstraints);
                ImGui::Checkbox("Show Bone Names", &ActiveState->bShowBoneNames);

                // Mesh 가시성 변경 시 적용
                if (ActiveState->PreviewActor)
                {
                    if (auto* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                    {
                        SkelComp->SetVisibility(ActiveState->bShowMesh);
                    }
                }
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

void SPhysicsAssetEditorWindow::PrepareForDelete()
{
    // ImGui 지연 렌더링 문제 방지: delete 전에 SRV를 먼저 해제
    // ImGui::Image에서 참조하던 SRV가 nullptr이 되어 안전하게 건너뜀
    ReleaseRenderTarget();
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
    if (!ActiveState || !ActiveState->Viewport || !PreviewRenderTargetView || !PreviewDepthStencilView)
        return;

    if (!ActiveState->Client)
        return;

    URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
    if (!Renderer)
        return;

    D3D11RHI* RHI = Renderer->GetRHIDevice();
    if (!RHI)
        return;

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();

    // 기존 렌더 타겟 백업
    ID3D11RenderTargetView* OldRTV = nullptr;
    ID3D11DepthStencilView* OldDSV = nullptr;
    Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

    UINT NumViewports = 1;
    D3D11_VIEWPORT OldViewport;
    Context->RSGetViewports(&NumViewports, &OldViewport);

    // 렌더 타겟 클리어 및 설정
    float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
    Context->ClearRenderTargetView(PreviewRenderTargetView, ClearColor);
    Context->ClearDepthStencilView(PreviewDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    Context->OMSetRenderTargets(1, &PreviewRenderTargetView, PreviewDepthStencilView);

    D3D11_VIEWPORT D3DViewport = {};
    D3DViewport.TopLeftX = 0.0f;
    D3DViewport.TopLeftY = 0.0f;
    D3DViewport.Width = static_cast<float>(PreviewRenderTargetWidth);
    D3DViewport.Height = static_cast<float>(PreviewRenderTargetHeight);
    D3DViewport.MinDepth = 0.0f;
    D3DViewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &D3DViewport);

    // 뷰포트 크기 업데이트
    ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

    // 씬 렌더링
    ActiveState->Client->Draw(ActiveState->Viewport);

    // Physics Bodies 디버그 드로잉
    if (ActiveState->bShowBodies && ActiveState->PhysicsAsset)
    {
        Renderer->BeginLineBatch();
        ActiveState->DrawPhysicsBodies(Renderer);
        Renderer->EndLineBatch(FMatrix::Identity());
    }

    // 렌더 타겟 복원
    Context->OMSetRenderTargets(1, &OldRTV, OldDSV);
    Context->RSSetViewports(1, &OldViewport);

    if (OldRTV)
        OldRTV->Release();
    if (OldDSV)
        OldDSV->Release();
}

// ============================================================================
// Panel Classes Implementation
// ============================================================================

// --- SPhysicsAssetToolbarPanel ---
SPhysicsAssetToolbarPanel::SPhysicsAssetToolbarPanel(SPhysicsAssetEditorWindow* InOwner)
    : Owner(InOwner)
{
}

void SPhysicsAssetToolbarPanel::OnRender()
{
    // 툴바는 DynamicEditorWindow에서 관리됨
}

// --- SPhysicsAssetViewportPanel ---
SPhysicsAssetViewportPanel::SPhysicsAssetViewportPanel(SPhysicsAssetEditorWindow* InOwner)
    : Owner(InOwner)
{
}

void SPhysicsAssetViewportPanel::OnRender()
{
    if (!Owner) return;

    // 삭제 예정이면 렌더링 스킵 (ImGui 지연 렌더링 크래시 방지)
    //if (!Owner->IsOpen() || Owner->IsPreparedForDelete()) return;

    PhysicsAssetViewerState* State = Owner->GetActiveState();
    if (!State) return;

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##PhysicsAssetViewport", nullptr, WindowFlags))
    {
        ImVec2 AvailSize = ImGui::GetContentRegionAvail();
        uint32 Width = static_cast<uint32>(AvailSize.x);
        uint32 Height = static_cast<uint32>(AvailSize.y);

        // SkeletalMesh가 없으면 가이드 UI 표시
        if (!State->CurrentMesh)
        {
            // 배경 채우기
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 WindowPos = ImGui::GetWindowPos();
            DrawList->AddRectFilled(
                WindowPos,
                ImVec2(WindowPos.x + AvailSize.x, WindowPos.y + AvailSize.y),
                IM_COL32(40, 40, 40, 255)
            );

            // 중앙 가이드 박스
            float BoxWidth = 350.0f;
            float BoxHeight = 120.0f;
            float BoxX = WindowPos.x + (AvailSize.x - BoxWidth) * 0.5f;
            float BoxY = WindowPos.y + (AvailSize.y - BoxHeight) * 0.5f;

            // 박스 배경
            DrawList->AddRectFilled(
                ImVec2(BoxX, BoxY),
                ImVec2(BoxX + BoxWidth, BoxY + BoxHeight),
                IM_COL32(60, 60, 60, 200),
                8.0f
            );

            // 박스 테두리
            DrawList->AddRect(
                ImVec2(BoxX, BoxY),
                ImVec2(BoxX + BoxWidth, BoxY + BoxHeight),
                IM_COL32(100, 100, 100, 255),
                8.0f,
                0,
                2.0f
            );

            // 텍스트
            const char* MainText = "Drop SkeletalMesh here to start";
            const char* SubText = "or use File > Load SkeletalMesh";
            ImVec2 MainTextSize = ImGui::CalcTextSize(MainText);
            ImVec2 SubTextSize = ImGui::CalcTextSize(SubText);

            // 메인 텍스트 (흰색)
            DrawList->AddText(
                ImVec2(BoxX + (BoxWidth - MainTextSize.x) * 0.5f, BoxY + 35.0f),
                IM_COL32(220, 220, 220, 255),
                MainText
            );

            // 서브 텍스트 (회색)
            DrawList->AddText(
                ImVec2(BoxX + (BoxWidth - SubTextSize.x) * 0.5f, BoxY + 70.0f),
                IM_COL32(150, 150, 150, 255),
                SubText
            );

            // 드래그앤드롭 타겟 (전체 영역)
            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::InvisibleButton("##DropTarget", AvailSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
                {
                    const char* FilePath = static_cast<const char*>(Payload->Data);
                    FString Path(FilePath);
                    FString LowerPath = Path;
                    std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ::tolower);

                    // SkeletalMesh 파일인지 확인 (.fbx)
                    if (LowerPath.ends_with(".fbx"))
                    {
                        Owner->LoadSkeletalMesh(Path);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
        else
        {
            // SkeletalMesh가 있으면 뷰포트 렌더링
            if (Width > 0 && Height > 0)
            {
                // 렌더 타겟 업데이트
                Owner->UpdateViewportRenderTarget(Width, Height);

                // 씬 렌더링 + Physics Bodies 디버그 드로잉
                Owner->RenderToPreviewRenderTarget();

                ID3D11ShaderResourceView* SRV = Owner->GetPreviewShaderResourceView();
                if (SRV)
                {
                    ImGui::Image((void*)SRV, AvailSize);
                }
            }

            // 드래그앤드롭으로 다른 Mesh 로드도 가능
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
                {
                    const char* FilePath = static_cast<const char*>(Payload->Data);
                    FString Path(FilePath);
                    FString LowerPath = Path;
                    std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ::tolower);

                    if (LowerPath.ends_with(".fbx"))
                    {
                        Owner->LoadSkeletalMesh(Path);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        // ContentRect 업데이트 (입력 처리용)
        ImVec2 WindowPos = ImGui::GetWindowPos();
        ContentRect.Left = WindowPos.x;
        ContentRect.Top = WindowPos.y;
        ContentRect.Right = WindowPos.x + AvailSize.x;
        ContentRect.Bottom = WindowPos.y + AvailSize.y;
        ContentRect.UpdateMinMax();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void SPhysicsAssetViewportPanel::OnUpdate(float DeltaSeconds)
{
    // Viewport update handled by owner
}

// --- SPhysicsAssetBodyListPanel ---
SPhysicsAssetBodyListPanel::SPhysicsAssetBodyListPanel(SPhysicsAssetEditorWindow* InOwner)
    : Owner(InOwner)
{
}

void SPhysicsAssetBodyListPanel::OnRender()
{
    if (!Owner) return;

    PhysicsAssetViewerState* State = Owner->GetActiveState();

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    if (ImGui::Begin("Bodies##PhysicsAssetBodyList", nullptr, WindowFlags))
    {
        // 탭 바: Bodies | Constraints
        if (ImGui::BeginTabBar("BodyConstraintTabs"))
        {
            if (ImGui::BeginTabItem("Bodies"))
            {
                RenderBodyTree(State);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Constraints"))
            {
                RenderConstraintList(State);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void SPhysicsAssetBodyListPanel::RenderBodyTree(PhysicsAssetViewerState* State)
{
    if (!State)
    {
        ImGui::TextDisabled("(No state)");
        return;
    }

    // SkeletalMesh가 없으면 가이드 메시지 표시
    if (!State->CurrentMesh)
    {
        ImGui::TextDisabled("Load a SkeletalMesh first");
        ImGui::TextDisabled("(File > Load SkeletalMesh)");
        return;
    }

    UPhysicsAsset* PhysAsset = State->PhysicsAsset;
    if (!PhysAsset)
    {
        ImGui::TextDisabled("(No physics asset loaded)");
        return;
    }

    // Skeleton 기반 Body Tree 렌더링
    USkeletalMesh* Mesh = State->CurrentMesh;
    const FSkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;

    if (Skeleton && !Skeleton->Bones.IsEmpty())
    {
        // Skeleton Tree + Body 연결 상태로 렌더링
        RenderSkeletonBodyTree(State, PhysAsset, Skeleton);
    }
    else
    {
        // Skeleton이 없으면 BodySetup 목록만 표시
        RenderBodySetupList(State, PhysAsset);
    }
}

void SPhysicsAssetBodyListPanel::RenderSkeletonBodyTree(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, const FSkeleton* Skeleton)
{
    const TArray<FBone>& Bones = Skeleton->Bones;

    // 본 계층 구조 빌드
    TArray<TArray<int32>> Children;
    Children.resize(Bones.size());

    for (int32 i = 0; i < Bones.size(); ++i)
    {
        int32 Parent = Bones[i].ParentIndex;
        if (Parent >= 0 && Parent < Bones.size())
        {
            Children[Parent].Add(i);
        }
    }

    // 재귀적 트리 렌더링
    std::function<void(int32)> DrawNode = [&](int32 Index)
    {
        const bool bLeaf = Children[Index].IsEmpty();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

        if (bLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        // 펼쳐진 상태 확인
        if (State->ExpandedBoneIndices.count(Index) > 0)
        {
            ImGui::SetNextItemOpen(true);
        }

        // 본에 연결된 Body가 있는지 확인
        FName BoneName(Bones[Index].Name);
        int32 BodyIndex = PhysAsset->FindBodyIndexByBoneName(BoneName);
        bool bHasBody = (BodyIndex >= 0);

        // 선택 상태 확인
        bool bSelected = false;
        if (State->EditMode == EPhysicsAssetEditMode::Body && bHasBody && State->SelectedBodyIndex == BodyIndex)
        {
            bSelected = true;
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(Index);

        // Body가 있는 본은 색상 표시
        if (bHasBody)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // 녹색
        }

        // 본 이름 + Body 아이콘
        char label[256];
        if (bHasBody)
        {
            UBodySetup* Setup = PhysAsset->BodySetups[BodyIndex];
            int32 ShapeCount = Setup ? Setup->GetElementCount() : 0;
            sprintf_s(label, "[B] %s (%d shapes)", Bones[Index].Name.c_str(), ShapeCount);
        }
        else
        {
            sprintf_s(label, "%s", Bones[Index].Name.c_str());
        }

        bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", label);

        if (bHasBody)
        {
            ImGui::PopStyleColor();
        }

        // 클릭 처리
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            if (bHasBody)
            {
                State->SelectBody(BodyIndex);
            }
            else
            {
                State->ClearSelection();
                State->SelectedBoneName = BoneName;
            }
        }

        // 우클릭 컨텍스트 메뉴
        if (ImGui::BeginPopupContextItem())
        {
            if (bHasBody)
            {
                if (ImGui::MenuItem("Add Sphere"))
                {
                    // TODO: Add sphere shape to body
                }
                if (ImGui::MenuItem("Add Box"))
                {
                    // TODO: Add box shape to body
                }
                if (ImGui::MenuItem("Add Capsule"))
                {
                    // TODO: Add capsule shape to body
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Body"))
                {
                    // TODO: Delete body
                }
            }
            else
            {
                if (ImGui::MenuItem("Add Body"))
                {
                    // TODO: Create new body for this bone
                }
            }
            ImGui::EndPopup();
        }

        // 펼침 상태 업데이트
        if (open)
        {
            State->ExpandedBoneIndices.insert(Index);
        }
        else
        {
            State->ExpandedBoneIndices.erase(Index);
        }

        // 자식 노드 렌더링
        if (!bLeaf && open)
        {
            // Body가 있으면 Shape 목록 표시
            if (bHasBody)
            {
                RenderBodyShapes(State, PhysAsset, BodyIndex);
            }

            for (int32 ChildIndex : Children[Index])
            {
                DrawNode(ChildIndex);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    // 루트 본들 렌더링
    ImGui::BeginChild("BodyTreeView", ImVec2(0, 0), false);
    for (int32 i = 0; i < Bones.size(); ++i)
    {
        if (Bones[i].ParentIndex < 0)
        {
            DrawNode(i);
        }
    }
    ImGui::EndChild();
}

void SPhysicsAssetBodyListPanel::RenderBodySetupList(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset)
{
    ImGui::BeginChild("BodyListView", ImVec2(0, 0), false);

    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->BodySetups[i];
        if (!Setup) continue;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

        bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Body && State->SelectedBodyIndex == i);
        if (bSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(i);

        char label[256];
        sprintf_s(label, "[Body %d] %s (%d shapes)", i, Setup->BoneName.ToString().c_str(), Setup->GetElementCount());

        bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", label);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            State->SelectBody(i);
        }

        if (open)
        {
            RenderBodyShapes(State, PhysAsset, i);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void SPhysicsAssetBodyListPanel::RenderBodyShapes(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, int32 BodyIndex)
{
    if (BodyIndex < 0 || BodyIndex >= PhysAsset->BodySetups.Num()) return;

    UBodySetup* Setup = PhysAsset->BodySetups[BodyIndex];
    if (!Setup) return;

    ImGui::Indent();

    // Sphere shapes
    for (int32 i = 0; i < Setup->AggGeom.SphereElems.Num(); ++i)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

        bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Shape &&
                         State->SelectedBodyIndex == BodyIndex &&
                         State->SelectedShapeType == EAggCollisionShape::Sphere &&
                         State->SelectedShapeIndex == i);
        if (bSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        char label[64];
        sprintf_s(label, "Sphere %d (R=%.2f)", i, Setup->AggGeom.SphereElems[i].Radius);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f)); // 주황색
        ImGui::TreeNodeEx((void*)(intptr_t)(1000 + i), flags, "%s", label);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked())
        {
            State->SelectShape(BodyIndex, EAggCollisionShape::Sphere, i);
        }
    }

    // Box shapes
    for (int32 i = 0; i < Setup->AggGeom.BoxElems.Num(); ++i)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

        bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Shape &&
                         State->SelectedBodyIndex == BodyIndex &&
                         State->SelectedShapeType == EAggCollisionShape::Box &&
                         State->SelectedShapeIndex == i);
        if (bSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const FKBoxElem& Box = Setup->AggGeom.BoxElems[i];
        char label[64];
        sprintf_s(label, "Box %d (%.1fx%.1fx%.1f)", i, Box.X, Box.Y, Box.Z);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // 파란색
        ImGui::TreeNodeEx((void*)(intptr_t)(2000 + i), flags, "%s", label);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked())
        {
            State->SelectShape(BodyIndex, EAggCollisionShape::Box, i);
        }
    }

    // Capsule shapes (SphylElems)
    for (int32 i = 0; i < Setup->AggGeom.SphylElems.Num(); ++i)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

        bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Shape &&
                         State->SelectedBodyIndex == BodyIndex &&
                         State->SelectedShapeType == EAggCollisionShape::Capsule &&
                         State->SelectedShapeIndex == i);
        if (bSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const FKCapsuleElem& Capsule = Setup->AggGeom.SphylElems[i];
        char label[64];
        sprintf_s(label, "Capsule %d (R=%.1f, L=%.1f)", i, Capsule.Radius, Capsule.Length);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.8f, 1.0f)); // 보라색
        ImGui::TreeNodeEx((void*)(intptr_t)(3000 + i), flags, "%s", label);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked())
        {
            State->SelectShape(BodyIndex, EAggCollisionShape::Capsule, i);
        }
    }

    ImGui::Unindent();
}

void SPhysicsAssetBodyListPanel::RenderConstraintList(PhysicsAssetViewerState* State)
{
    if (!State)
    {
        ImGui::TextDisabled("(No state)");
        return;
    }

    // SkeletalMesh가 없으면 가이드 메시지 표시
    if (!State->CurrentMesh)
    {
        ImGui::TextDisabled("Load a SkeletalMesh first");
        return;
    }

    // TODO: Render constraint list
    ImGui::TextDisabled("(Constraint list will appear here)");
}

// --- SPhysicsAssetPropertiesPanel ---
SPhysicsAssetPropertiesPanel::SPhysicsAssetPropertiesPanel(SPhysicsAssetEditorWindow* InOwner)
    : Owner(InOwner)
{
}

void SPhysicsAssetPropertiesPanel::OnRender()
{
    if (!Owner) return;

    PhysicsAssetViewerState* State = Owner->GetActiveState();

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    if (ImGui::Begin("Properties##PhysicsAssetProperties", nullptr, WindowFlags))
    {
        if (!State)
        {
            ImGui::TextDisabled("(No state)");
        }
        else if (!State->CurrentMesh)
        {
            ImGui::TextDisabled("Load a SkeletalMesh first");
            ImGui::TextDisabled("(File > Load SkeletalMesh)");
        }
        else
        {
            // Tools section
            if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Auto-Generate Bodies 버튼
                bool bCanGenerate = (State->CurrentMesh != nullptr);
                if (!bCanGenerate)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("Auto-Generate Bodies", ImVec2(-1, 0)))
                {
                    if (State->CurrentMesh)
                    {
                        // PhysicsAsset이 없으면 생성
                        if (!State->PhysicsAsset)
                        {
                            State->PhysicsAsset = NewObject<UPhysicsAsset>();
                        }

                        // 기존 데이터 클리어 확인 (Body가 있으면)
                        bool bProceed = true;
                        if (State->PhysicsAsset->BodySetups.Num() > 0)
                        {
                            // TODO: 확인 다이얼로그 추가 가능
                            // 지금은 바로 진행
                            bProceed = true;
                        }

                        if (bProceed)
                        {
                            FPhysicsAssetUtils::CreateFromSkeletalMesh(State->PhysicsAsset, State->CurrentMesh);
                            State->ClearSelection();
                        }
                    }
                }

                if (!bCanGenerate)
                {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Load a skeletal mesh first");
                }

                // Clear All Bodies 버튼
                bool bHasBodies = State->PhysicsAsset && State->PhysicsAsset->BodySetups.Num() > 0;
                if (!bHasBodies)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("Clear All Bodies", ImVec2(-1, 0)))
                {
                    if (State->PhysicsAsset)
                    {
                        State->PhysicsAsset->BodySetups.Empty();
                        State->PhysicsAsset->BoneNameToBodyIndex.clear();
                        State->ClearSelection();
                    }
                }

                if (!bHasBodies)
                {
                    ImGui::EndDisabled();
                }
            }

            ImGui::Separator();

            // View options
            if (ImGui::CollapsingHeader("View Options", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Show Bodies", &State->bShowBodies);
                ImGui::Checkbox("Show Constraints", &State->bShowConstraints);
                ImGui::Checkbox("Show Bone Names", &State->bShowBoneNames);
                ImGui::Checkbox("Show Mesh", &State->bShowMesh);
                ImGui::Checkbox("Wireframe", &State->bWireframe);
            }

            ImGui::Separator();

            // Selected item properties
            if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
            {
                RenderBodyProperties(State);
            }
        }
    }
    ImGui::End();
}

void SPhysicsAssetPropertiesPanel::RenderBodyProperties(PhysicsAssetViewerState* State)
{
    if (!State)
    {
        ImGui::TextDisabled("(No state)");
        return;
    }

    UPhysicsAsset* PhysAsset = State->PhysicsAsset;
    if (!PhysAsset)
    {
        ImGui::TextDisabled("(No physics asset)");
        return;
    }

    // Shape 모드
    if (State->EditMode == EPhysicsAssetEditMode::Shape && State->SelectedBodyIndex >= 0 && State->SelectedShapeIndex >= 0)
    {
        RenderShapeProperties(State);
        return;
    }

    // Body 모드
    if (State->EditMode == EPhysicsAssetEditMode::Body && State->SelectedBodyIndex >= 0)
    {
        UBodySetup* Setup = State->GetSelectedBodySetup();
        if (!Setup)
        {
            ImGui::TextDisabled("(Invalid body selection)");
            return;
        }

        ImGui::Text("Body: %s", Setup->BoneName.ToString().c_str());
        ImGui::Separator();

        // Collision Trace Flag
        const char* TraceFlagNames[] = { "UseDefault", "UseSimpleAsComplex", "UseComplexAsSimple", "UseSimpleAndComplex" };
        int currentFlag = static_cast<int>(Setup->CollisionTraceFlag);
        if (ImGui::Combo("Collision Trace", &currentFlag, TraceFlagNames, IM_ARRAYSIZE(TraceFlagNames)))
        {
            Setup->CollisionTraceFlag = static_cast<ECollisionTraceFlag>(currentFlag);
        }

        ImGui::Separator();

        // Shape 요약
        ImGui::Text("Shapes:");
        ImGui::BulletText("Spheres: %d", Setup->AggGeom.SphereElems.Num());
        ImGui::BulletText("Boxes: %d", Setup->AggGeom.BoxElems.Num());
        ImGui::BulletText("Capsules: %d", Setup->AggGeom.SphylElems.Num());

        ImGui::Separator();

        // Shape 추가 버튼들
        if (ImGui::Button("Add Sphere"))
        {
            FKSphereElem NewSphere;
            NewSphere.Radius = 0.5f;
            Setup->AggGeom.SphereElems.Add(NewSphere);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Box"))
        {
            FKBoxElem NewBox(0.5f, 0.5f, 0.5f);
            Setup->AggGeom.BoxElems.Add(NewBox);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Capsule"))
        {
            FKCapsuleElem NewCapsule(0.25f, 1.0f);
            Setup->AggGeom.SphylElems.Add(NewCapsule);
        }

        return;
    }

    // 아무것도 선택되지 않음
    if (State->SelectedBoneName.IsValid())
    {
        ImGui::Text("Bone: %s", State->SelectedBoneName.ToString().c_str());
        ImGui::TextDisabled("(No body attached)");
        ImGui::Separator();

        if (ImGui::Button("Create Body for This Bone"))
        {
            // 새 BodySetup 생성
            UBodySetup* NewSetup = NewObject<UBodySetup>();
            NewSetup->BoneName = State->SelectedBoneName;

            // 기본 Capsule 추가
            FKCapsuleElem DefaultCapsule(0.25f, 1.0f);
            NewSetup->AggGeom.SphylElems.Add(DefaultCapsule);

            // PhysicsAsset에 추가
            int32 NewIndex = PhysAsset->AddBodySetup(NewSetup);
            State->SelectBody(NewIndex);
        }
    }
    else
    {
        ImGui::TextDisabled("(Select a body or bone)");
    }
}

void SPhysicsAssetPropertiesPanel::RenderShapeProperties(PhysicsAssetViewerState* State)
{
    UBodySetup* Setup = State->GetSelectedBodySetup();
    if (!Setup)
    {
        ImGui::TextDisabled("(Invalid selection)");
        return;
    }

    int32 ShapeIndex = State->SelectedShapeIndex;
    EAggCollisionShape::Type ShapeType = State->SelectedShapeType;

    switch (ShapeType)
    {
    case EAggCollisionShape::Sphere:
        if (ShapeIndex >= 0 && ShapeIndex < Setup->AggGeom.SphereElems.Num())
        {
            FKSphereElem& Sphere = Setup->AggGeom.SphereElems[ShapeIndex];
            ImGui::Text("Sphere %d", ShapeIndex);
            ImGui::Separator();

            float Center[3] = { Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z };
            if (ImGui::DragFloat3("Center", Center, 0.01f))
            {
                Sphere.Center = FVector(Center[0], Center[1], Center[2]);
            }

            ImGui::DragFloat("Radius", &Sphere.Radius, 0.01f, 0.01f, 100.0f);

            ImGui::Separator();
            if (ImGui::Button("Delete Shape"))
            {
                Setup->AggGeom.SphereElems.RemoveAt(ShapeIndex);
                State->SelectBody(State->SelectedBodyIndex); // Shape 모드에서 Body 모드로
            }
        }
        break;

    case EAggCollisionShape::Box:
        if (ShapeIndex >= 0 && ShapeIndex < Setup->AggGeom.BoxElems.Num())
        {
            FKBoxElem& Box = Setup->AggGeom.BoxElems[ShapeIndex];
            ImGui::Text("Box %d", ShapeIndex);
            ImGui::Separator();

            float Center[3] = { Box.Center.X, Box.Center.Y, Box.Center.Z };
            if (ImGui::DragFloat3("Center", Center, 0.01f))
            {
                Box.Center = FVector(Center[0], Center[1], Center[2]);
            }

            float Rotation[3] = { Box.Rotation.X, Box.Rotation.Y, Box.Rotation.Z };
            if (ImGui::DragFloat3("Rotation", Rotation, 1.0f, -360.0f, 360.0f))
            {
                Box.Rotation = FVector(Rotation[0], Rotation[1], Rotation[2]);
            }

            ImGui::DragFloat("X Extent", &Box.X, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Y Extent", &Box.Y, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Z Extent", &Box.Z, 0.01f, 0.01f, 100.0f);

            ImGui::Separator();
            if (ImGui::Button("Delete Shape"))
            {
                Setup->AggGeom.BoxElems.RemoveAt(ShapeIndex);
                State->SelectBody(State->SelectedBodyIndex);
            }
        }
        break;

    case EAggCollisionShape::Capsule:
        if (ShapeIndex >= 0 && ShapeIndex < Setup->AggGeom.SphylElems.Num())
        {
            FKCapsuleElem& Capsule = Setup->AggGeom.SphylElems[ShapeIndex];
            ImGui::Text("Capsule %d", ShapeIndex);
            ImGui::Separator();

            float Center[3] = { Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z };
            if (ImGui::DragFloat3("Center", Center, 0.01f))
            {
                Capsule.Center = FVector(Center[0], Center[1], Center[2]);
            }

            float Rotation[3] = { Capsule.Rotation.X, Capsule.Rotation.Y, Capsule.Rotation.Z };
            if (ImGui::DragFloat3("Rotation", Rotation, 1.0f, -360.0f, 360.0f))
            {
                Capsule.Rotation = FVector(Rotation[0], Rotation[1], Rotation[2]);
            }

            ImGui::DragFloat("Radius", &Capsule.Radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Length", &Capsule.Length, 0.01f, 0.01f, 100.0f);

            ImGui::Separator();
            if (ImGui::Button("Delete Shape"))
            {
                Setup->AggGeom.SphylElems.RemoveAt(ShapeIndex);
                State->SelectBody(State->SelectedBodyIndex);
            }
        }
        break;

    default:
        ImGui::TextDisabled("(Unknown shape type)");
        break;
    }
}

void SPhysicsAssetPropertiesPanel::RenderConstraintProperties(PhysicsAssetViewerState* State)
{
    if (!State)
    {
        ImGui::TextDisabled("(Select a constraint)");
        return;
    }

    // TODO: Render selected constraint properties
    ImGui::TextDisabled("(Select a constraint to view properties)");
}
