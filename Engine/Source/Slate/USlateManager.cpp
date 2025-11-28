#include "pch.h"
#include "USlateManager.h"

#include "CameraActor.h"
#include "Source/Runtime/Engine/Animation/BlendSpace2D.h"
#include "Windows/SWindow.h"
#include "Windows/SSplitterV.h"
#include "Windows/SDetailsWindow.h"
#include "Windows/SControlPanel.h"
#include "Windows/ControlPanelWindow.h"
#include "Windows/SViewportWindow.h"
#include "Windows/PreviewWindow.h"
#include "Windows/BlendSpace2DEditorWindow.h"
#include "Windows/ConsoleWindow.h"
#include "Windows/ContentBrowserWindow.h"
#include "Widgets/MainToolbarWidget.h"
#include "Widgets/ConsoleWidget.h"
#include "FViewportClient.h"
#include "UIManager.h"
#include "GlobalConsole.h"
#include "ThumbnailManager.h"
#include "Windows/AnimStateMachineWindow.h"
#include "Source/Runtime/Engine/Particle/ParticleSystem.h"
#include "Gizmo/GizmoActor.h"

IMPLEMENT_CLASS(USlateManager)

USlateManager& USlateManager::GetInstance()
{
    static USlateManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<USlateManager>();
    }
    return *Instance;
}
#include "FViewportClient.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow* USlateManager::ActiveViewport;

void USlateManager::SaveSplitterConfig()
{
    if (!TopPanel) return;

    EditorINI["TopPanel"] = std::to_string(TopPanel->SplitRatio);
    EditorINI["LeftTop"] = std::to_string(LeftTop->SplitRatio);
    EditorINI["LeftBottom"] = std::to_string(LeftBottom->SplitRatio);
    EditorINI["LeftPanel"] = std::to_string(LeftPanel->SplitRatio);
    EditorINI["RightPanel"] = std::to_string(RightPanel->SplitRatio);
    EditorINI["ConsoleHeight"] = std::to_string(ConsoleHeight);
}

void USlateManager::LoadSplitterConfig()
{
    if (!TopPanel) return;

    if (EditorINI.Contains("TopPanel"))
        TopPanel->SplitRatio = std::stof(EditorINI["TopPanel"]);
    if (EditorINI.Contains("LeftTop"))
        LeftTop->SplitRatio = std::stof(EditorINI["LeftTop"]);
    if (EditorINI.Contains("LeftBottom"))
        LeftBottom->SplitRatio = std::stof(EditorINI["LeftBottom"]);
    if (EditorINI.Contains("LeftPanel"))
        LeftPanel->SplitRatio = std::stof(EditorINI["LeftPanel"]);
    if (EditorINI.Contains("RightPanel"))
        RightPanel->SplitRatio = std::stof(EditorINI["RightPanel"]);
    if (EditorINI.Contains("ConsoleHeight"))
        ConsoleHeight = std::stof(EditorINI["ConsoleHeight"]);
}

USlateManager::USlateManager()
{
    for (auto& Viewport : Viewports)
        Viewport = nullptr;
}

USlateManager::~USlateManager()
{
    Shutdown();
}

void USlateManager::Initialize(ID3D11Device* InDevice, UWorld* InWorld, const FRect& InRect)
{
    // MainToolbar 생성
    MainToolbar = NewObject<UMainToolbarWidget>();
    MainToolbar->Initialize();

    Device = InDevice;
    World = InWorld;
    Rect = InRect;

    // === 전체 화면: 좌(4뷰포트) + 우(Control + Details) ===
    TopPanel = new SSplitterH();  // 수평 분할 (좌우)
    TopPanel->SetSplitRatio(0.7f);  // 70% 뷰포트, 30% UI
    TopPanel->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);

    // 왼쪽: 4분할 뷰포트 영역
    LeftPanel = new SSplitterH();  // 수평 분할 (좌우)
    LeftTop = new SSplitterV();    // 수직 분할 (상하)
    LeftBottom = new SSplitterV(); // 수직 분할 (상하)
    LeftPanel->SideLT = LeftTop;
    LeftPanel->SideRB = LeftBottom;

    // 오른쪽: Control + Details (상하 분할)
    RightPanel = new SSplitterV();  // 수직 분할 (상하)
    RightPanel->SetSplitRatio(0.5f);  // 50-50 분할

    ControlPanel = new SControlPanel();
    DetailPanel = new SDetailsWindow();

    RightPanel->SideLT = ControlPanel;   // 위쪽: ControlPanel
    RightPanel->SideRB = DetailPanel;    // 아래쪽: DetailsWindow

    // TopPanel 좌우 배치
    TopPanel->SideLT = LeftPanel;
    TopPanel->SideRB = RightPanel;

    // === 뷰포트 생성 ===
    Viewports[0] = new SViewportWindow();
    Viewports[1] = new SViewportWindow();
    Viewports[2] = new SViewportWindow();
    Viewports[3] = new SViewportWindow();
    MainViewport = Viewports[0];

    Viewports[0]->Initialize(0, 0,
        Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        World, Device, EViewportType::Perspective);

    Viewports[1]->Initialize(Rect.GetWidth() / 2, 0,
        Rect.GetWidth(), Rect.GetHeight() / 2,
        World, Device, EViewportType::Orthographic_Front);

    Viewports[2]->Initialize(0, Rect.GetHeight() / 2,
        Rect.GetWidth() / 2, Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Left);

    Viewports[3]->Initialize(Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        Rect.GetWidth(), Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Top);

    World->SetEditorCameraActor(MainViewport->GetViewportClient()->GetCamera());

    // 뷰포트들을 2x2로 연결
    LeftTop->SideLT = Viewports[0];
    LeftTop->SideRB = Viewports[1];
    LeftBottom->SideLT = Viewports[2];
    LeftBottom->SideRB = Viewports[3];

    SwitchLayout(EViewportLayoutMode::SingleMain);

    LoadSplitterConfig();

    // === Console Overlay 생성 ===
    ConsoleWindow = new UConsoleWindow();
    if (ConsoleWindow)
    {
        UE_LOG("USlateManager: ConsoleWindow created successfully");
        UGlobalConsole::SetConsoleWidget(ConsoleWindow->GetConsoleWidget());
        UE_LOG("USlateManager: GlobalConsole connected to ConsoleWidget");
    }
    else
    {
        UE_LOG("ERROR: Failed to create ConsoleWindow");
    }

    // === Thumbnail Manager 초기화 ===
    FThumbnailManager::GetInstance().Initialize(Device, nullptr);
    UE_LOG("USlateManager: ThumbnailManager initialized");

    // === Content Browser 생성 ===
    ContentBrowserWindow = new UContentBrowserWindow();
    if (ContentBrowserWindow)
    {
        ContentBrowserWindow->Initialize();
        UE_LOG("USlateManager: ContentBrowserWindow created successfully");
    }
    else
    {
        UE_LOG("ERROR: Failed to create ContentBrowserWindow");
    }
}

