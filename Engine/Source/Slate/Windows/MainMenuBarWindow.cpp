#include "pch.h"
#include "MainMenuBarWindow.h"
#include <DirectXTK/DDSTextureLoader.h>

IMPLEMENT_CLASS(UMainMenuBarWindow)

UMainMenuBarWindow::UMainMenuBarWindow()
    : UUIWindow()
{
    // 윈도우 설정 - 메뉴바는 일반 UIWindow와 다르게 동작
    GetMutableConfig().WindowTitle = "MainMenuBar";
    GetMutableConfig().bResizable = false;
    GetMutableConfig().bMovable = false;
    GetMutableConfig().bCollapsible = false;
    GetMutableConfig().Priority = -100;  // 가장 먼저 렌더링
}

void UMainMenuBarWindow::Initialize()
{
    UE_LOG("MainMenuBarWindow: Initialized");
}

void UMainMenuBarWindow::LoadIcons()
{
    D3D11RHI* RHI = GEngine.GetRHIDevice();
    if (!RHI)
    {
        return;
    }

    ID3D11Device* Device = RHI->GetDevice();

    // 앱 아이콘
    DirectX::CreateDDSTextureFromFile(
        Device,
        L"Data/Default/Icon/FutureLogo.dds",
        nullptr,
        AppIconSRV.GetAddressOf()
    );

    // 윈도우 컨트롤 아이콘
    DirectX::CreateDDSTextureFromFile(
        Device,
        L"Data/Default/Icon/WindowMinimize.dds",
        nullptr,
        MinimizeIconSRV.GetAddressOf()
    );

    DirectX::CreateDDSTextureFromFile(
        Device,
        L"Data/Default/Icon/WindowMaximize.dds",
        nullptr,
        MaximizeIconSRV.GetAddressOf()
    );

    DirectX::CreateDDSTextureFromFile(
        Device,
        L"Data/Default/Icon/WindowClose.dds",
        nullptr,
        CloseIconSRV.GetAddressOf()
    );
}

