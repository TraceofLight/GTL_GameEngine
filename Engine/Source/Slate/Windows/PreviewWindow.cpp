#include "pch.h"
#include "PreviewWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/InputCore/InputManager.h"
#include "Source/Runtime/Core/Misc/Archive.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include <cmath> // for fmod
#include <filesystem>

SPreviewWindow::SPreviewWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SPreviewWindow::~SPreviewWindow()
{
    // 렌더 타겟 해제
    ReleaseRenderTarget();

    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

bool SPreviewWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // 타임라인 아이콘 로드
    IconGoToFront = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_Front_24x.png");
    IconGoToFrontOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_Front_24x_OFF.png");
    IconStepBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Backwards_24x.png");
    IconStepBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Backwards_24x_OFF.png");
    IconBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Backwards_24x.png");
    IconBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Backwards_24x_OFF.png");
    IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Record_24x.png");
    IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Pause_24x.png");
    IconPauseOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Pause_24x_OFF.png");
    IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Play_24x.png");
    IconPlayOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Play_24x_OFF.png");
    IconStepForward = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Forward_24x.png");
    IconStepForwardOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Forward_24x_OFF.png");
    IconGoToEnd = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_End_24x.png");
    IconGoToEndOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_End_24x_OFF.png");
    IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Loop_24x.png");
    IconLoopOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Loop_24x_OFF.png");

    ScanNotifyLibrary();

    // Create first tab/state
    OpenNewTab("Viewer 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SPreviewWindow::OnRender()
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
    if (ImGui::Begin("Preview", &bIsOpen, flags))
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

        static int HeightLogCount = 0;
        if (HeightLogCount < 5)
        {
            UE_LOG("PreviewWindow: bIsAnimationMode=%d, totalHeight=%.1f, timelineHeight=%.1f",
                   bIsAnimationMode, totalHeight, timelineHeight);
            ++HeightLogCount;
        }

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

                        // 이미 로드된 모든 AnimSequence를 이 메쉬에 추가
                        TArray<UAnimSequence*> AllAnimSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();
                        for (UAnimSequence* AnimSeq : AllAnimSequences)
                        {
                            if (AnimSeq && !AnimSeq->GetName().empty())
                            {
                                Mesh->AddAnimation(AnimSeq);
                            }
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

                                if (ActiveState->PreviewActor && ActiveState->World && ActiveState->CurrentMesh)
                                {
                                    // Set Gizmo to target this bone directly
                                    AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
                                    USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                                    if (GizmoActor && SkelComp)
                                    {
                                        GizmoActor->SetBoneTarget(SkelComp, Index);
                                        GizmoActor->SetbRender(true);
                                    }

                                    // Update selection manager to show gizmo
                                    ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
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

        // 뷰포트 크기 가져오기
        const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
        const uint32 NewWidth = static_cast<uint32>(ViewportSize.x);
        const uint32 NewHeight = static_cast<uint32>(ViewportSize.y);

        // 렌더 타겟 업데이트 (크기 변경 시)
        if (NewWidth > 0 && NewHeight > 0)
        {
            UpdateViewportRenderTarget(NewWidth, NewHeight);

            // 스켈레탈 메쉬 씬을 전용 렌더 타겟에 렌더링
            RenderToPreviewRenderTarget();

            // ImGui에 렌더 결과 표시
            ID3D11ShaderResourceView* PreviewSRV = GetPreviewShaderResourceView();
            if (PreviewSRV)
            {
                // 이미지 렌더링 전 위치 저장
                ImVec2 ImagePos = ImGui::GetCursorScreenPos();

                ImTextureID TextureID = reinterpret_cast<ImTextureID>(PreviewSRV);
                ImGui::Image(TextureID, ViewportSize);

                // CenterRect 업데이트 (이미지 실제 렌더링 영역)
                CenterRect.Left = ImagePos.x;
                CenterRect.Top = ImagePos.y;
                CenterRect.Right = ImagePos.x + ViewportSize.x;
                CenterRect.Bottom = ImagePos.y + ViewportSize.y;
                CenterRect.UpdateMinMax();

                // 팝업/모달이 열려있지 않을 때만 입력 처리
                if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
                {
                    // 마우스가 이미지 위에 있는지 확인하여 뷰포트 입력으로 간주
                    ImVec2 MousePos = ImGui::GetMousePos();
                    if (MousePos.x >= ImagePos.x && MousePos.x <= ImagePos.x + ViewportSize.x &&
                        MousePos.y >= ImagePos.y && MousePos.y <= ImagePos.y + ViewportSize.y)
                    {
                        ImGui::SetItemAllowOverlap();
                    }
                }

                // 드래그 앤 드랍 타겟 (Content Browser에서 .anim 파일 드롭)
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
                    {
                        const char* filePath = static_cast<const char*>(payload->Data);
                        FString path(filePath);

                        // .anim 파일인 경우 애니메이션 임포트
                        if (path.ends_with(".anim"))
                        {
                            // ResourceManager를 통해 AnimSequence 로드
                            UAnimSequence* Anim = UResourceManager::GetInstance().Load<UAnimSequence>(path);
                            if (Anim && ActiveState && ActiveState->CurrentMesh)
                            {
                                // 현재 메쉬에 애니메이션 추가
                                ActiveState->CurrentMesh->AddAnimation(Anim);

                                // 애니메이션 모드로 전환하고 해당 애니메이션 선택
                                ActiveState->ViewMode = EViewerMode::Animation;

                                // 추가된 애니메이션의 인덱스 찾기
                                TArray<UAnimSequence*> Animations = ActiveState->CurrentMesh->GetAnimations();
                                for (int32 i = 0; i < Animations.Num(); ++i)
                                {
                                    if (Animations[i] == Anim)
                                    {
                                        ActiveState->SelectedAnimationIndex = i;
                                        ActiveState->CurrentAnimation = Anim;
                                        ActiveState->CurrentAnimationTime = 0.0f;
                                        break;
                                    }
                                }

                                UE_LOG("PreviewWindow: Imported animation from drop: %s", Anim->GetName().c_str());
                            }
                            else if (!ActiveState || !ActiveState->CurrentMesh)
                            {
                                UE_LOG("PreviewWindow: Cannot import animation - no mesh loaded");
                            }
                        }
                        // .fbx 파일인 경우도 처리 (FBX에서 애니메이션 추출)
                        else if (path.ends_with(".fbx") || path.ends_with(".FBX"))
                        {
                            if (ActiveState && ActiveState->CurrentMesh && ActiveState->CurrentMesh->GetSkeleton())
                            {
                                TArray<UAnimSequence*> AnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(
                                    path, *ActiveState->CurrentMesh->GetSkeleton());

                                if (AnimSequences.Num() > 0)
                                {
                                    TArray<USkeletalMesh*> AllSkeletalMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();

                                    for (UAnimSequence* AnimSeq : AnimSequences)
                                    {
                                        if (!AnimSeq)
                                        {
                                            continue;
                                        }

                                        for (USkeletalMesh* Mesh : AllSkeletalMeshes)
                                        {
                                            Mesh->AddAnimation(AnimSeq);
                                        }
                                    }

                                    // 첫 번째 애니메이션으로 전환
                                    ActiveState->ViewMode = EViewerMode::Animation;
                                    TArray<UAnimSequence*> Animations = ActiveState->CurrentMesh->GetAnimations();
                                    if (Animations.Num() > 0)
                                    {
                                        ActiveState->SelectedAnimationIndex = Animations.Num() - AnimSequences.Num();
                                        ActiveState->CurrentAnimation = Animations[ActiveState->SelectedAnimationIndex];
                                        ActiveState->CurrentAnimationTime = 0.0f;
                                    }

                                    UE_LOG("PreviewWindow: Imported %d animation(s) from FBX: %s", AnimSequences.Num(), path.c_str());
                                }
                                else
                                {
                                    UE_LOG("PreviewWindow: No animations found in FBX: %s", path.c_str());
                                }
                            }
                            else
                            {
                                UE_LOG("PreviewWindow: Cannot import FBX animation - no skeleton available");
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }
        else
        {
            // 유효하지 않은 크기일 경우 CenterRect 초기화
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        // Recording 상태 오버레이 (우하단)
        if (ActiveState && ActiveState->bIsRecording)
        {
            ImGui::SetCursorPos(ImVec2(ViewportSize.x - 180, ViewportSize.y - 40));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.05f, 0.05f, 0.85f));
            ImGui::BeginChild("RecordingOverlay", ImVec2(170, 30), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Text("REC");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            ImGui::Text("%d frames", ActiveState->RecordedFrames.Num());
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

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
                UE_LOG("PreviewWindow: Switched to Animation mode");

                // Animation 모드 진입 시 첫 번째 애니메이션 자동 선택 및 재생
                if (ActiveState->CurrentMesh && !ActiveState->CurrentAnimation)
                {
                    const TArray<UAnimSequence*>& Animations = ActiveState->CurrentMesh->GetAnimations();
                    UE_LOG("PreviewWindow: CurrentMesh=%p, Animations.Num()=%d", ActiveState->CurrentMesh, Animations.Num());
                    if (Animations.Num() > 0)
                    {
                        ActiveState->SelectedAnimationIndex = 0;
                        ActiveState->CurrentAnimation = Animations[0];
                        ActiveState->CurrentAnimationTime = 0.0f;
                        if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr)
                        {
                            SkelComp->ClearAllBoneDeltas();
                        }
                        RebuildNotifyTracks(ActiveState);
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
        if (bIsAnimationMode)
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

            // Save/Save As 버튼 (Animation List 헤더 우측)
            ImGui::SameLine();

            bool bCanSave = (ActiveState->CurrentAnimation != nullptr);

            // Save 버튼 (기존 파일 덮어쓰기)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.80f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.40f, 0.60f, 1.0f));

            if (!bCanSave)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            }

            if (ImGui::SmallButton("Save") && bCanSave)
            {
                UAnimSequence* AnimToSave = ActiveState->CurrentAnimation;
                if (AnimToSave)
                {
                    AnimToSave->Save();
                }
            }

            if (!bCanSave)
            {
                ImGui::PopStyleVar();
            }

            ImGui::PopStyleColor(3);

            // Save As 메뉴 (New Notify Script 스타일)
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.75f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.35f, 0.55f, 1.0f));

            if (!bCanSave)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            }

            static char SaveAsFileName[256] = "";

            if (ImGui::SmallButton("Save As...") && bCanSave)
            {
                ImGui::OpenPopup("SaveAsMenu");
                if (ActiveState->CurrentAnimation)
                {
                    FString CurrentName = ActiveState->CurrentAnimation->GetName();
                    strncpy_s(SaveAsFileName, CurrentName.c_str(), sizeof(SaveAsFileName) - 1);
                }
            }

            if (!bCanSave)
            {
                ImGui::PopStyleVar();
            }

            ImGui::PopStyleColor(3);

            // Save As 팝업 메뉴 (옆에 열림)
            if (ImGui::BeginPopup("SaveAsMenu"))
            {
                ImGui::Text("Enter new file name:");
                ImGui::SetNextItemWidth(250.0f);
                bool bEnterPressed = ImGui::InputTextWithHint("##SaveAsFileName", "e.g., MyAnimation", SaveAsFileName, sizeof(SaveAsFileName), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Save to: Data/Animation/%s.anim", SaveAsFileName);
                ImGui::Spacing();

                if (ImGui::Button("Save", ImVec2(120, 0)) || bEnterPressed)
                {
                    if (strlen(SaveAsFileName) > 0 && ActiveState->CurrentAnimation)
                    {
                        FString NewFileName = SaveAsFileName;
                        FString SavePath = "Data/Animation/" + NewFileName + ".anim";

                        UAnimSequence* SourceAnim = ActiveState->CurrentAnimation;

                        // 원본 상태 저장 (SaveToFile이 Name과 FilePath를 변경하므로)
                        FString OriginalName = SourceAnim->GetName();
                        FString OriginalFilePath = SourceAnim->GetFilePath();

                        // 임시로 Name을 변경하여 저장
                        SourceAnim->SetName(NewFileName);

                        if (SourceAnim->SaveToFile(SavePath))
                        {
                            // 원본 애니메이션 상태 복원
                            SourceAnim->SetName(OriginalName);
                            SourceAnim->SetFilePath(OriginalFilePath);

                            // 새로운 AnimSequence 객체 생성 및 로드
                            UAnimSequence* NewAnim = UResourceManager::GetInstance().Load<UAnimSequence>(SavePath);

                            if (NewAnim && ActiveState->CurrentMesh)
                            {
                                // SkeletalMesh의 Animation List에 추가
                                ActiveState->CurrentMesh->AddAnimation(NewAnim);

                                // 새로 만든 애니메이션을 현재 애니메이션으로 설정
                                const TArray<UAnimSequence*>& Animations = ActiveState->CurrentMesh->GetAnimations();
                                for (int32 i = 0; i < Animations.Num(); ++i)
                                {
                                    if (Animations[i] == NewAnim)
                                    {
                                        ActiveState->SelectedAnimationIndex = i;
                                        ActiveState->CurrentAnimation = NewAnim;
                                        ActiveState->CurrentAnimationTime = 0.0f;
                                        if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr)
                                        {
                                            SkelComp->ClearAllBoneDeltas();
                                        }
                                        ActiveState->bIsPlaying = true;

                                        if (NewAnim->GetDataModel())
                                        {
                                            int32 TotalFrames = NewAnim->GetDataModel()->GetNumberOfFrames();
                                            ActiveState->WorkingRangeStartFrame = 0;
                                            ActiveState->WorkingRangeEndFrame = TotalFrames;
                                        }
                                        break;
                                    }
                                }
                            }

                            UE_LOG("SkeletalMeshViewer: SaveAs: Created new animation %s", SavePath.c_str());
                        }
                        else
                        {
                            // 저장 실패 시 원본 상태 복원
                            SourceAnim->SetName(OriginalName);
                            SourceAnim->SetFilePath(OriginalFilePath);
                        }

                        memset(SaveAsFileName, 0, sizeof(SaveAsFileName));
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    memset(SaveAsFileName, 0, sizeof(SaveAsFileName));
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }


            ImGui::Separator();
            ImGui::Spacing();

            // 로드된 모든 AnimSequence를 표시 (.anim 파일만 - .fbx는 제외)
            TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();

            // .anim 파일만 필터링 (.fbx에서 로드된 것은 제외)
            TArray<UAnimSequence*> Animations;
            for (UAnimSequence* Anim : AllAnimations)
            {
                if (Anim && Anim->GetFilePath().ends_with(".anim"))
                {
                    Animations.push_back(Anim);
                }
            }

            static int LogCount = 0;
            if (LogCount < 5)
            {
                UE_LOG("PreviewWindow: Animation List - Total Animations=%d, CurrentMesh=%p",
                       Animations.Num(), ActiveState->CurrentMesh);
                ++LogCount;
            }

            if (Animations.Num() > 0)
            {
                ImGui::BeginChild("AnimListScroll", ImVec2(0, 0), false);

                for (int32 i = 0; i < Animations.Num(); ++i)
                {
                    UAnimSequence* Anim = Animations[i];
                    if (!Anim) continue;

                    bool bIsSelected = (ActiveState->SelectedAnimationIndex == i);
                    char LabelBuffer[256];

                    // FilePath에서 "파일명#애니메이션이름" 형식으로 표시
                    FString FilePath = Anim->GetFilePath();
                    FString DisplayName;

                    size_t HashPos = FilePath.find('#');
                    if (HashPos != FString::npos)
                    {
                        FString FullPath = FilePath.substr(0, HashPos); // "path/to/file.fbx"
                        FString AnimStackName = FilePath.substr(HashPos + 1); // "AnimStackName"

                        // 파일명만 추출 (경로 제거)
                        size_t LastSlash = FullPath.find_last_of("/\\");
                        FString FileName;
                        if (LastSlash != FString::npos)
                        {
                            FileName = FullPath.substr(LastSlash + 1);
                        }
                        else
                        {
                            FileName = FullPath;
                        }

                        // 확장자 제거
                        size_t DotPos = FileName.find_last_of('.');
                        if (DotPos != FString::npos)
                        {
                            FileName = FileName.substr(0, DotPos);
                        }

                        DisplayName = FileName + "#" + AnimStackName;
                    }
                    else
                    {
                        // FilePath에 #이 없으면 GetName() 사용
                        DisplayName = Anim->GetName();
                        if (DisplayName.empty())
                        {
                            DisplayName = "Anim " + std::to_string(i);
                        }
                    }

                    // DataModel이 있으면 재생 길이 표시
                    UAnimDataModel* DataModel = Anim->GetDataModel();
                    if (DataModel)
                    {
                        (void)snprintf(LabelBuffer, sizeof(LabelBuffer), "%s (%.1fs)", DisplayName.c_str(), DataModel->GetPlayLength());
                    }
                    else
                    {
                        (void)snprintf(LabelBuffer, sizeof(LabelBuffer), "%s", DisplayName.c_str());
                    }
                    FString Label = LabelBuffer;

                    // 호환성 체크: 메쉬 스켈레톤과 애니메이션 스켈레톤 비교
                    bool bIsCompatible = true;
                    if (ActiveState->CurrentMesh && ActiveState->CurrentMesh->GetSkeleton() && Anim->GetSkeleton())
                    {
                        // 스켈레톤 본 개수 비교 (간단한 호환성 체크)
                        int32 MeshBoneCount = ActiveState->CurrentMesh->GetSkeleton()->Bones.Num();
                        int32 AnimBoneCount = Anim->GetSkeleton()->Bones.Num();
                        bIsCompatible = (MeshBoneCount == AnimBoneCount);
                    }
                    else if (!ActiveState->CurrentMesh)
                    {
                        // 메쉬가 없으면 호환성 알 수 없음 (중립)
                        bIsCompatible = true;
                    }

                    // 호환되지 않는 애니메이션은 회색으로 표시
                    if (!bIsCompatible)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                        Label += " (Incompatible)";
                    }

                    // ImGui ID 충돌 방지: ## 뒤에 고유 인덱스 추가
                    char SelectableID[512];
                    (void)snprintf(SelectableID, sizeof(SelectableID), "%s##anim_%d", Label.c_str(), i);

                    if (ImGui::Selectable(SelectableID, bIsSelected))
                    {
                        // 호환되지 않는 애니메이션을 선택하면 경고 로그
                        if (!bIsCompatible)
                        {
                            UE_LOG("PreviewWindow: Warning - Selected animation may not be compatible with current mesh skeleton");
                        }

                        ActiveState->SelectedAnimationIndex = i;
                        ActiveState->CurrentAnimation = Anim;
                        ActiveState->CurrentAnimationTime = 0.0f;
                        if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr)
                        {
                            SkelComp->ClearAllBoneDeltas();
                        }
                        RebuildNotifyTracks(ActiveState);
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

                            // T-Pose로 리셋
                            SkelComp->ResetToReferencePose();
                            UE_LOG("[Animation] Reset to Reference Pose (T-Pose)");

                            UAnimInstance* AnimInst = SkelComp->GetAnimInstance();

                            // AnimInstance가 없으면 새로 생성
                            if (!AnimInst)
                            {
                                UE_LOG("[Animation] Creating new AnimInstance for first animation");
                                UAnimSingleNodeInstance* SingleNodeInst = NewObject<UAnimSingleNodeInstance>();
                                AnimInst = SingleNodeInst;
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

                    // 호환성 체크에서 PushStyleColor를 했으면 Pop
                    if (!bIsCompatible)
                    {
                        ImGui::PopStyleColor();
                    }

                    // 우클릭 컨텍스트 메뉴
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Delete"))
                        {
                            if (Anim)
                            {
                                FString FilePath = Anim->GetFilePath();

                                // 현재 재생 중이면 상태 초기화
                                if (ActiveState->CurrentAnimation == Anim)
                                {
                                    ActiveState->CurrentAnimation = nullptr;
                                    ActiveState->SelectedAnimationIndex = -1;
                                    ActiveState->bIsPlaying = false;
                                    ActiveState->CurrentAnimationTime = 0.0f;
                                }

                                // 메쉬가 있으면 메쉬에서도 제거
                                if (ActiveState->CurrentMesh)
                                {
                                    ActiveState->CurrentMesh->RemoveAnimation(Anim);
                                }

                                // .anim 파일인 경우에만 실제 파일 삭제
                                if (FilePath.ends_with(".anim"))
                                {
                                    if (std::filesystem::exists(FilePath))
                                    {
                                        std::filesystem::remove(FilePath);
                                    }
                                    UResourceManager::GetInstance().Unload<UAnimSequence>(FilePath);
                                }

                                // 선택 인덱스 조정
                                if (ActiveState->SelectedAnimationIndex >= Animations.Num())
                                {
                                    ActiveState->SelectedAnimationIndex = -1;
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }
                }

                ImGui::EndChild();
            }
            else
            {
                static int NoAnimLogCount = 0;
                if (NoAnimLogCount < 3)
                {
                    UE_LOG("PreviewWindow: No animations - showing message");
                    ++NoAnimLogCount;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No animations loaded. Import .fbx or .anim files to see animations here.");
                ImGui::PopStyleColor();
            }

            ImGui::EndChild();
        }
    }

    // Record 파일명 입력 모달 (Timeline 바깥에서 렌더링)
    if (ActiveState && ActiveState->bShowRecordDialog)
    {
        ImGui::OpenPopup("Record Animation##RecordDialog");
        ActiveState->bShowRecordDialog = false;
    }

    if (ImGui::BeginPopupModal("Record Animation##RecordDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Enter animation file name:");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(300.0f);
        bool bEnterPressed = ImGui::InputTextWithHint("##RecordFileName", "e.g., MyAnimation",
            ActiveState->RecordFileNameBuffer, sizeof(ActiveState->RecordFileNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Start Recording", ImVec2(145, 0)) || bEnterPressed)
        {
            FString FileName(ActiveState->RecordFileNameBuffer);
            if (!FileName.empty())
            {
                ActiveState->bIsRecording = true;
                ActiveState->RecordedFileName = FileName;
                ActiveState->RecordedFrames.clear();
                ActiveState->RecordStartTime = ActiveState->CurrentAnimationTime;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(145, 0)))
        {
            memset(ActiveState->RecordFileNameBuffer, 0, sizeof(ActiveState->RecordFileNameBuffer));
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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

void SPreviewWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Spacebar 입력 처리 (Animation 모드이고, Bone이 선택되지 않았을 때만)
    UInputManager& InputMgr = UInputManager::GetInstance();

    // 기즈모 드래그 상태 감지
    bool bIsGizmoDragging = InputMgr.GetIsGizmoDragging();

    // 기즈모 드래그 시작 감지 - DragStartBoneIndex 설정만 수행
    if (!ActiveState->bWasGizmoDragging && bIsGizmoDragging)
    {
        AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
        if (Gizmo && Gizmo->IsBoneTarget() && ActiveState->SelectedBoneIndex >= 0 &&
            ActiveState->ViewMode == EViewerMode::Animation && ActiveState->PreviewActor)
        {
            ActiveState->DragStartBoneIndex = ActiveState->SelectedBoneIndex;
        }
    }

    // 기즈모 드래그 종료 감지 - 새로운 총 델타 계산 (누적 아님, 교체)
    if (ActiveState->bWasGizmoDragging && !bIsGizmoDragging)
    {
        // 드래그가 방금 종료됨 - Animation 모드에서 Bone 타겟일 때만 처리
        AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
        if (Gizmo && Gizmo->IsBoneTarget() && ActiveState->DragStartBoneIndex >= 0 &&
            ActiveState->ViewMode == EViewerMode::Animation && ActiveState->PreviewActor)
        {
            USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
            if (SkeletalComp)
            {
                int32 BoneIndex = ActiveState->DragStartBoneIndex;

                // 현재 본의 로컬 트랜스폼 (드래그 후 최종 위치)
                FTransform CurrentLocalTransform = SkeletalComp->GetBoneLocalTransform(BoneIndex);

                // 현재 애니메이션 시간의 순수 애니메이션 값 가져오기
                // AnimDataModel에서 인덱스로 직접 트랙 접근
                FTransform PureAnimTransform = CurrentLocalTransform;

                UAnimInstance* AnimInst = SkeletalComp->GetAnimInstance();
                if (AnimInst && AnimInst->GetCurrentAnimation())
                {
                    UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimInst->GetCurrentAnimation());
                    if (AnimSeq && AnimSeq->GetDataModel())
                    {
                        UAnimDataModel* DataModel = AnimSeq->GetDataModel();
                        const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByIndex(BoneIndex);
                        if (Track)
                        {
                            // 시간을 프레임으로 변환하여 보간
                            float FrameRate = DataModel->GetFrameRate().AsDecimal();
                            float CurrentTime = AnimInst->GetCurrentTime();
                            float FrameFloat = CurrentTime * FrameRate;
                            int32 Frame0 = static_cast<int32>(FrameFloat);
                            int32 Frame1 = Frame0 + 1;
                            float Alpha = FrameFloat - static_cast<float>(Frame0);

                            int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
                            Frame0 = FMath::Clamp(Frame0, 0, MaxFrame);
                            Frame1 = FMath::Clamp(Frame1, 0, MaxFrame);

                            // 키프레임 보간
                            const auto& PosKeys = Track->InternalTrack.PosKeys;
                            const auto& RotKeys = Track->InternalTrack.RotKeys;
                            const auto& ScaleKeys = Track->InternalTrack.ScaleKeys;

                            if (PosKeys.Num() > 0 && RotKeys.Num() > 0 && ScaleKeys.Num() > 0)
                            {
                                int32 PosIdx0 = FMath::Clamp(Frame0, 0, PosKeys.Num() - 1);
                                int32 PosIdx1 = FMath::Clamp(Frame1, 0, PosKeys.Num() - 1);
                                int32 RotIdx0 = FMath::Clamp(Frame0, 0, RotKeys.Num() - 1);
                                int32 RotIdx1 = FMath::Clamp(Frame1, 0, RotKeys.Num() - 1);
                                int32 ScaleIdx0 = FMath::Clamp(Frame0, 0, ScaleKeys.Num() - 1);
                                int32 ScaleIdx1 = FMath::Clamp(Frame1, 0, ScaleKeys.Num() - 1);

                                FVector Pos = FVector::Lerp(PosKeys[PosIdx0], PosKeys[PosIdx1], Alpha);
                                FQuat Rot = FQuat::Slerp(RotKeys[RotIdx0], RotKeys[RotIdx1], Alpha);
                                FVector Scl = FVector::Lerp(ScaleKeys[ScaleIdx0], ScaleKeys[ScaleIdx1], Alpha);

                                PureAnimTransform = FTransform(Pos, Rot, Scl);
                            }
                        }
                    }
                }

                // 새로운 총 델타 계산: 현재 위치 - 순수 애니메이션 값
                FTransform NewTotalDelta;
                NewTotalDelta.Translation = CurrentLocalTransform.Translation - PureAnimTransform.Translation;
                NewTotalDelta.Rotation = (PureAnimTransform.Rotation.Inverse() * CurrentLocalTransform.Rotation).GetNormalized();
                NewTotalDelta.Scale3D = FVector(
                    PureAnimTransform.Scale3D.X != 0.0f ? CurrentLocalTransform.Scale3D.X / PureAnimTransform.Scale3D.X : 1.0f,
                    PureAnimTransform.Scale3D.Y != 0.0f ? CurrentLocalTransform.Scale3D.Y / PureAnimTransform.Scale3D.Y : 1.0f,
                    PureAnimTransform.Scale3D.Z != 0.0f ? CurrentLocalTransform.Scale3D.Z / PureAnimTransform.Scale3D.Z : 1.0f
                );

                // 총 델타 저장 (교체, 누적 아님) - SkeletalMeshComponent에 저장
                SkeletalComp->SetBoneDelta(BoneIndex, NewTotalDelta);

                UE_LOG("[PreviewWindow] DragEnd: BoneIndex=%d, Current=(%.3f, %.3f, %.3f), PureAnim=(%.3f, %.3f, %.3f), TotalDelta=(%.3f, %.3f, %.3f)",
                    BoneIndex,
                    CurrentLocalTransform.Translation.X, CurrentLocalTransform.Translation.Y, CurrentLocalTransform.Translation.Z,
                    PureAnimTransform.Translation.X, PureAnimTransform.Translation.Y, PureAnimTransform.Translation.Z,
                    NewTotalDelta.Translation.X, NewTotalDelta.Translation.Y, NewTotalDelta.Translation.Z);

                // UI 업데이트
                ActiveState->EditBoneLocation = CurrentLocalTransform.Translation;
                ActiveState->EditBoneRotation = CurrentLocalTransform.Rotation.ToEulerZYXDeg();
                ActiveState->EditBoneScale = CurrentLocalTransform.Scale3D;

                // 드래그 상태 초기화
                ActiveState->DragStartBoneIndex = -1;
            }
        }
    }
    ActiveState->bWasGizmoDragging = bIsGizmoDragging;
    bool bIsGizmoOrBoneSelected = false;
    if (ActiveState->World && ActiveState->World->GetSelectionManager())
    {
        // Check if gizmo is selected (component mode) or bone is selected
        UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedActorComponent();
        AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
        bool bBoneSelected = (Gizmo && Gizmo->IsBoneTarget());

        bIsGizmoOrBoneSelected = (SelectedComp != nullptr) || bBoneSelected;
    }

    if (ActiveState->ViewMode == EViewerMode::Animation &&
        ActiveState->CurrentAnimation &&
        !InputMgr.GetIsGizmoDragging() &&
        !bIsGizmoOrBoneSelected &&
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

        // Note: 델타 적용은 AnimInstance::EvaluateAnimation에서 자동으로 처리됨

        // Recording: 본 트랜스폼 데이터 수집
        if (ActiveState->bIsRecording && ActiveState->ViewMode == EViewerMode::Animation)
        {
            const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
            if (Skeleton)
            {
                TMap<int32, FTransform> FrameData;
                for (int32 BoneIdx = 0; BoneIdx < Skeleton->Bones.size(); ++BoneIdx)
                {
                    FTransform BoneTransform = SkelComp->GetBoneLocalTransform(BoneIdx);
                    FrameData[BoneIdx] = BoneTransform;
                }
                ActiveState->RecordedFrames.Add(FrameData);
            }
        }
    }
}

void SPreviewWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    // 팝업/모달이 열려있으면 뷰포트 입력 무시
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    // 기즈모 드래그 중인지 확인
    AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
    bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

    // 기즈모 드래그 중이거나 CenterRect 안에 있을 때 마우스 이벤트 전달
    if (bGizmoDragging || CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SPreviewWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    // 팝업/모달이 열려있으면 뷰포트 입력 무시
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

        // First, always try gizmo picking (pass to viewport)
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // Left click: try bone picking or deselection
        if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
        {
            // Check if gizmo was actually clicked (hovering at the moment of click)
            AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
            bool bGizmoClicked = (Gizmo && Gizmo->GetbIsHovering());

            // 기즈모 위에서 클릭했고 본 타겟 모드이면 명시적으로 드래그 시작 (FutureEngine 방식)
            if (bGizmoClicked && Gizmo && Gizmo->IsBoneTarget())
            {
                ACameraActor* Camera = ActiveState->Client->GetCamera();
                if (Camera)
                {
                    Gizmo->StartDrag(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
                }
                return; // 기즈모 드래그 시작했으므로 본 피킹 하지 않음
            }

            // Only do bone picking/deselection if gizmo wasn't clicked
            if (!bGizmoClicked)
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

                        // Set Gizmo to target this bone directly
                        AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
                        USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                        if (GizmoActor && SkelComp)
                        {
                            GizmoActor->SetBoneTarget(SkelComp, PickedBoneIndex);
                            GizmoActor->SetbRender(true);
                        }

                        // Update selection manager to show gizmo
                        ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                        // Note: We don't select a component anymore since we're targeting bone directly
                    }
                    else
                    {
                        // No bone was picked - clear selection
                        ActiveState->SelectedBoneIndex = -1;
                        ActiveState->bBoneLinesDirty = true;

                        // Clear bone target from gizmo
                        AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
                        if (GizmoActor)
                        {
                            GizmoActor->ClearBoneTarget();
                            GizmoActor->SetbRender(false);
                        }

                        ActiveState->World->GetSelectionManager()->ClearSelection();
                    }
                }
            }
        }
    }
}

void SPreviewWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    // 팝업/모달이 열려있으면 뷰포트 입력 무시
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    {
        return;
    }

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SPreviewWindow::OnRenderViewport()
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
        bool bForceHighlightRefresh = false;
        bool bIsAnimationPlaying = ActiveState->bIsPlaying;

        if (ActiveState->bShowBones)
        {
            // 애니메이션 재생 중이거나 Dirty 플래그가 설정되어 있으면 재구축
            if (bIsAnimationPlaying || ActiveState->bBoneLinesDirty)
            {
                bNeedsRebuild = true;
                // Dirty 플래그가 설정되어 있으면 색상 강제 갱신
                if (ActiveState->bBoneLinesDirty)
                {
                    bForceHighlightRefresh = true;
                }
                ActiveState->bBoneLinesDirty = false;
            }
        }

        if (bNeedsRebuild && ActiveState->PreviewActor && ActiveState->CurrentMesh)
        {
            if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->SetLineVisible(true);
            }
            // 애니메이션 재생 중이면 모든 본 업데이트, Dirty 플래그가 있었으면 색상 강제 갱신
            ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, bIsAnimationPlaying, bForceHighlightRefresh);
        }

        // 애니메이션 재생 중이고 본이 선택되어 있으면 기즈모 위치도 실시간 업데이트
        if (bIsAnimationPlaying && ActiveState->SelectedBoneIndex >= 0 && ActiveState->PreviewActor && ActiveState->World)
        {
            AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
            USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
            if (Gizmo && SkelComp && Gizmo->IsBoneTarget())
            {
                FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(ActiveState->SelectedBoneIndex);
                Gizmo->SetActorLocation(BoneWorldTransform.Translation);
            }
        }

        // 뷰포트 렌더링 (ImGui보다 먼저)
        ActiveState->Viewport->Render();
    }
}

void SPreviewWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!State) return;

    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SPreviewWindow::CloseTab(int Index)
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

AGizmoActor* SPreviewWindow::GetGizmoActor() const
{
    if (ActiveState && ActiveState->World)
    {
        return ActiveState->World->GetGizmoActor();
    }
    return nullptr;
}

void SPreviewWindow::LoadSkeletalMesh(const FString& Path)
{
    UE_LOG("PreviewWindow::LoadSkeletalMesh called with: %s", Path.c_str());

    if (!ActiveState || Path.empty())
    {
        UE_LOG("PreviewWindow::LoadSkeletalMesh: Early return - ActiveState=%p, Path.empty()=%d", ActiveState, Path.empty());
        return;
    }

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    UE_LOG("PreviewWindow::LoadSkeletalMesh: Mesh=%p, PreviewActor=%p", Mesh, ActiveState->PreviewActor);

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

        // Preload에서 이미 모든 애니메이션이 추가되어 있음
        UE_LOG("PreviewWindow: Mesh already has %d animations from Preload", Mesh->GetAnimations().Num());

        // 애니메이션이 있으면 자동으로 Animation 모드로 전환 및 첫 번째 애니메이션 재생
        const TArray<UAnimSequence*>& Animations = Mesh->GetAnimations();
        if (Animations.Num() > 0)
        {
            ActiveState->ViewMode = EViewerMode::Animation;
            ActiveState->SelectedAnimationIndex = 0;
            ActiveState->CurrentAnimation = Animations[0];
            ActiveState->CurrentAnimationTime = 0.0f;
            if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr)
            {
                SkelComp->ClearAllBoneDeltas();
            }
            RebuildNotifyTracks(ActiveState);
            ActiveState->bIsPlaying = true;
        }

        UE_LOG("SSkeletalMeshViewerWindow: LoadSkeletalMesh: Loaded %s (%d animations)", Path.c_str(), Animations.Num());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SPreviewWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    // 본의 로컬 트랜스폼에서 값 추출
    const FTransform& BoneTransform = State->PreviewActor->GetSkeletalMeshComponent()->GetBoneLocalTransform(State->SelectedBoneIndex);
    State->EditBoneLocation = BoneTransform.Translation;
    State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = BoneTransform.Scale3D;
}

void SPreviewWindow::ApplyBoneTransform(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0 || !State->PreviewActor)
        return;

    USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp)
        return;

    FTransform NewTransform(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);

    // 현재 애니메이션 원본 트랜스폼 계산 (델타 제외)
    // AnimInstance에서 GetBoneTransformAtTime으로 순수 애니메이션 값을 가져옴
    FTransform AnimTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);

    // 기존 델타가 있으면 원본 트랜스폼 역산
    const FTransform* ExistingDelta = SkelComp->GetBoneDelta(State->SelectedBoneIndex);
    if (ExistingDelta)
    {
        // 델타 역적용하여 순수 애니메이션 값 계산
        AnimTransform.Translation = AnimTransform.Translation - ExistingDelta->Translation;
        AnimTransform.Rotation = (AnimTransform.Rotation * ExistingDelta->Rotation.Inverse()).GetNormalized();
        AnimTransform.Scale3D = FVector(
            ExistingDelta->Scale3D.X != 0.0f ? AnimTransform.Scale3D.X / ExistingDelta->Scale3D.X : AnimTransform.Scale3D.X,
            ExistingDelta->Scale3D.Y != 0.0f ? AnimTransform.Scale3D.Y / ExistingDelta->Scale3D.Y : AnimTransform.Scale3D.Y,
            ExistingDelta->Scale3D.Z != 0.0f ? AnimTransform.Scale3D.Z / ExistingDelta->Scale3D.Z : AnimTransform.Scale3D.Z
        );
    }

    // 델타 계산: 새 값 - 애니메이션 원본 값
    FTransform Delta;
    Delta.Translation = NewTransform.Translation - AnimTransform.Translation;
    Delta.Rotation = AnimTransform.Rotation.IsIdentity() ? NewTransform.Rotation : (AnimTransform.Rotation.Inverse() * NewTransform.Rotation).GetNormalized();
    Delta.Scale3D = FVector(
        AnimTransform.Scale3D.X != 0.0f ? NewTransform.Scale3D.X / AnimTransform.Scale3D.X : 1.0f,
        AnimTransform.Scale3D.Y != 0.0f ? NewTransform.Scale3D.Y / AnimTransform.Scale3D.Y : 1.0f,
        AnimTransform.Scale3D.Z != 0.0f ? NewTransform.Scale3D.Z / AnimTransform.Scale3D.Z : 1.0f
    );

    // 델타를 SkeletalMeshComponent에 저장
    SkelComp->SetBoneDelta(State->SelectedBoneIndex, Delta);

    // 즉시 적용 (다음 Tick에서 AnimInstance가 적용)
    SkelComp->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SPreviewWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
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