void USlateManager::OpenSkeletalMeshViewer()
{
    if (SkeletalViewerWindow)
    {
        return;
    }

    SkeletalViewerWindow = new SPreviewWindow();

    // Open as a detached window at a default size and position
    const float toolbarHeight = 50.0f;
    const float availableHeight = Rect.GetHeight() - toolbarHeight;
    const float w = Rect.GetWidth() * 0.85f;
    const float h = availableHeight * 0.85f;
    const float x = Rect.Left + (Rect.GetWidth() - w) * 0.5f;
    const float y = Rect.Top + toolbarHeight + (availableHeight - h) * 0.5f;
    SkeletalViewerWindow->Initialize(x, y, w, h, World, Device);
}

void USlateManager::OpenSkeletalMeshViewerWithFile(const char* FilePath)
{
    // 뷰어가 이미 열려있으면 그냥 사용, 아니면 새로 열기
    if (!SkeletalViewerWindow)
    {
        OpenSkeletalMeshViewer();
    }

    // Load the skeletal mesh into the viewer
    if (SkeletalViewerWindow && FilePath && FilePath[0] != '\0')
    {
        SkeletalViewerWindow->LoadSkeletalMesh(FilePath);
        UE_LOG("Opening SkeletalMeshViewer with file: %s", FilePath);
    }
}

void USlateManager::CloseSkeletalMeshViewer()
{
    if (!SkeletalViewerWindow)
    {
        return;
    }

    // 완전히 삭제
    delete SkeletalViewerWindow;
    SkeletalViewerWindow = nullptr;
}

void USlateManager::OpenBlendSpace2DEditor(UBlendSpace2D* BlendSpace)
{
    if (BlendSpace2DEditorWindow)
    {
        // 이미 열려있으면 BlendSpace만 교체
        if (BlendSpace)
        {
            BlendSpace2DEditorWindow->SetBlendSpace(BlendSpace);
        }
        return;
    }

    BlendSpace2DEditorWindow = new SBlendSpace2DEditorWindow();

    // 중앙에 적당한 크기로 열기
    const float toolbarHeight = 50.0f;
    const float availableHeight = Rect.GetHeight() - toolbarHeight;
    const float w = 1600.0f;  // 1200 -> 1600
    const float h = 1000.0f;  // 800 -> 1000
    const float x = Rect.Left + (Rect.GetWidth() - w) * 0.5f;
    const float y = Rect.Top + toolbarHeight + (availableHeight - h) * 0.5f;

    BlendSpace2DEditorWindow->Initialize(x, y, w, h, World, Device);

    // BlendSpace 설정
    if (BlendSpace)
    {
        BlendSpace2DEditorWindow->SetBlendSpace(BlendSpace);
    }
}

void USlateManager::CloseBlendSpace2DEditor()
{
    if (!BlendSpace2DEditorWindow)
    {
        return;
    }

    delete BlendSpace2DEditorWindow;
    BlendSpace2DEditorWindow = nullptr;
}

void USlateManager::CreateAnimStateMachineWindowIfNeeded()
{
	if (AnimStateMachineWindow)
		return;

	AnimStateMachineWindow = new SAnimStateMachineWindow();

	// Open as a detached window at a default size and position
	const float ToolbarHeight = 50.0f;
	const float AvailableHeight = Rect.GetHeight() - ToolbarHeight;
	const float w = Rect.GetWidth() * 0.85f;
	const float h = AvailableHeight * 0.85f;
	const float x = Rect.Left + (Rect.GetWidth() - w) * 0.5f;
	const float y = Rect.Top + ToolbarHeight + (AvailableHeight - h) * 0.5f;

	AnimStateMachineWindow->Initialize(x, y, w, h);
}

