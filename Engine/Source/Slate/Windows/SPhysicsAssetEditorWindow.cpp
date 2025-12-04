#include "pch.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Slate/Widgets/ViewportToolbarWidget.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerState.h"
#include "Source/Runtime/Engine/PhysicsAssetViewer/PhysicsAssetViewerBootstrap.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAssetUtils.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsConstraintSetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/ConstraintInstance.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/ClothComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
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

#include "StatsOverlayD2D.h"

// ============================================================================
// SPhysicsAssetEditorWindow
// ============================================================================

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    ViewportRect = FRect(0, 0, 0, 0);
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    if (ViewportToolbar)
    {
        delete ViewportToolbar;
        ViewportToolbar = nullptr;
    }

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
    LeftSplitter->SetLeftOrTop(BodyListPanel);
    LeftSplitter->SetRightOrBottom(GraphPanelWidget);
    LeftSplitter->SetSplitRatio(0.5f);
    LeftSplitter->LoadFromConfig("PAEWindow_Left");

    // 내부: Viewport(좌) | Properties(우)
    InnerSplitter = new SSplitterH();
    InnerSplitter->SetLeftOrTop(ViewportPanelWidget);
    InnerSplitter->SetRightOrBottom(PropertiesPanelWidget);
    InnerSplitter->SetSplitRatio(0.75f);  // Viewport 75%, Properties 25%
    InnerSplitter->LoadFromConfig("PAEWindow_Inner");

    // 메인: Left(좌) | Inner(우)
    MainSplitter = new SSplitterH();
    MainSplitter->SetLeftOrTop(LeftSplitter);
    MainSplitter->SetRightOrBottom(InnerSplitter);
    MainSplitter->SetSplitRatio(0.30f);  // Left 30%, Rest 70%
    MainSplitter->LoadFromConfig("PAEWindow_Main");

    // 스플리터 초기 Rect 설정
    MainSplitter->SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // ViewportToolbar 초기화
    ViewportToolbar = new SViewportToolbarWidget();
    ViewportToolbar->Initialize(InDevice);

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
        // LeftSplitter: BodyList(상) | Graph(하)
        if (LeftSplitter)
        {
            if (LeftSplitter->GetLeftOrTop())
            {
                BodyListRect = LeftSplitter->GetLeftOrTop()->GetRect();
            }
            if (LeftSplitter->GetRightOrBottom())
            {
                GraphRect = LeftSplitter->GetRightOrBottom()->GetRect();
            }
        }
        // InnerSplitter: Viewport(좌) | Properties(우)
        if (InnerSplitter)
        {
            if (InnerSplitter->GetLeftOrTop())
            {
                ViewportRect = InnerSplitter->GetLeftOrTop()->GetRect();
            }
            if (InnerSplitter->GetRightOrBottom())
            {
                PropertiesRect = InnerSplitter->GetRightOrBottom()->GetRect();
            }
        }
    }

    // 패널 윈도우들을 앞으로 가져오기 (DynamicEditorWindow 뒤에 가려지는 문제 해결)
    // Content Browser가 열려있으면 z-order 유지를 위해 front로 가져오지 않음
    if (!SLATE.IsContentBrowserVisible())
    {
        ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##PhysicsAssetViewport");
        ImGuiWindow* BodyListWin = ImGui::FindWindowByName("Bodies##PhysicsAssetBodyList");
        ImGuiWindow* PropertiesWin = ImGui::FindWindowByName("Properties##PhysicsAssetProperties");
        ImGuiWindow* GraphWin = ImGui::FindWindowByName("Constraint Graph##PhysicsAssetGraph");

        if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
        if (BodyListWin) ImGui::BringWindowToDisplayFront(BodyListWin);
        if (PropertiesWin) ImGui::BringWindowToDisplayFront(PropertiesWin);
        if (GraphWin) ImGui::BringWindowToDisplayFront(GraphWin);
    }

    // 팝업들도 패널 위로 가져오기
    ImGuiContext* g = ImGui::GetCurrentContext();
    if (g && g->OpenPopupStack.Size > 0)
    {
        for (int32 i = 0; i < g->OpenPopupStack.Size; ++i)
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

        // 기즈모 모드 전환 (Space 키)
        AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
        if (Gizmo)
        {
            Gizmo->ProcessGizmoModeSwitch();
        }
    }

    // Floor/Camera 설정 (메쉬 로드 완료 후 한 번만 실행)
    if (ActiveState->bNeedsFloorSetup && ActiveState->PreviewActor)
    {
        // 비동기 로드 완료 확인 (SkeletalMeshComponent가 실제 메쉬를 가지고 있는지)
        if (auto* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            if (SkelComp->GetSkeletalMesh())
            {
                PhysicsAssetViewerBootstrap::SetupFloorAndCamera(
                    ActiveState->PreviewActor,
                    ActiveState->FloorActor,
                    ActiveState->Client
                );
                ActiveState->bNeedsFloorSetup = false;
                ActiveState->LastSetupMeshPath = ActiveState->LoadedMeshPath;
            }
        }
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

        // ClothPaint 모드: 호버 정점 업데이트 및 드래그 페인팅
        if (ActiveState->EditMode == EPhysicsAssetEditMode::ClothPaint && ActiveState->Client)
        {
            ACameraActor* Camera = ActiveState->Client->GetCamera();
            if (Camera)
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

                // Cloth 정점 피킹 (호버)
                int32 HoverVertexIndex = -1;
                float HitDistance = 0.0f;
                ActiveState->PickClothVertex(Ray, HoverVertexIndex, HitDistance);
                ActiveState->SelectedClothVertexIndex = HoverVertexIndex;

                // 왼쪽 마우스 버튼이 눌려 있으면 브러시 적용 (드래그 페인팅)
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && HoverVertexIndex >= 0)
                {
                    // 호버된 정점 위치에서 브러시 적용
                    USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr;
                    UClothComponent* ClothComp = SkelComp ? SkelComp->GetInternalClothComponent() : nullptr;
                    if (ClothComp)
                    {
                        FVector BrushCenter = ClothComp->GetClothVertexPosition(HoverVertexIndex);
                        float PaintValue = ActiveState->bClothPaintEraseMode ? 1.0f : ActiveState->ClothPaintValue;
                        ActiveState->ApplyBrushToClothVertices(
                            BrushCenter,
                            ActiveState->ClothPaintBrushRadius,
                            ActiveState->ClothPaintBrushStrength,
                            ActiveState->ClothPaintBrushFalloff,
                            PaintValue
                        );
                    }
                }
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

    // Viewport 클릭 시 Node Editor 선택 해제 (입력 캡처 해제)
    if (bInViewport && GraphState && GraphState->Context)
    {
        ed::SetCurrentEditor(GraphState->Context);
        ed::ClearSelection();
        ed::SetCurrentEditor(nullptr);
    }

    if (bInViewport)
    {
        FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);

        // 왼쪽 마우스 버튼 클릭 시 처리
        if (Button == 0 && ActiveState->Client && !ActiveState->bSimulating)
        {
            ACameraActor* Camera = ActiveState->Client->GetCamera();
            if (Camera)
            {
                // ClothPaint 모드: 클릭 시 브러시 적용
                if (ActiveState->EditMode == EPhysicsAssetEditMode::ClothPaint)
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

                    int32 HitVertexIndex = -1;
                    float HitDistance = 0.0f;
                    if (ActiveState->PickClothVertex(Ray, HitVertexIndex, HitDistance) && HitVertexIndex >= 0)
                    {
                        USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr;
                        UClothComponent* ClothComp = SkelComp ? SkelComp->GetInternalClothComponent() : nullptr;
                        if (ClothComp)
                        {
                            FVector BrushCenter = ClothComp->GetClothVertexPosition(HitVertexIndex);
                            float PaintValue = ActiveState->bClothPaintEraseMode ? 1.0f : ActiveState->ClothPaintValue;
                            ActiveState->ApplyBrushToClothVertices(
                                BrushCenter,
                                ActiveState->ClothPaintBrushRadius,
                                ActiveState->ClothPaintBrushStrength,
                                ActiveState->ClothPaintBrushFalloff,
                                PaintValue
                            );
                        }
                    }
                }
                else
                {
                    // Body/Shape 피킹 (기존 모드들)
                    // 기즈모가 호버링/드래그 중이 아닐 때만 피킹 (기즈모 조작 시 선택이 풀리지 않도록)
                    AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
                    bool bGizmoInteracting = Gizmo && (Gizmo->GetbIsDragging() || Gizmo->GetbIsHovering());

                    if (!bGizmoInteracting)
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

                        // 항상 Constraint 먼저 피킹 시도 (Shape보다 작으므로 우선)
                        int32 HitConstraintIndex = -1;
                        float ConstraintHitDist = 0.0f;
                        bool bHitConstraint = ActiveState->PickConstraint(Ray, HitConstraintIndex, ConstraintHitDist);

                        // Shape/Body 피킹
                        bool bHitShape = ActiveState->PickBodyOrShape(Ray, HitBodyIndex, HitShapeType, HitShapeIndex, HitDistance);

                        // Constraint가 hit되면 무조건 Constraint 선택 (Shape보다 작으므로 우선)
                        if (bHitConstraint)
                        {
                            ActiveState->SelectConstraint(HitConstraintIndex);
                        }
                        else if (bHitShape)
                        {
                            // 뷰포트에서 Shape를 직접 클릭하면 항상 Shape 선택 (개별 Shape 편집 가능)
                            // EditMode는 SelectShape 내에서 Shape로 설정됨
                            ActiveState->SelectShape(HitBodyIndex, HitShapeType, HitShapeIndex);
                        }
                        else
                        {
                            // 빈 공간 클릭 - 선택 해제
                            ActiveState->ClearSelection();
                        }
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

        // 본 라인 비활성화 (PAE에서는 사용하지 않음)
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(false);
        }
    }

    // State 업데이트
    ActiveState->CurrentMesh = Mesh;
    ActiveState->LoadedMeshPath = Path;
    ActiveState->bNeedsFloorSetup = true;  // Floor/Camera 설정 필요 표시

    // 기존 Physics Asset이 있으면 유지, 없으면 새로 생성
    bool bNewPhysicsAsset = false;
    if (!ActiveState->PhysicsAsset)
    {
        // 새 Physics Asset 생성
        ActiveState->PhysicsAsset = NewObject<UPhysicsAsset>();
        ActiveState->PhysicsAsset->SourceSkeletalPath = Path;
        bNewPhysicsAsset = true;
    }

    // 새로 생성된 PhysicsAsset이면 자동으로 Bodies 생성
    if (bNewPhysicsAsset && ActiveState->PhysicsAsset && Mesh)
    {
        FPhysicsAssetUtils::CreateFromSkeletalMesh(ActiveState->PhysicsAsset, Mesh);
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
        UE_LOG("PAE: LoadAsset: Failed %s", Path.c_str());
        return;
    }

    // State 업데이트
    ActiveState->PhysicsAsset = PhysAsset;
    ActiveState->LoadedPhysicsAssetPath = Path;

    // PhysicsAsset의 ClothVertexWeights를 State에 복사
    if (!PhysAsset->ClothVertexWeights.empty())
    {
        if (!ActiveState->ClothVertexWeights)
        {
            ActiveState->ClothVertexWeights = std::make_unique<std::unordered_map<uint32, float>>();
        }
        ActiveState->ClothVertexWeights->clear();
        for (const auto& Pair : PhysAsset->ClothVertexWeights)
        {
            (*ActiveState->ClothVertexWeights)[Pair.first] = Pair.second;
        }
    }

    // SourceSkeletalPath가 있으면 SkeletalMesh도 로드
    if (!PhysAsset->SourceSkeletalPath.empty())
    {
        LoadSkeletalMesh(PhysAsset->SourceSkeletalPath);
    }

    // 선택 상태 초기화
    ActiveState->ClearSelection();

    UE_LOG("PAE: LoadAsset: %s", Path.c_str());
}