void SPreviewWindow::ScanNotifyLibrary()
{
    AvailableNotifyClasses.clear();
    AvailableNotifyStateClasses.clear();

    WIN32_FIND_DATAA FindData;
    HANDLE hFind;

    // Notify 폴더 스캔 (*.lua 모든 파일)
    FString NotifyDir = "Data/Scripts/Notify/";
    FString NotifyPattern = NotifyDir + "*.lua";
    hFind = FindFirstFileA(NotifyPattern.c_str(), &FindData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                FString FileName(FindData.cFileName);
                FString FileNameWithoutExt = FileName.substr(0, FileName.find_last_of('.'));
                AvailableNotifyClasses.push_back(FileNameWithoutExt);
            }
        } while (FindNextFileA(hFind, &FindData));
        FindClose(hFind);
    }

    // NotifyState 폴더 스캔 (*.lua 모든 파일)
    FString StateDir = "Data/Scripts/NotifyState/";
    FString StatePattern = StateDir + "*.lua";
    hFind = FindFirstFileA(StatePattern.c_str(), &FindData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                FString FileName(FindData.cFileName);
                FString FileNameWithoutExt = FileName.substr(0, FileName.find_last_of('.'));
                AvailableNotifyStateClasses.push_back(FileNameWithoutExt);
            }
        } while (FindNextFileA(hFind, &FindData));
        FindClose(hFind);
    }

    UE_LOG("[SkeletalMeshViewer] Scanned Notify Library: %d Notifies, %d NotifyStates",
        AvailableNotifyClasses.Num(), AvailableNotifyStateClasses.Num());
}