void USlateManager::OpenAnimStateMachineWindow()
{
	CreateAnimStateMachineWindowIfNeeded();

	// Create a new empty tab
	if (AnimStateMachineWindow)
	{
		AnimStateMachineWindow->CreateNewEmptyTab();
	}
}

void USlateManager::OpenAnimStateMachineWindowWithFile(const char* FilePath)
{
	CreateAnimStateMachineWindowIfNeeded();

	// 파일 로드 로직
	if (AnimStateMachineWindow && FilePath && FilePath[0] != '\0')
	{
		AnimStateMachineWindow->LoadStateMachineFile(FilePath);
	}
}

void USlateManager::CloseAnimStateMachineWindow()
{
	if (!AnimStateMachineWindow) { return; }

	delete AnimStateMachineWindow;
	AnimStateMachineWindow = nullptr;
}

void USlateManager::OpenParticleEditorWindow()
{
	if (ParticleEditorWindow)
	{
		return;
	}

	ParticleEditorWindow = new SParticleEditorWindow();

	// 중앙에 적당한 크기로 열기
	const float toolbarHeight = 50.0f;
	const float availableHeight = Rect.GetHeight() - toolbarHeight;
	const float w = Rect.GetWidth() * 0.92f;
	const float h = availableHeight * 0.92f;
	const float x = Rect.Left + (Rect.GetWidth() - w) * 0.5f;
	const float y = Rect.Top + toolbarHeight + (availableHeight - h) * 0.5f;

	ParticleEditorWindow->Initialize(x, y, w, h, World, Device);
}

void USlateManager::OpenParticleEditorWindowWithSystem(UParticleSystem* System)
{
	if (!ParticleEditorWindow)
	{
		OpenParticleEditorWindow();
	}

	if (ParticleEditorWindow && System)
	{
		ParticleEditorWindow->SetParticleSystem(System);
	}
}

void USlateManager::OpenParticleEditorWindowWithFile(const char* FilePath)
{
	if (!ParticleEditorWindow)
	{
		OpenParticleEditorWindow();
	}

	if (ParticleEditorWindow && FilePath)
	{
		ParticleEditorWindow->LoadParticleSystem(FilePath);
	}
}

void USlateManager::CloseParticleEditorWindow()
{
	if (!ParticleEditorWindow)
	{
		return;
	}

	delete ParticleEditorWindow;
	ParticleEditorWindow = nullptr;
}

void USlateManager::OpenPhysicsAssetEditorWindow()
{
	if (PhysicsAssetEditorWindow)
	{
		return; // 이미 열려있음
	}

	PhysicsAssetEditorWindow = new SPhysicsAssetEditorWindow();

	// 화면 중앙에 800x600 크기로 배치
	float w = 900.0f;
	float h = 650.0f;
	float x = (Rect.GetWidth() - w) * 0.5f;
	float y = (Rect.GetHeight() - h) * 0.5f;

	PhysicsAssetEditorWindow->Initialize(x, y, w, h, World, Device);
}

void USlateManager::ClosePhysicsAssetEditorWindow()
{
	if (!PhysicsAssetEditorWindow)
	{
		return;
	}

	delete PhysicsAssetEditorWindow;
	PhysicsAssetEditorWindow = nullptr;
}

void USlateManager::RequestSceneLoad(const FString& ScenePath)
{
	if (MainViewport && !ScenePath.empty())
	{
		MainViewport->RequestSceneLoad(ScenePath);
		UE_LOG("USlateManager: Scene load requested: %s", ScenePath.c_str());
	}
}

void USlateManager::SwitchLayout(EViewportLayoutMode NewMode)
{
    if (NewMode == CurrentMode) return;

    if (NewMode == EViewportLayoutMode::FourSplit)
    {
        TopPanel->SideLT = LeftPanel;
    }
    else if (NewMode == EViewportLayoutMode::SingleMain)
    {
        TopPanel->SideLT = MainViewport;
    }

    CurrentMode = NewMode;
}

void USlateManager::SwitchPanel(SWindow* SwitchPanel)
{
    if (TopPanel->SideLT != SwitchPanel) {
        TopPanel->SideLT = SwitchPanel;
        CurrentMode = EViewportLayoutMode::SingleMain;
    }
    else {
        TopPanel->SideLT = LeftPanel;
        CurrentMode = EViewportLayoutMode::FourSplit;
    }
}