void UMainMenuBarWindow::RenderMenuBar()
{
    // 아이콘이 모두 로드될 때까지 매 프레임 시도
    if (!AppIconSRV || !MinimizeIconSRV || !MaximizeIconSRV || !CloseIconSRV)
    {
        LoadIcons();
    }

    if (!bIsMenuBarVisible)
    {
        MenuBarHeight = 0.0f;
        return;
    }

    const ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
    constexpr float TopPadding = 4.0f;
    constexpr float SingleLineHeight = 22.0f;  // 메뉴바 높이
    constexpr float LevelTabHeight = 28.0f;
    constexpr float TotalHeight = TopPadding + SingleLineHeight + LevelTabHeight;  // 패딩 + 메뉴바 + 레벨탭

    MenuBarHeight = TotalHeight;

    // 테마 색상 정의 (전체에서 공유)
    constexpr ImVec4 MenuBarBg = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);  // 메뉴바 배경
    constexpr ImVec4 TextColor = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    constexpr ImVec4 HeaderHovered = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    constexpr ImVec4 HeaderActive = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);

    // === 0. 상단 메뉴바 영역 배경 채우기 (패딩 + 메뉴바만) ===
    ImDrawList* BgDrawList = ImGui::GetBackgroundDrawList();
    BgDrawList->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(ScreenSize.x, TopPadding + SingleLineHeight),
        ImGui::ColorConvertFloat4ToU32(MenuBarBg)
    );

    // === 1. 메뉴바 (첫 번째 줄) ===
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));  // 투명 배경
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, MenuBarBg);
    ImGui::PushStyleColor(ImGuiCol_Text, TextColor);
    ImGui::PushStyleColor(ImGuiCol_Header, MenuBarBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HeaderHovered);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, HeaderActive);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(0.0f, TopPadding));
    ImGui::SetNextWindowSize(ImVec2(ScreenSize.x, SingleLineHeight));

    ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("MainMenuBarContainer", nullptr, Flags);

    // === 앱 아이콘 렌더링 (메뉴바 윈도우에서 클리핑 확장) ===
    if (AppIconSRV)
    {
        constexpr float IconX = 10.0f;
        const float IconY = TopPadding + 3.0f;
        constexpr float IconSize = 42.0f;

        ImDrawList* MenuDrawList = ImGui::GetWindowDrawList();
        MenuDrawList->PushClipRect(ImVec2(0, 0), ImVec2(ScreenSize.x, ScreenSize.y), false);
        MenuDrawList->AddImage(
            reinterpret_cast<ImTextureID>(AppIconSRV.Get()),
            ImVec2(IconX, IconY),
            ImVec2(IconX + IconSize, IconY + IconSize)
        );
        MenuDrawList->PopClipRect();
    }

    if (ImGui::BeginMenuBar())
    {
        // 로고 영역 여백 - 커서 위치 직접 설정
        ImGui::SetCursorPosX(LeftPadding);

        // 드롭다운 메뉴 스타일 (팝업 패딩)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

        // 메뉴 렌더링
        RenderFileMenu();
        RenderViewMenu();
        RenderWindowsMenu();
        RenderHelpMenu();

        ImGui::PopStyleVar(3);

        // 윈도우 컨트롤 버튼
        RenderWindowControls();

        ImGui::EndMenuBar();
    }
    ImGui::End();

    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(6);

    // === 2. 레벨 탭 바 (두 번째 줄) - 커스텀 탭 렌더링 ===
    // 테마 색상 정의
    constexpr ImVec4 Background = ImVec4(0.11f, 0.11f, 0.14f, 1.0f);
    constexpr ImVec4 TabColor = ImVec4(0.18f, 0.19f, 0.23f, 1.0f);
    constexpr ImVec4 SeparatorColor = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);

    constexpr float LevelTabY = TopPadding + SingleLineHeight;

    // 레벨탭 배경을 BackgroundDrawList로 그림 (아이콘보다 뒤에)
    BgDrawList->AddRectFilled(
        ImVec2(0.0f, LevelTabY),
        ImVec2(ScreenSize.x, LevelTabY + LevelTabHeight),
        ImGui::ColorConvertFloat4ToU32(Background)
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));  // 투명

    ImGui::SetNextWindowPos(ImVec2(0.0f, LevelTabY));
    ImGui::SetNextWindowSize(ImVec2(ScreenSize.x, LevelTabHeight));

    ImGuiWindowFlags LevelTabFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    // 현재 레벨 이름 가져오기
    FString LevelName = "New Level";
    if (GWorld)
    {
        LevelName = GWorld->GetLevelName();
    }

    ImGui::Begin("LevelTabBar", nullptr, LevelTabFlags);
    {
        // 윈도우 내부 DrawList 사용 (플로팅 윈도우보다 아래에 렌더링됨)
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        // 탭 위치 및 크기 계산
        const float TabX = LeftPadding;
        constexpr float TabY = LevelTabY + 4.0f;
        constexpr float TabPaddingX = 12.0f;
        constexpr float TabRounding = 6.0f;

        // 텍스트 크기 계산
        ImVec2 TextSize = ImGui::CalcTextSize(LevelName.c_str());
        const float TabWidth = TextSize.x + TabPaddingX * 2.0f;
        const float TabHeight = LevelTabHeight - 6.0f;  // 상하 여백 고려

        // 탭 배경 (상단만 라운딩)
        DrawList->AddRectFilled(
            ImVec2(TabX, TabY),
            ImVec2(TabX + TabWidth, TabY + TabHeight),
            ImGui::ColorConvertFloat4ToU32(TabColor),
            TabRounding,
            ImDrawFlags_RoundCornersTop
        );

        // 탭 텍스트
        ImVec2 TextPos = ImVec2(
            TabX + TabPaddingX,
            TabY + (TabHeight - TextSize.y) * 0.5f
        );
        DrawList->AddText(TextPos, ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.85f, 0.85f, 1.0f)), LevelName.c_str());

        // 하단 구분선 (전체 너비)
        const float SeparatorY = LevelTabY + LevelTabHeight - 1.0f;
        DrawList->AddLine(
            ImVec2(0.0f, SeparatorY),
            ImVec2(ScreenSize.x, SeparatorY),
            ImGui::ColorConvertFloat4ToU32(SeparatorColor),
            1.0f
        );

    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(3);
}