void SPreviewWindow::CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState)
{
    // 대상 폴더 및 접두사
    FString DestDir = bIsNotifyState ? "Data/Scripts/NotifyState/" : "Data/Scripts/Notify/";
    FString Prefix = bIsNotifyState ? "ANS_" : "AN_";

    FString FileName;
    FString DestPath;

    // 사용자가 이름을 입력하지 않았으면 자동 번호 매기기
    if (ScriptName.empty())
    {
        // 기존 파일 중 가장 높은 번호 찾기
        int32 MaxNum = 0;
        WIN32_FIND_DATAA FindData;
        FString SearchPattern = DestDir + Prefix + "Custom_*.lua";
        HANDLE hFind = FindFirstFileA(SearchPattern.c_str(), &FindData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    FString ExistingFile(FindData.cFileName);
                    // "AN_Custom_123.lua" → "123" 추출
                    size_t UnderscorePos = ExistingFile.rfind('_');
                    size_t DotPos = ExistingFile.find_last_of('.');
                    if (UnderscorePos != FString::npos && DotPos != FString::npos)
                    {
                        FString NumStr = ExistingFile.substr(UnderscorePos + 1, DotPos - UnderscorePos - 1);
                        int32 Num = std::stoi(NumStr);
                        if (Num > MaxNum)
                        {
                            MaxNum = Num;
                        }
                    }
                }
            } while (FindNextFileA(hFind, &FindData));
            FindClose(hFind);
        }

        // 다음 번호로 파일명 생성
        FileName = Prefix + "Custom_" + std::to_string(MaxNum + 1) + ".lua";
    }
    else
    {
        // 사용자가 입력한 이름 사용
        FileName = Prefix + ScriptName + ".lua";
    }

    DestPath = DestDir + FileName;

    // 파일이 이미 존재하는지 확인
    FILE* ExistingFile = nullptr;
    fopen_s(&ExistingFile, DestPath.c_str(), "r");
    if (ExistingFile)
    {
        fclose(ExistingFile);
        UE_LOG("[SkeletalMeshViewer] CreateNewNotifyScript: File already exists: %s", DestPath.c_str());
        return;
    }

    // 템플릿 파일 경로
    FString TemplatePath = bIsNotifyState ? "Data/Scripts/Template/ANS_Template.lua" : "Data/Scripts/Template/AN_Template.lua";

    // 템플릿 파일 읽기
    FILE* TemplateFile = nullptr;
    fopen_s(&TemplateFile, TemplatePath.c_str(), "rb");
    if (!TemplateFile)
    {
        UE_LOG("[SkeletalMeshViewer] CreateNewNotifyScript: Template file not found: %s", TemplatePath.c_str());
        return;
    }

    // 파일 크기 확인
    fseek(TemplateFile, 0, SEEK_END);
    size_t FileSize = ftell(TemplateFile);
    fseek(TemplateFile, 0, SEEK_SET);

    // 템플릿 내용 읽기
    FString TemplateContent;
    TemplateContent.resize(FileSize);
    fread(&TemplateContent[0], 1, FileSize, TemplateFile);
    fclose(TemplateFile);

    // 새 파일 생성
    FILE* OutFile = nullptr;
    fopen_s(&OutFile, DestPath.c_str(), "wb");
    if (!OutFile)
    {
        UE_LOG("[SkeletalMeshViewer] CreateNewNotifyScript: Failed to create file: %s", DestPath.c_str());
        return;
    }

    fwrite(TemplateContent.c_str(), 1, TemplateContent.size(), OutFile);
    fclose(OutFile);

    UE_LOG("[SkeletalMeshViewer] Created new Notify script: %s (from template: %s)", DestPath.c_str(), TemplatePath.c_str());

    // 라이브러리 재스캔
    ScanNotifyLibrary();

    // 생성된 파일을 기본 에디터에서 열기 (VS Code, Notepad++ 등)
    std::wstring WideDestPath(DestPath.begin(), DestPath.end());
    FPlatformProcess::OpenFileInDefaultEditor(WideDestPath);
}