void USlateManager::Render()
{
    // 메인 툴바 렌더링 (항상 최상단에)
    MainToolbar->RenderWidget();
    if (TopPanel)
    {
        TopPanel->OnRender();
    }

    // Content Browser 오버레이 렌더링 (하단에서 슬라이드 업)
    if (ContentBrowserWindow && ContentBrowserAnimationProgress > 0.0f)
    {
        extern float CLIENTWIDTH;
        extern float CLIENTHEIGHT;

        // 부드러운 감속을 위한 ease-out 곡선 적용
        float EasedProgress = 1.0f - (1.0f - ContentBrowserAnimationProgress) * (1.0f - ContentBrowserAnimationProgress);

        // 좌우 여백을 포함한 Content Browser 크기 계산
        float ContentBrowserHeight = CLIENTHEIGHT * ContentBrowserHeightRatio;
        float ContentBrowserWidth = CLIENTWIDTH - (ContentBrowserHorizontalMargin * 2.0f);
        float ContentBrowserXPos = ContentBrowserHorizontalMargin;

        // Y 위치 계산 (하단에서 슬라이드 업)
        float YPosWhenHidden = CLIENTHEIGHT; // 화면 밖 (하단)
        float YPosWhenVisible = CLIENTHEIGHT - ContentBrowserHeight; // 화면 내 (하단)
        float CurrentYPos = YPosWhenHidden + (YPosWhenVisible - YPosWhenHidden) * EasedProgress;

        // 둥근 모서리 스타일 적용
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.6f, 0.8f, 0.8f));

        // 윈도우 위치 및 크기 설정
        ImGui::SetNextWindowPos(ImVec2(ContentBrowserXPos, CurrentYPos));
        ImGui::SetNextWindowSize(ImVec2(ContentBrowserWidth, ContentBrowserHeight));

        // 윈도우 플래그
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar;

        // Content Browser 렌더링
        bool isWindowOpen = true;
        if (ImGui::Begin("ContentBrowserOverlay", &isWindowOpen, flags))
        {
            // 포커스를 잃으면 닫기
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                bIsContentBrowserVisible &&
                !bIsContentBrowserAnimating)
            {
                ToggleContentBrowser(); // Content Browser 닫기
            }

            // 둥근 모서리가 있는 반투명 배경 추가
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 WindowPos = ImGui::GetWindowPos();
            ImVec2 WindowSize = ImGui::GetWindowSize();
            DrawList->AddRectFilled(
                WindowPos,
                ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y),
                IM_COL32(25, 25, 30, 240), // 높은 불투명도의 어두운 배경
                12.0f // 둥근 정도
            );

            // Content Browser 내용 렌더링
            if (ContentBrowserWindow)
            {
                ContentBrowserWindow->RenderContent();
            }
        }
        ImGui::End();

        // 스타일 변수 및 색상 복원
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
    }

    // 콘솔 오버레이 렌더링 (모든 것 위에 표시)
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        extern float CLIENTWIDTH;
        extern float CLIENTHEIGHT;

        // 부드러운 감속을 위한 ease-out 곡선 적용
        float EasedProgress = 1.0f - (1.0f - ConsoleAnimationProgress) * (1.0f - ConsoleAnimationProgress);

        // 최대 높이 제한 적용
        float MaxConsoleHeight = CLIENTHEIGHT * ConsoleMaxHeightRatio;
        float ClampedConsoleHeight = std::max(ConsoleMinHeight, std::min(ConsoleHeight, MaxConsoleHeight));

        // 좌우 여백을 포함한 콘솔 크기 계산
        float ConsoleWidth = CLIENTWIDTH - (ConsoleHorizontalMargin * 2.0f);
        float ConsoleXPos = ConsoleHorizontalMargin;

        // Y 위치 계산 (하단에서 슬라이드 업)
        float YPosWhenHidden = CLIENTHEIGHT; // 화면 밖 (하단)
        float YPosWhenVisible = CLIENTHEIGHT - ClampedConsoleHeight; // 화면 내 (하단)
        float CurrentYPos = YPosWhenHidden + (YPosWhenVisible - YPosWhenHidden) * EasedProgress;

        // 둥근 모서리 스타일 적용
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));

        // 윈도우 위치 및 크기 설정
        ImGui::SetNextWindowPos(ImVec2(ConsoleXPos, CurrentYPos));
        ImGui::SetNextWindowSize(ImVec2(ConsoleWidth, ClampedConsoleHeight));

        // 윈도우 플래그
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;

        // 처음 열렸을 때 콘솔에 포커스
        if (bConsoleShouldFocus)
        {
            ImGui::SetNextWindowFocus();
            bConsoleShouldFocus = false;
        }

        // 콘솔 렌더링
        bool isWindowOpen = true;
        if (ImGui::Begin("ConsoleOverlay", &isWindowOpen, flags))
        {
            // 둥근 모서리가 있는 반투명 배경 추가
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 WindowPos = ImGui::GetWindowPos();
            ImVec2 WindowSize = ImGui::GetWindowSize();
            DrawList->AddRectFilled(
                WindowPos,
                ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y),
                IM_COL32(20, 20, 20, 240), // 높은 불투명도의 어두운 배경
                12.0f // 둥근 정도
            );

            // === 상단 Status Bar (드래그로 높이 조절) ===
            ImVec2 StatusBarMin = WindowPos;
            ImVec2 StatusBarMax = ImVec2(WindowPos.x + WindowSize.x - 30.0f, WindowPos.y + ConsoleStatusBarHeight);

            // Status bar 영역에 드래그 핸들 표시 (중앙에 짧은 라인)
            float HandleWidth = 50.0f;
            float HandleX = WindowPos.x + (WindowSize.x - 30.0f - HandleWidth) * 0.5f;
            DrawList->AddRectFilled(
                ImVec2(HandleX, WindowPos.y + 2.0f),
                ImVec2(HandleX + HandleWidth, WindowPos.y + ConsoleStatusBarHeight - 2.0f),
                IM_COL32(100, 100, 100, 180),
                2.0f
            );

            // 마우스가 status bar 위에 있는지 확인
            ImVec2 MousePos = ImGui::GetMousePos();
            bool bMouseOverStatusBar = (MousePos.x >= StatusBarMin.x && MousePos.x <= StatusBarMax.x &&
                                        MousePos.y >= StatusBarMin.y && MousePos.y <= StatusBarMax.y);

            // 드래그 시작
            if (bMouseOverStatusBar && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                bIsConsoleDragging = true;
                ConsoleDragStartY = MousePos.y;
                ConsoleDragStartHeight = ConsoleHeight;
            }

            // 드래그 중 (status bar 위에서 드래그 시작한 경우에만)
            if (bIsConsoleDragging)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    float DeltaY = ConsoleDragStartY - MousePos.y; // 위로 드래그하면 양수
                    ConsoleHeight = ConsoleDragStartHeight + DeltaY;
                    ConsoleHeight = std::max(ConsoleMinHeight, std::min(ConsoleHeight, MaxConsoleHeight));
                }
                else
                {
                    bIsConsoleDragging = false;
                }

                // 커서 변경
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            else if (bMouseOverStatusBar)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }

            // === X 버튼 (우상단) ===
            float CloseButtonSize = 20.0f;
            ImVec2 CloseButtonPos = ImVec2(WindowPos.x + WindowSize.x - CloseButtonSize - 8.0f, WindowPos.y + 4.0f);
            ImVec2 CloseButtonMax = ImVec2(CloseButtonPos.x + CloseButtonSize, CloseButtonPos.y + CloseButtonSize);

            bool bMouseOverClose = (MousePos.x >= CloseButtonPos.x && MousePos.x <= CloseButtonMax.x &&
                                    MousePos.y >= CloseButtonPos.y && MousePos.y <= CloseButtonMax.y);

            // X 버튼 배경
            ImU32 CloseButtonColor = bMouseOverClose ? IM_COL32(180, 60, 60, 220) : IM_COL32(80, 80, 80, 180);
            DrawList->AddRectFilled(CloseButtonPos, CloseButtonMax, CloseButtonColor, 4.0f);

            // X 그리기
            float XPadding = 5.0f;
            ImU32 XColor = IM_COL32(220, 220, 220, 255);
            DrawList->AddLine(
                ImVec2(CloseButtonPos.x + XPadding, CloseButtonPos.y + XPadding),
                ImVec2(CloseButtonMax.x - XPadding, CloseButtonMax.y - XPadding),
                XColor, 2.0f);
            DrawList->AddLine(
                ImVec2(CloseButtonMax.x - XPadding, CloseButtonPos.y + XPadding),
                ImVec2(CloseButtonPos.x + XPadding, CloseButtonMax.y - XPadding),
                XColor, 2.0f);

            // X 버튼 클릭 처리
            if (bMouseOverClose && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                ToggleConsole(); // 콘솔 닫기
            }

            // Status bar 아래에 콘솔 내용 렌더링
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ConsoleStatusBarHeight);

            // 콘솔 위젯 렌더링
            ConsoleWindow->RenderWidget();
        }
        ImGui::End();

        // 스타일 변수 및 색상 복원
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(3);
    }

    // Render detached viewer on top
    if (SkeletalViewerWindow)
    {
        SkeletalViewerWindow->OnRender();
    }

    // Render BlendSpace2D Editor
    if (BlendSpace2DEditorWindow)
    {
        BlendSpace2DEditorWindow->OnRender();

        // 윈도우가 닫혔으면 삭제
        if (!BlendSpace2DEditorWindow->IsOpen())
        {
            CloseBlendSpace2DEditor();
        }
    }

    if (AnimStateMachineWindow)
    {
        AnimStateMachineWindow->OnRender();
    }

    // Render Particle Editor Window (Cascade 스타일)
    if (ParticleEditorWindow)
    {
        ParticleEditorWindow->OnRender();

        // 윈도우가 닫혔으면 삭제
        if (!ParticleEditorWindow->IsOpen())
        {
            CloseParticleEditorWindow();
        }
    }

    // Render Physics Asset Editor Window
    if (PhysicsAssetEditorWindow)
    {
        PhysicsAssetEditorWindow->OnRender();

        // 윈도우가 닫혔으면 삭제
        if (!PhysicsAssetEditorWindow->IsOpen())
        {
            ClosePhysicsAssetEditorWindow();
        }
    }

    // 로딩 UI (우상단)
    auto& RM = UResourceManager::GetInstance();
    int32 PendingCount = RM.GetPendingLoadCount();
    bool bIsLoading = PendingCount > 0;

    // 로딩 완료 감지: 이전 프레임에 로딩 중이었고, 이번 프레임에 완료됨
    if (bWasLoadingLastFrame && !bIsLoading)
    {
        // 로딩 세션 완료, 다음 로딩 세션을 위해 카운터 리셋
        FAsyncLoader::Get().ResetSessionCounters();
    }
    bWasLoadingLastFrame = bIsLoading;

    if (bIsLoading)
    {
        // 화면 중앙 하단에 로딩 UI 표시
        ImGui::SetNextWindowPos(ImVec2((CLIENTWIDTH - 310) * 0.5f, CLIENTHEIGHT - 120 - 50), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(310, 120), ImGuiCond_Always);

        ImGui::Begin("Loading Assets", nullptr,
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar);

        ImGui::Text("Loading Assets...");
        ImGui::Separator();

        float Progress = RM.GetLoadProgress();
        ImGui::ProgressBar(Progress, ImVec2(-1, 0));

        int32 CompletedCount = RM.GetCompletedCount();
        int32 TotalCount = FAsyncLoader::Get().GetTotalRequestedCount();
        ImGui::Text("%d / %d assets loaded", CompletedCount, TotalCount);

        TArray<FString> CurrentAssets = RM.GetCurrentlyLoadingAssets();
        if (!CurrentAssets.IsEmpty())
        {
            ImGui::Separator();

            // 윈도우 너비에 맞춰 텍스트 자르기 (1줄 유지)
            FString AssetPath = CurrentAssets[0];
            float AvailableWidth = ImGui::GetContentRegionAvail().x;

            // 텍스트 크기 계산
            ImVec2 TextSize = ImGui::CalcTextSize(AssetPath.c_str());

            // 너비 초과 시 "..."로 자르기
            if (TextSize.x > AvailableWidth)
            {
                const char* Ellipsis = "...";
                float EllipsisWidth = ImGui::CalcTextSize(Ellipsis).x;
                float TargetWidth = AvailableWidth - EllipsisWidth;

                // 이진 탐색으로 적절한 길이 찾기
                int32 Left = 0;
                int32 Right = static_cast<int32>(AssetPath.length());
                int32 BestLength = 0;

                while (Left <= Right)
                {
                    int32 Mid = (Left + Right) / 2;
                    FString Truncated = AssetPath.substr(0, Mid);
                    float Width = ImGui::CalcTextSize(Truncated.c_str()).x;

                    if (Width <= TargetWidth)
                    {
                        BestLength = Mid;
                        Left = Mid + 1;
                    }
                    else
                    {
                        Right = Mid - 1;
                    }
                }

                AssetPath = AssetPath.substr(0, BestLength) + Ellipsis;
            }

            ImGui::Text("%s", AssetPath.c_str());
        }

        ImGui::End();
    }
}

