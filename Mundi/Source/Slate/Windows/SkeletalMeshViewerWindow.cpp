#include "pch.h"
#include "SkeletalMeshViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "BoneAnchorComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/InputCore/InputManager.h"
#include <cmath> // for fmod

SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

bool SSkeletalMeshViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // 타임라인 아이콘 로드
    IconGoToFront = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_Front_24x.png");
    IconGoToFrontOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_Front_24x_OFF.png");
    IconStepBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Backwards_24x.png");
    IconStepBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Backwards_24x_OFF.png");
    IconBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Backwards_24x.png");
    IconBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Backwards_24x_OFF.png");
    IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Record_24x.png");
    IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Pause_24x.png");
    IconPauseOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Pause_24x_OFF.png");
    IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Play_24x.png");
    IconPlayOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Play_24x_OFF.png");
    IconStepForward = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Forward_24x.png");
    IconStepForwardOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Forward_24x_OFF.png");
    IconGoToEnd = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_End_24x.png");
    IconGoToEndOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_End_24x_OFF.png");
    IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Loop_24x.png");
    IconLoopOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Loop_24x_OFF.png");

    // Create first tab/state
    OpenNewTab("Viewer 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // If window is closed, don't render
    if (!bIsOpen)
    {
        return;
    }

    // 탭이 없으면 렌더링하지 않음
    if (Tabs.Num() == 0)
    {
        bIsOpen = false;
        return;
    }

    // Parent detachable window (movable, top-level) with solid background
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }
    bool bViewerVisible = false;
    if (ImGui::Begin("Skeletal Mesh Viewer", &bIsOpen, flags))
    {
        bViewerVisible = true;
        // Render tab bar and switch active state
        int32 PreviousTabIndex = ActiveTabIndex;
        bool bTabClosed = false;
        if (ImGui::BeginTabBar("SkeletalViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.Num(); ++i)
            {
                ViewerState* State = Tabs[i];
                bool open = true;
                if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
                {
                    ActiveTabIndex = i;
                    ActiveState = State;
                    ImGui::EndTabItem();
                }
                if (!open)
                {
                    CloseTab(i);
                    bTabClosed = true;
                    break;
                }
            }
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            {
                char label[32]; sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
                OpenNewTab(label);
            }
            ImGui::EndTabBar();
        }

        // 탭이 닫혔으면 즉시 종료 (댕글링 포인터 방지)
        if (bTabClosed)
        {
            ImGui::End();
            return;
        }

        // 탭 전환 시 애니메이션 자동 재생
        if (ActiveState && PreviousTabIndex != ActiveTabIndex)
        {
            if (ActiveState->CurrentAnimation && ActiveState->PreviewActor)
            {
                USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                if (SkelComp)
                {
                    UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
                    if (AnimInst && !ActiveState->bIsPlaying)
                    {
                        AnimInst->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->PlaybackSpeed);
                        ActiveState->bIsPlaying = true;
                    }
                }
            }
        }
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        // Animation 모드일 때는 좌측 패널 숨김
        bool bShowLeftPanel = (ActiveState && ActiveState->ViewMode == EViewerMode::Skeletal);
        float leftWidth = bShowLeftPanel ? (totalWidth * LeftPanelRatio) : 0.0f;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;

        // Animation 모드일 때는 Timeline을 하단에 배치 (전체 높이의 40% 할당)
        bool bIsAnimationMode = (ActiveState && ActiveState->ViewMode == EViewerMode::Animation);
        float timelineHeight = bIsAnimationMode ? (totalHeight * 0.4f) : 0.0f; // 40% 할당
        float mainAreaHeight = totalHeight - timelineHeight;

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel - Asset Browser & Bone Hierarchy (Skeletal 모드에서만 표시)
        if (bShowLeftPanel)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, mainAreaHeight), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();

        if (ActiveState)
        {
            // Asset Browser Section
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::Indent(8.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::Text("Asset Browser");
            ImGui::PopFont();
            ImGui::Unindent(8.0f);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Mesh path section
            ImGui::BeginGroup();
            ImGui::Text("Mesh Path:");
            ImGui::PushItemWidth(-1.0f);
            ImGui::InputTextWithHint("##MeshPath", "Browse for FBX file...", ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer));
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // Buttons
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

            float buttonWidth = (leftWidth - 24.0f) * 0.5f - 4.0f;
            if (ImGui::Button("Browse...", ImVec2(buttonWidth, 32)))
            {
                auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
                if (!widePath.empty())
                {
                    std::string s = widePath.string();
                    strncpy_s(ActiveState->MeshPathBuffer, s.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.50f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.30f, 1.0f));
            if (ImGui::Button("Load FBX", ImVec2(buttonWidth, 32)))
            {
                FString Path = ActiveState->MeshPathBuffer;
                if (!Path.empty())
                {
                    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
                    if (Mesh && ActiveState->PreviewActor)
                    {
                        ActiveState->PreviewActor->SetSkeletalMesh(Path);
                        ActiveState->CurrentMesh = Mesh;
                        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading
                        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                        {
                            Skeletal->SetVisibility(ActiveState->bShowMesh);
                        }
                        ActiveState->bBoneLinesDirty = true;
                        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                        {
                            LineComp->ClearLines();
                            LineComp->SetLineVisible(ActiveState->bShowBones);
                        }
                    }
                }
            }
            ImGui::PopStyleColor(6);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Display Options
            ImGui::BeginGroup();
            ImGui::Text("Display Options:");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.30f, 0.35f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.70f, 1.00f, 1.0f));

            if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                {
                    ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(ActiveState->bShowMesh);
                }
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
                }
                if (ActiveState->bShowBones)
                {
                    ActiveState->bBoneLinesDirty = true;
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Animation Import Section
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::Indent(8.0f);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::Text("Animation Import");
            ImGui::PopFont();
            ImGui::Unindent(8.0f);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::BeginGroup();
            ImGui::Text("Animation Path:");
            ImGui::PushItemWidth(-1.0f);
            ImGui::InputTextWithHint("##AnimPath", "Browse for animation FBX...", ActiveState->AnimPathBuffer, sizeof(ActiveState->AnimPathBuffer));
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // Skeleton Selection ComboBox (선택사항 - Without Skin 전용)
            ImGui::Text("Target Skeleton (Optional):");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Required for Without Skin FBX.");
                ImGui::Text("Optional for With Skin FBX (auto-detected).");
                ImGui::EndTooltip();
            }

            TArray<USkeletalMesh*> AllSkeletalMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();

            FString PreviewValueStr = "Auto-detect or Select...";
            if (ActiveState->SelectedSkeletonMesh)
            {
                const FSkeleton* Skeleton = ActiveState->SelectedSkeletonMesh->GetSkeleton();
                if (Skeleton)
                {
                    // Skeleton Name이 비어있으면 파일명 사용
                    if (!Skeleton->Name.empty())
                    {
                        PreviewValueStr = Skeleton->Name;
                    }
                    else
                    {
                        PreviewValueStr = ActiveState->SelectedSkeletonMesh->GetFilePath();
                    }
                }
            }

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::BeginCombo("##SkeletonCombo", PreviewValueStr.c_str()))
            {
                // "None" 옵션 추가 (With Skin의 경우)
                bool bNoneSelected = (ActiveState->SelectedSkeletonMesh == nullptr);
                if (ImGui::Selectable("Auto-detect (With Skin)", bNoneSelected))
                {
                    ActiveState->SelectedSkeletonIndex = -1;
                    ActiveState->SelectedSkeletonMesh = nullptr;
                }

                for (int32 i = 0; i < AllSkeletalMeshes.Num(); ++i)
                {
                    USkeletalMesh* Mesh = AllSkeletalMeshes[i];
                    const FSkeleton* Skeleton = Mesh->GetSkeleton();
                    if (Skeleton)
                    {
                        bool bIsSelected = (ActiveState->SelectedSkeletonMesh == Mesh);

                        // 레이블 생성: Skeleton 이름이 있으면 사용, 없으면 파일명 사용
                        FString Label;
                        if (!Skeleton->Name.empty())
                        {
                            Label = Skeleton->Name;
                        }
                        else
                        {
                            Label = Mesh->GetFilePath();
                        }

                        if (ImGui::Selectable(Label.c_str(), bIsSelected))
                        {
                            ActiveState->SelectedSkeletonIndex = i;
                            ActiveState->SelectedSkeletonMesh = Mesh;

                            // 선택된 SkeletalMesh 로드
                            if (ActiveState->PreviewActor)
                            {
                                USkeletalMeshComponent* Component = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                                if (Component)
                                {
                                    Component->SetSkeletalMesh(Mesh->GetFilePath());
                                    UE_LOG("Loaded SkeletalMesh: %s", Mesh->GetFilePath().c_str());

                                    // 애니메이션 리스트도 업데이트 (첫 번째 애니메이션 자동 선택 및 재생)
                                    const TArray<UAnimSequence*>& Animations = Mesh->GetAnimations();
                                    if (Animations.Num() > 0)
                                    {
                                        ActiveState->SelectedAnimationIndex = 0;
                                        ActiveState->CurrentAnimation = Animations[0];
                                        ActiveState->CurrentAnimationTime = 0.0f;
                                        // 편집된 bone transform 캐시 클리어 (새로운 메시/애니메이션 로드)
                                        ActiveState->EditedBoneTransforms.clear();

                                        // Working Range 초기화 (0 ~ TotalFrames)
                                        if (ActiveState->CurrentAnimation && ActiveState->CurrentAnimation->GetDataModel())
                                        {
                                            int32 TotalFrames = ActiveState->CurrentAnimation->GetDataModel()->GetNumberOfFrames();
                                            ActiveState->WorkingRangeStartFrame = 0;
                                            ActiveState->WorkingRangeEndFrame = TotalFrames;
                                            ActiveState->ViewRangeStartFrame = 0;
                                            ActiveState->ViewRangeEndFrame = FMath::Min(30, TotalFrames);  // View Range 초기값 30
                                        }

                                        // AnimInstance 생성 또는 재사용 후 첫 번째 애니메이션 재생
                                        UAnimInstance* AnimInst = Component->GetAnimInstance();
                                        if (!AnimInst)
                                        {
                                            UE_LOG("[Animation] Creating new AnimInstance for auto-play");
                                            AnimInst = NewObject<UAnimInstance>();
                                            if (AnimInst)
                                            {
                                                Component->SetAnimInstance(AnimInst);
                                            }
                                            else
                                            {
                                                UE_LOG("[Animation] ERROR: Failed to create AnimInstance!");
                                            }
                                        }

                                        if (AnimInst)
                                        {
                                            UE_LOG("[Animation] Auto-playing first animation (PlaybackSpeed: %.2f)", ActiveState->PlaybackSpeed);
                                            AnimInst->PlayAnimation(Animations[0], ActiveState->PlaybackSpeed);
                                            ActiveState->bIsPlaying = true;
                                        }

                                        UE_LOG("Auto-selected first animation (%d total animations)", Animations.Num());
                                    }
                                    else
                                    {
                                        ActiveState->SelectedAnimationIndex = -1;
                                        ActiveState->CurrentAnimation = nullptr;
                                        ActiveState->bIsPlaying = false;
                                        // 편집된 bone transform 캐시 클리어
                                        ActiveState->EditedBoneTransforms.clear();
                                    }
                                }
                            }
                        }

                        if (bIsSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // Import Buttons
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

            float animButtonWidth = (leftWidth - 24.0f) * 0.5f - 4.0f;
            if (ImGui::Button("Browse Anim...", ImVec2(animButtonWidth, 32)))
            {
                auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
                if (!widePath.empty())
                {
                    std::string s = widePath.string();
                    strncpy_s(ActiveState->AnimPathBuffer, s.c_str(), sizeof(ActiveState->AnimPathBuffer) - 1);
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.40f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.50f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.30f, 0.15f, 1.0f));

            // 경로만 있으면 Import 가능 (With Skin은 auto-detect, Without Skin은 Skeleton 필요)
            bool bCanImport = strlen(ActiveState->AnimPathBuffer) > 0;
            if (!bCanImport)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            }

            if (ImGui::Button("Import Anim", ImVec2(animButtonWidth, 32)) && bCanImport)
            {
                FString AnimPath = ActiveState->AnimPathBuffer;

                // 1. FBX에서 Skeleton 추출 (With Skin인지 확인)
                FSkeleton* FbxSkeleton = UFbxLoader::GetInstance().ExtractSkeletonFromFbx(AnimPath);

                if (FbxSkeleton)
                {
                    // With Skin FBX - Skeleton이 포함됨
                    UE_LOG("Detected With Skin FBX. Checking skeleton compatibility...");

                    // 기존 SkeletalMesh들과 호환성 체크
                    TArray<USkeletalMesh*> AllSkeletalMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();
                    USkeletalMesh* CompatibleMesh = nullptr;

                    for (USkeletalMesh* Mesh : AllSkeletalMeshes)
                    {
                        const FSkeleton* ExistingSkeleton = Mesh->GetSkeleton();
                        if (ExistingSkeleton && ExistingSkeleton->IsCompatibleWith(*FbxSkeleton))
                        {
                            CompatibleMesh = Mesh;
                            UE_LOG("Found compatible skeleton: %s", ExistingSkeleton->Name.c_str());
                            break;
                        }
                    }

                    if (CompatibleMesh)
                    {
                        // 호환되는 Skeleton이 있음 - 모든 AnimStack을 Animation으로 추출
                        const FSkeleton* TargetSkeleton = CompatibleMesh->GetSkeleton();
                        TArray<UAnimSequence*> AnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(
                            AnimPath,
                            *TargetSkeleton
                        );

                        if (AnimSequences.Num() > 0)
                        {
                            UE_LOG("========================================");
                            UE_LOG("Animations Imported Successfully (With Skin - Animation Only)");
                            UE_LOG("  File: %s", AnimPath.c_str());
                            UE_LOG("  Total AnimStacks: %d", AnimSequences.Num());
                            UE_LOG("========================================");

                            for (UAnimSequence* AnimSequence : AnimSequences)
                            {
                                if (!AnimSequence)
                                    continue;

                                // ViewerState의 ImportedAnimSequences에 등록 (메모리 관리용)
                                ActiveState->ImportedAnimSequences.Add(AnimSequence);

                                // 호환되는 모든 SkeletalMesh에 Animation 추가
                                int32 AddedCount = 0;
                                for (USkeletalMesh* Mesh : AllSkeletalMeshes)
                                {
                                    const FSkeleton* ExistingSkeleton = Mesh->GetSkeleton();
                                    if (ExistingSkeleton && ExistingSkeleton->IsCompatibleWith(*FbxSkeleton))
                                    {
                                        Mesh->AddAnimation(AnimSequence);
                                        AddedCount++;
                                    }
                                }

                                if (UAnimDataModel* DataModel = AnimSequence->GetDataModel())
                                {
                                    UE_LOG("  [AnimStack: %s]", AnimSequence->GetName().c_str());
                                    UE_LOG("    Duration: %.2f seconds", DataModel->GetPlayLength());
                                    UE_LOG("    Frame Rate: %d FPS", DataModel->GetFrameRate().Numerator);
                                    UE_LOG("    Total Frames: %d", DataModel->GetNumberOfFrames());
                                    UE_LOG("    Bone Tracks: %d", DataModel->GetBoneAnimationTracks().Num());
                                    UE_LOG("    Applied to %d compatible SkeletalMesh(es)", AddedCount);
                                }
                            }

                            UE_LOG("========================================");
                        }
                        else
                        {
                            UE_LOG("ERROR: Failed to import animations: %s", AnimPath.c_str());
                        }
                    }
                    else
                    {
                        // 호환되는 Skeleton이 없음 - 새로운 SkeletalMesh로 로드
                        UE_LOG("No compatible skeleton found. Importing as new SkeletalMesh...");
                        USkeletalMesh* NewMesh = UFbxLoader::GetInstance().LoadFbxMesh(AnimPath);
                        if (NewMesh)
                        {
                            UE_LOG("Imported new SkeletalMesh with animation: %s", AnimPath.c_str());
                        }
                        else
                        {
                            UE_LOG("Failed to import SkeletalMesh: %s", AnimPath.c_str());
                        }
                    }

                    delete FbxSkeleton;
                }
                else
                {
                    // Without Skin FBX - Skeleton이 없음, 선택한 Skeleton 사용
                    UE_LOG("Detected Without Skin FBX. Using selected skeleton...");

                    if (!ActiveState->SelectedSkeletonMesh)
                    {
                        UE_LOG("ERROR: Without Skin FBX requires a target skeleton to be selected!");
                        // TODO: UI에 에러 메시지 표시
                    }
                    else
                    {
                        const FSkeleton* TargetSkeleton = ActiveState->SelectedSkeletonMesh->GetSkeleton();
                        if (TargetSkeleton)
                        {
                            TArray<UAnimSequence*> AnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(
                                AnimPath,
                                *TargetSkeleton
                            );

                            if (AnimSequences.Num() > 0)
                            {
                                UE_LOG("========================================");
                                UE_LOG("Animations Imported Successfully (Without Skin)");
                                UE_LOG("  File: %s", AnimPath.c_str());
                                UE_LOG("  Total AnimStacks: %d", AnimSequences.Num());
                                UE_LOG("  Target Skeleton: %s", TargetSkeleton->Name.c_str());
                                UE_LOG("========================================");

                                for (UAnimSequence* AnimSequence : AnimSequences)
                                {
                                    if (!AnimSequence)
                                        continue;

                                    // ViewerState의 ImportedAnimSequences에 등록 (메모리 관리용)
                                    ActiveState->ImportedAnimSequences.Add(AnimSequence);

                                    ActiveState->SelectedSkeletonMesh->AddAnimation(AnimSequence);

                                    UAnimDataModel* DataModel = AnimSequence->GetDataModel();
                                    if (DataModel)
                                    {
                                        UE_LOG("  [AnimStack: %s]", AnimSequence->GetName().c_str());
                                        UE_LOG("    Duration: %.2f seconds", DataModel->GetPlayLength());
                                        UE_LOG("    Frame Rate: %d FPS", DataModel->GetFrameRate().Numerator);
                                        UE_LOG("    Total Frames: %d", DataModel->GetNumberOfFrames());
                                        UE_LOG("    Bone Tracks: %d", DataModel->GetBoneAnimationTracks().Num());
                                    }
                                }

                                UE_LOG("  Total Animations on this mesh: %d", ActiveState->SelectedSkeletonMesh->GetAnimations().Num());
                                UE_LOG("========================================");
                            }
                            else
                            {
                                UE_LOG("ERROR: Failed to import animations: %s", AnimPath.c_str());
                            }
                        }
                    }
                }
            }

            if (!bCanImport)
            {
                ImGui::PopStyleVar();
            }

            ImGui::PopStyleColor(6);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Bone Hierarchy Section
            ImGui::Text("Bone Hierarchy:");
            ImGui::Spacing();

            if (!ActiveState->CurrentMesh)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No skeletal mesh loaded.");
                ImGui::PopStyleColor();
            }
            else
            {
                const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                if (!Skeleton || Skeleton->Bones.IsEmpty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("This mesh has no skeleton data.");
                    ImGui::PopStyleColor();
                }
                else
                {
                    // Scrollable tree view
                    ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), true);
                    const TArray<FBone>& Bones = Skeleton->Bones;
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

                    std::function<void(int32)> DrawNode = [&](int32 Index)
                    {
                        const bool bLeaf = Children[Index].IsEmpty();
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

                        if (bLeaf)
                        {
                            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        }

                        // 펼쳐진 노드는 명시적으로 열린 상태로 설정
                        if (ActiveState->ExpandedBoneIndices.count(Index) > 0)
                        {
                            ImGui::SetNextItemOpen(true);
                        }

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::PushID(Index);
                        const char* Label = Bones[Index].Name.c_str();

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
                        }

                        bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PopStyleColor(3);

                            // 선택된 본까지 스크롤
                            ImGui::SetScrollHereY(0.5f);
                        }

                        // 사용자가 수동으로 노드를 접거나 펼쳤을 때 상태 업데이트
                        if (ImGui::IsItemToggledOpen())
                        {
                            if (open)
                                ActiveState->ExpandedBoneIndices.insert(Index);
                            else
                                ActiveState->ExpandedBoneIndices.erase(Index);
                        }

                        if (ImGui::IsItemClicked())
                        {
                            if (ActiveState->SelectedBoneIndex != Index)
                            {
                                ActiveState->SelectedBoneIndex = Index;
                                ActiveState->bBoneLinesDirty = true;

                                ExpandToSelectedBone(ActiveState, Index);

                                if (ActiveState->PreviewActor && ActiveState->World)
                                {
                                    ActiveState->PreviewActor->RepositionAnchorToBone(Index);
                                    if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                    {
                                        // BoneAnchor에 ViewerState 설정 (편집된 transform 캐시 접근용)
                                        Anchor->SetViewerState(ActiveState);
                                        ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                        ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                    }
                                }
                            }
                        }

                        if (!bLeaf && open)
                        {
                            for (int32 Child : Children[Index])
                            {
                                DrawNode(Child);
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    };

                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        if (Bones[i].ParentIndex < 0)
                        {
                            DrawNode(i);
                        }
                    }

                    ImGui::EndChild();
                }
            }
        }
        else
        {
            ImGui::EndChild();
            ImGui::End();
            return;
        }
            ImGui::EndChild();

            ImGui::SameLine(0, 0); // No spacing between panels
        }

        // Center panel (viewport area) — draw with border to see the viewport area
        ImGui::BeginChild("SkeletalMeshViewport", ImVec2(centerWidth, mainAreaHeight), true, ImGuiWindowFlags_NoScrollbar);

        // Viewport rendering area (전체 높이 사용)
        ImGui::BeginChild("ViewportRenderArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
        ImVec2 childPos = ImGui::GetWindowPos();
        ImVec2 childSize = ImGui::GetWindowSize();
        ImVec2 rectMin = childPos;
        ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);
        CenterRect.Left = rectMin.x; CenterRect.Top = rectMin.y; CenterRect.Right = rectMax.x; CenterRect.Bottom = rectMax.y; CenterRect.UpdateMinMax();
        ImGui::EndChild();

        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Right panel - Bone Properties
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, mainAreaHeight), true);
        ImGui::PopStyleVar();

        // View Mode Toggle Buttons (상단)
        if (ActiveState)
        {
            ImGui::Indent(8.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

            bool bIsSkeletalMode = (ActiveState->ViewMode == EViewerMode::Skeletal);
            bool bIsAnimationMode = (ActiveState->ViewMode == EViewerMode::Animation);

            float buttonWidth = (rightWidth - 24.0f) * 0.5f;

            if (bIsSkeletalMode)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
            }

            if (ImGui::Button("Skeletal", ImVec2(buttonWidth, 0)))
            {
                ActiveState->ViewMode = EViewerMode::Skeletal;
            }

            if (bIsSkeletalMode)
            {
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            if (bIsAnimationMode)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
            }

            if (ImGui::Button("Animation", ImVec2(buttonWidth, 0)))
            {
                ActiveState->ViewMode = EViewerMode::Animation;

                // Animation 모드 진입 시 첫 번째 애니메이션 자동 선택 및 재생
                if (ActiveState->CurrentMesh && !ActiveState->CurrentAnimation)
                {
                    const TArray<UAnimSequence*>& Animations = ActiveState->CurrentMesh->GetAnimations();
                    if (Animations.Num() > 0)
                    {
                        ActiveState->SelectedAnimationIndex = 0;
                        ActiveState->CurrentAnimation = Animations[0];
                        ActiveState->CurrentAnimationTime = 0.0f;
                        ActiveState->EditedBoneTransforms.clear();
                        ActiveState->bIsPlaying = true;
                    }
                }
            }

            if (bIsAnimationMode)
            {
                ImGui::PopStyleColor(3);
            }

            ImGui::Unindent(8.0f);
            ImGui::Spacing();
        }

        // Panel header
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::Indent(8.0f);
        ImGui::Text(ActiveState && ActiveState->ViewMode == EViewerMode::Animation ? "Animation Properties" : "Bone Properties");
        ImGui::Unindent(8.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // === Animation Mode: Details Panel (Bone Transform) ===
        if (ActiveState->ViewMode == EViewerMode::Animation)
        {
            if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh)
            {
                const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
                {
                    const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

                    // Bone Name
                    ImGui::Text("Bone Name");
                    ImGui::SameLine(100.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                    ImGui::Text("%s", SelectedBone.Name.c_str());
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    // Transforms 섹션
                    if (ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        if (!ActiveState->bBoneRotationEditing)
                        {
                            UpdateBoneTransformFromSkeleton(ActiveState);
                        }

                        float labelWidth = 70.0f;
                        float inputWidth = (rightWidth - labelWidth - 40.0f) / 3.0f;

                        // === Bone (편집 가능) ===
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
                        ImGui::Text("Bone");
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        // Location
                        ImGui::Text("Location");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        bool bLocationChanged = false;
                        bLocationChanged |= ImGui::DragFloat("##BoneLocX", &ActiveState->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "%.2f");
                        ImGui::SameLine();
                        bLocationChanged |= ImGui::DragFloat("##BoneLocY", &ActiveState->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "%.2f");
                        ImGui::SameLine();
                        bLocationChanged |= ImGui::DragFloat("##BoneLocZ", &ActiveState->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "%.2f");
                        ImGui::PopItemWidth();

                        if (bLocationChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }

                        // Rotation
                        ImGui::Text("Rotation");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        bool bRotationChanged = false;

                        if (ImGui::IsAnyItemActive())
                        {
                            ActiveState->bBoneRotationEditing = true;
                        }

                        bRotationChanged |= ImGui::DragFloat("##BoneRotX", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "%.2f");
                        ImGui::SameLine();
                        bRotationChanged |= ImGui::DragFloat("##BoneRotY", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "%.2f");
                        ImGui::SameLine();
                        bRotationChanged |= ImGui::DragFloat("##BoneRotZ", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "%.2f");
                        ImGui::PopItemWidth();

                        if (!ImGui::IsAnyItemActive())
                        {
                            ActiveState->bBoneRotationEditing = false;
                        }

                        if (bRotationChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }

                        // Scale
                        ImGui::Text("Scale");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        bool bScaleChanged = false;
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleX", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "%.2f");
                        ImGui::SameLine();
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleY", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "%.2f");
                        ImGui::SameLine();
                        bScaleChanged |= ImGui::DragFloat("##BoneScaleZ", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "%.2f");
                        ImGui::PopItemWidth();

                        if (bScaleChanged)
                        {
                            ApplyBoneTransform(ActiveState);
                            ActiveState->bBoneLinesDirty = true;
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // === Reference (읽기 전용) ===
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.75f, 0.8f, 1.0f));
                        ImGui::Text("Reference");
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        float zeroVec[3] = {0.0f, 0.0f, 0.0f};
                        float oneVec[3] = {1.0f, 1.0f, 1.0f};

                        // Location
                        ImGui::Text("Location");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##RefLocX", &zeroVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefLocY", &zeroVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefLocZ", &zeroVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        // Rotation
                        ImGui::Text("Rotation");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##RefRotX", &zeroVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefRotY", &zeroVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefRotZ", &zeroVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        // Scale
                        ImGui::Text("Scale");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##RefScaleX", &oneVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefScaleY", &oneVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##RefScaleZ", &oneVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        ImGui::PopStyleVar();

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // === Mesh Relative (읽기 전용) ===
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.75f, 0.8f, 1.0f));
                        ImGui::Text("Mesh Relative");
                        ImGui::PopStyleColor();
                        ImGui::Spacing();

                        // Location
                        ImGui::Text("Location");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##MeshLocX", &zeroVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshLocY", &zeroVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshLocZ", &zeroVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        // Rotation
                        ImGui::Text("Rotation");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##MeshRotX", &zeroVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshRotY", &zeroVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshRotZ", &zeroVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        // Scale
                        ImGui::Text("Scale");
                        ImGui::SameLine(labelWidth);
                        ImGui::PushItemWidth(inputWidth);
                        ImGui::InputFloat("##MeshScaleX", &oneVec[0], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshScaleY", &oneVec[1], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::SameLine();
                        ImGui::InputFloat("##MeshScaleZ", &oneVec[2], 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();

                        ImGui::PopStyleVar();
                    }
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("Select a bone from the skeleton to edit its transform.");
                ImGui::PopStyleColor();
            }
        }
        // === Skeletal Mode: Bone Transform 편집 ===
        else if (ActiveState->ViewMode == EViewerMode::Skeletal)
        {
            if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh)
            {
                const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
                {
                const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

                // Selected bone header with icon-like prefix
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.40f, 1.0f));
                ImGui::Text("> Selected Bone");
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
                ImGui::TextWrapped("%s", SelectedBone.Name.c_str());
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
                ImGui::Separator();
                ImGui::PopStyleColor();

                // 본의 현재 트랜스폼 가져오기 (편집 중이 아닐 때만)
                if (!ActiveState->bBoneRotationEditing)
                {
                    UpdateBoneTransformFromSkeleton(ActiveState);
                }

                ImGui::Spacing();

                // Location 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                ImGui::Text("Location");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.20f, 0.20f, 0.6f));
                bool bLocationChanged = false;
                bLocationChanged |= ImGui::DragFloat("##BoneLocX", &ActiveState->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "X: %.3f");
                bLocationChanged |= ImGui::DragFloat("##BoneLocY", &ActiveState->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "Y: %.3f");
                bLocationChanged |= ImGui::DragFloat("##BoneLocZ", &ActiveState->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "Z: %.3f");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (bLocationChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }

                ImGui::Spacing();

                // Rotation 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                ImGui::Text("Rotation");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.28f, 0.20f, 0.6f));
                bool bRotationChanged = false;

                if (ImGui::IsAnyItemActive())
                {
                    ActiveState->bBoneRotationEditing = true;
                }

                bRotationChanged |= ImGui::DragFloat("##BoneRotX", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "X: %.2f°");
                bRotationChanged |= ImGui::DragFloat("##BoneRotY", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "Y: %.2f°");
                bRotationChanged |= ImGui::DragFloat("##BoneRotZ", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "Z: %.2f°");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (!ImGui::IsAnyItemActive())
                {
                    ActiveState->bBoneRotationEditing = false;
                }

                if (bRotationChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }

                ImGui::Spacing();

                // Scale 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                ImGui::Text("Scale");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                bool bScaleChanged = false;
                bScaleChanged |= ImGui::DragFloat("##BoneScaleX", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "X: %.3f");
                bScaleChanged |= ImGui::DragFloat("##BoneScaleY", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "Y: %.3f");
                bScaleChanged |= ImGui::DragFloat("##BoneScaleZ", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "Z: %.3f");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (bScaleChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }
                }
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("Select a bone from the hierarchy to edit its transform properties.");
                ImGui::PopStyleColor();
            }
        }

        ImGui::EndChild(); // RightPanel

        // Pop the ItemSpacing style
        ImGui::PopStyleVar();

        // Timeline Area (하단, Animation 모드일 때만 표시)
        if (bIsAnimationMode && ActiveState->CurrentAnimation)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Timeline + Animation List 패널 분할
            float bottomPanelWidth = totalWidth;
            float animListWidth = rightWidth; // Animation List 가로를 Property 패널과 동일하게
            float timelineWidth = bottomPanelWidth - animListWidth - 8.0f; // 여백 포함

            // Timeline 패널 (왼쪽)
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("TimelineArea", ImVec2(timelineWidth, timelineHeight - 20), true);
            ImGui::PopStyleVar();

            // 타임라인 컨트롤 렌더링
            if (ActiveState)
            {
                RenderTimelineControls(ActiveState);
            }

            ImGui::EndChild();

            // Animation List 패널 (오른쪽)
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("AnimationListBottom", ImVec2(animListWidth, timelineHeight - 20), true);
            ImGui::PopStyleVar();

            // Animation List 렌더링
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::Text("Animation List");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            if (ActiveState->CurrentMesh)
            {
                const TArray<UAnimSequence*>& Animations = ActiveState->CurrentMesh->GetAnimations();

                if (Animations.Num() > 0)
                {
                    ImGui::BeginChild("AnimListScroll", ImVec2(0, 0), false);

                    for (int32 i = 0; i < Animations.Num(); ++i)
                    {
                        UAnimSequence* Anim = Animations[i];
                        if (!Anim) continue;

                        UAnimDataModel* DataModel = Anim->GetDataModel();
                        if (!DataModel) continue;

                        bool bIsSelected = (ActiveState->SelectedAnimationIndex == i);
                        char LabelBuffer[128];
                        snprintf(LabelBuffer, sizeof(LabelBuffer), "Anim %d (%.1fs)", i, DataModel->GetPlayLength());
                        FString Label = LabelBuffer;

                        if (ImGui::Selectable(Label.c_str(), bIsSelected))
                        {
                            ActiveState->SelectedAnimationIndex = i;
                            ActiveState->CurrentAnimation = Anim;
                            ActiveState->CurrentAnimationTime = 0.0f;
                            ActiveState->EditedBoneTransforms.clear();
                            ActiveState->bIsPlaying = true;

                            // Working Range 초기화 (0 ~ TotalFrames)
                            if (ActiveState->CurrentAnimation && ActiveState->CurrentAnimation->GetDataModel())
                            {
                                int32 TotalFrames = ActiveState->CurrentAnimation->GetDataModel()->GetNumberOfFrames();
                                ActiveState->WorkingRangeStartFrame = 0;
                                ActiveState->WorkingRangeEndFrame = TotalFrames;
                                ActiveState->ViewRangeStartFrame = 0;
                                ActiveState->ViewRangeEndFrame = FMath::Min(60, TotalFrames);  // View Range 초기값 60
                            }

                            // AnimInstance 생성 또는 재사용
                            if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                            {
                                USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                                UAnimInstance* AnimInst = SkelComp->GetAnimInstance();

                                // AnimInstance가 없으면 새로 생성
                                if (!AnimInst)
                                {
                                    UE_LOG("[Animation] Creating new AnimInstance for first animation");
                                    AnimInst = NewObject<UAnimInstance>();
                                    if (AnimInst)
                                    {
                                        SkelComp->SetAnimInstance(AnimInst);
                                    }
                                    else
                                    {
                                        UE_LOG("[Animation] ERROR: Failed to create AnimInstance!");
                                    }
                                }

                                // AnimInstance에 애니메이션 재생
                                if (AnimInst)
                                {
                                    UE_LOG("[Animation] Playing animation, PlaybackSpeed: %.2f", ActiveState->PlaybackSpeed);
                                    AnimInst->PlayAnimation(Anim, ActiveState->PlaybackSpeed);
                                    ActiveState->bIsPlaying = true;
                                }
                            }
                        }
                    }

                    ImGui::EndChild();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("No animations imported.");
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndChild();
        }
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        USlateManager::GetInstance().CloseSkeletalMeshViewer();
    }

    bRequestFocus = false;
}

void SSkeletalMeshViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Spacebar 입력 처리 (Animation 모드이고, Gizmo/BoneAnchor가 선택되지 않았을 때만)
    UInputManager& InputMgr = UInputManager::GetInstance();
    bool bIsGizmoSelected = false;
    if (ActiveState->World && ActiveState->World->GetSelectionManager())
    {
        UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedActorComponent();
        // BoneAnchorComponent 또는 Gizmo 관련 컴포넌트가 선택되어 있으면 Spacebar 무시
        bIsGizmoSelected = (SelectedComp != nullptr);
    }

    if (ActiveState->ViewMode == EViewerMode::Animation &&
        ActiveState->CurrentAnimation &&
        !InputMgr.GetIsGizmoDragging() &&
        !bIsGizmoSelected &&
        InputMgr.IsKeyPressed(VK_SPACE))
    {
        // AnimInstance를 통한 재생/일시정지 토글
        if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            UAnimInstance* AnimInst = ActiveState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
            if (AnimInst)
            {
                if (AnimInst->IsPlaying())
                {
                    // 재생 중이면 일시정지
                    AnimInst->StopAnimation();
                    ActiveState->bIsPlaying = false;
                }
                else
                {
                    // 일시정지 중이면 재생 (정방향, 일시정지 위치에서 시작)
                    ActiveState->PlaybackSpeed = FMath::Abs(ActiveState->PlaybackSpeed);
                    AnimInst->SetPlayRate(ActiveState->PlaybackSpeed);

                    // 현재 애니메이션이 설정되지 않았을 때만 PlayAnimation 호출
                    if (AnimInst->GetCurrentAnimation() != ActiveState->CurrentAnimation)
                    {
                        AnimInst->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->PlaybackSpeed);
                    }
                    else
                    {
                        // 같은 애니메이션이면 현재 위치에서 재생 재개
                        AnimInst->ResumeAnimation();
                    }

                    ActiveState->bIsPlaying = true;
                }
            }
        }
    }

    // Tick the preview world so editor actors (e.g., gizmo) update visibility/state
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);
        if (ActiveState->World->GetGizmoActor())
            ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
    }

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }

    // AnimInstance를 통한 애니메이션 재생 컨트롤
    if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
        UAnimInstance* AnimInst = SkelComp->GetAnimInstance();

        if (AnimInst)
        {
            // UI 상태를 AnimInstance와 동기화
            ActiveState->bIsPlaying = AnimInst->IsPlaying();
            ActiveState->CurrentAnimationTime = AnimInst->GetCurrentTime();

            // PlaybackSpeed가 변경되었으면 AnimInstance에 반영
            if (FMath::Abs(AnimInst->GetPlayRate() - ActiveState->PlaybackSpeed) > 0.001f)
            {
                AnimInst->SetPlayRate(ActiveState->PlaybackSpeed);
            }
        }

        // Animation 탭에서 재생 중일 때만 Component Tick 활성화
        bool bShouldTick = (ActiveState->ViewMode == EViewerMode::Animation) && ActiveState->bIsPlaying;

        static bool bPrevTickState = false;
        if (bShouldTick != bPrevTickState)
        {
            UE_LOG("[Animation] Component Tick: %s (ViewMode=%d, bIsPlaying=%d)",
                bShouldTick ? "ENABLED" : "DISABLED",
                (int)ActiveState->ViewMode,
                ActiveState->bIsPlaying);
            bPrevTickState = bShouldTick;
        }

        SkelComp->SetTickEnabled(bShouldTick);

        // 애니메이션 업데이트 후, 에디터에서 편집된 본 트랜스폼 재적용
        // (0c3c88a 커밋 이전에 UpdateBonesFromAnimation에서 수행하던 로직)
        if (ActiveState->ViewMode == EViewerMode::Animation && !ActiveState->EditedBoneTransforms.empty())
        {
            for (const auto& Pair : ActiveState->EditedBoneTransforms)
            {
                int32 BoneIndex = Pair.first;
                const FTransform& EditedTransform = Pair.second;
                SkelComp->SetBoneLocalTransform(BoneIndex, EditedTransform);
            }
        }
    }
}

void SSkeletalMeshViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SSkeletalMeshViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

        // First, always try gizmo picking (pass to viewport)
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // Left click: if no gizmo was picked, try bone picking
        if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
        {
            // Check if gizmo was picked by checking selection
            UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedComponent();

            // Only do bone picking if gizmo wasn't selected
            if (!SelectedComp || !Cast<UBoneAnchorComponent>(SelectedComp))
            {
                // Get camera from viewport client
                ACameraActor* Camera = ActiveState->Client->GetCamera();
                if (Camera)
                {
                    // Get camera vectors
                    FVector CameraPos = Camera->GetActorLocation();
                    FVector CameraRight = Camera->GetRight();
                    FVector CameraUp = Camera->GetUp();
                    FVector CameraForward = Camera->GetForward();

                    // Calculate viewport-relative mouse position
                    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
                    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

                    // Generate ray from mouse position
                    FRay Ray = MakeRayFromViewport(
                        Camera->GetViewMatrix(),
                        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), ActiveState->Viewport),
                        CameraPos,
                        CameraRight,
                        CameraUp,
                        CameraForward,
                        ViewportMousePos,
                        ViewportSize
                    );

                    // Try to pick a bone
                    float HitDistance;
                    int32 PickedBoneIndex = ActiveState->PreviewActor->PickBone(Ray, HitDistance);

                    if (PickedBoneIndex >= 0)
                    {
                        // Bone was picked
                        ActiveState->SelectedBoneIndex = PickedBoneIndex;
                        ActiveState->bBoneLinesDirty = true;

                        ExpandToSelectedBone(ActiveState, PickedBoneIndex);

                        // Move gizmo to the selected bone
                        ActiveState->PreviewActor->RepositionAnchorToBone(PickedBoneIndex);
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            // BoneAnchor에 ViewerState 설정 (편집된 transform 캐시 접근용)
                            Anchor->SetViewerState(ActiveState);
                            ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                            ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                        }
                    }
                    else
                    {
                        // No bone was picked - clear selection
                        ActiveState->SelectedBoneIndex = -1;
                        ActiveState->bBoneLinesDirty = true;

                        // Hide gizmo and clear selection
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->SetVisibility(false);
                            Anchor->SetEditability(false);
                        }
                        ActiveState->World->GetSelectionManager()->ClearSelection();
                    }
                }
            }
        }
    }
}

void SSkeletalMeshViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OnRenderViewport()
{
    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
        const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // 본 오버레이 재구축 및 기즈모 위치 업데이트
        bool bNeedsRebuild = false;
        bool bIsAnimationPlaying = ActiveState->bIsPlaying;

        if (ActiveState->bShowBones)
        {
            // 애니메이션 재생 중이거나 Dirty 플래그가 설정되어 있으면 재구축
            if (bIsAnimationPlaying || ActiveState->bBoneLinesDirty)
            {
                bNeedsRebuild = true;
                ActiveState->bBoneLinesDirty = false;
            }
        }

        if (bNeedsRebuild && ActiveState->PreviewActor && ActiveState->CurrentMesh)
        {
            if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->SetLineVisible(true);
            }
            // 애니메이션 재생 중이면 모든 본 업데이트
            ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, bIsAnimationPlaying);
        }

        // 애니메이션 재생 중이고 본이 선택되어 있으면 기즈모 위치도 실시간 업데이트
        if (bIsAnimationPlaying && ActiveState->SelectedBoneIndex >= 0 && ActiveState->PreviewActor)
        {
            ActiveState->PreviewActor->RepositionAnchorToBone(ActiveState->SelectedBoneIndex);
        }

        // 뷰포트 렌더링 (ImGui보다 먼저)
        ActiveState->Viewport->Render();
    }
}

void SSkeletalMeshViewerWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!State) return;

    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SSkeletalMeshViewerWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num()) return;

    // ViewerState 해제
    SkeletalViewerBootstrap::DestroyViewerState(Tabs[Index]);
    Tabs.RemoveAt(Index);

    if (Tabs.Num() == 0)
    {
        ActiveTabIndex = -1;
        ActiveState = nullptr;
    }
    else
    {
        ActiveTabIndex = std::min(Index, Tabs.Num() - 1);
        ActiveState = Tabs[ActiveTabIndex];
    }
}

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(const FString& Path)
{
    if (!ActiveState || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && ActiveState->PreviewActor)
    {
        // Set the mesh on the preview actor
        ActiveState->PreviewActor->SetSkeletalMesh(Path);
        ActiveState->CurrentMesh = Mesh;
        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading

        // Update mesh path buffer for display in UI
        strncpy_s(ActiveState->MeshPathBuffer, Path.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(ActiveState->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        ActiveState->bBoneLinesDirty = true;

        // Clear and sync bone line visibility
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(ActiveState->bShowBones);
        }

        // 애니메이션이 있으면 자동으로 Animation 모드로 전환 및 첫 번째 애니메이션 재생
        const TArray<UAnimSequence*>& Animations = Mesh->GetAnimations();
        if (Animations.Num() > 0)
        {
            ActiveState->ViewMode = EViewerMode::Animation;
            ActiveState->SelectedAnimationIndex = 0;
            ActiveState->CurrentAnimation = Animations[0];
            ActiveState->CurrentAnimationTime = 0.0f;
            ActiveState->EditedBoneTransforms.clear();
            ActiveState->bIsPlaying = true;
        }

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SSkeletalMeshViewerWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    // 본의 로컬 트랜스폼에서 값 추출
    const FTransform& BoneTransform = State->PreviewActor->GetSkeletalMeshComponent()->GetBoneLocalTransform(State->SelectedBoneIndex);
    State->EditBoneLocation = BoneTransform.Translation;
    State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = BoneTransform.Scale3D;
}

void SSkeletalMeshViewerWindow::ApplyBoneTransform(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    FTransform NewTransform(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);

    // 캐시에 저장 (애니메이션 재생 중에도 유지되도록)
    State->EditedBoneTransforms[State->SelectedBoneIndex] = NewTransform;

    // SkeletalMeshComponent에 적용
    State->PreviewActor->GetSkeletalMeshComponent()->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SSkeletalMeshViewerWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
{
    if (!State || !State->CurrentMesh)
        return;

    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return;

    // 선택된 본부터 루트까지 모든 부모를 펼침
    int32 CurrentIndex = BoneIndex;
    while (CurrentIndex >= 0)
    {
        State->ExpandedBoneIndices.insert(CurrentIndex);
        CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
    }
}