void SPreviewWindow::OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState)
{
    if (NotifyClassName.empty())
    {
        UE_LOG("[SkeletalMeshViewer] OpenNotifyScriptInEditor: Empty class name");
        return;
    }

    // 파일 경로 생성
    FString DestDir = bIsNotifyState ? "Data/Scripts/NotifyState/" : "Data/Scripts/Notify/";
    FString FilePath = DestDir + NotifyClassName + ".lua";

    // 절대 경로 생성
    std::filesystem::path AbsolutePath;
    try
    {
        AbsolutePath = std::filesystem::absolute(FilePath);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        UE_LOG("[SkeletalMeshViewer] OpenNotifyScriptInEditor: Path conversion failed: %s", e.what());
        return;
    }

    // 파일이 존재하는지 확인
    if (!std::filesystem::exists(AbsolutePath))
    {
        UE_LOG("[SkeletalMeshViewer] OpenNotifyScriptInEditor: File not found: %s", AbsolutePath.string().c_str());
        return;
    }

    // FPlatformProcess로 파일 열기
    std::wstring WideAbsolutePath = AbsolutePath.wstring();
    FPlatformProcess::OpenFileInDefaultEditor(WideAbsolutePath);
    UE_LOG("[SkeletalMeshViewer] OpenNotifyScriptInEditor: Opened file: %s", AbsolutePath.string().c_str());
}