void USlateManager::RenderAfterUI()
{
    // ImGui::Image 방식으로 전환하면서 OnRenderViewport 호출 불필요
    // 각 윈도우의 RenderToPreviewRenderTarget이 전용 렌더 타겟에 렌더링함
    // (OnRenderViewport를 호출하면 메인 백버퍼에 중복 렌더링됨)
}

void USlateManager::Update(float DeltaSeconds)
{
    ProcessInput();
    // MainToolbar 업데이트
    MainToolbar->Update();

    if (TopPanel)
    {
        // 툴바 높이만큼 아래로 이동 (50px)
        const float toolbarHeight = 50.0f;
        TopPanel->Rect = FRect(0, toolbarHeight, CLIENTWIDTH, CLIENTHEIGHT);
        TopPanel->OnUpdate(DeltaSeconds);
    }

    if (SkeletalViewerWindow)
    {
        SkeletalViewerWindow->OnUpdate(DeltaSeconds);
    }

    if (BlendSpace2DEditorWindow)
    {
        BlendSpace2DEditorWindow->OnUpdate(DeltaSeconds);
    }

    if (ParticleEditorWindow)
    {
        ParticleEditorWindow->OnUpdate(DeltaSeconds);
    }

    if (PhysicsAssetEditorWindow)
    {
        PhysicsAssetEditorWindow->OnUpdate(DeltaSeconds);
    }

    // 콘솔 애니메이션 업데이트
    if (bIsConsoleAnimating)
    {
        if (bIsConsoleVisible)
        {
            // 애니메이션 인 (나타남)
            ConsoleAnimationProgress += DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress >= 1.0f)
            {
                ConsoleAnimationProgress = 1.0f;
                bIsConsoleAnimating = false;
            }
        }
        else
        {
            // 애니메이션 아웃 (사라짐)
            ConsoleAnimationProgress -= DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress <= 0.0f)
            {
                ConsoleAnimationProgress = 0.0f;
                bIsConsoleAnimating = false;
            }
        }
    }

    // Content Browser 애니메이션 업데이트
    if (bIsContentBrowserAnimating)
    {
        if (bIsContentBrowserVisible)
        {
            // 애니메이션 인 (나타남)
            ContentBrowserAnimationProgress += DeltaSeconds / ContentBrowserAnimationDuration;
            if (ContentBrowserAnimationProgress >= 1.0f)
            {
                ContentBrowserAnimationProgress = 1.0f;
                bIsContentBrowserAnimating = false;
            }
        }
        else
        {
            // 애니메이션 아웃 (사라짐)
            ContentBrowserAnimationProgress -= DeltaSeconds / ContentBrowserAnimationDuration;
            if (ContentBrowserAnimationProgress <= 0.0f)
            {
                ContentBrowserAnimationProgress = 0.0f;
                bIsContentBrowserAnimating = false;
            }
        }
    }

    // ConsoleWindow 업데이트
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        ConsoleWindow->Update();
    }
}

