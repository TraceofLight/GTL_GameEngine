#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerBootstrap.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAssetUtils.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsConstraintSetup.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
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

    // Graph State 정리
    if (GraphState)
    {
        delete GraphState;
        GraphState = nullptr;
    }

    // 스플리터 정리 (패널들은 스플리터가 소유하지 않으므로 별도 삭제)
    if (MainSplitter)
    {
        // InnerSplitter 자식들 (ViewportPanelWidget, PropertiesPanelWidget)
        // LeftSplitter 자식들 (BodyListPanel, GraphPanelWidget)
        // 스플리터들
        delete LeftSplitter;
        delete InnerSplitter;
        delete MainSplitter;
        MainSplitter = nullptr;
        LeftSplitter = nullptr;
        InnerSplitter = nullptr;
    }

    // 패널 정리
    delete ToolbarPanel;
    delete ViewportPanelWidget;
    delete BodyListPanel;
    delete PropertiesPanelWidget;
    delete GraphPanelWidget;

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

    // Graph State 생성
    GraphState = new FPAEGraphState();

    // 패널 생성
    ToolbarPanel = new SPhysicsAssetToolbarPanel(this);
    ViewportPanelWidget = new SPhysicsAssetViewportPanel(this);
    BodyListPanel = new SPhysicsAssetBodyListPanel(this);
    PropertiesPanelWidget = new SPhysicsAssetPropertiesPanel(this);
    GraphPanelWidget = new SPhysicsAssetGraphPanel(this);

    // 스플리터 계층 구조 생성 (새 레이아웃)
    // 레이아웃: [Left: SkeletonTree|NodeGraph] | [Center: Viewport] | [Right: Properties]
    //
    // MainSplitter (H): LeftSplitter | InnerSplitter
    //   LeftSplitter (V): BodyListPanel(상) | GraphPanelWidget(하)
    //   InnerSplitter (H): ViewportPanelWidget(좌) | PropertiesPanelWidget(우)

    // 좌측: BodyList(상) | Graph(하)
    LeftSplitter = new SSplitterV();
    LeftSplitter->SetSplitRatio(0.5f);
    LeftSplitter->SideLT = BodyListPanel;
    LeftSplitter->SideRB = GraphPanelWidget;

    // 내부: Viewport(좌) | Properties(우)
    InnerSplitter = new SSplitterH();
    InnerSplitter->SetSplitRatio(0.70f);  // Viewport 70%, Properties 30%
    InnerSplitter->SideLT = ViewportPanelWidget;
    InnerSplitter->SideRB = PropertiesPanelWidget;

    // 메인: Left(좌) | Inner(우)
    MainSplitter = new SSplitterH();
    MainSplitter->SetSplitRatio(0.20f);  // Left 20%, Rest 80%
    MainSplitter->SideLT = LeftSplitter;
    MainSplitter->SideRB = InnerSplitter;

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

        // 왼쪽 마우스 버튼 클릭 시 Body/Shape 피킹
        if (Button == 0 && ActiveState->Client && !ActiveState->bSimulating)
        {
            ACameraActor* Camera = ActiveState->Client->GetCamera();
            if (Camera)
            {
                // 기즈모가 드래그 중이 아닐 때만 피킹
                AGizmoActor* Gizmo = ActiveState->GizmoActor;
                bool bGizmoDragging = Gizmo && Gizmo->GetbIsDragging();

                if (!bGizmoDragging)
                {
                    FVector CameraPos = Camera->GetActorLocation();
                    FVector CameraRight = Camera->GetRight();
                    FVector CameraUp = Camera->GetUp();
                    FVector CameraForward = Camera->GetForward();

                    FVector2D ViewportMousePos(LocalPos.X, LocalPos.Y);
                    FVector2D ViewportSize(VPRect.GetWidth(), VPRect.GetHeight());

                    FRay Ray = MakeRayFromViewport(
                        Camera->GetViewMatrix(),
                        Camera->GetProjectionMatrix(VPRect.GetWidth() / VPRect.GetHeight(), ActiveState->Viewport),
                        CameraPos, CameraRight, CameraUp, CameraForward,
                        ViewportMousePos, ViewportSize
                    );

                    int32 HitBodyIndex = -1;
                    EAggCollisionShape::Type HitShapeType = EAggCollisionShape::Unknown;
                    int32 HitShapeIndex = -1;
                    float HitDistance = 0.0f;

                    if (ActiveState->PickBodyOrShape(Ray, HitBodyIndex, HitShapeType, HitShapeIndex, HitDistance))
                    {
                        // 피킹 성공 - 편집 모드에 따라 선택
                        if (ActiveState->EditMode == EPhysicsAssetEditMode::Shape)
                        {
                            // Shape 모드: 개별 Shape 선택
                            ActiveState->SelectShape(HitBodyIndex, HitShapeType, HitShapeIndex);
                        }
                        else
                        {
                            // Body 모드: Body 전체 선택
                            ActiveState->SelectBody(HitBodyIndex);
                        }
                    }
                    else
                    {
                        // 빈 공간 클릭 - 선택 해제
                        ActiveState->ClearSelection();
                    }
                }
            }
        }

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

    // Physics Asset 생성 및 파일에서 로드
    UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>();
    if (!PhysAsset)
    {
        return;
    }

    if (!PhysAsset->LoadFromFile(Path))
    {
        UE_LOG("[PAE] Failed to load physics asset: %s", Path.c_str());
        return;
    }

    // State 업데이트
    ActiveState->PhysicsAsset = PhysAsset;
    ActiveState->LoadedPhysicsAssetPath = Path;

    // SourceSkeletalPath가 있으면 SkeletalMesh도 로드
    if (!PhysAsset->SourceSkeletalPath.empty())
    {
        LoadSkeletalMesh(PhysAsset->SourceSkeletalPath);
    }

    // 선택 상태 초기화
    ActiveState->ClearSelection();

    UE_LOG("[PAE] Loaded physics asset: %s", Path.c_str());
}