/**
 * @brief 애니메이션의 Notify들을 스캔하여 NotifyTrackNames 복원
 * @details .anim 파일 로드 시 Notify의 TrackIndex 정보로부터 Track 목록 재구성
 */
void SPreviewWindow::RebuildNotifyTracks(ViewerState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    // 기존 Track 정보 초기화
    State->NotifyTrackNames.clear();
    State->UsedTrackNumbers.clear();

    // 현재 애니메이션의 모든 Notify에서 TrackIndex 수집
    std::set<int32> TrackIndices;
    const TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
    for (const FAnimNotifyEvent& Notify : Notifies)
    {
        TrackIndices.insert(Notify.TrackIndex);
    }

    // Track이 하나도 없으면 기본 Track 1 생성
    if (TrackIndices.empty())
    {
        State->NotifyTrackNames.push_back("Track 1");
        State->UsedTrackNumbers.insert(1);
        return;
    }

    // TrackIndex에 맞게 Track 생성 (인덱스 순서대로)
    for (int32 TrackIdx : TrackIndices)
    {
        // TrackIndex는 0부터 시작하지만, Track 이름은 1부터 시작
        int32 TrackNumber = TrackIdx + 1;
        char TrackNameBuf[64];
        snprintf(TrackNameBuf, sizeof(TrackNameBuf), "Track %d", TrackNumber);
        State->NotifyTrackNames.push_back(TrackNameBuf);
        State->UsedTrackNumbers.insert(TrackNumber);
    }
}