void USlateManager::ProcessInput()
{
    const FVector2D MousePosition = INPUT.GetMousePosition();

    // Check if any tool window is focused and should block editor input
    bool bToolWindowBlockingInput = (ParticleEditorWindow && ParticleEditorWindow->ShouldBlockEditorInput());

    // Update main editor gizmo interaction state BEFORE processing any input
    // This ensures the gizmo doesn't respond to global InputManager state
    if (World && World->GetGizmoActor())
    {
        World->GetGizmoActor()->SetInteractionEnabled(!bToolWindowBlockingInput);
    }

    if (SkeletalViewerWindow && SkeletalViewerWindow->Rect.Contains(MousePosition))
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            OnMouseDown(MousePosition, 0);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            OnMouseDown(MousePosition, 1);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            OnMouseUp(MousePosition, 0);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            OnMouseUp(MousePosition, 1);
        }
    }

    if (INPUT.IsMouseButtonPressed(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 1);
        }
    }
    if (INPUT.IsMouseButtonReleased(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonReleased(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 1);
        }
    }
    OnMouseMove(MousePosition);

    // Alt + ` (억음 부호 키)로 콘솔 토글
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent) && ImGui::GetIO().KeyAlt)
    {
        ToggleConsole();
    }

    // Ctrl + Space로 Content Browser 토글
    if (ImGui::IsKeyPressed(ImGuiKey_Space) && ImGui::GetIO().KeyCtrl)
    {
        ToggleContentBrowser();
    }

    // Ctrl + Shift + P로 Physics Asset Editor 토글
    if (ImGui::IsKeyPressed(ImGuiKey_P) && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
    {
        if (IsPhysicsAssetEditorWindowOpen())
            ClosePhysicsAssetEditorWindow();
        else
            OpenPhysicsAssetEditorWindow();
    }

    // ESC closes the Skeletal Mesh Viewer if open
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && SkeletalViewerWindow)
    {
        CloseSkeletalMeshViewer();
    }

    // ParticleEditorWindow가 포커스된 경우 EditorWorld 입력 차단
    bool bParticleEditorFocused = ParticleEditorWindow && ParticleEditorWindow->ShouldBlockEditorInput();

    // Update main editor gizmo interaction state based on tool window focus
    if (World->GetGizmoActor())
    {
        World->GetGizmoActor()->SetInteractionEnabled(!bParticleEditorFocused);
    }

    if (bParticleEditorFocused)
    {
        return;
    }

    // 단축키로 기즈모 모드 변경
    if (World->GetGizmoActor())
    {
        World->GetGizmoActor()->ProcessGizmoModeSwitch();
    }
}