void SPhysicsAssetEditorWindow::SavePhysicsAsset(const FString& Path)
{
    if (Path.empty() || !ActiveState || !ActiveState->PhysicsAsset)
    {
        return;
    }

    // SourceSkeletalPath 업데이트
    if (ActiveState->CurrentMesh)
    {
        ActiveState->PhysicsAsset->SourceSkeletalPath = ActiveState->LoadedMeshPath;
    }

    if (ActiveState->PhysicsAsset->SaveToFile(Path))
    {
        ActiveState->LoadedPhysicsAssetPath = Path;
        UE_LOG("[PAE] Saved physics asset: %s", Path.c_str());
    }
    else
    {
        UE_LOG("[PAE] Failed to save physics asset: %s", Path.c_str());
    }
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
                std::filesystem::path FilePath = FPlatformProcess::OpenSaveFileDialog(
                    L"Data",
                    L".physicsasset",
                    L"Physics Asset Files (*.physicsasset)"
                );
                if (!FilePath.empty())
                {
                    FString PathStr = FilePath.string();
                    // 확장자가 없으면 추가
                    if (PathStr.find(".physicsasset") == FString::npos)
                    {
                        PathStr += ".physicsasset";
                    }
                    SavePhysicsAsset(PathStr);
                }
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

// ============================================================================
// Physics Simulation
// ============================================================================

void SPhysicsAssetEditorWindow::StartSimulation()
{
    if (!ActiveState || !ActiveState->PhysicsAsset || !ActiveState->PreviewActor)
        return;

    USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp)
        return;

    USkeletalMesh* CompMesh = SkelComp->GetSkeletalMesh();
    if (!CompMesh)
        return;

    // PhysicsAsset 설정
    CompMesh->SetPhysicsAsset(ActiveState->PhysicsAsset);

    // bPie 활성화
    if (!ActiveState->World)
        return;

    ActiveState->World->bPie = true;

    // Floor의 Static Physics Body 생성 (bPie=true 후에 호출해야 함)
    if (ActiveState->FloorActor)
    {
        UStaticMeshComponent* FloorComp = ActiveState->FloorActor->GetStaticMeshComponent();
        if (FloorComp)
        {
            FloorComp->SetSimulatePhysics(false);  // Static body
        }
    }

    // 캐릭터 시뮬레이션 시작
    SkelComp->SetSimulatePhysics(true);

    ActiveState->bSimulating = true;
    ActiveState->bSimulationPaused = false;
}

void SPhysicsAssetEditorWindow::StopSimulation()
{
    if (!ActiveState)
    {
        return;
    }

    // 물리 시뮬레이션 비활성화
    if (ActiveState->PreviewActor)
    {
        USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
        if (SkelComp)
        {
            SkelComp->SetSimulatePhysics(false);

            // 메시를 다시 설정하여 초기 포즈로 리셋
            if (!ActiveState->LoadedMeshPath.empty())
            {
                ActiveState->PreviewActor->SetSkeletalMesh(ActiveState->LoadedMeshPath);
            }
        }
    }

    // bPie 비활성화 (시뮬레이션 종료)
    if (ActiveState->World)
    {
        ActiveState->World->bPie = false;
    }

    ActiveState->bSimulating = false;
    ActiveState->bSimulationPaused = false;

    UE_LOG("[PAE] Simulation stopped");
}