// ============================================================================
// Render Target Management (전용 렌더 타겟 관리)
// ============================================================================

/**
 * @brief 전용 렌더 타겟 생성
 */
void SPreviewWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
    // 기존 렌더 타겟 해제
    ReleaseRenderTarget();

    if (Width == 0 || Height == 0)
    {
        return;
    }

    if (!Device)
    {
        return;
    }

    // 렌더 타겟 텍스처 생성 (DXGI_FORMAT_B8G8R8A8_UNORM - D2D 호환)
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.MiscFlags = 0;

    HRESULT hr = Device->CreateTexture2D(&TextureDesc, nullptr, &PreviewRenderTargetTexture);
    if (FAILED(hr))
    {
        UE_LOG("PreviewWindow: 렌더 타겟 텍스처 생성 실패");
        return;
    }

    // 렌더 타겟 뷰 생성
    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateRenderTargetView(PreviewRenderTargetTexture, &RTVDesc, &PreviewRenderTargetView);
    if (FAILED(hr))
    {
        UE_LOG("PreviewWindow: 렌더 타겟 뷰 생성 실패");
        ReleaseRenderTarget();
        return;
    }

    // 셰이더 리소스 뷰 생성 (ImGui::Image용)
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(PreviewRenderTargetTexture, &SRVDesc, &PreviewShaderResourceView);
    if (FAILED(hr))
    {
        UE_LOG("PreviewWindow: 셰이더 리소스 뷰 생성 실패");
        ReleaseRenderTarget();
        return;
    }

    // 깊이 스텐실 텍스처 생성
    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = Width;
    DepthDesc.Height = Height;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.SampleDesc.Quality = 0;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    DepthDesc.CPUAccessFlags = 0;
    DepthDesc.MiscFlags = 0;

    hr = Device->CreateTexture2D(&DepthDesc, nullptr, &PreviewDepthStencilTexture);
    if (FAILED(hr))
    {
        UE_LOG("PreviewWindow: 깊이 스텐실 텍스처 생성 실패");
        ReleaseRenderTarget();
        return;
    }

    // 깊이 스텐실 뷰 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
    DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DSVDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(PreviewDepthStencilTexture, &DSVDesc, &PreviewDepthStencilView);
    if (FAILED(hr))
    {
        UE_LOG("PreviewWindow: 깊이 스텐실 뷰 생성 실패");
        ReleaseRenderTarget();
        return;
    }

    PreviewRenderTargetWidth = Width;
    PreviewRenderTargetHeight = Height;
}

