#include "pch.h"
#include "DetailsWindow.h"
#include "Windows/PropertyWindow.h"
#include "UIManager.h"
#include "World.h"
#include "EditorEngine.h"
#include "GameModeBase.h"
#include "Pawn.h"
#include "PlayerController.h"

SDetailsWindow::SDetailsWindow()
    : SWindow()
    , DetailsWidget(nullptr)
{
    Initialize();
}

SDetailsWindow::~SDetailsWindow()
{
    delete DetailsWidget;
}

void SDetailsWindow::Initialize()
{
    DetailsWidget = new UPropertyWindow();
    DetailsWidget->Initialize();
}

void SDetailsWindow::OnRender()
{
    // === 패널 위치/크기 고정 (SceneIOWindow와 동일) ===
    ImGui::SetNextWindowPos(ImVec2(Rect.Min.X, Rect.Min.Y));
    ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("디테일", nullptr, flags))
    {
        if (ImGui::BeginTabBar("DetailsTabBar"))
        {
            // Details 탭
            if (ImGui::BeginTabItem("Details"))
            {
                if (DetailsWidget)
                {
                    DetailsWidget->RenderWidget();
                }
                ImGui::EndTabItem();
            }

            // World Settings 탭
            if (ImGui::BeginTabItem("World Settings"))
            {
                RenderWorldSettings();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void SDetailsWindow::OnUpdate(float deltaSecond)
{
    DetailsWidget->Update();
}

void SDetailsWindow::RenderWorldSettings()
{
    UWorld* World = GEngine.GetDefaultWorld();
    if (!World)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No active editor world");
        return;
    }

    ImGui::SeparatorText("Game Mode Settings");
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════
    // GameMode 클래스 선택
    // ═══════════════════════════════════════════════════════════════════════
    UClass* CurrentGameMode = World->GetGameModeClass();
    FString GameModeName = CurrentGameMode ? CurrentGameMode->Name : "None";

    ImGui::Text("GameMode Class:");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##GameModeClass", GameModeName.c_str()))
    {
        // None 옵션
        if (ImGui::Selectable("None", !CurrentGameMode))
        {
            World->SetGameModeClass(nullptr);
        }

        // AGameModeBase 파생 클래스들 나열
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && Class->IsChildOf(AGameModeBase::StaticClass()))
            {
                bool bIsSelected = (CurrentGameMode == Class);
                if (ImGui::Selectable(Class->Name, bIsSelected))
                {
                    World->SetGameModeClass(Class);
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════
    // DefaultPawn 클래스 선택
    // ═══════════════════════════════════════════════════════════════════════
    UClass* CurrentPawn = World->GetDefaultPawnClass();
    FString PawnName = CurrentPawn ? CurrentPawn->Name : "None";

    ImGui::Text("Default Pawn Class:");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##DefaultPawnClass", PawnName.c_str()))
    {
        // None 옵션
        if (ImGui::Selectable("None", !CurrentPawn))
        {
            World->SetDefaultPawnClass(nullptr);
        }

        // APawn 파생 클래스들 나열
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && Class->IsChildOf(APawn::StaticClass()))
            {
                bool bIsSelected = (CurrentPawn == Class);
                if (ImGui::Selectable(Class->Name, bIsSelected))
                {
                    World->SetDefaultPawnClass(Class);
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════
    // PlayerController 클래스 선택
    // ═══════════════════════════════════════════════════════════════════════
    UClass* CurrentController = World->GetPlayerControllerClass();
    FString ControllerName = CurrentController ? CurrentController->Name : "None";

    ImGui::Text("Player Controller Class:");
    ImGui::SameLine();

    if (ImGui::BeginCombo("##PlayerControllerClass", ControllerName.c_str()))
    {
        // None 옵션
        if (ImGui::Selectable("None", !CurrentController))
        {
            World->SetPlayerControllerClass(nullptr);
        }

        // APlayerController 파생 클래스들 나열
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && Class->IsChildOf(APlayerController::StaticClass()))
            {
                bool bIsSelected = (CurrentController == Class);
                if (ImGui::Selectable(Class->Name, bIsSelected))
                {
                    World->SetPlayerControllerClass(Class);
                }

                if (bIsSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ═══════════════════════════════════════════════════════════════════════
    // 현재 설정 정보 표시
    // ═══════════════════════════════════════════════════════════════════════
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "PIE Configuration:");

    if (CurrentGameMode)
    {
        ImGui::Text("  GameMode: %s", CurrentGameMode->Name);
    }

    if (CurrentPawn)
    {
        ImGui::Text("  Default Pawn: %s", CurrentPawn->Name);
    }

    if (CurrentController)
    {
        ImGui::Text("  Player Controller: %s", CurrentController->Name);
    }

    if (!CurrentGameMode && !CurrentPawn && !CurrentController)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No classes configured (using defaults)");
    }
}