void USlateManager::OnMouseMove(FVector2D MousePos)
{
    // Route to detached viewer if hovered or if gizmo is being dragged (for smooth bone editing)
    if (SkeletalViewerWindow)
    {
        AGizmoActor* Gizmo = SkeletalViewerWindow->GetGizmoActor();
        bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());
        if (bGizmoDragging || SkeletalViewerWindow->IsHover(MousePos))
        {
            SkeletalViewerWindow->OnMouseMove(MousePos);
            return;
        }
    }

    // Route to BlendSpace2D editor if hovered
    if (BlendSpace2DEditorWindow && BlendSpace2DEditorWindow->IsHover(MousePos))
    {
        BlendSpace2DEditorWindow->OnMouseMove(MousePos);
        return;
    }

    // Route to Particle Editor if hovered
    if (ParticleEditorWindow && ParticleEditorWindow->IsHover(MousePos))
    {
        ParticleEditorWindow->OnMouseMove(MousePos);
        return;
    }

    if (ActiveViewport)
    {
        ActiveViewport->OnMouseMove(MousePos);
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseMove(MousePos);
    }
}

void USlateManager::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    // 플로팅 윈도우가 열려있고 마우스가 그 영역 안에 있으면 해당 윈도우에 이벤트 전달
    // IsOpen 체크를 추가하여 윈도우가 실제로 열려있는지 확인
    if (SkeletalViewerWindow && SkeletalViewerWindow->IsOpen() && SkeletalViewerWindow->Rect.Contains(MousePos))
    {
        SkeletalViewerWindow->OnMouseDown(MousePos, Button);
        return;
    }

    if (BlendSpace2DEditorWindow && BlendSpace2DEditorWindow->IsOpen() && BlendSpace2DEditorWindow->Rect.Contains(MousePos))
    {
        BlendSpace2DEditorWindow->OnMouseDown(MousePos, Button);
        return;
    }

    if (ParticleEditorWindow && ParticleEditorWindow->IsOpen() && ParticleEditorWindow->Rect.Contains(MousePos))
    {
        ParticleEditorWindow->OnMouseDown(MousePos, Button);
        return;
    }

    if (ActiveViewport)
    {
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseDown(MousePos, Button);

        // 어떤 뷰포트 안에서 눌렸는지 확인
        for (auto* VP : Viewports)
        {
            if (VP && VP->Rect.Contains(MousePos))
            {
                ActiveViewport = VP; // 고정

                // 우클릭인 경우 커서 숨김 및 잠금
                if (Button == 1)
                {
                    INPUT.SetCursorVisible(false);
                    INPUT.LockCursor();
                }
                break;
            }
        }
    }
}