/**
 * @brief 전용 렌더 타겟 해제
 */
void SPreviewWindow::ReleaseRenderTarget()
{
    if (PreviewDepthStencilView)
    {
        PreviewDepthStencilView->Release();
        PreviewDepthStencilView = nullptr;
    }

    if (PreviewDepthStencilTexture)
    {
        PreviewDepthStencilTexture->Release();
        PreviewDepthStencilTexture = nullptr;
    }

    if (PreviewShaderResourceView)
    {
        PreviewShaderResourceView->Release();
        PreviewShaderResourceView = nullptr;
    }

    if (PreviewRenderTargetView)
    {
        PreviewRenderTargetView->Release();
        PreviewRenderTargetView = nullptr;
    }

    if (PreviewRenderTargetTexture)
    {
        PreviewRenderTargetTexture->Release();
        PreviewRenderTargetTexture = nullptr;
    }

    PreviewRenderTargetWidth = 0;
    PreviewRenderTargetHeight = 0;
}

/**
 * @brief 렌더 타겟 크기 업데이트 (리사이즈 처리)
 */
void SPreviewWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
    // 크기가 변경되지 않았으면 스킵
    if (PreviewRenderTargetWidth == NewWidth && PreviewRenderTargetHeight == NewHeight)
    {
        return;
    }

    // 새 크기로 렌더 타겟 재생성
    CreateRenderTarget(NewWidth, NewHeight);
}