void UMainMenuBarWindow::RenderFileMenu()
{
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Level", "Ctrl+N"))
        {
            UE_LOG("MainMenuBar: New Level");
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Open Level", "Ctrl+O"))
        {
            UE_LOG("MainMenuBar: Open Level");
        }

        if (ImGui::MenuItem("Save Level", "Ctrl+S"))
        {
            UE_LOG("MainMenuBar: Save Level");
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4"))
        {
            PostMessageW(GEngine.GetHWND(), WM_CLOSE, 0, 0);
        }

        ImGui::EndMenu();
    }
}

void UMainMenuBarWindow::RenderViewMenu()
{
    if (ImGui::BeginMenu("View"))
    {
        if (ImGui::MenuItem("Lit"))
        {
            UE_LOG("MainMenuBar: View Mode - Lit");
        }

        if (ImGui::MenuItem("Unlit"))
        {
            UE_LOG("MainMenuBar: View Mode - Unlit");
        }

        if (ImGui::MenuItem("Wireframe"))
        {
            UE_LOG("MainMenuBar: View Mode - Wireframe");
        }

        ImGui::EndMenu();
    }
}

void UMainMenuBarWindow::RenderWindowsMenu()
{
    if (ImGui::BeginMenu("Windows"))
    {
        // UIManager에서 등록된 윈도우 목록을 가져와서 토글
        const auto& AllWindows = UI.GetAllUIWindows();

        for (auto* Window : AllWindows)
        {
            if (!Window)
            {
                continue;
            }

            // MainMenuBar는 제외
            if (Window->GetWindowTitle() == "MainMenuBar")
            {
                continue;
            }

            if (ImGui::MenuItem(Window->GetWindowTitle().c_str(), nullptr, Window->IsVisible()))
            {
                Window->ToggleVisibility();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show All"))
        {
            UI.ShowAllWindows();
        }

        if (ImGui::MenuItem("Hide All"))
        {
            UI.HideAllWindows();
        }

        ImGui::EndMenu();
    }
}

void UMainMenuBarWindow::RenderHelpMenu()
{
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About", "F1"))
        {
            UE_LOG("MainMenuBar: About");
        }

        ImGui::EndMenu();
    }
}

void UMainMenuBarWindow::RenderWindowControls()
{
    // 포커스 여부와 관계없이 동작하도록 GetHWND 사용
    HWND MainWindowHandle = GEngine.GetHWND();
    if (!MainWindowHandle)
    {
        return;
    }

    // 우측 정렬을 위해 여백 계산
    constexpr float ButtonWidth = 46.0f;
    constexpr float ButtonHeight = 20.0f;
    constexpr float IconSize = 16.0f;
    constexpr float ButtonCount = 3.0f;
    constexpr float Padding = 30.0f;
    constexpr float TotalWidth = ButtonWidth * ButtonCount + Padding;

    // DisplaySize 사용 (윈도우 크기 변경 시 즉시 반영)
    const float ScreenWidth = ImGui::GetIO().DisplaySize.x;
    const float ButtonPosX = ScreenWidth - TotalWidth;

    // 버튼이 화면 밖으로 나가지 않도록 보호
    if (ButtonPosX > 0.0f)
    {
        ImGui::SameLine(ButtonPosX);
    }
    else
    {
        ImGui::SameLine();
    }

    // 버튼 프레임 패딩 설정
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((ButtonWidth - IconSize) * 0.5f, (ButtonHeight - IconSize) * 0.5f));

    // 최소화/최대화 버튼 스타일 (밝은 배경)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.40f, 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.38f, 1.0f));

    // 최소화 버튼
    if (MinimizeIconSRV)
    {
        if (ImGui::ImageButton("##Minimize", reinterpret_cast<ImTextureID>(MinimizeIconSRV.Get()), ImVec2(IconSize, IconSize)))
        {
            ShowWindow(MainWindowHandle, SW_MINIMIZE);
        }
    }
    else
    {
        if (ImGui::Button("-", ImVec2(ButtonWidth, ButtonHeight)))
        {
            ShowWindow(MainWindowHandle, SW_MINIMIZE);
        }
    }

    // 최대화 / 복원 버튼
    ImGui::SameLine();
    bool bIsMaximized = IsZoomed(MainWindowHandle);
    if (MaximizeIconSRV)
    {
        if (ImGui::ImageButton("##Maximize", reinterpret_cast<ImTextureID>(MaximizeIconSRV.Get()), ImVec2(IconSize, IconSize)))
        {
            SendMessageW(MainWindowHandle, WM_SYSCOMMAND, bIsMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);
        }
    }
    else
    {
        if (ImGui::Button(bIsMaximized ? "[]" : "[ ]", ImVec2(ButtonWidth, ButtonHeight)))
        {
            SendMessageW(MainWindowHandle, WM_SYSCOMMAND, bIsMaximized ? SC_RESTORE : SC_MAXIMIZE, 0);
        }
    }

    ImGui::PopStyleColor(3);

    // 닫기 버튼 (호버 시에만 레드)
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.28f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.20f, 0.20f, 1.0f));

    if (CloseIconSRV)
    {
        if (ImGui::ImageButton("##Close", CloseIconSRV.Get(), ImVec2(IconSize, IconSize)))
        {
            PostMessageW(MainWindowHandle, WM_CLOSE, 0, 0);
        }
    }
    else
    {
        if (ImGui::Button("X", ImVec2(ButtonWidth, ButtonHeight)))
        {
            PostMessageW(MainWindowHandle, WM_CLOSE, 0, 0);
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}