void USlateManager::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // 우클릭 해제 시 커서 복원 (ActiveViewport와 무관하게 처리)
    if (Button == 1 && INPUT.IsCursorLocked())
    {
        INPUT.SetCursorVisible(true);
        INPUT.ReleaseCursor();
    }

    if (SkeletalViewerWindow && SkeletalViewerWindow->Rect.Contains(MousePos))
    {
        SkeletalViewerWindow->OnMouseUp(MousePos, Button);
        // do not return; still allow panels to finish mouse up
    }

    if (BlendSpace2DEditorWindow && BlendSpace2DEditorWindow->Rect.Contains(MousePos))
    {
        BlendSpace2DEditorWindow->OnMouseUp(MousePos, Button);
        // do not return; still allow panels to finish mouse up
    }

    if (ParticleEditorWindow && ParticleEditorWindow->Rect.Contains(MousePos))
    {
        ParticleEditorWindow->OnMouseUp(MousePos, Button);
        // do not return; still allow panels to finish mouse up
    }

    if (ActiveViewport)
    {
        ActiveViewport->OnMouseUp(MousePos, Button);
        ActiveViewport = nullptr; // 드래그 끝나면 해제
    }
    // NOTE: ActiveViewport가 있더라도 Up 이벤트는 항상 보내주어 드래그 관련 버그를 제거
    if (TopPanel)
    {
        TopPanel->OnMouseUp(MousePos, Button);
    }
}

void USlateManager::OnShutdown()
{
    SaveSplitterConfig();
}

void USlateManager::Shutdown()
{
    if (bIsShutdown) { return; }
    bIsShutdown = true;
    // 레이아웃/설정 저장
    SaveSplitterConfig();

    // 콘솔 윈도우 삭제
    if (ConsoleWindow)
    {
        delete ConsoleWindow;
        ConsoleWindow = nullptr;
        UE_LOG("USlateManager: ConsoleWindow destroyed");
    }

    // Content Browser 윈도우 삭제
    if (ContentBrowserWindow)
    {
        delete ContentBrowserWindow;
        ContentBrowserWindow = nullptr;
        UE_LOG("USlateManager: ContentBrowserWindow destroyed");
    }

    // Thumbnail Manager 종료
    FThumbnailManager::GetInstance().Shutdown();
    UE_LOG("USlateManager: ThumbnailManager shutdown");

    // D3D 컨텍스트를 해제하기 위해 UI 패널과 뷰포트를 명시적으로 삭제
    if (TopPanel) { delete TopPanel; TopPanel = nullptr; }
    if (LeftTop) { delete LeftTop; LeftTop = nullptr; }
    if (LeftBottom) { delete LeftBottom; LeftBottom = nullptr; }
    if (LeftPanel) { delete LeftPanel; LeftPanel = nullptr; }
    if (RightPanel) { delete RightPanel; RightPanel = nullptr; }

    if (ControlPanel) { delete ControlPanel; ControlPanel = nullptr; }
    if (DetailPanel) { delete DetailPanel; DetailPanel = nullptr; }

    for (int i = 0; i < 4; ++i)
    {
        if (Viewports[i]) { delete Viewports[i]; Viewports[i] = nullptr; }
    }
    MainViewport = nullptr;
    ActiveViewport = nullptr;

    if (BlendSpace2DEditorWindow)
    {
        delete BlendSpace2DEditorWindow;
        BlendSpace2DEditorWindow = nullptr;
    }
    if (ParticleEditorWindow)
    {
        delete ParticleEditorWindow;
        ParticleEditorWindow = nullptr;
    }
	CloseSkeletalMeshViewer();
	CloseAnimStateMachineWindow();
}

void USlateManager::SetPIEWorld(UWorld* InWorld)
{
    MainViewport->SetVClientWorld(InWorld);
    // PIE에도 Main Camera Set
    InWorld->SetEditorCameraActor(MainViewport->GetViewportClient()->GetCamera());
}

void USlateManager::ToggleConsole()
{
    bIsConsoleVisible = !bIsConsoleVisible;
    bIsConsoleAnimating = true;

    // 콘솔을 열 때 포커스 플래그 설정
    if (bIsConsoleVisible)
    {
        bConsoleShouldFocus = true;
    }
}

void USlateManager::ForceOpenConsole()
{
    if (!bIsConsoleVisible)
    {
        // 2. 토글 함수를 호출하여 열기 상태(true)로 전환하고 애니메이션 시작
        ToggleConsole();
    }
}

void USlateManager::ToggleContentBrowser()
{
    bIsContentBrowserVisible = !bIsContentBrowserVisible;
    bIsContentBrowserAnimating = true;
}

bool USlateManager::IsContentBrowserVisible() const
{
    return bIsContentBrowserVisible;
}