/**
 * @brief 스켈레탈 메쉬 프리뷰를 전용 렌더 타겟에 렌더링
 */
void SPreviewWindow::RenderToPreviewRenderTarget()
{
    if (!PreviewRenderTargetView || !PreviewDepthStencilView || !ActiveState)
    {
        return;
    }

    if (!ActiveState->Viewport || !ActiveState->Client)
    {
        return;
    }

    D3D11RHI* RHI = GEngine.GetRHIDevice();
    if (!RHI)
    {
        return;
    }

    ID3D11DeviceContext* Context = RHI->GetDeviceContext();

    // 현재 렌더 타겟 백업
    ID3D11RenderTargetView* OldRTV = nullptr;
    ID3D11DepthStencilView* OldDSV = nullptr;
    Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

    // D3D 뷰포트 백업
    UINT NumViewports = 1;
    D3D11_VIEWPORT OldViewport;
    Context->RSGetViewports(&NumViewports, &OldViewport);

    // 렌더 타겟 클리어
    const float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    Context->ClearRenderTargetView(PreviewRenderTargetView, ClearColor);
    Context->ClearDepthStencilView(PreviewDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 프리뷰 전용 렌더 타겟 설정
    Context->OMSetRenderTargets(1, &PreviewRenderTargetView, PreviewDepthStencilView);

    // 프리뷰 전용 뷰포트 설정
    D3D11_VIEWPORT D3DViewport = {};
    D3DViewport.TopLeftX = 0.0f;
    D3DViewport.TopLeftY = 0.0f;
    D3DViewport.Width = static_cast<float>(PreviewRenderTargetWidth);
    D3DViewport.Height = static_cast<float>(PreviewRenderTargetHeight);
    D3DViewport.MinDepth = 0.0f;
    D3DViewport.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &D3DViewport);

    // Viewport 크기 업데이트
    ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

    // 본 오버레이 재구축 및 기즈모 위치 업데이트
    bool bNeedsRebuild = false;
    bool bForceHighlightRefresh = false;
    bool bIsAnimationPlaying = ActiveState->bIsPlaying;

    if (ActiveState->bShowBones)
    {
        // 애니메이션 재생 중이거나 Dirty 플래그가 설정되어 있으면 재구축
        if (bIsAnimationPlaying || ActiveState->bBoneLinesDirty)
        {
            bNeedsRebuild = true;
            // Dirty 플래그가 설정되어 있으면 색상 강제 갱신
            if (ActiveState->bBoneLinesDirty)
            {
                bForceHighlightRefresh = true;
            }
            ActiveState->bBoneLinesDirty = false;
        }
    }

    if (bNeedsRebuild && ActiveState->PreviewActor && ActiveState->CurrentMesh)
    {
        if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->SetLineVisible(true);
        }
        // 애니메이션 재생 중이면 모든 본 업데이트, Dirty 플래그가 있었으면 색상 강제 갱신
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, bIsAnimationPlaying, bForceHighlightRefresh);
    }

    // 애니메이션 재생 중이고 본이 선택되어 있으면 기즈모 위치도 실시간 업데이트
    if (bIsAnimationPlaying && ActiveState->SelectedBoneIndex >= 0 && ActiveState->PreviewActor && ActiveState->World)
    {
        AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
        USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
        if (Gizmo && SkelComp && Gizmo->IsBoneTarget())
        {
            FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(ActiveState->SelectedBoneIndex);
            Gizmo->SetActorLocation(BoneWorldTransform.Translation);
        }
    }

    // 스켈레탈 메쉬 씬 렌더링
    if (ActiveState->Client)
    {
        ActiveState->Client->Draw(ActiveState->Viewport);
    }

    // 원래 렌더 타겟 복원
    Context->OMSetRenderTargets(1, &OldRTV, OldDSV);
    Context->RSSetViewports(1, &OldViewport);

    // 백업한 렌더 타겟 Release
    if (OldRTV) OldRTV->Release();
    if (OldDSV) OldDSV->Release();
}