bool SPhysicsAssetEditorWindow::IsSimulating() const
{
    return ActiveState && ActiveState->bSimulating;
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
        // 반투명 솔리드 Body 렌더링 (언리얼 스타일)
        ActiveState->DrawPhysicsBodiesSolid(Renderer);

        // 와이어프레임 라인 렌더링 (비활성화)
        // Renderer->BeginLineBatch();
        // ActiveState->DrawPhysicsBodies(Renderer);
        // Renderer->EndLineBatch(FMatrix::Identity());
    }

    // Constraint 디버그 드로잉
    if (ActiveState->bShowConstraints && ActiveState->PhysicsAsset)
    {
        Renderer->BeginLineBatch();
        ActiveState->DrawConstraints(Renderer);
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
                UBodySetup* Setup = PhysAsset->BodySetups[BodyIndex];
                if (ImGui::MenuItem("Add Sphere"))
                {
                    if (Setup)
                    {
                        FKSphereElem NewSphere;
                        NewSphere.Radius = 5.0f;
                        Setup->AggGeom.SphereElems.Add(NewSphere);
                    }
                }
                if (ImGui::MenuItem("Add Box"))
                {
                    if (Setup)
                    {
                        FKBoxElem NewBox(5.0f, 5.0f, 5.0f);
                        Setup->AggGeom.BoxElems.Add(NewBox);
                    }
                }
                if (ImGui::MenuItem("Add Capsule"))
                {
                    if (Setup)
                    {
                        FKCapsuleElem NewCapsule(3.0f, 10.0f);
                        Setup->AggGeom.SphylElems.Add(NewCapsule);
                    }
                }
                ImGui::Separator();

                // Constraint 생성 서브 메뉴
                if (ImGui::BeginMenu("Add Constraint To..."))
                {
                    // 다른 Body 목록 표시
                    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
                    {
                        UBodySetup* OtherBody = PhysAsset->BodySetups[i];
                        if (!OtherBody || OtherBody == Setup) continue;  // 자기 자신은 제외

                        // 이미 Constraint가 있는지 확인
                        bool bConstraintExists = false;
                        for (UPhysicsConstraintSetup* Constraint : PhysAsset->ConstraintSetups)
                        {
                            if (Constraint &&
                                ((Constraint->ConstraintBone1 == Setup->BoneName && Constraint->ConstraintBone2 == OtherBody->BoneName) ||
                                 (Constraint->ConstraintBone1 == OtherBody->BoneName && Constraint->ConstraintBone2 == Setup->BoneName)))
                            {
                                bConstraintExists = true;
                                break;
                            }
                        }

                        // 메뉴 아이템 표시
                        FString MenuLabel = OtherBody->BoneName.ToString();
                        if (bConstraintExists)
                        {
                            MenuLabel += " (Already Connected)";
                        }

                        bool bEnabled = !bConstraintExists;
                        if (ImGui::MenuItem(MenuLabel.c_str(), nullptr, false, bEnabled))
                        {
                            // 새 Constraint 생성
                            UPhysicsConstraintSetup* NewConstraint = NewObject<UPhysicsConstraintSetup>();
                            NewConstraint->ConstraintBone1 = Setup->BoneName;
                            NewConstraint->ConstraintBone2 = OtherBody->BoneName;
                            NewConstraint->BodyIndex1 = BodyIndex;
                            NewConstraint->BodyIndex2 = i;

                            // 기본 제한값 설정
                            NewConstraint->LinearXMotion = ELinearConstraintMotion::Locked;
                            NewConstraint->LinearYMotion = ELinearConstraintMotion::Locked;
                            NewConstraint->LinearZMotion = ELinearConstraintMotion::Locked;

                            NewConstraint->Swing1Motion = EAngularConstraintMotion::Limited;
                            NewConstraint->Swing2Motion = EAngularConstraintMotion::Limited;
                            NewConstraint->TwistMotion = EAngularConstraintMotion::Limited;

                            NewConstraint->Swing1LimitAngle = 45.0f;
                            NewConstraint->Swing2LimitAngle = 45.0f;
                            NewConstraint->TwistLimitAngle = 45.0f;

                            PhysAsset->AddConstraintSetup(NewConstraint);

                            UE_LOG("[PAE] Created constraint between '%s' and '%s'",
                                   Setup->BoneName.ToString().c_str(),
                                   OtherBody->BoneName.ToString().c_str());
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Delete Body"))
                {
                    // Body 삭제
                    if (PhysAsset->RemoveBodyByBoneName(BoneName))
                    {
                        // 선택 상태 초기화
                        State->ClearSelection();
                    }
                }
            }
            else
            {
                if (ImGui::MenuItem("Add Body"))
                {
                    // 새 Body 생성
                    UBodySetup* NewBody = NewObject<UBodySetup>();
                    if (NewBody)
                    {
                        NewBody->BoneName = BoneName;
                        // 기본 Capsule Shape 추가
                        FKCapsuleElem DefaultCapsule(3.0f, 10.0f);
                        NewBody->AggGeom.SphylElems.Add(DefaultCapsule);
                        PhysAsset->AddBodySetup(NewBody);
                        // 새로 생성된 Body 선택
                        int32 NewIndex = PhysAsset->FindBodyIndexByBoneName(BoneName);
                        if (NewIndex >= 0)
                        {
                            State->SelectBody(NewIndex);
                        }
                    }
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

    // PhysicsAsset이 없거나 Constraint가 없으면 안내 메시지
    if (!State->PhysicsAsset || State->PhysicsAsset->ConstraintSetups.IsEmpty())
    {
        ImGui::TextDisabled("No constraints");
        ImGui::TextDisabled("Use 'Generate Ragdoll' to create");
        return;
    }

    // Constraint 목록 렌더링
    UPhysicsAsset* PhysAsset = State->PhysicsAsset;
    for (int32 i = 0; i < PhysAsset->ConstraintSetups.Num(); ++i)
    {
        UPhysicsConstraintSetup* Setup = PhysAsset->ConstraintSetups[i];
        if (!Setup) continue;

        char label[256];
        sprintf_s(label, "[C%d] %s <-> %s",
            i,
            Setup->ConstraintBone1.ToString().c_str(),
            Setup->ConstraintBone2.ToString().c_str());

        bool bSelected = (State->SelectedConstraintIndex == i);
        if (ImGui::Selectable(label, bSelected))
        {
            State->SelectedConstraintIndex = i;
            State->EditMode = EPhysicsAssetEditMode::Constraint;
        }

        // 툴팁
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Constraint %d", i);
            ImGui::Text("Body1: %s (idx %d)", Setup->ConstraintBone1.ToString().c_str(), Setup->BodyIndex1);
            ImGui::Text("Body2: %s (idx %d)", Setup->ConstraintBone2.ToString().c_str(), Setup->BodyIndex2);
            ImGui::Separator();
            ImGui::Text("Swing1: %.1f deg", Setup->Swing1LimitAngle);
            ImGui::Text("Swing2: %.1f deg", Setup->Swing2LimitAngle);
            ImGui::Text("Twist: %.1f deg", Setup->TwistLimitAngle);
            ImGui::EndTooltip();
        }
    }
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

                // Constraint 자동 생성 체크박스 옵션
                static bool bAlsoGenerateConstraints = true;
                ImGui::Checkbox("Also Generate Constraints", &bAlsoGenerateConstraints);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("When enabled, ragdoll constraints will be\nautomatically generated along with bodies");
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
                            // Bodies 생성
                            FPhysicsAssetUtils::CreateFromSkeletalMesh(State->PhysicsAsset, State->CurrentMesh);
                            UE_LOG("[PAE] Generated %d bodies", State->PhysicsAsset->BodySetups.Num());

                            // 체크박스가 켜져 있으면 Constraints도 함께 생성
                            UE_LOG("[PAE] bAlsoGenerateConstraints = %s", bAlsoGenerateConstraints ? "true" : "false");
                            if (bAlsoGenerateConstraints)
                            {
                                State->PhysicsAsset->ConstraintSetups.Empty();
                                FPhysicsAssetUtils::CreateConstraintsForRagdoll(State->PhysicsAsset, State->CurrentMesh);
                                UE_LOG("[PAE] Auto-generated %d constraints (checkbox ON)", State->PhysicsAsset->ConstraintSetups.Num());
                            }
                            else
                            {
                                UE_LOG("[PAE] Skipping constraint generation (checkbox OFF)");
                            }

                            // Graph 동기화
                            Owner->SyncGraphFromPhysicsAsset();

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

                // Generate Ragdoll 버튼
                bool bCanGenerateRagdoll = bHasBodies && State->CurrentMesh;
                if (!bCanGenerateRagdoll)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("Generate Ragdoll", ImVec2(-1, 0)))
                {
                    if (State->PhysicsAsset && State->CurrentMesh)
                    {
                        // 기존 Constraints 지우고 새로 생성
                        State->PhysicsAsset->ConstraintSetups.Empty();
                        FPhysicsAssetUtils::CreateConstraintsForRagdoll(State->PhysicsAsset, State->CurrentMesh);
                        UE_LOG("[PAE] Generated %d ragdoll constraints", State->PhysicsAsset->ConstraintSetups.Num());

                        // Graph 동기화
                        Owner->SyncGraphFromPhysicsAsset();

                        State->ClearSelection();
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Generate ragdoll constraints between bodies");
                }

                if (!bCanGenerateRagdoll)
                {
                    ImGui::EndDisabled();
                }

                ImGui::Separator();

                // Simulation 버튼들
                ImGui::Text("Simulation");
                bool bCanSimulate = bHasBodies && State->CurrentMesh;
                if (!bCanSimulate)
                {
                    ImGui::BeginDisabled();
                }

                if (!State->bSimulating)
                {
                    // Simulate 시작 버튼
                    if (ImGui::Button("Simulate", ImVec2(-1, 0)))
                    {
                        Owner->StartSimulation();
                    }
                }
                else
                {
                    // 시뮬레이션 중일 때 컨트롤
                    if (State->bSimulationPaused)
                    {
                        if (ImGui::Button("Resume", ImVec2(-1, 0)))
                        {
                            State->bSimulationPaused = false;
                        }
                    }
                    else
                    {
                        if (ImGui::Button("Pause", ImVec2(-1, 0)))
                        {
                            State->bSimulationPaused = true;
                        }
                    }

                    if (ImGui::Button("Stop", ImVec2(-1, 0)))
                    {
                        Owner->StopSimulation();
                    }

                    if (ImGui::Button("Reset", ImVec2(-1, 0)))
                    {
                        // 시뮬레이션을 멈추고 다시 시작
                        Owner->StopSimulation();
                        Owner->StartSimulation();
                    }
                }

                if (!bCanSimulate)
                {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Generate bodies first");
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

// ============================================================================
// SPhysicsAssetGraphPanel - Node Graph 패널
// ============================================================================

SPhysicsAssetGraphPanel::SPhysicsAssetGraphPanel(SPhysicsAssetEditorWindow* InOwner)
    : Owner(InOwner)
{
}

void SPhysicsAssetGraphPanel::OnRender()
{
    if (!Owner) return;

    PhysicsAssetViewerState* State = Owner->GetActiveState();
    FPAEGraphState* GraphState = Owner->GraphState;

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    if (ImGui::Begin("Graph##PhysicsAssetGraph", nullptr, WindowFlags))
    {
        if (!State || !State->CurrentMesh)
        {
            ImGui::TextDisabled("Load a SkeletalMesh first");
        }
        else if (!State->PhysicsAsset || State->PhysicsAsset->BodySetups.Num() == 0)
        {
            ImGui::TextDisabled("No bodies to display");
            ImGui::TextDisabled("Use 'Auto-Generate Bodies' or add bodies manually");
        }
        else
        {
            RenderNodeGraph(State, GraphState);
        }
    }
    ImGui::End();
}

// 노드 그리기 헬퍼 함수
static void DrawBodyNode(FPAEBodyNode& BodyNode, PhysicsAssetViewerState* State, bool bIsSelected, bool bIsCenter)
{
    // 노드 색상 (선택된 것은 파란색, 중앙 노드는 녹색)
    ImU32 HeaderColor;
    if (bIsCenter)
        HeaderColor = IM_COL32(60, 120, 80, 255);  // 녹색 (선택된 메인 Body)
    else if (bIsSelected)
        HeaderColor = IM_COL32(70, 130, 180, 255); // 파란색
    else
        HeaderColor = IM_COL32(60, 60, 70, 255);   // 회색

    ed::BeginNode(BodyNode.ID);

    ImGui::PushID(static_cast<int>(BodyNode.ID.Get()));

    // 헤더
    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::Indent(4.0f);

        // Body 이름 (Bone 이름)
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", BodyNode.BoneName.c_str());

        // Shape 수 표시
        if (BodyNode.BodyIndex >= 0 && BodyNode.BodyIndex < State->PhysicsAsset->BodySetups.Num())
        {
            UBodySetup* Setup = State->PhysicsAsset->BodySetups[BodyNode.BodyIndex];
            if (Setup)
            {
                int32 ShapeCount = Setup->GetElementCount();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("%d shapes", ShapeCount);
                ImGui::PopStyleColor();
            }
        }

        ImGui::Unindent(4.0f);
        ImGui::Dummy(ImVec2(0, 2));
    }
    ImGui::EndGroup();

    // 헤더 크기
    ImRect HeaderRect;
    HeaderRect.Min = ImGui::GetItemRectMin();
    HeaderRect.Max = ImGui::GetItemRectMax();

    // 최소 너비 보장
    float minWidth = 120.0f;
    if (HeaderRect.GetWidth() < minWidth)
    {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(minWidth - HeaderRect.GetWidth(), 0));
    }

    ImGui::Dummy(ImVec2(0, 4));

    // 핀 영역
    ImGui::BeginGroup();
    {
        // 입력 핀 (왼쪽)
        if (!BodyNode.InputPins.empty())
        {
            ed::BeginPin(BodyNode.InputPins[0], ed::PinKind::Input);
            ImGui::TextDisabled(">");
            ed::EndPin();
        }

        // 공간 확보
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(60, 0));
        ImGui::SameLine();

        // 출력 핀 (오른쪽)
        if (!BodyNode.OutputPins.empty())
        {
            ed::BeginPin(BodyNode.OutputPins[0], ed::PinKind::Output);
            ImGui::TextDisabled(">");
            ed::EndPin();
        }
    }
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PopID();

    ed::EndNode();

    // 배경 그리기
    if (ImGui::IsItemVisible())
    {
        ImDrawList* drawList = ed::GetNodeBackgroundDrawList(BodyNode.ID);

        ImVec2 nodeMin = ed::GetNodePosition(BodyNode.ID);
        ImVec2 nodeSize = ed::GetNodeSize(BodyNode.ID);
        ImVec2 nodeMax = ImVec2(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);

        float headerHeight = HeaderRect.GetHeight() + 4;
        ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);

        // 헤더 배경
        drawList->AddRectFilled(nodeMin, headerMax, HeaderColor,
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);

        // 바디 배경
        drawList->AddRectFilled(ImVec2(nodeMin.x, headerMax.y), nodeMax,
            IM_COL32(30, 30, 35, 230),
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersBottom);

        // 테두리 (중앙 노드는 녹색 테두리)
        if (bIsCenter)
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(100, 200, 120, 255),
                ed::GetStyle().NodeRounding, 0, 2.5f);
        }
        else if (bIsSelected)
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(100, 180, 255, 255),
                ed::GetStyle().NodeRounding, 0, 2.0f);
        }
    }
}