void SPhysicsAssetEditorWindow::SavePhysicsAsset(const FString& Path)
{
    if (Path.empty() || !ActiveState || !ActiveState->PhysicsAsset)
    {
        return;
    }

    // SourceSkeletalPath 업데이트 (상대 경로로 변환)
    if (ActiveState->CurrentMesh)
    {
        FString MeshPath = ActiveState->LoadedMeshPath;

        // 절대 경로를 상대 경로로 변환 ("Data/" 이후 부분만 사용)
        size_t DataPos = MeshPath.find("Data/");
        if (DataPos == FString::npos)
        {
            DataPos = MeshPath.find("Data\\");
        }
        if (DataPos != FString::npos)
        {
            MeshPath = MeshPath.substr(DataPos);
        }

        // 백슬래시를 슬래시로 변환
        std::replace(MeshPath.begin(), MeshPath.end(), '\\', '/');

        ActiveState->PhysicsAsset->SourceSkeletalPath = MeshPath;
    }

    // State의 ClothVertexWeights를 PhysicsAsset에 복사
    ActiveState->PhysicsAsset->ClothVertexWeights.clear();
    if (ActiveState->ClothVertexWeights)
    {
        for (const auto& Pair : *ActiveState->ClothVertexWeights)
        {
            ActiveState->PhysicsAsset->ClothVertexWeights[Pair.first] = Pair.second;
        }
    }

    if (ActiveState->PhysicsAsset->SaveToFile(Path))
    {
        ActiveState->LoadedPhysicsAssetPath = Path;
        UE_LOG("PAE: SaveAsset: %s", Path.c_str());
    }
    else
    {
        UE_LOG("PAE: SaveAsset: Failed %s", Path.c_str());
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
                // PhysicsAsset 폴더가 없으면 생성
                std::filesystem::path PhysicsAssetDir = "Data/PhysicsAsset";
                if (!std::filesystem::exists(PhysicsAssetDir))
                {
                    std::filesystem::create_directories(PhysicsAssetDir);
                }

                std::filesystem::path FilePath = FPlatformProcess::OpenLoadFileDialog(
                    L"Data/PhysicsAsset",
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
                // PhysicsAsset 폴더가 없으면 생성
                std::filesystem::path PhysicsAssetDir = "Data/PhysicsAsset";
                if (!std::filesystem::exists(PhysicsAssetDir))
                {
                    std::filesystem::create_directories(PhysicsAssetDir);
                }

                std::filesystem::path FilePath = FPlatformProcess::OpenSaveFileDialog(
                    L"Data/PhysicsAsset",
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

        if (ImGui::BeginMenu("Mode"))
        {
            if (ActiveState)
            {
                bool bIsBodyMode = (ActiveState->EditMode == EPhysicsAssetEditMode::Body);
                bool bIsConstraintMode = (ActiveState->EditMode == EPhysicsAssetEditMode::Constraint);
                bool bIsShapeMode = (ActiveState->EditMode == EPhysicsAssetEditMode::Shape);
                bool bIsClothPaintMode = (ActiveState->EditMode == EPhysicsAssetEditMode::ClothPaint);

                if (ImGui::MenuItem("Body Mode", nullptr, bIsBodyMode))
                {
                    ActiveState->EditMode = EPhysicsAssetEditMode::Body;
                }
                if (ImGui::MenuItem("Constraint Mode", nullptr, bIsConstraintMode))
                {
                    ActiveState->EditMode = EPhysicsAssetEditMode::Constraint;
                }
                if (ImGui::MenuItem("Shape Mode", nullptr, bIsShapeMode))
                {
                    ActiveState->EditMode = EPhysicsAssetEditMode::Shape;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cloth Paint Mode", nullptr, bIsClothPaintMode))
                {
                    ActiveState->EditMode = EPhysicsAssetEditMode::ClothPaint;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ActiveState)
            {
                // Show Mesh 체크박스 - 변경 시 즉시 적용
                if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
                {
                    if (ActiveState->PreviewActor)
                    {
                        if (auto* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                        {
                            SkelComp->SetVisibility(ActiveState->bShowMesh);
                        }
                    }
                }
                // Show Cloth 체크박스 - 변경 시 즉시 적용
                if (ImGui::Checkbox("Show Cloth", &ActiveState->bShowCloth))
                {
                    if (ActiveState->PreviewActor)
                    {
                        if (auto* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                        {
                            if (auto* ClothComp = SkelComp->GetInternalClothComponent())
                            {
                                ClothComp->SetVisibility(ActiveState->bShowCloth);
                            }
                        }
                    }
                }
                ImGui::Checkbox("Show Bodies", &ActiveState->bShowBodies);
                ImGui::Checkbox("Show Constraints", &ActiveState->bShowConstraints);
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

        // PhysicsAsset Stats를 StatsOverlayD2D로 전달 (독립 실행 모드)
        if (ActiveState && ActiveState->PhysicsAsset)
        {
            UPhysicsAsset* PhysAsset = ActiveState->PhysicsAsset;
            int32 NumBodies = static_cast<int32>(PhysAsset->BodySetups.size());
            int32 NumConstraints = static_cast<int32>(PhysAsset->ConstraintSetups.size());

            int32 NumShapes = 0;
            for (UBodySetup* Body : PhysAsset->BodySetups)
            {
                if (Body)
                {
                    NumShapes += Body->GetElementCount();
                }
            }

            const char* EditModeStr = "Body";
            switch (ActiveState->EditMode)
            {
            case EPhysicsAssetEditMode::Body:      EditModeStr = "Body"; break;
            case EPhysicsAssetEditMode::Constraint: EditModeStr = "Constraint"; break;
            case EPhysicsAssetEditMode::Shape:     EditModeStr = "Shape"; break;
            case EPhysicsAssetEditMode::ClothPaint: EditModeStr = "ClothPaint"; break;
            }

            UStatsOverlayD2D::Get().SetPhysicsAssetStats(NumBodies, NumConstraints, NumShapes, EditModeStr, ActiveState->bSimulating);
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

    // ========================================================
    // CRITICAL FIX: Physics State 재생성으로 런타임 포즈 기반 Joint 위치 계산
    // ========================================================
    // PAE의 "Generate Ragdoll"은 BindPose(T-Pose) 기준으로 Joint 위치 계산
    // PIE의 CreateConstraints()는 CurrentComponentSpacePose(런타임 포즈) 기준으로 재계산
    // 이 차이로 인해 PAE 시뮬레이션이 제대로 동작하지 않았음
    //
    // 해결: 기존 Physics State를 파괴하고 다시 생성하여
    // OnCreatePhysicsState() → BeginPlay() → CreateConstraints() 흐름 실행
    // CreateConstraints()에서 런타임 포즈 기반으로 Joint 위치 재계산됨
    // ========================================================

    // 1. 기존 Physics State 파괴 (Bodies + Constraints 모두 해제)
    SkelComp->OnDestroyPhysicsState();

    // 2. Physics State 재생성 (Bodies 생성)
    SkelComp->OnCreatePhysicsState();

    // 3. Constraints 생성 (런타임 포즈 기반으로 Joint 위치 재계산)
    //    BeginPlay()가 이미 호출되었으므로 직접 CreateConstraints() 호출
    SkelComp->CreateConstraints();

    // 4. 캐릭터 시뮬레이션 시작 (Kinematic → Dynamic 전환)
    // 주의: SetSimulatePhysics()가 아닌 SetAllBodiesSimulatePhysics() 사용!
    // SetSimulatePhysics()는 RecreatePhysicsBody()를 호출하여 Bodies를 다시 생성함
    // SetAllBodiesSimulatePhysics()는 기존 Bodies의 Kinematic 플래그만 변경
    SkelComp->SetAllBodiesSimulatePhysics(true);

    ActiveState->bSimulating = true;
    ActiveState->bSimulationPaused = false;

    UE_LOG("PAE: StartSimulation: Started");
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
            // SetAllBodiesSimulatePhysics(false)로 Kinematic 복원
            SkelComp->SetAllBodiesSimulatePhysics(false);

            // Physics State 파괴 (Bodies + Constraints 해제)
            SkelComp->OnDestroyPhysicsState();

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

    UE_LOG("PAE: StopSimulation: Stopped");
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

    // ClothPaint 모드: Weight 시각화
    if (ActiveState->EditMode == EPhysicsAssetEditMode::ClothPaint && ActiveState->bShowClothWeightVisualization)
    {
        Renderer->BeginLineBatch();
        ActiveState->DrawClothWeights(Renderer);
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

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Begin("##PhysicsAssetViewport", nullptr, WindowFlags))
    {
        // Viewport Toolbar 렌더링
        SViewportToolbarWidget* Toolbar = Owner->GetViewportToolbar();
        if (Toolbar)
        {
            AGizmoActor* GizmoActor = nullptr;
            if (State->Client && State->Client->GetWorld())
            {
                GizmoActor = State->Client->GetWorld()->GetGizmoActor();
            }
            Toolbar->Render(State->Client, GizmoActor, false);
        }

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

                // --- PhysicsAsset Stats를 StatsOverlayD2D로 전달 ---
                UPhysicsAsset* PhysAsset = State->PhysicsAsset;
                int32 NumBodies = PhysAsset ? static_cast<int32>(PhysAsset->BodySetups.size()) : 0;
                int32 NumConstraints = PhysAsset ? static_cast<int32>(PhysAsset->ConstraintSetups.size()) : 0;

                // 총 Shape 수 계산
                int32 NumShapes = 0;
                if (PhysAsset)
                {
                    for (UBodySetup* Body : PhysAsset->BodySetups)
                    {
                        if (Body)
                        {
                            NumShapes += Body->GetElementCount();
                        }
                    }
                }

                // Edit Mode 문자열
                const char* EditModeStr = "Body";
                switch (State->EditMode)
                {
                case EPhysicsAssetEditMode::Body:      EditModeStr = "Body"; break;
                case EPhysicsAssetEditMode::Constraint: EditModeStr = "Constraint"; break;
                case EPhysicsAssetEditMode::Shape:     EditModeStr = "Shape"; break;
                case EPhysicsAssetEditMode::ClothPaint: EditModeStr = "ClothPaint"; break;
                }

                UStatsOverlayD2D::Get().SetPhysicsAssetStats(NumBodies, NumConstraints, NumShapes, EditModeStr, State->bSimulating);
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
    ImGui::PopStyleColor();
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

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

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

        // 선택 상태 확인 (Body 모드 또는 Shape 모드에서 해당 Body가 선택된 경우)
        bool bSelected = false;
        if ((State->EditMode == EPhysicsAssetEditMode::Body || State->EditMode == EPhysicsAssetEditMode::Shape) &&
            bHasBody && State->SelectedBodyIndex == BodyIndex)
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
                // Body가 없는 bone 클릭 시: 선택만 초기화, 필터링 상태 유지
                int32 savedFilter = State->GraphFilterRootBodyIndex;
                State->ClearSelection();
                State->GraphFilterRootBodyIndex = savedFilter;  // 필터링 유지
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

                            UE_LOG("PAE: CreateConstraint: %s <-> %s",
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

        // Body 모드 또는 Shape 모드에서 해당 Body가 선택된 경우 하이라이트
        bool bSelected = ((State->EditMode == EPhysicsAssetEditMode::Body || State->EditMode == EPhysicsAssetEditMode::Shape) &&
                          State->SelectedBodyIndex == i);
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
            State->GraphFilterRootBodyIndex = BodyIndex;  // Shape 선택 시에도 해당 Body로 필터링
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
            State->GraphFilterRootBodyIndex = BodyIndex;  // Shape 선택 시에도 해당 Body로 필터링
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
            State->GraphFilterRootBodyIndex = BodyIndex;  // Shape 선택 시에도 해당 Body로 필터링
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

    UPhysicsAsset* PhysAsset = State->PhysicsAsset;

    // ===== Add Constraint 버튼 =====
    if (PhysAsset && PhysAsset->BodySetups.Num() >= 2)
    {
        if (ImGui::Button("+ Add Constraint", ImVec2(-1, 0)))
        {
            ImGui::OpenPopup("AddConstraintPopup");
        }

        // Add Constraint 팝업
        if (ImGui::BeginPopup("AddConstraintPopup"))
        {
            static int SelectedBody1 = 0;
            static int SelectedBody2 = 1;

            ImGui::Text("Create New Constraint");
            ImGui::Separator();

            // Body 1 선택
            if (ImGui::BeginCombo("Body 1", PhysAsset->BodySetups[SelectedBody1]->BoneName.ToString().c_str()))
            {
                for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
                {
                    UBodySetup* Setup = PhysAsset->BodySetups[i];
                    if (!Setup) continue;
                    bool bSelected = (SelectedBody1 == i);
                    if (ImGui::Selectable(Setup->BoneName.ToString().c_str(), bSelected))
                    {
                        SelectedBody1 = i;
                    }
                }
                ImGui::EndCombo();
            }

            // Body 2 선택
            if (ImGui::BeginCombo("Body 2", PhysAsset->BodySetups[SelectedBody2]->BoneName.ToString().c_str()))
            {
                for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
                {
                    UBodySetup* Setup = PhysAsset->BodySetups[i];
                    if (!Setup) continue;
                    bool bSelected = (SelectedBody2 == i);
                    if (ImGui::Selectable(Setup->BoneName.ToString().c_str(), bSelected))
                    {
                        SelectedBody2 = i;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();

            // 생성 버튼
            bool bCanCreate = (SelectedBody1 != SelectedBody2);
            if (!bCanCreate)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                UBodySetup* Body1 = PhysAsset->BodySetups[SelectedBody1];
                UBodySetup* Body2 = PhysAsset->BodySetups[SelectedBody2];

                // 새 Constraint 생성
                UPhysicsConstraintSetup* NewConstraint = NewObject<UPhysicsConstraintSetup>();
                NewConstraint->ConstraintBone1 = Body1->BoneName;
                NewConstraint->ConstraintBone2 = Body2->BoneName;
                NewConstraint->BodyIndex1 = SelectedBody1;
                NewConstraint->BodyIndex2 = SelectedBody2;

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

                // Graph 동기화
                if (Owner)
                {
                    Owner->SyncGraphFromPhysicsAsset();
                }

                UE_LOG("PAE: CreateConstraint: %s <-> %s",
                       Body1->BoneName.ToString().c_str(),
                       Body2->BoneName.ToString().c_str());

                ImGui::CloseCurrentPopup();
            }

            if (!bCanCreate)
            {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Separator();
    }

    // PhysicsAsset이 없거나 Constraint가 없으면 안내 메시지
    if (!PhysAsset || PhysAsset->ConstraintSetups.IsEmpty())
    {
        ImGui::TextDisabled("No constraints");
        ImGui::TextDisabled("Use 'Generate Ragdoll' or '+ Add Constraint'");
        return;
    }

    // Constraint 목록 렌더링
    int32 ConstraintToDelete = -1;  // 삭제 예약 인덱스

    for (int32 i = 0; i < PhysAsset->ConstraintSetups.Num(); ++i)
    {
        UPhysicsConstraintSetup* Setup = PhysAsset->ConstraintSetups[i];
        if (!Setup) continue;

        ImGui::PushID(i);

        char label[256];
        sprintf_s(label, "[C%d] %s <-> %s",
            i,
            Setup->ConstraintBone1.ToString().c_str(),
            Setup->ConstraintBone2.ToString().c_str());

        bool bSelected = (State->SelectedConstraintIndex == i);
        if (ImGui::Selectable(label, bSelected))
        {
            // SelectConstraint 호출로 기즈모 설정
            State->SelectConstraint(i);
            // Parent Body(BodyIndex1)로 필터링 설정
            State->GraphFilterRootBodyIndex = Setup->BodyIndex1;
        }

        // 우클릭 컨텍스트 메뉴
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Constraint"))
            {
                ConstraintToDelete = i;
            }
            ImGui::EndPopup();
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

        ImGui::PopID();
    }

    // 삭제 처리 (루프 후에 처리하여 iterator 무효화 방지)
    if (ConstraintToDelete >= 0)
    {
        PhysAsset->ConstraintSetups.RemoveAt(ConstraintToDelete);
        State->ClearSelection();

        // Graph 동기화
        if (Owner)
        {
            Owner->SyncGraphFromPhysicsAsset();
        }

        UE_LOG("PAE: DeleteConstraint: %d", ConstraintToDelete);
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

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

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
            // Edit Mode 선택 콤보박스
            {
                const char* ModeNames[] = { "Body", "Constraint", "Shape", "Cloth Paint" };
                int CurrentMode = static_cast<int>(State->EditMode);
                ImGui::Text("Edit Mode:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##EditMode", &CurrentMode, ModeNames, IM_ARRAYSIZE(ModeNames)))
                {
                    State->EditMode = static_cast<EPhysicsAssetEditMode>(CurrentMode);
                }
            }
            ImGui::Separator();

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
                            UE_LOG("PAE: GenerateBodies: %d bodies", State->PhysicsAsset->BodySetups.Num());

                            // 체크박스가 켜져 있으면 Constraints도 함께 생성
                            UE_LOG("PAE: GenerateBodies: bAlsoGenerateConstraints=%s", bAlsoGenerateConstraints ? "true" : "false");
                            if (bAlsoGenerateConstraints)
                            {
                                State->PhysicsAsset->ConstraintSetups.Empty();
                                FPhysicsAssetUtils::CreateConstraintsForRagdoll(State->PhysicsAsset, State->CurrentMesh);
                                UE_LOG("PAE: GenerateConstraints: %d constraints", State->PhysicsAsset->ConstraintSetups.Num());
                            }
                            else
                            {
                                UE_LOG("PAE: GenerateBodies: Skipping constraint generation");
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

                // Clear All 버튼 (Bodies + Constraints 모두 삭제)
                bool bHasAnything = State->PhysicsAsset &&
                    (State->PhysicsAsset->BodySetups.Num() > 0 || State->PhysicsAsset->ConstraintSetups.Num() > 0);
                if (!bHasAnything)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("Clear All", ImVec2(-1, 0)))
                {
                    if (State->PhysicsAsset)
                    {
                        // Bodies 삭제
                        State->PhysicsAsset->BodySetups.Empty();
                        State->PhysicsAsset->BoneNameToBodyIndex.clear();

                        // Constraints도 함께 삭제 (Bodies 없으면 의미 없음)
                        State->PhysicsAsset->ConstraintSetups.Empty();

                        State->ClearSelection();

                        // Graph 동기화
                        Owner->SyncGraphFromPhysicsAsset();
                    }
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear all bodies and constraints");
                }

                if (!bHasAnything)
                {
                    ImGui::EndDisabled();
                }

                // Generate Ragdoll 버튼
                bool bHasBodies = State->PhysicsAsset && State->PhysicsAsset->BodySetups.Num() > 0;
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
                        UE_LOG("PAE: GenerateRagdoll: %d constraints", State->PhysicsAsset->ConstraintSetups.Num());

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
                // Show Mesh 체크박스 - 변경 시 즉시 적용
                if (ImGui::Checkbox("Show Mesh", &State->bShowMesh))
                {
                    if (State->PreviewActor)
                    {
                        if (auto* SkelComp = State->PreviewActor->GetSkeletalMeshComponent())
                        {
                            SkelComp->SetVisibility(State->bShowMesh);
                        }
                    }
                }
                // Show Cloth 체크박스 - 변경 시 즉시 적용
                if (ImGui::Checkbox("Show Cloth", &State->bShowCloth))
                {
                    if (State->PreviewActor)
                    {
                        if (auto* SkelComp = State->PreviewActor->GetSkeletalMeshComponent())
                        {
                            if (auto* ClothComp = SkelComp->GetInternalClothComponent())
                            {
                                ClothComp->SetVisibility(State->bShowCloth);
                            }
                        }
                    }
                }
                ImGui::Checkbox("Show Bodies", &State->bShowBodies);
                ImGui::Checkbox("Show Constraints", &State->bShowConstraints);
            }

            ImGui::Separator();

            // Selected item properties
            if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // EditMode에 따라 다른 Properties 표시
                if (State->EditMode == EPhysicsAssetEditMode::ClothPaint)
                {
                    RenderClothPaintProperties(State);
                }
                else if (State->EditMode == EPhysicsAssetEditMode::Constraint)
                {
                    RenderConstraintProperties(State);
                }
                else
                {
                    RenderBodyProperties(State);
                }
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
    if (!State || !State->PhysicsAsset)
    {
        ImGui::TextDisabled("(Select a constraint)");
        return;
    }

    if (State->SelectedConstraintIndex < 0 || State->SelectedConstraintIndex >= State->PhysicsAsset->ConstraintSetups.Num())
    {
        ImGui::TextDisabled("(Select a constraint to view properties)");
        return;
    }

    UPhysicsConstraintSetup* Constraint = State->PhysicsAsset->ConstraintSetups[State->SelectedConstraintIndex];
    if (!Constraint)
    {
        ImGui::TextDisabled("(Invalid constraint)");
        return;
    }

    // 런타임 ConstraintInstance 찾기 (시뮬레이션 모드일 때만 유효)
    FConstraintInstance* ConstraintInst = nullptr;
    if (State->PreviewActor && State->bSimulating)
    {
        USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
        if (SkelComp && State->SelectedConstraintIndex < SkelComp->Constraints.Num())
        {
            ConstraintInst = SkelComp->Constraints[State->SelectedConstraintIndex];
        }
    }

    // Constraint 정보 표시
    ImGui::Text("Constraint %d", State->SelectedConstraintIndex);
    if (ConstraintInst && ConstraintInst->IsValid())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[Live]");
    }
    ImGui::Separator();

    ImGui::Text("Body 1: %s", Constraint->ConstraintBone1.ToString().c_str());
    ImGui::Text("Body 2: %s", Constraint->ConstraintBone2.ToString().c_str());
    ImGui::Separator();

    // Angular Limits
    if (ImGui::CollapsingHeader("Angular Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool bAngularChanged = false;

        // Swing1 (Y축)
        ImGui::Text("Swing 1 (Y-axis)");
        int Swing1Motion = (int)Constraint->Swing1Motion;
        const char* MotionTypes[] = { "Free", "Limited", "Locked" };
        if (ImGui::Combo("Swing1 Motion##S1", &Swing1Motion, MotionTypes, 3))
        {
            Constraint->Swing1Motion = (EAngularConstraintMotion)Swing1Motion;
            bAngularChanged = true;
        }
        if (Constraint->Swing1Motion == EAngularConstraintMotion::Limited)
        {
            if (ImGui::DragFloat("Swing1 Limit##S1Angle", &Constraint->Swing1LimitAngle, 1.0f, 0.0f, 180.0f, "%.1f deg"))
            {
                bAngularChanged = true;
            }
        }
        ImGui::Spacing();

        // Swing2 (Z축)
        ImGui::Text("Swing 2 (Z-axis)");
        int Swing2Motion = (int)Constraint->Swing2Motion;
        if (ImGui::Combo("Swing2 Motion##S2", &Swing2Motion, MotionTypes, 3))
        {
            Constraint->Swing2Motion = (EAngularConstraintMotion)Swing2Motion;
            bAngularChanged = true;
        }
        if (Constraint->Swing2Motion == EAngularConstraintMotion::Limited)
        {
            if (ImGui::DragFloat("Swing2 Limit##S2Angle", &Constraint->Swing2LimitAngle, 1.0f, 0.0f, 180.0f, "%.1f deg"))
            {
                bAngularChanged = true;
            }
        }
        ImGui::Spacing();

        // Twist (X축)
        ImGui::Text("Twist (X-axis)");
        int TwistMotion = (int)Constraint->TwistMotion;
        if (ImGui::Combo("Twist Motion##T", &TwistMotion, MotionTypes, 3))
        {
            Constraint->TwistMotion = (EAngularConstraintMotion)TwistMotion;
            bAngularChanged = true;
        }
        if (Constraint->TwistMotion == EAngularConstraintMotion::Limited)
        {
            if (ImGui::DragFloat("Twist Limit##TAngle", &Constraint->TwistLimitAngle, 1.0f, 0.0f, 180.0f, "%.1f deg"))
            {
                bAngularChanged = true;
            }
        }

        // 실시간 업데이트
        if (bAngularChanged && ConstraintInst && ConstraintInst->IsValid())
        {
            ConstraintInst->SetAngularLimits(Constraint);
        }
    }

    // Linear Limits
    if (ImGui::CollapsingHeader("Linear Limits"))
    {
        bool bLinearChanged = false;
        const char* LinearMotionTypes[] = { "Free", "Limited", "Locked" };

        int LinearXMotion = (int)Constraint->LinearXMotion;
        if (ImGui::Combo("X Motion##LX", &LinearXMotion, LinearMotionTypes, 3))
        {
            Constraint->LinearXMotion = (ELinearConstraintMotion)LinearXMotion;
            bLinearChanged = true;
        }

        int LinearYMotion = (int)Constraint->LinearYMotion;
        if (ImGui::Combo("Y Motion##LY", &LinearYMotion, LinearMotionTypes, 3))
        {
            Constraint->LinearYMotion = (ELinearConstraintMotion)LinearYMotion;
            bLinearChanged = true;
        }

        int LinearZMotion = (int)Constraint->LinearZMotion;
        if (ImGui::Combo("Z Motion##LZ", &LinearZMotion, LinearMotionTypes, 3))
        {
            Constraint->LinearZMotion = (ELinearConstraintMotion)LinearZMotion;
            bLinearChanged = true;
        }

        // Linear Limit 값 (하나라도 Limited면 표시)
        if (Constraint->LinearXMotion == ELinearConstraintMotion::Limited ||
            Constraint->LinearYMotion == ELinearConstraintMotion::Limited ||
            Constraint->LinearZMotion == ELinearConstraintMotion::Limited)
        {
            if (ImGui::DragFloat("Linear Limit##LL", &Constraint->LinearLimit, 0.1f, 0.0f, 1000.0f, "%.2f cm"))
            {
                bLinearChanged = true;
            }
        }

        // 실시간 업데이트
        if (bLinearChanged && ConstraintInst && ConstraintInst->IsValid())
        {
            ConstraintInst->SetLinearLimits(
                Constraint->LinearXMotion, Constraint->LinearYMotion,
                Constraint->LinearZMotion, Constraint->LinearLimit);
        }
    }

    // Soft Limits
    if (ImGui::CollapsingHeader("Soft Limits"))
    {
        bool bSoftSwingChanged = false;
        bool bSoftTwistChanged = false;

        if (ImGui::Checkbox("Soft Swing Limit", &Constraint->bSoftSwingLimit))
        {
            bSoftSwingChanged = true;
        }
        if (Constraint->bSoftSwingLimit)
        {
            if (ImGui::DragFloat("Swing Stiffness", &Constraint->SwingStiffness, 1.0f, 0.0f, 10000.0f))
                bSoftSwingChanged = true;
            if (ImGui::DragFloat("Swing Damping", &Constraint->SwingDamping, 1.0f, 0.0f, 10000.0f))
                bSoftSwingChanged = true;
        }

        if (ImGui::Checkbox("Soft Twist Limit", &Constraint->bSoftTwistLimit))
        {
            bSoftTwistChanged = true;
        }
        if (Constraint->bSoftTwistLimit)
        {
            if (ImGui::DragFloat("Twist Stiffness", &Constraint->TwistStiffness, 1.0f, 0.0f, 10000.0f))
                bSoftTwistChanged = true;
            if (ImGui::DragFloat("Twist Damping", &Constraint->TwistDamping, 1.0f, 0.0f, 10000.0f))
                bSoftTwistChanged = true;
        }

        // 실시간 업데이트
        if (ConstraintInst && ConstraintInst->IsValid())
        {
            if (bSoftSwingChanged)
                ConstraintInst->SetSoftSwingLimit(Constraint->bSoftSwingLimit, Constraint->SwingStiffness, Constraint->SwingDamping);
            if (bSoftTwistChanged)
                ConstraintInst->SetSoftTwistLimit(Constraint->bSoftTwistLimit, Constraint->TwistStiffness, Constraint->TwistDamping);
        }
    }

    // Break Settings
    if (ImGui::CollapsingHeader("Break Settings"))
    {
        bool bLinearBreakChanged = false;
        bool bAngularBreakChanged = false;

        if (ImGui::Checkbox("Linear Breakable", &Constraint->bLinearBreakable))
        {
            bLinearBreakChanged = true;
        }
        if (Constraint->bLinearBreakable)
        {
            if (ImGui::DragFloat("Linear Break Threshold", &Constraint->LinearBreakThreshold, 10.0f, 0.0f, 100000.0f, "%.1f"))
                bLinearBreakChanged = true;
        }

        if (ImGui::Checkbox("Angular Breakable", &Constraint->bAngularBreakable))
        {
            bAngularBreakChanged = true;
        }
        if (Constraint->bAngularBreakable)
        {
            if (ImGui::DragFloat("Angular Break Threshold", &Constraint->AngularBreakThreshold, 10.0f, 0.0f, 100000.0f, "%.1f"))
                bAngularBreakChanged = true;
        }

        // 실시간 업데이트
        if (ConstraintInst && ConstraintInst->IsValid())
        {
            if (bLinearBreakChanged)
                ConstraintInst->SetLinearBreakThreshold(Constraint->bLinearBreakable, Constraint->LinearBreakThreshold);
            if (bAngularBreakChanged)
                ConstraintInst->SetAngularBreakThreshold(Constraint->bAngularBreakable, Constraint->AngularBreakThreshold);
        }
    }
}

void SPhysicsAssetPropertiesPanel::RenderClothPaintProperties(PhysicsAssetViewerState* State)
{
    if (!State)
    {
        ImGui::TextDisabled("(No state)");
        return;
    }

    ImGui::Text("Cloth Paint Mode");
    ImGui::Separator();

    // Brush Settings
    if (ImGui::CollapsingHeader("Brush Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Brush Radius", &State->ClothPaintBrushRadius, 0.5f, 1.0f, 100.0f, "%.1f cm");
        ImGui::DragFloat("Brush Strength", &State->ClothPaintBrushStrength, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Brush Falloff", &State->ClothPaintBrushFalloff, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();

        // Paint Value (0 = Fixed, 1 = Free)
        ImGui::Text("Paint Value (0=Fixed, 1=Free)");
        ImGui::DragFloat("##PaintValue", &State->ClothPaintValue, 0.01f, 0.0f, 1.0f, "%.2f");

        // Quick buttons for common values
        if (ImGui::Button("Fixed (0.0)"))
        {
            State->ClothPaintValue = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Free (1.0)"))
        {
            State->ClothPaintValue = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Half (0.5)"))
        {
            State->ClothPaintValue = 0.5f;
        }

        ImGui::Separator();

        ImGui::Checkbox("Erase Mode (Set to 1.0)", &State->bClothPaintEraseMode);
    }

    // Visualization Settings
    if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Weight Visualization", &State->bShowClothWeightVisualization);

        ImGui::Separator();
        ImGui::Text("Color Legend:");
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Red = Fixed (0.0)");
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Green = Free (1.0)");
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Yellow = Partial");
    }

    // Cloth Info
    if (ImGui::CollapsingHeader("Cloth Info"))
    {
        if (State->PreviewActor)
        {
            USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
            UClothComponent* ClothComp = SkelComp ? SkelComp->GetInternalClothComponent() : nullptr;
            if (ClothComp)
            {
                ImGui::Text("Cloth Vertices: %d", ClothComp->GetClothVertexCount());
                ImGui::Text("Weights Initialized: %s", ClothComp->GetVertexWeights().Num() > 0 ? "Yes" : "No");
            }
            else
            {
                ImGui::TextDisabled("(No cloth component)");
            }
        }
        else
        {
            ImGui::TextDisabled("(No preview actor)");
        }
    }

    // Actions
    ImGui::Separator();

    // Get cloth component
    USkeletalMeshComponent* SkelComp = State->PreviewActor ? State->PreviewActor->GetSkeletalMeshComponent() : nullptr;
    UClothComponent* ClothComp = SkelComp ? SkelComp->GetInternalClothComponent() : nullptr;

    if (ImGui::Button("Initialize Weights"))
    {
        if (ClothComp)
        {
            ClothComp->InitializeVertexWeights();
            UE_LOG("ClothPaint: InitWeights: %d vertices", ClothComp->GetClothVertexCount());
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Initialize weight array from current simulation state");
    }

    if (ImGui::Button("Apply Weights to Simulation"))
    {
        if (ClothComp)
        {
            ClothComp->ApplyPaintedWeights();
            UE_LOG("ClothPaint: ApplyWeights: Applied");
        }
    }

    if (ImGui::Button("Reset All Weights to 1.0"))
    {
        if (ClothComp)
        {
            int32 VertexCount = ClothComp->GetClothVertexCount();
            for (int32 i = 0; i < VertexCount; ++i)
            {
                ClothComp->SetVertexWeight(i, 1.0f);
            }
            ClothComp->ApplyPaintedWeights();
            UE_LOG("ClothPaint: ResetWeights: %d to 1.0", VertexCount);
        }
    }

    if (ImGui::Button("Set All Weights to 0.0 (Fixed)"))
    {
        if (ClothComp)
        {
            int32 VertexCount = ClothComp->GetClothVertexCount();
            for (int32 i = 0; i < VertexCount; ++i)
            {
                ClothComp->SetVertexWeight(i, 0.0f);
            }
            ClothComp->ApplyPaintedWeights();
            UE_LOG("ClothPaint: SetWeights: %d to 0.0", VertexCount);
        }
    }
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

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    if (ImGui::Begin("Constraint Graph##PhysicsAssetGraph", nullptr, WindowFlags))
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
    ImGui::PopStyleColor();
}

// 노드 그리기 헬퍼 함수
static void DrawBodyNode(FPAEBodyNode& BodyNode, PhysicsAssetViewerState* State, bool bIsSelected, bool bIsCenter, bool bShowPins = true)
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

    // 핀 영역 (필터링된 그래프에서는 표시 안 함)
    if (bShowPins)
    {
        ImGui::Dummy(ImVec2(0, 4));

        ImGui::BeginGroup();
        {
            // 입력 핀 (왼쪽)
            if (BodyNode.InputPin.Get() != 0)
            {
                ed::BeginPin(BodyNode.InputPin, ed::PinKind::Input);
                ImGui::TextDisabled(">");
                ed::EndPin();
            }

            // 공간 확보
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(60, 0));
            ImGui::SameLine();

            // 출력 핀 (오른쪽)
            if (BodyNode.OutputPin.Get() != 0)
            {
                ed::BeginPin(BodyNode.OutputPin, ed::PinKind::Output);
                ImGui::TextDisabled(">");
                ed::EndPin();
            }
        }
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0, 2));
    }
    else
    {
        ImGui::Dummy(ImVec2(0, 2));
    }
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

// Row용 Body 노드 그리기 헬퍼 함수 (Unreal 스타일 3-Column 레이아웃용)
static void DrawRowBodyNode(ed::NodeId NodeID, ed::PinId OutputPin, ed::PinId InputPin,
                           int32 BodyIndex, const FString& BoneName,
                           PhysicsAssetViewerState* State, bool bIsParent)
{
    bool bIsSelected = (BodyIndex == State->SelectedBodyIndex);

    // 노드 색상
    ImU32 HeaderColor;
    if (bIsSelected)
        HeaderColor = IM_COL32(70, 130, 180, 255);  // 파란색 (선택됨)
    else
        HeaderColor = IM_COL32(60, 60, 70, 255);    // 회색

    ed::BeginNode(NodeID);
    ImGui::PushID(static_cast<int>(NodeID.Get()));

    // 헤더
    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::Indent(4.0f);
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", BoneName.c_str());

        // Shape 수 표시
        if (BodyIndex >= 0 && BodyIndex < State->PhysicsAsset->BodySetups.Num())
        {
            UBodySetup* Setup = State->PhysicsAsset->BodySetups[BodyIndex];
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

    ImRect HeaderRect;
    HeaderRect.Min = ImGui::GetItemRectMin();
    HeaderRect.Max = ImGui::GetItemRectMax();

    float minWidth = 100.0f;
    if (HeaderRect.GetWidth() < minWidth)
    {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(minWidth - HeaderRect.GetWidth(), 0));
    }

    // 핀 영역 (Parent는 오른쪽 출력만, Child는 왼쪽 입력만)
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::BeginGroup();
    {
        if (bIsParent)
        {
            // Parent Body: 오른쪽에 출력 핀만
            ImGui::Dummy(ImVec2(70, 0));
            ImGui::SameLine();
            ed::BeginPin(OutputPin, ed::PinKind::Output);
            ImGui::TextDisabled(">");
            ed::EndPin();
        }
        else
        {
            // Child Body: 왼쪽에 입력 핀만
            ed::BeginPin(InputPin, ed::PinKind::Input);
            ImGui::TextDisabled(">");
            ed::EndPin();
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(70, 0));
        }
    }
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::PopID();
    ed::EndNode();

    // 배경 그리기
    if (ImGui::IsItemVisible())
    {
        ImDrawList* drawList = ed::GetNodeBackgroundDrawList(NodeID);
        ImVec2 nodeMin = ed::GetNodePosition(NodeID);
        ImVec2 nodeSize = ed::GetNodeSize(NodeID);
        ImVec2 nodeMax = ImVec2(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);

        float headerHeight = HeaderRect.GetHeight() + 4;
        ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);

        drawList->AddRectFilled(nodeMin, headerMax, HeaderColor,
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
        drawList->AddRectFilled(ImVec2(nodeMin.x, headerMax.y), nodeMax,
            IM_COL32(30, 30, 35, 230),
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersBottom);

        if (bIsSelected)
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(100, 180, 255, 255),
                ed::GetStyle().NodeRounding, 0, 2.0f);
        }
    }
}

// Row용 Constraint 노드 그리기 헬퍼 함수
static void DrawRowConstraintNode(ed::NodeId NodeID, ed::PinId InputPin, ed::PinId OutputPin,
                                  int32 ConstraintIndex, PhysicsAssetViewerState* State)
{
    bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Constraint &&
                     State->SelectedConstraintIndex == ConstraintIndex);

    ImU32 HeaderColor = bSelected ? IM_COL32(230, 210, 150, 255) : IM_COL32(200, 180, 140, 255);

    ed::BeginNode(NodeID);
    ImGui::PushID(static_cast<int>(NodeID.Get()));

    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Indent(2.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::Text("[C]");
        ImGui::PopStyleColor();
        ImGui::Unindent(2.0f);
        ImGui::Dummy(ImVec2(0, 1));
    }
    ImGui::EndGroup();

    ImRect HeaderRect;
    HeaderRect.Min = ImGui::GetItemRectMin();
    HeaderRect.Max = ImGui::GetItemRectMax();

    float minWidth = 40.0f;
    if (HeaderRect.GetWidth() < minWidth)
    {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(minWidth - HeaderRect.GetWidth(), 0));
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::BeginGroup();
    {
        ed::BeginPin(InputPin, ed::PinKind::Input);
        ImGui::TextDisabled("<");
        ed::EndPin();

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();

        ed::BeginPin(OutputPin, ed::PinKind::Output);
        ImGui::TextDisabled(">");
        ed::EndPin();
    }
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::PopID();
    ed::EndNode();

    if (ImGui::IsItemVisible())
    {
        ImDrawList* drawList = ed::GetNodeBackgroundDrawList(NodeID);
        ImVec2 nodeMin = ed::GetNodePosition(NodeID);
        ImVec2 nodeSize = ed::GetNodeSize(NodeID);
        ImVec2 nodeMax = ImVec2(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);

        float headerHeight = HeaderRect.GetHeight() + 4;
        ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);

        drawList->AddRectFilled(nodeMin, headerMax, HeaderColor,
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
        drawList->AddRectFilled(ImVec2(nodeMin.x, headerMax.y), nodeMax,
            IM_COL32(245, 235, 215, 230),
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersBottom);

        if (bSelected)
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 200, 100, 255),
                ed::GetStyle().NodeRounding, 0, 2.5f);
        }
        else
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(180, 160, 120, 200),
                ed::GetStyle().NodeRounding, 0, 1.5f);
        }
    }
}

// Constraint 노드 그리기 헬퍼 함수 (레거시 호환용)
static void DrawConstraintNode(FPAEConstraintNode& ConstraintNode, PhysicsAssetViewerState* State)
{
    // Constraint 노드 색상 (베이지색 - Unreal 스타일)
    bool bSelected = (State->EditMode == EPhysicsAssetEditMode::Constraint &&
                     State->SelectedConstraintIndex == ConstraintNode.ConstraintIndex);

    // 베이지색 헤더 (선택 시 더 밝은 베이지)
    ImU32 HeaderColor = bSelected ? IM_COL32(230, 210, 150, 255) : IM_COL32(200, 180, 140, 255);

    ed::BeginNode(ConstraintNode.ID);

    ImGui::PushID(static_cast<int>(ConstraintNode.ID.Get()));

    // 간단한 헤더 (작은 크기)
    ImGui::BeginGroup();
    {
        ImGui::Dummy(ImVec2(0, 1));
        ImGui::Indent(2.0f);

        // Constraint 아이콘만 표시 (컴팩트)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::Text("[C]");
        ImGui::PopStyleColor();

        ImGui::Unindent(2.0f);
        ImGui::Dummy(ImVec2(0, 1));
    }
    ImGui::EndGroup();

    // 헤더 크기
    ImRect HeaderRect;
    HeaderRect.Min = ImGui::GetItemRectMin();
    HeaderRect.Max = ImGui::GetItemRectMax();

    // 최소 너비 보장 (작게)
    float minWidth = 40.0f;
    if (HeaderRect.GetWidth() < minWidth)
    {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(minWidth - HeaderRect.GetWidth(), 0));
    }

    // 핀 영역 (Input <-> Output)
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::BeginGroup();
    {
        // 입력 핀 (왼쪽)
        ed::BeginPin(ConstraintNode.InputPin, ed::PinKind::Input);
        ImGui::TextDisabled("<");
        ed::EndPin();

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();

        // 출력 핀 (오른쪽)
        ed::BeginPin(ConstraintNode.OutputPin, ed::PinKind::Output);
        ImGui::TextDisabled(">");
        ed::EndPin();
    }
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::PopID();

    ed::EndNode();

    // 배경 그리기
    if (ImGui::IsItemVisible())
    {
        ImDrawList* drawList = ed::GetNodeBackgroundDrawList(ConstraintNode.ID);

        ImVec2 nodeMin = ed::GetNodePosition(ConstraintNode.ID);
        ImVec2 nodeSize = ed::GetNodeSize(ConstraintNode.ID);
        ImVec2 nodeMax = ImVec2(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);

        float headerHeight = HeaderRect.GetHeight() + 4;
        ImVec2 headerMax = ImVec2(nodeMax.x, nodeMin.y + headerHeight);

        // 헤더 배경 (베이지색)
        drawList->AddRectFilled(nodeMin, headerMax, HeaderColor,
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);

        // 바디 배경 (밝은 베이지)
        drawList->AddRectFilled(ImVec2(nodeMin.x, headerMax.y), nodeMax,
            IM_COL32(245, 235, 215, 230),
            ed::GetStyle().NodeRounding, ImDrawFlags_RoundCornersBottom);

        // 테두리
        if (bSelected)
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 200, 100, 255),
                ed::GetStyle().NodeRounding, 0, 2.5f);
        }
        else
        {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(180, 160, 120, 200),
                ed::GetStyle().NodeRounding, 0, 1.5f);
        }
    }
}

void SPhysicsAssetGraphPanel::RenderNodeGraph(PhysicsAssetViewerState* State, FPAEGraphState* GraphState)
{
    if (!State || !GraphState || !State->PhysicsAsset) return;

    // Graph State와 PhysicsAsset 동기화 (Constraint 수가 달라졌을 때)
    if (GraphState->ConstraintRows.Num() != State->PhysicsAsset->ConstraintSetups.Num() ||
        GraphState->BodyNodes.Num() != State->PhysicsAsset->BodySetups.Num())
    {
        Owner->SyncGraphFromPhysicsAsset();
    }

    ed::SetCurrentEditor(GraphState->Context);
    ed::Begin("PhysicsAssetGraph", ImVec2(0.0f, 0.0f));

    UPhysicsAsset* PhysAsset = State->PhysicsAsset;

    // GraphFilterRootBodyIndex가 설정되어 있으면 필터링된 그래프 표시
    // (Skeleton Tree 클릭 시만 설정됨, 그래프 노드 클릭 시에는 유지)
    // Constraint 모드에서도 필터링 유지 (Constraint 노드 클릭 시 전체 그래프로 전환 방지)
    bool bShowFilteredGraph = (State->GraphFilterRootBodyIndex >= 0 &&
        (State->EditMode == EPhysicsAssetEditMode::Body ||
         State->EditMode == EPhysicsAssetEditMode::Constraint ||
         State->EditMode == EPhysicsAssetEditMode::Shape));

    if (!bShowFilteredGraph)
    {
        // ========================================================================
        // Unreal 스타일 3-Column 전체 그래프 표시 (Group 기반)
        // 같은 Parent를 가진 Constraint들을 하나의 그룹으로 표시:
        //               ┌─ [C] → Child1
        // Parent ───────┼─ [C] → Child2
        //               └─ [C] → Child3
        // ========================================================================
        ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

        // Group 기반 렌더링
        for (auto& Group : GraphState->ConstraintGroups)
        {
            // 1. Parent Body Node (왼쪽, 그룹당 1개)
            DrawRowBodyNode(Group.ParentBodyNodeID, Group.ParentBodyOutputPin, ed::PinId(0),
                           Group.ParentBodyIndex, Group.ParentBoneName, State, true);

            // 2. 각 Child마다 Constraint + Child Body 노드 그리기
            for (auto& Child : Group.Children)
            {
                // Constraint Node (중앙)
                DrawRowConstraintNode(Child.ConstraintNodeID, Child.ConstraintInputPin, Child.ConstraintOutputPin,
                                     Child.ConstraintIndex, State);

                // Child Body Node (오른쪽)
                DrawRowBodyNode(Child.ChildBodyNodeID, ed::PinId(0), Child.ChildBodyInputPin,
                               Child.ChildBodyIndex, Child.ChildBoneName, State, false);
            }
        }

        ed::PopStyleVar(2);

        // Group 기반 Link 그리기
        ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
        for (auto& Group : GraphState->ConstraintGroups)
        {
            for (auto& Child : Group.Children)
            {
                // Link 1: Parent Body → Constraint
                ed::Link(Child.Link1ID, Group.ParentBodyOutputPin, Child.ConstraintInputPin,
                        ImColor(180, 180, 180, 255), 2.0f);

                // Link 2: Constraint → Child Body
                ed::Link(Child.Link2ID, Child.ConstraintOutputPin, Child.ChildBodyInputPin,
                        ImColor(180, 180, 180, 255), 2.0f);
            }
        }
        ed::PopStyleVar();
    }
    else
    {
        // ========================================================================
        // 필터링된 그래프 표시 (선택된 Body + 연결된 Constraint + 자식 Body)
        // ========================================================================

        // 그래프 필터링 기준 Body 찾기 (GraphFilterRootBodyIndex 사용)
        FPAEBodyNode* FilterRootBodyNode = nullptr;
        for (auto& Node : GraphState->BodyNodes)
        {
            if (Node.BodyIndex == State->GraphFilterRootBodyIndex)
            {
                FilterRootBodyNode = &Node;
                break;
            }
        }

        // 필터 루트 Body를 찾지 못하면 전체 그래프 표시로 폴백
        if (!FilterRootBodyNode)
        {
            // 전체 그래프 표시
            ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
            ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

            for (auto& Node : GraphState->BodyNodes)
            {
                bool bIsSelected = (Node.BodyIndex == State->SelectedBodyIndex);
                DrawBodyNode(Node, State, bIsSelected, false, true);  // 폴백 전체 그래프: 핀 표시
            }

            for (auto& ConstraintNode : GraphState->ConstraintNodes)
            {
                DrawConstraintNode(ConstraintNode, State);
            }

            ed::PopStyleVar(2);

            ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
            for (auto& Link : GraphState->Links)
            {
                ed::Link(Link.ID, Link.StartPinID, Link.EndPinID, ImColor(180, 180, 180, 255), 2.0f);
            }
            ed::PopStyleVar();
        }
        else
        {
            // 필터 루트 Body에 연결된 Constraint 노드들 찾기
            TArray<FPAEConstraintNode*> ConnectedConstraints;
            TArray<int32> ConnectedBodyIndices;
            ConnectedBodyIndices.Add(FilterRootBodyNode->BodyIndex); // 필터 루트 Body도 포함

            UBodySetup* FilterRootBody = PhysAsset->BodySetups[FilterRootBodyNode->BodyIndex];
            FString FilterRootBoneName = FilterRootBody->BoneName.ToString();

    for (auto& ConstraintNode : GraphState->ConstraintNodes)
    {
        // 필터 루트 Body가 부모(Bone1)인 Constraint만 추가 (자식 방향만 표시)
        // Constraint 구조: Bone1(부모) -> Bone2(자식)
        if (ConstraintNode.Bone1Name == FilterRootBoneName)
        {
            ConnectedConstraints.Add(&ConstraintNode);

            // 자식 Body 찾기 (Bone2)
            FString ChildBoneName = ConstraintNode.Bone2Name;

            for (auto& BodyNode : GraphState->BodyNodes)
            {
                if (BodyNode.BoneName == ChildBoneName)
                {
                    ConnectedBodyIndices.Add(BodyNode.BodyIndex);
                    break;
                }
            }
        }
    }

    // ============================================================================
    // 필터링된 노드들을 깔끔한 트리 레이아웃으로 재배치 (필터 루트가 바뀔 때만)
    // 구조: [Filter Root Body] → [Constraint1] → [Child Body1]
    //                             [Constraint2] → [Child Body2]
    //                             [Constraint3] → [Child Body3]
    // ============================================================================
    {
        // 필터 루트가 바뀔 때만 레이아웃 재계산 (매 프레임마다 하면 드래그 불가능)
        static ed::NodeId lastFilterRootNodeID = ed::NodeId::Invalid;
        if (lastFilterRootNodeID != FilterRootBodyNode->ID)
        {
            lastFilterRootNodeID = FilterRootBodyNode->ID;

            const float COLUMN_SPACING = 150.0f;        // 열 간 거리 (좌→우)
            const float ROW_SPACING = 60.0f;            // 행 간 거리 (위→아래)

            int32 NumConstraints = ConnectedConstraints.Num();

            // 1. 필터 루트 Body를 왼쪽(Column 0)에 배치
            //    여러 Constraint가 있을 경우 중앙에 오도록 Y 위치 계산
            float totalHeight = (NumConstraints - 1) * ROW_SPACING;
            float filterRootBodyY = -totalHeight * 0.5f;  // 세로 중앙 정렬

            ImVec2 filterRootBodyPos = ImVec2(0.0f, filterRootBodyY);
            ed::SetNodePosition(FilterRootBodyNode->ID, filterRootBodyPos);

            // 2. 연결된 Constraint들을 중간(Column 1)에 위→아래로 배치
            float startY = -totalHeight * 0.5f;
            for (int32 i = 0; i < NumConstraints; ++i)
            {
                FPAEConstraintNode* ConstraintNode = ConnectedConstraints[i];
                ImVec2 constraintPos = ImVec2(COLUMN_SPACING, startY + i * ROW_SPACING);
                ed::SetNodePosition(ConstraintNode->ID, constraintPos);
            }

            // 3. 각 Constraint에 연결된 자식 Body를 오른쪽(Column 2)에 배치
            for (int32 i = 0; i < NumConstraints; ++i)
            {
                FPAEConstraintNode* ConstraintNode = ConnectedConstraints[i];
                float constraintY = startY + i * ROW_SPACING;

                // 자식 Body 찾기 (Bone2는 항상 자식)
                FString ChildBoneName = ConstraintNode->Bone2Name;

                for (auto& BodyNode : GraphState->BodyNodes)
                {
                    if (BodyNode.BoneName == ChildBoneName)
                    {
                        ImVec2 childPos = ImVec2(COLUMN_SPACING * 2.0f, constraintY);
                        ed::SetNodePosition(BodyNode.ID, childPos);
                        break;
                    }
                }
            }

            // 뷰를 재배치된 노드들로 이동
            ed::NavigateToContent(0.0f);
        }
    }

    // 스타일 설정
    ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

    // 연결된 Body 노드들만 그리기 (핀 표시하여 링크 가능)
    for (int32 BodyIdx : ConnectedBodyIndices)
    {
        if (BodyIdx >= 0 && BodyIdx < GraphState->BodyNodes.Num())
        {
            auto& Node = GraphState->BodyNodes[BodyIdx];
            bool bIsSelected = (Node.BodyIndex == State->SelectedBodyIndex);
            bool bIsFilterRoot = (Node.BodyIndex == State->GraphFilterRootBodyIndex);
            DrawBodyNode(Node, State, bIsSelected, bIsFilterRoot, true);  // 필터링된 그래프: 핀 표시
        }
    }

    // 연결된 Constraint 노드들만 그리기
    for (auto* ConstraintNode : ConnectedConstraints)
    {
        DrawConstraintNode(*ConstraintNode, State);
    }

    ed::PopStyleVar(2);

    // 연결된 Link들 직접 그리기 (레거시 BodyNodes와 ConstraintNodes의 핀 ID 사용)
    ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);

    // Filter Root Body의 OutputPin과 각 Constraint의 InputPin 연결
    for (auto* ConstraintNode : ConnectedConstraints)
    {
        // Link 1: Filter Root Body → Constraint
        ed::LinkId linkId1(GraphState->NextLinkID++);
        ed::Link(linkId1, FilterRootBodyNode->OutputPin, ConstraintNode->InputPin,
            ImColor(180, 180, 180, 255), 2.0f);

        // Link 2: Constraint → Child Body
        // 자식 Body 찾기 (Bone2)
        FString ChildBoneName = ConstraintNode->Bone2Name;
        for (auto& BodyNode : GraphState->BodyNodes)
        {
            if (BodyNode.BoneName == ChildBoneName)
            {
                ed::LinkId linkId2(GraphState->NextLinkID++);
                ed::Link(linkId2, ConstraintNode->OutputPin, BodyNode.InputPin,
                    ImColor(180, 180, 180, 255), 2.0f);
                break;
            }
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
            // 시작 핀과 끝 핀에서 BodyIndex 찾기 (Group 구조 사용)
            int32 StartBodyIndex = Owner->FindBodyIndexByGroupPin(startPinId);
            int32 EndBodyIndex = Owner->FindBodyIndexByGroupPin(endPinId);

            if (StartBodyIndex >= 0 && EndBodyIndex >= 0 && StartBodyIndex != EndBodyIndex &&
                StartBodyIndex < PhysAsset->BodySetups.Num() && EndBodyIndex < PhysAsset->BodySetups.Num())
            {
                // 두 Body 간에 이미 Constraint가 있는지 확인
                UBodySetup* Body1 = PhysAsset->BodySetups[StartBodyIndex];
                UBodySetup* Body2 = PhysAsset->BodySetups[EndBodyIndex];

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
                        NewConstraint->BodyIndex1 = StartBodyIndex;
                        NewConstraint->BodyIndex2 = EndBodyIndex;

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

                        UE_LOG("PAE: CreateConstraint: %s <-> %s",
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
        // Constraint 노드 삭제
        ed::NodeId deletedNodeId;
        while (ed::QueryDeletedNode(&deletedNodeId))
        {
            if (ed::AcceptDeletedItem())
            {
                // 삭제할 Constraint 노드 찾기
                FPAEConstraintNode* ConstraintNode = Owner->FindConstraintNode(deletedNodeId);
                if (ConstraintNode && ConstraintNode->ConstraintIndex >= 0 &&
                    ConstraintNode->ConstraintIndex < PhysAsset->ConstraintSetups.Num())
                {
                    UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[ConstraintNode->ConstraintIndex];
                    if (Constraint)
                    {
                        UE_LOG("PAE: DeleteConstraint: %s <-> %s",
                               Constraint->ConstraintBone1.ToString().c_str(),
                               Constraint->ConstraintBone2.ToString().c_str());

                        // Constraint 삭제
                        ObjectFactory::DeleteObject(Constraint);
                        PhysAsset->ConstraintSetups.erase(PhysAsset->ConstraintSetups.begin() + ConstraintNode->ConstraintIndex);

                        // Graph 동기화
                        Owner->SyncGraphFromPhysicsAsset();
                    }
                }
            }
        }
    }
        ed::EndDelete();
        }  // else (SelectedBodyNode found) 끝
    }  // else (bShowFilteredGraph) 끝

    // ============================================================================
    // 더블클릭 처리 (필터링 모드 전환)
    // 방법 1: imgui-node-editor의 GetDoubleClickedNode 사용
    // 방법 2: ImGui 더블클릭 + 선택된 노드 결합
    // ============================================================================
    ed::NodeId doubleClickedNodeId = ed::GetDoubleClickedNode();

    // 방법 2: ImGui 더블클릭과 선택된 노드 결합 (백업)
    if (doubleClickedNodeId.Get() == 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        // 현재 선택된 노드가 있으면 그것을 더블클릭된 것으로 처리
        if (ed::GetSelectedObjectCount() > 0)
        {
            ed::NodeId selectedId;
            if (ed::GetSelectedNodes(&selectedId, 1) > 0)
            {
                doubleClickedNodeId = selectedId;
            }
        }
    }

    if (doubleClickedNodeId.Get() != 0)
    {
        bool bDoubleClickHandled = false;

        // Group 구조에서 더블클릭된 노드 찾기
        for (auto& Group : GraphState->ConstraintGroups)
        {
            // Parent Body 노드 더블클릭 → 해당 Body를 루트로 필터링
            if (Group.ParentBodyNodeID == doubleClickedNodeId)
            {
                State->GraphFilterRootBodyIndex = Group.ParentBodyIndex;
                State->SelectedBodyIndex = Group.ParentBodyIndex;
                State->EditMode = EPhysicsAssetEditMode::Body;
                State->SelectedConstraintIndex = -1;
                State->SelectedShapeIndex = -1;
                State->SelectedShapeType = EAggCollisionShape::Unknown;
                bDoubleClickHandled = true;
                break;
            }

            for (auto& Child : Group.Children)
            {
                // Constraint 노드 더블클릭 → Parent Body를 루트로 필터링
                if (Child.ConstraintNodeID == doubleClickedNodeId)
                {
                    // SelectConstraint가 ClearSelection을 호출하므로 나중에 필터 설정
                    State->SelectConstraint(Child.ConstraintIndex);
                    State->GraphFilterRootBodyIndex = Group.ParentBodyIndex;  // SelectConstraint 후 설정
                    bDoubleClickHandled = true;
                    break;
                }

                // Child Body 노드 더블클릭 → Child Body를 루트로 필터링
                if (Child.ChildBodyNodeID == doubleClickedNodeId)
                {
                    State->GraphFilterRootBodyIndex = Child.ChildBodyIndex;
                    State->SelectedBodyIndex = Child.ChildBodyIndex;
                    State->EditMode = EPhysicsAssetEditMode::Body;
                    State->SelectedConstraintIndex = -1;
                    State->SelectedShapeIndex = -1;
                    State->SelectedShapeType = EAggCollisionShape::Unknown;
                    bDoubleClickHandled = true;
                    break;
                }
            }

            if (bDoubleClickHandled) break;
        }

        // 레거시 구조에서도 검색 (필터링된 그래프 모드에서 사용)
        if (!bDoubleClickHandled)
        {
            // Body 노드 검색
            FPAEBodyNode* DoubleClickedBody = Owner->FindBodyNode(doubleClickedNodeId);
            if (DoubleClickedBody && DoubleClickedBody->BodyIndex >= 0)
            {
                State->GraphFilterRootBodyIndex = DoubleClickedBody->BodyIndex;
                State->SelectedBodyIndex = DoubleClickedBody->BodyIndex;
                State->EditMode = EPhysicsAssetEditMode::Body;
                bDoubleClickHandled = true;
            }

            // Constraint 노드 검색 (필터링된 그래프에서 Constraint 더블클릭 시)
            if (!bDoubleClickHandled)
            {
                FPAEConstraintNode* DoubleClickedConstraint = Owner->FindConstraintNode(doubleClickedNodeId);
                if (DoubleClickedConstraint && DoubleClickedConstraint->ConstraintIndex >= 0)
                {
                    // Constraint의 Parent Body(Bone1)를 필터 루트로 설정
                    FString ParentBoneName = DoubleClickedConstraint->Bone1Name;
                    for (auto& BodyNode : GraphState->BodyNodes)
                    {
                        if (BodyNode.BoneName == ParentBoneName)
                        {
                            State->SelectConstraint(DoubleClickedConstraint->ConstraintIndex);
                            State->GraphFilterRootBodyIndex = BodyNode.BodyIndex;
                            break;
                        }
                    }
                }
            }
        }
    }

    // ============================================================================
    // 우클릭 컨텍스트 메뉴 감지 (ed::End() 이후에 팝업 렌더링)
    // ============================================================================
    ed::NodeId contextMenuNodeId;
    static bool bOpenContextMenu = false;
    if (ed::ShowNodeContextMenu(&contextMenuNodeId))
    {
        GraphState->ContextMenuNodeId = contextMenuNodeId;
        bOpenContextMenu = true;
    }

    // ============================================================================
    // 노드 선택 처리 (항상 실행)
    // Group 기반 구조에서 노드 ID로 Body/Constraint 찾기
    // ============================================================================
    if (ed::GetSelectedObjectCount() > 0)
    {
        ed::NodeId selectedNodeId;
        if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0)
        {
            GraphState->SelectedNodeID = selectedNodeId;
            GraphState->SelectedLinkID = ed::LinkId::Invalid;

            bool bFound = false;

            // 그래프 노드 클릭 시 필터링 상태 유지를 위해 인덱스 저장
            int32 savedFilterIndex = State->GraphFilterRootBodyIndex;

            // Group 구조에서 노드 찾기 (Unreal 스타일 3-Column)
            for (auto& Group : GraphState->ConstraintGroups)
            {
                // Parent Body 노드 확인 (그룹당 1개)
                if (Group.ParentBodyNodeID == selectedNodeId)
                {
                    // SelectBody 호출로 Gizmo 설정 + 필터링 유지
                    State->SelectBody(Group.ParentBodyIndex);
                    State->GraphFilterRootBodyIndex = savedFilterIndex;  // 필터 복원
                    bFound = true;
                    break;
                }

                // Children에서 Constraint 또는 Child Body 노드 확인
                for (auto& Child : Group.Children)
                {
                    // Constraint 노드 확인
                    if (Child.ConstraintNodeID == selectedNodeId)
                    {
                        // SelectConstraint가 ClearSelection을 호출하므로 필터 인덱스 복원 필요
                        State->SelectConstraint(Child.ConstraintIndex);
                        State->GraphFilterRootBodyIndex = savedFilterIndex;  // 필터 복원
                        bFound = true;
                        break;
                    }

                    // Child Body 노드 확인
                    if (Child.ChildBodyNodeID == selectedNodeId)
                    {
                        // SelectBody 호출로 Gizmo 설정 + 필터링 유지
                        State->SelectBody(Child.ChildBodyIndex);
                        State->GraphFilterRootBodyIndex = savedFilterIndex;  // 필터 복원
                        bFound = true;
                        break;
                    }
                }

                if (bFound) break;
            }

            // Row에서 못 찾으면 레거시 구조에서 찾기 (필터링된 그래프 모드 등)
            if (!bFound)
            {
                FPAEBodyNode* SelectedBody = Owner->FindBodyNode(selectedNodeId);
                if (SelectedBody && SelectedBody->BodyIndex >= 0)
                {
                    // SelectBody 호출로 Gizmo 설정 + 필터링 유지
                    State->SelectBody(SelectedBody->BodyIndex);
                    State->GraphFilterRootBodyIndex = savedFilterIndex;  // 필터 복원
                }
                else
                {
                    FPAEConstraintNode* SelectedConstraint = Owner->FindConstraintNode(selectedNodeId);
                    if (SelectedConstraint && SelectedConstraint->ConstraintIndex >= 0)
                    {
                        State->SelectConstraint(SelectedConstraint->ConstraintIndex);
                        State->GraphFilterRootBodyIndex = savedFilterIndex;  // 필터 복원
                    }
                }
            }
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);

    // ============================================================================
    // 우클릭 컨텍스트 메뉴 렌더링 (ed::End() 이후 - 올바른 스크린 좌표 사용)
    // ============================================================================
    if (bOpenContextMenu)
    {
        ImGui::OpenPopup("NodeContextMenu");
        bOpenContextMenu = false;
    }

    if (ImGui::BeginPopup("NodeContextMenu"))
    {
        ed::NodeId nodeId = GraphState->ContextMenuNodeId;
        bool bIsBodyNode = false;
        bool bIsConstraintNode = false;
        int32 BodyIndexToDelete = -1;
        int32 ConstraintIndexToDelete = -1;
        std::string NodeName;

        // Group 구조에서 노드 타입 확인
        for (auto& Group : GraphState->ConstraintGroups)
        {
            // Parent Body 노드 확인
            if (Group.ParentBodyNodeID == nodeId)
            {
                bIsBodyNode = true;
                BodyIndexToDelete = Group.ParentBodyIndex;
                NodeName = Group.ParentBoneName;
                break;
            }

            // Children에서 확인
            for (auto& Child : Group.Children)
            {
                if (Child.ConstraintNodeID == nodeId)
                {
                    bIsConstraintNode = true;
                    ConstraintIndexToDelete = Child.ConstraintIndex;
                    NodeName = Group.ParentBoneName + " -> " + Child.ChildBoneName;
                    break;
                }
                if (Child.ChildBodyNodeID == nodeId)
                {
                    bIsBodyNode = true;
                    BodyIndexToDelete = Child.ChildBodyIndex;
                    NodeName = Child.ChildBoneName;
                    break;
                }
            }
            if (bIsBodyNode || bIsConstraintNode) break;
        }

        // 레거시 구조에서도 확인
        if (!bIsBodyNode && !bIsConstraintNode)
        {
            FPAEBodyNode* BodyNode = Owner->FindBodyNode(nodeId);
            if (BodyNode && BodyNode->BodyIndex >= 0)
            {
                bIsBodyNode = true;
                BodyIndexToDelete = BodyNode->BodyIndex;
                NodeName = BodyNode->BoneName;
            }
            else
            {
                FPAEConstraintNode* ConstraintNode = Owner->FindConstraintNode(nodeId);
                if (ConstraintNode && ConstraintNode->ConstraintIndex >= 0)
                {
                    bIsConstraintNode = true;
                    ConstraintIndexToDelete = ConstraintNode->ConstraintIndex;
                    NodeName = ConstraintNode->Bone1Name + " -> " + ConstraintNode->Bone2Name;
                }
            }
        }

        // Body 노드 메뉴
        if (bIsBodyNode && BodyIndexToDelete >= 0)
        {
            ImGui::Text("Body: %s", NodeName.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Body"))
            {
                // Cascade Delete: 이 Body와 연결된 모든 Constraint 먼저 삭제
                UBodySetup* BodyToDelete = PhysAsset->BodySetups[BodyIndexToDelete];
                if (BodyToDelete)
                {
                    FName BoneNameToDelete = BodyToDelete->BoneName;

                    // 연결된 Constraint들 찾아서 삭제 (역순으로 삭제해야 인덱스 꼬이지 않음)
                    for (int32 i = PhysAsset->ConstraintSetups.Num() - 1; i >= 0; --i)
                    {
                        UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[i];
                        if (Constraint &&
                            (Constraint->ConstraintBone1 == BoneNameToDelete ||
                             Constraint->ConstraintBone2 == BoneNameToDelete))
                        {
                            UE_LOG("PAE: CascadeDelete: Constraint %s <-> %s",
                                Constraint->ConstraintBone1.ToString().c_str(),
                                Constraint->ConstraintBone2.ToString().c_str());
                            ObjectFactory::DeleteObject(Constraint);
                            PhysAsset->ConstraintSetups.erase(PhysAsset->ConstraintSetups.begin() + i);
                        }
                    }

                    // Body 삭제
                    UE_LOG("PAE: DeleteBody: %s", BoneNameToDelete.ToString().c_str());
                    PhysAsset->BoneNameToBodyIndex.erase(BoneNameToDelete.ToString().c_str());
                    ObjectFactory::DeleteObject(BodyToDelete);
                    PhysAsset->BodySetups.erase(PhysAsset->BodySetups.begin() + BodyIndexToDelete);

                    // BoneNameToBodyIndex 맵 재구성
                    PhysAsset->BoneNameToBodyIndex.clear();
                    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
                    {
                        if (PhysAsset->BodySetups[i])
                        {
                            PhysAsset->BoneNameToBodyIndex[PhysAsset->BodySetups[i]->BoneName.ToString().c_str()] = i;
                        }
                    }

                    // 선택 해제 및 그래프 동기화
                    State->ClearSelection();
                    Owner->SyncGraphFromPhysicsAsset();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        // Constraint 노드 메뉴
        else if (bIsConstraintNode && ConstraintIndexToDelete >= 0)
        {
            ImGui::Text("Constraint: %s", NodeName.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Constraint"))
            {
                UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[ConstraintIndexToDelete];
                if (Constraint)
                {
                    UE_LOG("PAE: DeleteConstraint: %s <-> %s",
                        Constraint->ConstraintBone1.ToString().c_str(),
                        Constraint->ConstraintBone2.ToString().c_str());
                    ObjectFactory::DeleteObject(Constraint);
                    PhysAsset->ConstraintSetups.erase(PhysAsset->ConstraintSetups.begin() + ConstraintIndexToDelete);

                    State->ClearSelection();
                    Owner->SyncGraphFromPhysicsAsset();
                }
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::TextDisabled("Unknown node");
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// Node Graph 헬퍼 함수들
// ============================================================================

void SPhysicsAssetEditorWindow::SyncGraphFromPhysicsAsset()
{
    if (!GraphState || !ActiveState || !ActiveState->PhysicsAsset) return;

    ed::SetCurrentEditor(GraphState->Context);

    UPhysicsAsset* PhysAsset = ActiveState->PhysicsAsset;

    // 기존 데이터 클리어
    GraphState->Clear();
    GraphState->NextNodeID = 1;
    GraphState->NextPinID = 100000;
    GraphState->NextLinkID = 200000;

    // ============================================================================
    // Unreal 스타일 3-Column 레이아웃 (Group 기반)
    // 같은 Parent를 가진 Constraint들을 하나의 그룹으로 묶음:
    //               ┌─ [C] → Child1
    // Parent ───────┼─ [C] → Child2
    //               └─ [C] → Child3
    // ============================================================================

    const float COLUMN_WIDTH = 220.0f;   // 열 너비 (노드 겹침 방지)
    const float ROW_HEIGHT = 80.0f;      // 행 높이 (Child 간 간격 증가)
    const float GROUP_SPACING = 50.0f;   // 그룹 간 추가 간격 증가
    const float START_X = 50.0f;         // 시작 X 위치
    const float START_Y = 50.0f;         // 시작 Y 위치

    // Body 이름 → Body 인덱스 맵 (빠른 조회용)
    TMap<FString, int32> BoneNameToBodyIndex;
    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->BodySetups[i];
        if (Setup)
        {
            BoneNameToBodyIndex.Add(Setup->BoneName.ToString(), i);
        }
    }

    // Step 1: Constraint들을 Parent Body 기준으로 그룹화
    TMap<FString, TArray<int32>> ParentToConstraints;  // ParentBoneName → ConstraintIndices

    for (int32 i = 0; i < PhysAsset->ConstraintSetups.Num(); ++i)
    {
        UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[i];
        if (!Constraint) continue;

        FString Bone1Name = Constraint->ConstraintBone1.ToString();

        // 해당 Parent에 이 Constraint 추가
        if (!ParentToConstraints.Contains(Bone1Name))
        {
            ParentToConstraints.Add(Bone1Name, TArray<int32>());
        }
        ParentToConstraints[Bone1Name].Add(i);
    }

    // Step 2: 그룹 생성
    float currentY = START_Y;
    int32 groupIndex = 0;

    for (auto& Pair : ParentToConstraints)
    {
        const FString& ParentBoneName = Pair.first;
        const TArray<int32>& ConstraintIndices = Pair.second;

        if (ConstraintIndices.Num() == 0) continue;

        // Parent Body 인덱스 찾기
        int32* ParentBodyIndexPtr = BoneNameToBodyIndex.Find(ParentBoneName);
        if (!ParentBodyIndexPtr) continue;
        int32 ParentBodyIndex = *ParentBodyIndexPtr;

        // 그룹 생성
        FPAEConstraintGroup Group;
        Group.GroupIndex = groupIndex++;
        Group.ParentBodyIndex = ParentBodyIndex;
        Group.ParentBoneName = ParentBoneName;

        // Parent Body Node 생성 (그룹당 1개)
        Group.ParentBodyNodeID = GraphState->GetNextNodeId();
        Group.ParentBodyOutputPin = GraphState->GetNextPinId();

        // Parent 노드 위치: 그룹의 중앙에 배치
        float groupCenterY = currentY + (ConstraintIndices.Num() - 1) * ROW_HEIGHT * 0.5f;
        ed::SetNodePosition(Group.ParentBodyNodeID, ImVec2(START_X, groupCenterY));

        // 각 Constraint마다 Child 생성
        for (int32 childIdx = 0; childIdx < ConstraintIndices.Num(); ++childIdx)
        {
            int32 constraintIndex = ConstraintIndices[childIdx];
            UPhysicsConstraintSetup* Constraint = PhysAsset->ConstraintSetups[constraintIndex];
            if (!Constraint) continue;

            FString ChildBoneName = Constraint->ConstraintBone2.ToString();

            // Child Body 인덱스 찾기
            int32* ChildBodyIndexPtr = BoneNameToBodyIndex.Find(ChildBoneName);
            if (!ChildBodyIndexPtr) continue;
            int32 ChildBodyIndex = *ChildBodyIndexPtr;

            float childY = currentY + childIdx * ROW_HEIGHT;

            // Constraint Child 생성
            FPAEConstraintChild Child;
            Child.ConstraintIndex = constraintIndex;
            Child.ChildBodyIndex = ChildBodyIndex;
            Child.ChildBoneName = ChildBoneName;

            // Constraint Node 생성
            Child.ConstraintNodeID = GraphState->GetNextNodeId();
            Child.ConstraintInputPin = GraphState->GetNextPinId();
            Child.ConstraintOutputPin = GraphState->GetNextPinId();
            ed::SetNodePosition(Child.ConstraintNodeID, ImVec2(START_X + COLUMN_WIDTH, childY));

            // Child Body Node 생성
            Child.ChildBodyNodeID = GraphState->GetNextNodeId();
            Child.ChildBodyInputPin = GraphState->GetNextPinId();
            ed::SetNodePosition(Child.ChildBodyNodeID, ImVec2(START_X + COLUMN_WIDTH * 2.0f, childY));

            // Links 생성
            Child.Link1ID = GraphState->GetNextLinkId();  // Parent → Constraint
            Child.Link2ID = GraphState->GetNextLinkId();  // Constraint → Child

            Group.Children.Add(Child);

            // 레거시 호환용 ConstraintNode 추가
            FPAEConstraintNode LegacyConstraintNode;
            LegacyConstraintNode.ID = Child.ConstraintNodeID;
            LegacyConstraintNode.ConstraintIndex = constraintIndex;
            LegacyConstraintNode.InputPin = Child.ConstraintInputPin;
            LegacyConstraintNode.OutputPin = Child.ConstraintOutputPin;
            LegacyConstraintNode.Bone1Name = ParentBoneName;
            LegacyConstraintNode.Bone2Name = ChildBoneName;
            GraphState->ConstraintNodes.Add(LegacyConstraintNode);

            // 레거시 호환용 Row 추가
            FPAEConstraintRow LegacyRow;
            LegacyRow.RowIndex = GraphState->ConstraintRows.Num();
            LegacyRow.ConstraintIndex = constraintIndex;
            LegacyRow.ParentBodyNodeID = Group.ParentBodyNodeID;
            LegacyRow.ParentBodyOutputPin = Group.ParentBodyOutputPin;
            LegacyRow.ParentBodyIndex = ParentBodyIndex;
            LegacyRow.ParentBoneName = ParentBoneName;
            LegacyRow.ConstraintNodeID = Child.ConstraintNodeID;
            LegacyRow.ConstraintInputPin = Child.ConstraintInputPin;
            LegacyRow.ConstraintOutputPin = Child.ConstraintOutputPin;
            LegacyRow.ChildBodyNodeID = Child.ChildBodyNodeID;
            LegacyRow.ChildBodyInputPin = Child.ChildBodyInputPin;
            LegacyRow.ChildBodyIndex = ChildBodyIndex;
            LegacyRow.ChildBoneName = ChildBoneName;
            LegacyRow.Link1ID = Child.Link1ID;
            LegacyRow.Link2ID = Child.Link2ID;
            GraphState->ConstraintRows.Add(LegacyRow);
        }

        GraphState->ConstraintGroups.Add(Group);

        // 다음 그룹 Y 위치
        currentY += ConstraintIndices.Num() * ROW_HEIGHT + GROUP_SPACING;
    }

    // Body가 있지만 Constraint가 없는 경우를 위한 레거시 BodyNodes 생성
    // (선택 기능 등에서 사용)
    for (int32 i = 0; i < PhysAsset->BodySetups.Num(); ++i)
    {
        UBodySetup* Setup = PhysAsset->BodySetups[i];
        if (!Setup) continue;

        FPAEBodyNode LegacyBodyNode;
        LegacyBodyNode.ID = GraphState->GetNextNodeId();
        LegacyBodyNode.BodyIndex = i;
        LegacyBodyNode.BoneName = Setup->BoneName.ToString();
        LegacyBodyNode.OutputPin = GraphState->GetNextPinId();
        LegacyBodyNode.InputPin = GraphState->GetNextPinId();
        GraphState->BodyNodes.Add(LegacyBodyNode);
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
        if (Node.InputPin == PinID || Node.OutputPin == PinID)
            return &Node;
    }
    return nullptr;
}

FPAEConstraintNode* SPhysicsAssetEditorWindow::FindConstraintNode(ed::NodeId NodeID)
{
    if (!GraphState) return nullptr;

    for (auto& Node : GraphState->ConstraintNodes)
    {
        if (Node.ID == NodeID) return &Node;
    }
    return nullptr;
}

FPAEConstraintNode* SPhysicsAssetEditorWindow::FindConstraintNodeByPin(ed::PinId PinID)
{
    if (!GraphState) return nullptr;

    for (auto& Node : GraphState->ConstraintNodes)
    {
        if (Node.InputPin == PinID || Node.OutputPin == PinID)
            return &Node;
    }
    return nullptr;
}

FPAEConstraintLink* SPhysicsAssetEditorWindow::FindConstraintLink(ed::LinkId LinkID)
{
    if (!GraphState) return nullptr;

    for (auto& Link : GraphState->Links)
    {
        if (Link.ID == LinkID) return &Link;
    }
    return nullptr;
}

int32 SPhysicsAssetEditorWindow::FindBodyIndexByGroupPin(ed::PinId PinID)
{
    if (!GraphState) return -1;

    // Group 구조에서 핀으로 BodyIndex 찾기
    for (auto& Group : GraphState->ConstraintGroups)
    {
        // Parent Body의 출력 핀
        if (Group.ParentBodyOutputPin == PinID)
            return Group.ParentBodyIndex;

        // Children의 입력 핀 확인
        for (auto& Child : Group.Children)
        {
            if (Child.ChildBodyInputPin == PinID)
                return Child.ChildBodyIndex;
        }
    }

    // 레거시 BodyNodes에서도 검색 (폴백)
    for (auto& Node : GraphState->BodyNodes)
    {
        if (Node.InputPin == PinID || Node.OutputPin == PinID)
            return Node.BodyIndex;
    }

    return -1;
}