void SPhysicsAssetGraphPanel::RenderNodeGraph(PhysicsAssetViewerState* State, FPAEGraphState* GraphState)
{
    if (!State || !GraphState || !State->PhysicsAsset) return;

    // Graph State와 PhysicsAsset 동기화 (Body 수가 달라졌을 때)
    if (GraphState->BodyNodes.Num() != State->PhysicsAsset->BodySetups.Num())
    {
        Owner->SyncGraphFromPhysicsAsset();
    }

    ed::SetCurrentEditor(GraphState->Context);
    ed::Begin("PhysicsAssetGraph", ImVec2(0.0f, 0.0f));

    // 선택된 Body가 없으면 빈 화면
    if (State->SelectedBodyIndex < 0 || State->EditMode != EPhysicsAssetEditMode::Body)
    {
        // 빈 화면에 안내 텍스트
        ImVec2 canvasSize = ed::GetScreenSize();
        ImVec2 textPos = ImVec2(canvasSize.x * 0.5f - 100, canvasSize.y * 0.5f);
        ImGui::SetCursorScreenPos(textPos);
        ImGui::TextDisabled("Select a body from the tree");

        ed::End();
        ed::SetCurrentEditor(nullptr);
        return;
    }

    // 스켈레톤 정보 가져오기
    const FSkeleton* Skeleton = State->CurrentMesh ? State->CurrentMesh->GetSkeleton() : nullptr;
    if (!Skeleton)
    {
        ed::End();
        ed::SetCurrentEditor(nullptr);
        return;
    }

    UPhysicsAsset* PhysAsset = State->PhysicsAsset;

    // 선택된 Body 찾기
    FPAEBodyNode* CenterNode = nullptr;
    for (auto& Node : GraphState->BodyNodes)
    {
        if (Node.BodyIndex == State->SelectedBodyIndex)
        {
            CenterNode = &Node;
            break;
        }
    }

    if (!CenterNode)
    {
        ed::End();
        ed::SetCurrentEditor(nullptr);
        return;
    }

    // 선택된 Body의 뼈 인덱스 찾기
    int32 CenterBoneIndex = -1;
    for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        if (Skeleton->Bones[i].Name == CenterNode->BoneName)
        {
            CenterBoneIndex = i;
            break;
        }
    }

    // 자식 뼈들 중 Body가 있는 것들 찾기
    TArray<FPAEBodyNode*> ChildNodes;
    TArray<int32> ChildBoneIndices;

    if (CenterBoneIndex >= 0)
    {
        for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
        {
            // 이 뼈의 부모가 선택된 뼈인지 확인
            if (Skeleton->Bones[i].ParentIndex == CenterBoneIndex)
            {
                // 이 자식 뼈에 Body가 있는지 확인
                FString ChildBoneName = Skeleton->Bones[i].Name;
                for (auto& Node : GraphState->BodyNodes)
                {
                    if (Node.BoneName == ChildBoneName)
                    {
                        ChildNodes.Add(&Node);
                        ChildBoneIndices.Add(i);
                        break;
                    }
                }
            }
        }
    }

    // 스타일 설정
    ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

    // 노드 위치 계산 (동적으로)
    float centerX = 50.0f;
    float centerY = 100.0f + (ChildNodes.Num() * 40.0f);  // 자식 수에 따라 중앙 조정
    float childX = 350.0f;  // 자식들은 오른쪽에
    float childSpacing = 100.0f;

    // 중앙 노드 위치 설정
    ed::SetNodePosition(CenterNode->ID, ImVec2(centerX, centerY));

    // 중앙 Body 노드 그리기
    DrawBodyNode(*CenterNode, State, true, true);

    // 자식 Body 노드들 그리기
    for (int32 i = 0; i < ChildNodes.Num(); ++i)
    {
        FPAEBodyNode* ChildNode = ChildNodes[i];
        float childY = 50.0f + i * childSpacing;

        ed::SetNodePosition(ChildNode->ID, ImVec2(childX, childY));
        DrawBodyNode(*ChildNode, State, false, false);
    }

    ed::PopStyleVar(2);

    // Constraint 링크 그리기 (중앙 -> 자식)
    // 직선 스타일로 설정
    ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);

    for (int32 i = 0; i < ChildNodes.Num(); ++i)
    {
        FPAEBodyNode* ChildNode = ChildNodes[i];

        // 임시 링크 ID 생성 (중앙 노드 ID와 자식 노드 ID 조합)
        ed::LinkId LinkID = ed::LinkId(1000000 + CenterNode->BodyIndex * 1000 + ChildNode->BodyIndex);

        // 중앙 노드의 출력 핀 -> 자식 노드의 입력 핀
        if (!CenterNode->OutputPins.empty() && !ChildNode->InputPins.empty())
        {
            ed::Link(LinkID, CenterNode->OutputPins[0], ChildNode->InputPins[0],
                ImColor(180, 180, 180, 255), 2.0f);
        }
    }

    ed::PopStyleVar();

    // ============================================================================
    // Constraint 생성 처리 (Pin 드래그)
    // ============================================================================
    if (ed::BeginCreate())
    {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId))
        {
            // 시작 핀과 끝 핀의 노드 찾기
            FPAEBodyNode* StartNode = Owner->FindBodyNodeByPin(startPinId);
            FPAEBodyNode* EndNode = Owner->FindBodyNodeByPin(endPinId);

            if (StartNode && EndNode && StartNode != EndNode)
            {
                // 두 Body 간에 이미 Constraint가 있는지 확인
                UBodySetup* Body1 = PhysAsset->BodySetups[StartNode->BodyIndex];
                UBodySetup* Body2 = PhysAsset->BodySetups[EndNode->BodyIndex];

                bool bConstraintExists = false;
                for (UPhysicsConstraintSetup* Constraint : PhysAsset->ConstraintSetups)
                {
                    if (Constraint &&
                        ((Constraint->ConstraintBone1 == Body1->BoneName && Constraint->ConstraintBone2 == Body2->BoneName) ||
                         (Constraint->ConstraintBone1 == Body2->BoneName && Constraint->ConstraintBone2 == Body1->BoneName)))
                    {
                        bConstraintExists = true;
                        break;
                    }
                }

                if (!bConstraintExists)
                {
                    // 새 Constraint 생성 허용
                    if (ed::AcceptNewItem())
                    {
                        // 새 Constraint 생성
                        UPhysicsConstraintSetup* NewConstraint = NewObject<UPhysicsConstraintSetup>();
                        NewConstraint->ConstraintBone1 = Body1->BoneName;
                        NewConstraint->ConstraintBone2 = Body2->BoneName;
                        NewConstraint->BodyIndex1 = StartNode->BodyIndex;
                        NewConstraint->BodyIndex2 = EndNode->BodyIndex;

                        // 기본 제한값 설정 (Unreal 스타일 - Limited Angular, Locked Linear)
                        NewConstraint->LinearXMotion = ELinearConstraintMotion::Locked;
                        NewConstraint->LinearYMotion = ELinearConstraintMotion::Locked;
                        NewConstraint->LinearZMotion = ELinearConstraintMotion::Locked;

                        NewConstraint->Swing1Motion = EAngularConstraintMotion::Limited;
                        NewConstraint->Swing2Motion = EAngularConstraintMotion::Limited;
                        NewConstraint->TwistMotion = EAngularConstraintMotion::Limited;

                        NewConstraint->Swing1LimitAngle = 45.0f;
                        NewConstraint->Swing2LimitAngle = 45.0f;
                        NewConstraint->TwistLimitAngle = 45.0f;

                        // PhysicsAsset에 추가
                        PhysAsset->AddConstraintSetup(NewConstraint);

                        // Graph 동기화
                        Owner->SyncGraphFromPhysicsAsset();

                        UE_LOG("[PAE] Created constraint between '%s' and '%s'",
                               Body1->BoneName.ToString().c_str(),
                               Body2->BoneName.ToString().c_str());
                    }
                }
                else
                {
                    // 이미 존재하는 Constraint - 거부
                    ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                }
            }
            else
            {
                // 유효하지 않은 연결 - 거부
                ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
            }
        }
    }
    ed::EndCreate();

    // ============================================================================
    // Constraint 삭제 처리
    // ============================================================================
    if (ed::BeginDelete())
    {
        ed::LinkId deletedLinkId;
        while (ed::QueryDeletedLink(&deletedLinkId))
        {
            if (ed::AcceptDeletedItem())
            {
                // 삭제할 Constraint 찾기
                FPAEConstraintLink* Link = Owner->FindConstraintLink(deletedLinkId);
                if (Link && Link->ConstraintIndex >= 0 && Link->ConstraintIndex < PhysAsset->ConstraintSetups.Num())
                {
                    UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[Link->ConstraintIndex];
                    if (Constraint)
                    {
                        UE_LOG("[PAE] Deleting constraint between '%s' and '%s'",
                               Constraint->ConstraintBone1.ToString().c_str(),
                               Constraint->ConstraintBone2.ToString().c_str());

                        // Constraint 삭제
                        ObjectFactory::DeleteObject(Constraint);
                        PhysAsset->ConstraintSetups.erase(PhysAsset->ConstraintSetups.begin() + Link->ConstraintIndex);

                        // Graph 동기화
                        Owner->SyncGraphFromPhysicsAsset();
                    }
                }
            }
        }
    }
    ed::EndDelete();

    // ============================================================================
    // 노드 선택 처리
    // ============================================================================
    if (ed::GetSelectedObjectCount() > 0)
    {
        ed::NodeId selectedNodeId;
        if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0)
        {
            GraphState->SelectedNodeID = selectedNodeId;
            GraphState->SelectedLinkID = ed::LinkId::Invalid;

            // PhysicsAssetViewerState에 선택 반영
            FPAEBodyNode* SelectedBody = Owner->FindBodyNode(selectedNodeId);
            if (SelectedBody && SelectedBody->BodyIndex >= 0)
            {
                State->SelectBody(SelectedBody->BodyIndex);
            }
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

// ============================================================================
// Node Graph 헬퍼 함수들
// ============================================================================

void SPhysicsAssetEditorWindow::SyncGraphFromPhysicsAsset()
{
    if (!GraphState || !ActiveState || !ActiveState->PhysicsAsset) return;

    // 기존 데이터 클리어
    GraphState->Clear();
    GraphState->NextNodeID = 1;
    GraphState->NextPinID = 100000;
    GraphState->NextLinkID = 200000;

    ed::SetCurrentEditor(GraphState->Context);

    UPhysicsAsset* PhysAsset = ActiveState->PhysicsAsset;

    // Body 노드 생성
    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->BodySetups[i];
        if (!Setup) continue;

        FPAEBodyNode Node;
        Node.ID = GraphState->GetNextNodeId();
        Node.BodyIndex = i;
        Node.BoneName = Setup->BoneName.ToString();

        // 입력/출력 핀 생성
        Node.InputPins.push_back(GraphState->GetNextPinId());
        Node.OutputPins.push_back(GraphState->GetNextPinId());

        GraphState->BodyNodes.Add(Node);

        // 노드 위치 설정 (그리드 배치)
        int32 row = i / 4;
        int32 col = i % 4;
        ed::SetNodePosition(Node.ID, ImVec2(50.0f + col * 180.0f, 50.0f + row * 120.0f));
    }

    // Constraint 링크 생성
    for (int32 i = 0; i < PhysAsset->ConstraintSetups.Num(); ++i)
    {
        UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[i];
        if (!Constraint) continue;

        // Bone 이름으로 Body 노드 찾기
        FPAEBodyNode* Node1 = nullptr;
        FPAEBodyNode* Node2 = nullptr;

        for (auto& Node : GraphState->BodyNodes)
        {
            if (Node.BoneName == Constraint->ConstraintBone1.ToString())
                Node1 = &Node;
            if (Node.BoneName == Constraint->ConstraintBone2.ToString())
                Node2 = &Node;
        }

        if (Node1 && Node2 && !Node1->OutputPins.empty() && !Node2->InputPins.empty())
        {
            FPAEConstraintLink Link;
            Link.ID = ed::LinkId(GraphState->NextLinkID++);
            Link.StartPinID = Node1->OutputPins[0];
            Link.EndPinID = Node2->InputPins[0];
            Link.ConstraintIndex = i;

            GraphState->ConstraintLinks.Add(Link);
        }
    }

    ed::SetCurrentEditor(nullptr);
}

FPAEBodyNode* SPhysicsAssetEditorWindow::FindBodyNode(ed::NodeId NodeID)
{
    if (!GraphState) return nullptr;

    for (auto& Node : GraphState->BodyNodes)
    {
        if (Node.ID == NodeID) return &Node;
    }
    return nullptr;
}

FPAEBodyNode* SPhysicsAssetEditorWindow::FindBodyNodeByPin(ed::PinId PinID)
{
    if (!GraphState) return nullptr;

    for (auto& Node : GraphState->BodyNodes)
    {
        for (auto& Pin : Node.InputPins)
            if (Pin == PinID) return &Node;
        for (auto& Pin : Node.OutputPins)
            if (Pin == PinID) return &Node;
    }
    return nullptr;
}

FPAEConstraintLink* SPhysicsAssetEditorWindow::FindConstraintLink(ed::LinkId LinkID)
{
    if (!GraphState) return nullptr;

    for (auto& Link : GraphState->ConstraintLinks)
    {
        if (Link.ID == LinkID) return &Link;
    }
    return nullptr;
}
