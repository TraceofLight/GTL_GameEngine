#include "pch.h"
#include "AnimationWindow.h"
#include "Source/Slate/Widgets/PlaybackControls.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include <filesystem>

// Timeline 컨트롤 UI 렌더링
void SAnimationWindow::RenderTimelineControls(FAnimationTabState* State)
{
    if (!State)
    {
        return;
    }

    // 애니메이션이 없어도 빈 타임라인 UI는 표시
    UAnimDataModel* DataModel = State->CurrentAnimation ? State->CurrentAnimation->GetDataModel() : nullptr;

    float MaxTime = DataModel ? DataModel->GetPlayLength() : 1.0f;
    int32 TotalFrames = DataModel ? DataModel->GetNumberOfFrames() : 30;

    // Working Range 기본값 설정 (프레임 단위)
    if (State->WorkingRangeEndFrame < 0)
    {
        State->WorkingRangeEndFrame = TotalFrames;
    }
    if (State->ViewRangeEndFrame < 0)
    {
        State->ViewRangeEndFrame = TotalFrames;
    }

    // === 1. Timeline 통합 영역 (Notify 패널 포함) ===
    ImGui::BeginChild("TimelineInner", ImVec2(0, -40), true);
    {
        RenderTimeline(State);
    }
    ImGui::EndChild();

    ImGui::Separator();

    // === 2. 재생 컨트롤 버튼들 (항상 하단 고정) ===
    PlaybackControls::BeginStyle();

    float ButtonSize = 20.0f;
    ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);

    // ToFront |<<
    if (PlaybackControls::RenderToFrontButton(IconGoToFront, ButtonSize))
    {
        TimelineToFront(State);
    }
    ImGui::SameLine();

    // ToPrevious |<
    if (PlaybackControls::RenderStepBackwardsButton(IconStepBackwards, ButtonSize))
    {
        TimelineToPrevious(State);
    }
    ImGui::SameLine();

    // Reverse << (Animation 전용)
    if (IconBackwards && IconBackwards->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##Backwards", IconBackwards->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineReverse(State);
        }
    }
    else
    {
        if (ImGui::Button("<<", ButtonSizeVec))
        {
            TimelineReverse(State);
        }
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reverse"); }
    ImGui::SameLine();

    // Record 버튼 (Animation 전용 - 녹화 중이면 빨간색)
    bool bWasRecording = State->bIsRecording;
    if (bWasRecording)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
    }
    if (IconRecord && IconRecord->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##Record", IconRecord->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineRecord(State);
        }
    }
    else
    {
        if (ImGui::Button("O", ButtonSizeVec))
        {
            TimelineRecord(State);
        }
    }
    if (bWasRecording) { ImGui::PopStyleColor(3); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip(State->bIsRecording ? "Stop Recording" : "Record"); }
    ImGui::SameLine();

    // Play/Pause (AnimInstance 컨트롤)
    if (PlaybackControls::RenderPlayPauseButton(IconPlay, IconPause, State->bIsPlaying, ButtonSize))
    {
        if (State->bIsPlaying)
        {
            // 일시정지
            if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
            {
                UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
                if (AnimInst) { AnimInst->StopAnimation(); }
            }
            State->bIsPlaying = false;
        }
        else
        {
            // 재생
            TimelinePlay(State);
        }
    }
    ImGui::SameLine();

    // ToNext >|
    if (PlaybackControls::RenderStepForwardButton(IconStepForward, ButtonSize))
    {
        TimelineToNext(State);
    }
    ImGui::SameLine();

    // ToEnd >>|
    if (PlaybackControls::RenderToEndButton(IconGoToEnd, ButtonSize))
    {
        TimelineToEnd(State);
    }
    ImGui::SameLine();

    // Loop 토글
    if (PlaybackControls::RenderLoopButton(IconLoop, IconLoopOff, State->bLoopAnimation, ButtonSize))
    {
        State->bLoopAnimation = !State->bLoopAnimation;

        // AnimInstance의 루프 설정만 업데이트 (재시작하지 않음)
        if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
        {
            USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
            if (UAnimInstance* AnimInst = SkelComp->GetAnimInstance())
            {
                if (UAnimSingleNodeInstance* SingleNodeInst = dynamic_cast<UAnimSingleNodeInstance*>(AnimInst))
                {
                    SingleNodeInst->SetLooping(State->bLoopAnimation);
                }
            }
        }
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    // 재생 속도
    PlaybackControls::RenderSpeedControl(State->PlaybackSpeed);

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    // === Range 컨트롤 (재생 컨트롤 옆에 배치, 프레임 단위) ===
    int32 WorkingStartFrame = State->WorkingRangeStartFrame;
    int32 WorkingEndFrame = (State->WorkingRangeEndFrame < 0) ? TotalFrames : State->WorkingRangeEndFrame;
    int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
    int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? TotalFrames : State->PlaybackRangeEndFrame;
    int32 ViewStartFrame = State->ViewRangeStartFrame;
    int32 ViewEndFrame = (State->ViewRangeEndFrame < 0) ? TotalFrames : State->ViewRangeEndFrame;

    int32 WorkingDurationFrames = WorkingEndFrame - WorkingStartFrame;
    int32 ViewDurationFrames = ViewEndFrame - ViewStartFrame;

    ImGui::SetNextItemWidth(45.0f);
    if (ImGui::DragInt("##WS", &State->WorkingRangeStartFrame, 1.0f, -9999, 9999))
    {
        State->WorkingRangeStartFrame = FMath::Clamp(State->WorkingRangeStartFrame, -9999, WorkingEndFrame - 1);

        if (State->ViewRangeStartFrame < State->WorkingRangeStartFrame)
        {
            State->ViewRangeStartFrame = State->WorkingRangeStartFrame;
        }

        int32 NewViewDuration = State->ViewRangeEndFrame - State->ViewRangeStartFrame;
        int32 NewWorkingDuration = WorkingEndFrame - State->WorkingRangeStartFrame;

        if (NewViewDuration > NewWorkingDuration)
        {
            State->ViewRangeEndFrame = State->WorkingRangeStartFrame + NewWorkingDuration;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Working Range Start (Frame)");
    }
    ImGui::SameLine();
    char PlaybackStartText[16];
    snprintf(PlaybackStartText, sizeof(PlaybackStartText), "0");

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 PlaybackStartSize(45.0f, ImGui::GetFrameHeight());
    ImVec2 PlaybackStartMin = ImGui::GetCursorScreenPos();
    ImVec2 PlaybackStartMax = ImVec2(PlaybackStartMin.x + PlaybackStartSize.x, PlaybackStartMin.y + PlaybackStartSize.y);

    ImGui::InvisibleButton("##PS", PlaybackStartSize);
    bool bIsHovered = ImGui::IsItemHovered();

    if (bIsHovered)
    {
        DrawList->AddRectFilled(PlaybackStartMin, PlaybackStartMax, IM_COL32(50, 75, 50, 128));
    }
    else
    {
        DrawList->AddRectFilled(PlaybackStartMin, PlaybackStartMax, ImGui::GetColorU32(ImGuiCol_FrameBg));
    }

    DrawList->AddRect(PlaybackStartMin, PlaybackStartMax, ImGui::GetColorU32(ImGuiCol_Border));

    ImVec2 TextSize = ImGui::CalcTextSize(PlaybackStartText);
    float CenterX = PlaybackStartMin.x + (PlaybackStartSize.x - TextSize.x) * 0.5f;
    float CenterY = PlaybackStartMin.y + (PlaybackStartSize.y - TextSize.y) * 0.5f;
    DrawList->AddText(ImVec2(CenterX, CenterY), IM_COL32(0, 255, 0, 255), PlaybackStartText);

    if (bIsHovered)
    {
        ImGui::SetTooltip("Playback Range Start (Frame)");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(45.0f);
    if (ImGui::DragInt("##VS", &State->ViewRangeStartFrame, 1.0f, WorkingStartFrame, WorkingEndFrame))
    {
        int32 CurrentEnd = (State->ViewRangeEndFrame < 0) ? TotalFrames : State->ViewRangeEndFrame;
        State->ViewRangeStartFrame = FMath::Clamp(State->ViewRangeStartFrame, WorkingStartFrame, CurrentEnd - 1);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("View Range Start (Frame)");
    }
    ImGui::SameLine();

    float AvailWidth = ImGui::GetContentRegionAvail().x;
    float RightButtonsWidth = 45.0f * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    float ScrollBarWidth = AvailWidth - RightButtonsWidth - 10.0f;
    ScrollBarWidth = std::max(ScrollBarWidth, 100.0f);

    ImVec2 BarSize(ScrollBarWidth, 20.0f);
    ImVec2 BarMin = ImGui::GetCursorScreenPos();
    ImVec2 BarMax = ImVec2(BarMin.x + BarSize.x, BarMin.y + BarSize.y);

    ImGui::InvisibleButton("##RangeScrollBar", BarSize);

    static bool bDraggingViewRange = false;
    static int32 DragStartViewStartFrame = 0;
    static float DragStartMouseX = 0.0f;

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 MousePos = ImGui::GetMousePos();

        if (!bDraggingViewRange)
        {
            bDraggingViewRange = true;
            DragStartViewStartFrame = ViewStartFrame;
            DragStartMouseX = MousePos.x;
        }

        float MouseDeltaX = MousePos.x - DragStartMouseX;
        int32 FrameDelta = static_cast<int32>((MouseDeltaX / BarSize.x) * WorkingDurationFrames);
        int32 NewViewStartFrame = DragStartViewStartFrame + FrameDelta;

        NewViewStartFrame = FMath::Clamp(NewViewStartFrame, WorkingStartFrame, WorkingEndFrame - ViewDurationFrames);

        State->ViewRangeStartFrame = NewViewStartFrame;
        State->ViewRangeEndFrame = NewViewStartFrame + ViewDurationFrames;
    }
    else
    {
        bDraggingViewRange = false;
    }

    DrawList->AddRectFilled(BarMin, BarMax, IM_COL32(40, 40, 40, 255));
    DrawList->AddRect(BarMin, BarMax, IM_COL32(100, 100, 100, 255));

    if (WorkingDurationFrames > 0 && ViewDurationFrames < WorkingDurationFrames)
    {
        float ViewStartNorm = static_cast<float>(ViewStartFrame - WorkingStartFrame) / WorkingDurationFrames;
        float ViewEndNorm = static_cast<float>(ViewEndFrame - WorkingStartFrame) / WorkingDurationFrames;

        ImVec2 HandleMin = ImVec2(BarMin.x + ViewStartNorm * BarSize.x, BarMin.y + 2);
        ImVec2 HandleMax = ImVec2(BarMin.x + ViewEndNorm * BarSize.x, BarMax.y - 2);

        DrawList->AddRectFilled(HandleMin, HandleMax, IM_COL32(100, 150, 200, 255));
        DrawList->AddRect(HandleMin, HandleMax, IM_COL32(150, 200, 255, 255), 0.0f, 0, 2.0f);
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(45.0f);
    int32 TempViewEndFrame = ViewEndFrame;
    if (ImGui::DragInt("##VE", &TempViewEndFrame, 1.0f, WorkingStartFrame, WorkingEndFrame))
    {
        int32 CurrentStart = State->ViewRangeStartFrame;
        State->ViewRangeEndFrame = FMath::Clamp(TempViewEndFrame, CurrentStart + 1, WorkingEndFrame);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("View Range End (Frame)");
    }
    ImGui::SameLine();

    char PlaybackEndText[16];
    snprintf(PlaybackEndText, sizeof(PlaybackEndText), "%d", TotalFrames);

    ImVec2 PlaybackEndSize(45.0f, ImGui::GetFrameHeight());
    ImVec2 PlaybackEndMin = ImGui::GetCursorScreenPos();
    ImVec2 PlaybackEndMax = ImVec2(PlaybackEndMin.x + PlaybackEndSize.x, PlaybackEndMin.y + PlaybackEndSize.y);

    ImGui::InvisibleButton("##PE", PlaybackEndSize);
    bool bIsHoveredEnd = ImGui::IsItemHovered();

    if (bIsHoveredEnd)
    {
        DrawList->AddRectFilled(PlaybackEndMin, PlaybackEndMax, IM_COL32(75, 50, 50, 128));
    }
    else
    {
        DrawList->AddRectFilled(PlaybackEndMin, PlaybackEndMax, ImGui::GetColorU32(ImGuiCol_FrameBg));
    }

    DrawList->AddRect(PlaybackEndMin, PlaybackEndMax, ImGui::GetColorU32(ImGuiCol_Border));

    ImVec2 TextSizeEnd = ImGui::CalcTextSize(PlaybackEndText);
    float CenterXEnd = PlaybackEndMin.x + (PlaybackEndSize.x - TextSizeEnd.x) * 0.5f;
    float CenterYEnd = PlaybackEndMin.y + (PlaybackEndSize.y - TextSizeEnd.y) * 0.5f;
    DrawList->AddText(ImVec2(CenterXEnd, CenterYEnd), IM_COL32(255, 0, 0, 255), PlaybackEndText);

    if (bIsHoveredEnd)
    {
        ImGui::SetTooltip("Playback Range End (Frame)");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(45.0f);
    int32 TempWorkingEndFrame = WorkingEndFrame;
    if (ImGui::DragInt("##WE", &TempWorkingEndFrame, 1.0f, -9999, 9999))
    {
        State->WorkingRangeEndFrame = FMath::Clamp(TempWorkingEndFrame, WorkingStartFrame + 1, 9999);

        State->ViewRangeEndFrame = std::min(State->ViewRangeEndFrame, State->WorkingRangeEndFrame);

        int32 NewViewDuration = State->ViewRangeEndFrame - State->ViewRangeStartFrame;
        int32 NewWorkingDuration = State->WorkingRangeEndFrame - WorkingStartFrame;

        if (NewViewDuration > NewWorkingDuration)
        {
            State->ViewRangeStartFrame = State->WorkingRangeEndFrame - NewWorkingDuration;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Working Range End (Frame)");
    }

    PlaybackControls::EndStyle();
}

// Timeline 헬퍼: 프레임 변경 시 공통 갱신 로직
void SAnimationWindow::RefreshAnimationFrame(FAnimationTabState* State)
{
    if (!State)
    {
        return;
    }

    USkeletalMeshComponent* SkelComp = nullptr;
    if (State->PreviewActor)
    {
        SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
    }

    // AnimInstance를 통해 포즈 업데이트 (EvaluateAnimation이 자동 호출됨)
    if (SkelComp)
    {
        UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
        if (AnimInst)
        {
            AnimInst->SetPosition(State->CurrentAnimationTime);
        }
    }

    // Bone Line 강제 갱신 (모든 본 업데이트)
    State->bBoneLinesDirty = true;
    if (State->PreviewActor)
    {
        State->PreviewActor->RebuildBoneLines(State->SelectedBoneIndex, true);

        // 기즈모 위치 동기화 (본이 선택되어 있으면)
        if (State->SelectedBoneIndex >= 0 && State->World)
        {
            AGizmoActor* Gizmo = State->World->GetGizmoActor();
            USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
            if (Gizmo && SkelComp && Gizmo->IsBoneTarget())
            {
                FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(State->SelectedBoneIndex);
                Gizmo->SetActorLocation(BoneWorldTransform.Translation);
            }
        }
    }
}

void SAnimationWindow::TimelineToFront(FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    State->CurrentAnimationTime = 0.0f;

    RefreshAnimationFrame(State);
}

void SAnimationWindow::TimelineToPrevious(FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    // 프레임 단위로 스냅하여 이동
    const FFrameRate& FrameRate = DataModel->GetFrameRate();
    float TimePerFrame = 1.0f / FrameRate.AsDecimal();

    // 현재 시간을 프레임으로 변환 (반올림)
    int32 CurrentFrame = static_cast<int32>(State->CurrentAnimationTime / TimePerFrame + 0.5f);

    // 이전 프레임으로 이동
    CurrentFrame = FMath::Max(0, CurrentFrame - 1);

    // 프레임을 시간으로 변환
    State->CurrentAnimationTime = static_cast<float>(CurrentFrame) * TimePerFrame;

    RefreshAnimationFrame(State);
}

void SAnimationWindow::TimelineReverse(FAnimationTabState* State)
{
    if (!State)
    {
        return;
    }

    // 역재생 (음수 속도) - AnimInstance를 통해 설정
    State->PlaybackSpeed = -FMath::Abs(State->PlaybackSpeed);

    if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
    {
        UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
        if (AnimInst)
        {
            // 일시정지 위치에서 역재생 시작
            AnimInst->SetPlayRate(State->PlaybackSpeed);

            // 현재 애니메이션이 설정되지 않았을 때만 PlayAnimation 호출
            if (AnimInst->GetCurrentAnimation() != State->CurrentAnimation)
            {
                AnimInst->PlayAnimation(State->CurrentAnimation, State->PlaybackSpeed);
            }
            else
            {
                // 같은 애니메이션이면 현재 위치에서 재생 재개
                AnimInst->ResumeAnimation();
            }

            State->bIsPlaying = true;
        }
    }
}

void SAnimationWindow::TimelineRecord(FAnimationTabState* State)
{
    if (!State || !State->CurrentMesh)
    {
        return;
    }

    if (State->bIsRecording)
    {
        // Stop Recording
        State->bIsRecording = false;

        // RecordedFrames를 AnimSequence로 변환하고 저장
        if (State->RecordedFrames.Num() > 0 && !State->RecordedFileName.empty())
        {
            // 새 AnimSequence 생성
            UAnimSequence* NewAnim = new UAnimSequence();
            NewAnim->SetName(State->RecordedFileName);

            // DataModel 생성 및 Skeleton 설정
            UAnimDataModel* DataModel = new UAnimDataModel();
            const FSkeleton* SourceSkeleton = State->CurrentMesh->GetSkeleton();
            if (SourceSkeleton)
            {
                DataModel->SetSkeleton(*SourceSkeleton);
            }

            // 프레임 정보 설정
            int32 NumFrames = State->RecordedFrames.Num();
            float FrameRate = State->RecordFrameRate;
            DataModel->NumberOfFrames = NumFrames;
            DataModel->FrameRate = FFrameRate(static_cast<int32>(FrameRate), 1);
            DataModel->PlayLength = static_cast<float>(NumFrames) / FrameRate;
            DataModel->NumberOfKeys = NumFrames;

            // 본별 트랙 생성
            if (SourceSkeleton && NumFrames > 0)
            {
                DataModel->BoneAnimationTracks.resize(SourceSkeleton->Bones.size());

                for (size_t BoneIdx = 0; BoneIdx < SourceSkeleton->Bones.size(); ++BoneIdx)
                {
                    FBoneAnimationTrack& Track = DataModel->BoneAnimationTracks[BoneIdx];
                    Track.BoneName = SourceSkeleton->Bones[BoneIdx].Name;

                    // 각 프레임의 트랜스폼 데이터 추출
                    Track.InternalTrack.PosKeys.reserve(NumFrames);
                    Track.InternalTrack.RotKeys.reserve(NumFrames);
                    Track.InternalTrack.ScaleKeys.reserve(NumFrames);

                    for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
                    {
                        const TMap<int32, FTransform>& FrameData = State->RecordedFrames[FrameIdx];

                        // 해당 본의 트랜스폼 찾기
                        auto It = FrameData.find(static_cast<int32>(BoneIdx));
                        if (It != FrameData.end())
                        {
                            const FTransform& BoneTransform = It->second;
                            Track.InternalTrack.PosKeys.push_back(BoneTransform.Translation);
                            Track.InternalTrack.RotKeys.push_back(BoneTransform.Rotation);
                            Track.InternalTrack.ScaleKeys.push_back(BoneTransform.Scale3D);
                        }
                        else
                        {
                            // 데이터 없으면 기본값 (Identity)
                            Track.InternalTrack.PosKeys.push_back(FVector(0, 0, 0));
                            Track.InternalTrack.RotKeys.push_back(FQuat::Identity());
                            Track.InternalTrack.ScaleKeys.push_back(FVector(1, 1, 1));
                        }
                    }
                }
            }

            NewAnim->SetDataModel(DataModel);

            // 파일로 저장
            FString SavePath = "Data/Animation/" + State->RecordedFileName + ".anim";

            if (NewAnim->SaveToFile(SavePath))
            {
                // SkeletalMesh의 Animation List에 추가
                State->CurrentMesh->AddAnimation(NewAnim);

                // 새로 만든 애니메이션을 현재 애니메이션으로 설정
                const TArray<UAnimSequence*>& Animations = State->CurrentMesh->GetAnimations();
                for (int32 i = 0; i < Animations.Num(); ++i)
                {
                    if (Animations[i] == NewAnim)
                    {
                        State->SelectedAnimationIndex = i;
                        State->CurrentAnimation = NewAnim;
                        State->CurrentAnimationTime = 0.0f;
                        if (USkeletalMeshComponent* SkelComp = State->PreviewActor ? State->PreviewActor->GetSkeletalMeshComponent() : nullptr)
                        {
                            SkelComp->ClearAllBoneDeltas();
                        }
                        State->bIsPlaying = false;

                        if (NewAnim->GetDataModel())
                        {
                            int32 TotalFrames = NewAnim->GetDataModel()->GetNumberOfFrames();
                            State->WorkingRangeStartFrame = 0;
                            State->WorkingRangeEndFrame = TotalFrames;
                        }
                        break;
                    }
                }

                UE_LOG("[PreviewWindow] Recording saved: %s (%d frames)", SavePath.c_str(), NumFrames);
            }
            else
            {
                UE_LOG("[PreviewWindow] Recording save failed");
            }
        }
        else
        {
            UE_LOG("[PreviewWindow] Recording stopped: No frames or filename empty");
        }
    }
    else
    {
        // Show dialog to enter filename
        State->bShowRecordDialog = true;
        memset(State->RecordFileNameBuffer, 0, sizeof(State->RecordFileNameBuffer));
    }
}

void SAnimationWindow::TimelinePlay(FAnimationTabState* State)
{
    if (!State)
    {
        return;
    }

    // 정방향 재생 - AnimInstance를 통해 설정
    State->PlaybackSpeed = FMath::Abs(State->PlaybackSpeed);

    if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
    {
        UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
        if (AnimInst)
        {
            // 일시정지 위치에서 재생 시작
            AnimInst->SetPlayRate(State->PlaybackSpeed);

            // 현재 애니메이션이 설정되지 않았을 때만 PlayAnimation 호출
            if (AnimInst->GetCurrentAnimation() != State->CurrentAnimation)
            {
                AnimInst->PlayAnimation(State->CurrentAnimation, State->PlaybackSpeed);
            }
            else
            {
                // 같은 애니메이션이면 현재 위치에서 재생 재개
                AnimInst->ResumeAnimation();
            }

            State->bIsPlaying = true;
        }
    }
}

void SAnimationWindow::TimelineToNext(FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    // 프레임 단위로 스냅하여 이동
    const FFrameRate& FrameRate = DataModel->GetFrameRate();
    float TimePerFrame = 1.0f / FrameRate.AsDecimal();
    int32 TotalFrames = DataModel->GetNumberOfFrames();

    // 현재 시간을 프레임으로 변환 (반올림)
    int32 CurrentFrame = static_cast<int32>(State->CurrentAnimationTime / TimePerFrame + 0.5f);

    // 다음 프레임으로 이동
    CurrentFrame = FMath::Min(TotalFrames, CurrentFrame + 1);

    // 프레임을 시간으로 변환
    State->CurrentAnimationTime = static_cast<float>(CurrentFrame) * TimePerFrame;

    RefreshAnimationFrame(State);
}

void SAnimationWindow::TimelineToEnd(FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    State->CurrentAnimationTime = DataModel->GetPlayLength();

    RefreshAnimationFrame(State);
}

// ========================================
// 새로운 커스텀 타임라인 렌더링
// ========================================

void SAnimationWindow::RenderTimeline(FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    float MaxTime = DataModel->GetPlayLength();
    int32 TotalFrames = DataModel->GetNumberOfFrames();

    // Track이 하나도 없으면 기본 Track 생성
    if (State->NotifyTrackNames.empty())
    {
        State->NotifyTrackNames.push_back("Track 1");
        State->UsedTrackNumbers.insert(1);
    }

    // 타임라인 영역 크기 설정 (남은 전체 높이 사용)
    ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
    ImVec2 TimelineSize = ImVec2(ContentAvail.x, ContentAvail.y);

    ImVec2 CursorPos = ImGui::GetCursorScreenPos();
    ImVec2 TimelineMin = CursorPos;
    ImVec2 TimelineMax = ImVec2(CursorPos.x + TimelineSize.x, CursorPos.y + TimelineSize.y);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // View Range 범위만 표시 (프레임을 시간으로 변환)
    int32 ViewStartFrame = State->ViewRangeStartFrame;
    int32 ViewEndFrame = (State->ViewRangeEndFrame < 0) ? TotalFrames : State->ViewRangeEndFrame;

    float FrameRate = DataModel->GetFrameRate().AsDecimal();
    float StartTime = (FrameRate > 0.0f) ? (ViewStartFrame / FrameRate) : 0.0f;
    float EndTime = (FrameRate > 0.0f) ? (ViewEndFrame / FrameRate) : MaxTime;

    // 눈금자 영역 (상단 30픽셀)
    const float RulerHeight = 30.0f;
    const float TrackHeight = 25.0f;
    const float NotifyPanelWidth = 150.0f;
    ImVec2 RulerMin = TimelineMin;
    ImVec2 RulerMax = ImVec2(TimelineMax.x, TimelineMin.y + RulerHeight);

    // 왼쪽 Ruler 영역에 필터 검색 UI 배치
    ImGui::SetCursorScreenPos(ImVec2(RulerMin.x + 5.0f, RulerMin.y + 5.0f));
    ImGui::PushItemWidth(NotifyPanelWidth - 10.0f);
    static char FilterBuffer[128] = "";
    ImGui::InputTextWithHint("##NotifyFilter", "Filter...", FilterBuffer, sizeof(FilterBuffer));
    ImGui::PopItemWidth();

    // 눈금자 렌더링 (필터 UI 영역 제외)
    ImVec2 RulerTimelineMin = ImVec2(RulerMin.x + NotifyPanelWidth, RulerMin.y);
    DrawTimelineRuler(DrawList, RulerTimelineMin, RulerMax, StartTime, EndTime, State);

    // === 스크롤 가능한 Track 영역 시작 ===
    ImGui::SetCursorScreenPos(ImVec2(TimelineMin.x, RulerMax.y));
    ImVec2 ScrollAreaSize = ImVec2(TimelineSize.x, TimelineMax.y - RulerMax.y);

    // 내부 콘텐츠 높이는 실제 Track 수에 따라 결정
    int32 NumTracks = FMath::Max(1, State->NotifyTrackNames.Num());
    float ActualContentHeight = (NumTracks + 1) * TrackHeight;  // Notifies 헤더 + Track들
    float ContentHeight = ActualContentHeight;  // 실제 콘텐츠 높이만 사용

    // 스크롤바 활성화 (세로 스크롤만, 가로 스크롤 비활성화)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));  // 패딩 제거
    ImGui::BeginChild("##TrackScrollArea", ScrollAreaSize, false, ImGuiWindowFlags_NoScrollWithMouse);

    // 스크롤 영역 내부의 좌표 시작점
    ImVec2 ScrollCursor = ImGui::GetCursorScreenPos();
    DrawList = ImGui::GetWindowDrawList();  // 스크롤 영역의 DrawList로 교체

    // === 왼쪽: Notify Track 목록 패널 (ImGui UI + DrawList 혼합) ===
    ImVec2 NotifyPanelMin = ScrollCursor;

    // NumTracks는 이미 742번 줄에서 선언됨

    // Notify 패널 높이는 스크롤 영역과 무관하게 실제 Track 수만큼만
    float ActualPanelHeight = TrackHeight;  // Notifies 헤더는 항상 표시
    if (State->bNotifiesExpanded)
    {
        ActualPanelHeight += NumTracks * TrackHeight;  // Track 추가
    }
    // ContentHeight가 아닌 실제 패널 높이만큼만 배경 렌더링
    ImVec2 NotifyPanelMax = ImVec2(ScrollCursor.x + NotifyPanelWidth, ScrollCursor.y + ActualPanelHeight);

    // Notify 패널 배경
    DrawList->AddRectFilled(NotifyPanelMin, NotifyPanelMax, IM_COL32(35, 35, 35, 255));
    DrawList->AddLine(ImVec2(NotifyPanelMax.x, NotifyPanelMin.y), ImVec2(NotifyPanelMax.x, NotifyPanelMax.y), IM_COL32(60, 60, 60, 255));
    float CurrentY = ScrollCursor.y;

    // 첫 번째 줄: Notifies 토글 (CollapsingHeader 스타일)
    ImGui::SetCursorScreenPos(ImVec2(NotifyPanelMin.x + 5.0f, CurrentY + 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.40f, 0.55f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.45f, 0.60f, 0.8f));
    ImGui::PushItemWidth(NotifyPanelWidth - 35.0f);

    if (ImGui::CollapsingHeader("Notifies", ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_DefaultOpen))
    {
        State->bNotifiesExpanded = true;
    }
    else
    {
        State->bNotifiesExpanded = false;
    }

    // Notifies 헤더 우측에 + 버튼 (새 Track 추가)
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
    if (ImGui::SmallButton("+##AddTrack"))
    {
        // Track 번호 재사용 로직
        int32 NewTrackNumber = 1;
        if (!State->UsedTrackNumbers.empty())
        {
            int32 Expected = 1;
            for (int32 UsedNum : State->UsedTrackNumbers)
            {
                if (UsedNum != Expected)
                {
                    NewTrackNumber = Expected;
                    break;
                }
                ++Expected;
            }
            if (NewTrackNumber == 1 && State->UsedTrackNumbers.count(1) > 0)
            {
                NewTrackNumber = static_cast<int32>(State->UsedTrackNumbers.size()) + 1;
            }
        }

        char TrackNameBuf[64];
        snprintf(TrackNameBuf, sizeof(TrackNameBuf), "Track %d", NewTrackNumber);
        State->NotifyTrackNames.push_back(TrackNameBuf);
        State->UsedTrackNumbers.insert(NewTrackNumber);
    }
    ImGui::PopStyleColor(3);

    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);

    // 첫 줄은 Notifies 토글이 차지
    CurrentY += TrackHeight;

    // Notifies가 펼쳐져 있을 때만 Track 이름 렌더링
    static int32 TrackToRemove = -1;
    if (State->bNotifiesExpanded)
    {
        for (int32 TrackIndex = 0; TrackIndex < State->NotifyTrackNames.Num(); ++TrackIndex)
        {
            float TrackY = CurrentY + TrackHeight * 0.5f;
            const char* TrackName = State->NotifyTrackNames[TrackIndex].c_str();
            ImVec2 TextSize = ImGui::CalcTextSize(TrackName);

            // Track 이름 렌더링 (InvisibleButton으로 클릭 영역 확보)
            ImGui::SetCursorScreenPos(ImVec2(NotifyPanelMin.x + 10.0f, CurrentY));
            ImGui::PushID(TrackIndex);
            ImGui::InvisibleButton("##TrackLabel", ImVec2(NotifyPanelWidth - 40.0f, TrackHeight));

            // 우클릭 컨텍스트 메뉴
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Insert Track Below"))
                {
                    // Track 번호 재사용 로직
                    int32 NewTrackNumber = 1;
                    if (!State->UsedTrackNumbers.empty())
                    {
                        int32 Expected = 1;
                        for (int32 UsedNum : State->UsedTrackNumbers)
                        {
                            if (UsedNum != Expected)
                            {
                                NewTrackNumber = Expected;
                                break;
                            }
                            ++Expected;
                        }
                        if (NewTrackNumber == 1 && State->UsedTrackNumbers.count(1) > 0)
                        {
                            NewTrackNumber = static_cast<int32>(State->UsedTrackNumbers.size()) + 1;
                        }
                    }

                    char TrackNameBuf[64];
                    snprintf(TrackNameBuf, sizeof(TrackNameBuf), "Track %d", NewTrackNumber);

                    // Track 중간에 삽입 시 이후 Notify들의 TrackIndex 조정
                    int32 InsertIndex = TrackIndex + 1;
                    if (State->CurrentAnimation)
                    {
                        TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
                        for (int32 i = 0; i < Notifies.Num(); ++i)
                        {
                            if (Notifies[i].TrackIndex >= InsertIndex)
                            {
                                Notifies[i].TrackIndex++;
                            }
                        }
                    }

                    State->NotifyTrackNames.insert(State->NotifyTrackNames.begin() + InsertIndex, TrackNameBuf);
                    State->UsedTrackNumbers.insert(NewTrackNumber);
                }

                // 마지막 남은 Track은 삭제 불가
                // Notify가 있는 Track도 삭제 불가
                bool bHasNotify = false;
                if (State->CurrentAnimation)
                {
                    const TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
                    for (const FAnimNotifyEvent& Notify : Notifies)
                    {
                        if (Notify.TrackIndex == TrackIndex)
                        {
                            bHasNotify = true;
                            break;
                        }
                    }
                }
                bool bCanRemove = State->NotifyTrackNames.Num() > 1 && !bHasNotify;
                if (ImGui::MenuItem("Remove Track", nullptr, false, bCanRemove))
                {
                    TrackToRemove = TrackIndex;
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(2);  // WindowPadding + ItemSpacing
            ImGui::PopID();

            // Track 이름 텍스트 렌더링
            DrawList->AddText(ImVec2(NotifyPanelMin.x + 10.0f, TrackY - TextSize.y * 0.5f), IM_COL32(220, 220, 220, 255), TrackName);

            CurrentY += TrackHeight;
        }
    }

    // Track 제거 처리 (루프 밖에서 수행)
    if (TrackToRemove >= 0 && TrackToRemove < State->NotifyTrackNames.Num())
    {
        // Track 번호 추출 (형식: "Track 3")
        const FString& TrackName = State->NotifyTrackNames[TrackToRemove];
        int32 TrackNumber = 0;
        if (sscanf_s(TrackName.c_str(), "Track %d", &TrackNumber) == 1)
        {
            State->UsedTrackNumbers.erase(TrackNumber);
        }

        State->NotifyTrackNames.erase(State->NotifyTrackNames.begin() + TrackToRemove);

        // 해당 Track의 Notify들 제거
        if (State->CurrentAnimation)
        {
            TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
            for (int32 i = Notifies.Num() - 1; i >= 0; --i)
            {
                if (Notifies[i].TrackIndex == TrackToRemove)
                {
                    Notifies.erase(Notifies.begin() + i);
                }
                else if (Notifies[i].TrackIndex > TrackToRemove)
                {
                    // 이후 Track들의 인덱스 조정
                    Notifies[i].TrackIndex--;
                }
            }
        }

        TrackToRemove = -1;
    }

    // Timeline 영역 조정 (스크롤 영역 기준, Notify 패널 제외)
    ImVec2 ScrollTimelineMin = ImVec2(ScrollCursor.x + NotifyPanelWidth, ScrollCursor.y);
    ImVec2 ScrollTimelineMax = ImVec2(ScrollCursor.x + ScrollAreaSize.x, ScrollCursor.y + ScrollAreaSize.y);
    float ScrollTimelineWidth = ScrollTimelineMax.x - ScrollTimelineMin.x;

    // 세로 그리드 (프레임 단위): 항상 스크롤 영역 전체 높이까지
    // ViewStartFrame, ViewEndFrame은 이미 713-714번 줄에서 선언됨
    int32 NumFrames = ViewEndFrame - ViewStartFrame;

    for (int32 FrameIndex = ViewStartFrame; FrameIndex <= ViewEndFrame; ++FrameIndex)
    {
        float NormalizedX = static_cast<float>(FrameIndex - ViewStartFrame) / static_cast<float>(NumFrames);
        float ScreenX = ScrollTimelineMin.x + NormalizedX * ScrollTimelineWidth;

        // 세로 줄 (스크롤 영역 전체 높이, Track 개수와 무관)
        DrawList->AddLine(
            ImVec2(ScreenX, ScrollCursor.y),
            ImVec2(ScreenX, ScrollCursor.y + FMath::Max(ScrollAreaSize.y, ContentHeight)),
            IM_COL32(70, 70, 70, 255),
            1.0f
        );
    }

    // 가로 그리드 (Track 단위): Track 개수 + 1줄만 (Notifies 헤더 포함)
    int32 TotalLines = NumTracks + 2;  // Notifies 헤더 + Track들 + 1줄 여유
    for (int32 LineIndex = 0; LineIndex < TotalLines; ++LineIndex)
    {
        float LineY = ScrollCursor.y + LineIndex * TrackHeight;
        DrawList->AddLine(
            ImVec2(ScrollTimelineMin.x, LineY),
            ImVec2(ScrollTimelineMax.x, LineY),
            IM_COL32(60, 60, 60, 80),
            1.0f
        );
    }

    // Notify 렌더링을 위해 CurrentY 재설정 (Notifies 헤더 다음부터 시작)
    CurrentY = ScrollCursor.y + TrackHeight;

    // Notify 마커 위에서 마우스가 hover 중인지 플래그 (Timeline 우클릭 메뉴 방지용)
    static bool bIsHoveringNotify = false;
    bIsHoveringNotify = false;  // 매 프레임 초기화

    for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
    {
        ImVec2 TrackMin = ImVec2(ScrollTimelineMin.x, CurrentY);
        ImVec2 TrackMax = ImVec2(ScrollTimelineMax.x, CurrentY + TrackHeight);

        // Track 배경 제거 (격자만 표시)

        // Notifies가 펼쳐져 있고 Track이 존재하면 해당 Track의 Notify 렌더링
        if (State->bNotifiesExpanded && TrackIndex < State->NotifyTrackNames.Num())
        {
            TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
            for (int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
            {
                FAnimNotifyEvent& Notify = Notifies[NotifyIndex];

                // 이 Track에 속한 Notify만 표시
                if (Notify.TrackIndex != TrackIndex)
                {
                    continue;
                }

                // 현재 보이는 시간 범위 내에 있는지 확인
                // NotifyState는 시작이 범위 밖이어도 끝이 범위 안이면 표시
                float NotifyEndTime = Notify.TriggerTime + Notify.Duration;
                bool bIsVisible = (Notify.TriggerTime <= EndTime) && (NotifyEndTime >= StartTime);

                if (bIsVisible)
                {
                    float NormalizedTime = (Notify.TriggerTime - StartTime) / (EndTime - StartTime);
                    float NotifyX = ScrollTimelineMin.x + NormalizedTime * ScrollTimelineWidth;

                    FString NotifyNameString = Notify.NotifyName.ToString();
                    const char* NotifyNameStr = NotifyNameString.c_str();
                    ImVec2 TextSize = ImGui::CalcTextSize(NotifyNameStr);
                    ImU32 NotifyColor = (State->SelectedNotifyIndex == NotifyIndex) ? IM_COL32(255, 200, 0, 255) : IM_COL32(100, 150, 255, 255);

                    bool bIsNotifyState = (Notify.Duration > 0.0f);

                    if (bIsNotifyState)
                    {
                        // NotifyState: 박스 형태 + 시작/끝 다이아몬드
                        float EndTime_Notify = Notify.TriggerTime + Notify.Duration;
                        float NormalizedEndTime = (EndTime_Notify - StartTime) / (EndTime - StartTime);
                        float EndX = ScrollTimelineMin.x + NormalizedEndTime * ScrollTimelineWidth;

                        // ViewRange에 맞춰 클리핑
                        float ClippedStartX = FMath::Max(NotifyX, ScrollTimelineMin.x);
                        float ClippedEndX = FMath::Min(EndX, ScrollTimelineMin.x + ScrollTimelineWidth);

                        // 박스 (Track 라인 상단에 표시)
                        const float BoxHeight = 18.0f;
                        float BoxY = CurrentY + (TrackHeight - BoxHeight) * 0.5f;
                        ImVec2 BoxMin = ImVec2(ClippedStartX, BoxY);
                        ImVec2 BoxMax = ImVec2(ClippedEndX, BoxY + BoxHeight);
                        DrawList->AddRectFilled(BoxMin, BoxMax, IM_COL32(100, 150, 255, 150));
                        DrawList->AddRect(BoxMin, BoxMax, NotifyColor, 0.0f, 0, 2.0f);

                        // 텍스트 (박스 중앙)
                        ImVec2 TextPos = ImVec2(ClippedStartX + (ClippedEndX - ClippedStartX - TextSize.x) * 0.5f, BoxY + (BoxHeight - TextSize.y) * 0.5f);
                        DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), NotifyNameStr);

                        // 시작 다이아몬드 (ViewRange 안에 있을 때만 표시)
                        const float DiamondSize = 5.0f;
                        if (NotifyX >= ScrollTimelineMin.x && NotifyX <= ScrollTimelineMin.x + ScrollTimelineWidth)
                        {
                            ImVec2 StartCenter = ImVec2(NotifyX, BoxY + BoxHeight * 0.5f);
                            DrawList->AddQuadFilled(
                                ImVec2(StartCenter.x, StartCenter.y - DiamondSize),
                                ImVec2(StartCenter.x + DiamondSize, StartCenter.y),
                            ImVec2(StartCenter.x, StartCenter.y + DiamondSize),
                            ImVec2(StartCenter.x - DiamondSize, StartCenter.y),
                            NotifyColor
                        );
                        }

                        // 끝 다이아몬드 (ViewRange 안에 있을 때만 표시)
                        if (EndX >= ScrollTimelineMin.x && EndX <= ScrollTimelineMin.x + ScrollTimelineWidth)
                        {
                            ImVec2 EndCenter = ImVec2(EndX, BoxY + BoxHeight * 0.5f);
                            DrawList->AddQuadFilled(
                                ImVec2(EndCenter.x, EndCenter.y - DiamondSize),
                                ImVec2(EndCenter.x + DiamondSize, EndCenter.y),
                                ImVec2(EndCenter.x, EndCenter.y + DiamondSize),
                                ImVec2(EndCenter.x - DiamondSize, EndCenter.y),
                                NotifyColor
                            );
                        }

                        // 드래그용 InvisibleButton 2개: 오른쪽 끝(Duration 조정) + 박스 중앙(전체 이동)
                        ImGui::PushID(NotifyIndex * 1000 + TrackIndex);

                        // 1. 오른쪽 끝 리사이즈 핸들 (EndX ± 8px 범위)
                        const float ResizeHandleWidth = 16.0f;
                        ImVec2 ResizeHandleMin = ImVec2(EndX - ResizeHandleWidth * 0.5f, BoxY);
                        ImVec2 ResizeHandleSize = ImVec2(ResizeHandleWidth, BoxHeight);
                        ImGui::SetCursorScreenPos(ResizeHandleMin);
                        ImGui::InvisibleButton("##NotifyResize", ResizeHandleSize);

                        bool bIsResizing = ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                        bool bIsHoveringResize = ImGui::IsItemHovered();

                        // 리사이즈 호버 시 커서 변경
                        if (bIsHoveringResize || bIsResizing)
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }

                        // Duration 조정 (오른쪽 끝 드래그)
                        if (bIsResizing)
                        {
                            ImVec2 MousePos = ImGui::GetMousePos();
                            float MouseNormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
                            float MouseTime = FMath::Lerp(StartTime, EndTime, FMath::Clamp(MouseNormalizedX, 0.0f, 1.0f));

                            // Playback Range로 제한
                            UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
                            if (DataModel)
                            {
                                const FFrameRate& FrameRate = DataModel->GetFrameRate();
                                float TimePerFrame = 1.0f / FrameRate.AsDecimal();

                                int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
                                int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? DataModel->GetNumberOfFrames() : State->PlaybackRangeEndFrame;
                                float PlaybackStartTime = static_cast<float>(PlaybackStartFrame) * TimePerFrame;
                                float PlaybackEndTime = static_cast<float>(PlaybackEndFrame) * TimePerFrame;

                                MouseTime = FMath::Clamp(MouseTime, PlaybackStartTime, PlaybackEndTime);
                            }

                            // Duration = MouseTime - TriggerTime (최소 0.01초)
                            float NewDuration = FMath::Max(0.01f, MouseTime - Notify.TriggerTime);
                            Notify.Duration = NewDuration;
                            State->SelectedNotifyIndex = NotifyIndex;
                        }

                        // 2. 박스 중앙 이동 핸들 (전체 박스 - 리사이즈 핸들 제외)
                        ImVec2 MoveHandleMin = BoxMin;
                        ImVec2 MoveHandleSize = ImVec2(EndX - NotifyX - ResizeHandleWidth * 0.5f, BoxHeight);
                        ImGui::SetCursorScreenPos(MoveHandleMin);
                        ImGui::InvisibleButton("##NotifyMove", MoveHandleSize);

                        // NotifyState 전체 이동 드래그 (리사이즈 중이 아닐 때만)
                        if (!bIsResizing && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                        {
                            ImVec2 MousePos = ImGui::GetMousePos();
                            float MouseNormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
                            float MouseTime = FMath::Lerp(StartTime, EndTime, FMath::Clamp(MouseNormalizedX, 0.0f, 1.0f));

                            // Playback Range로 제한
                            UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
                            if (DataModel)
                            {
                                const FFrameRate& FrameRate = DataModel->GetFrameRate();
                                float TimePerFrame = 1.0f / FrameRate.AsDecimal();

                                int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
                                int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? DataModel->GetNumberOfFrames() : State->PlaybackRangeEndFrame;
                                float PlaybackStartTime = static_cast<float>(PlaybackStartFrame) * TimePerFrame;
                                float PlaybackEndTime = static_cast<float>(PlaybackEndFrame) * TimePerFrame;

                                MouseTime = FMath::Clamp(MouseTime, PlaybackStartTime, PlaybackEndTime);
                            }

                            Notify.TriggerTime = MouseTime;
                            State->SelectedNotifyIndex = NotifyIndex;
                        }

                        // Hover 시 플래그 설정 (Timeline 우클릭 메뉴 방지용)
                        if (ImGui::IsItemHovered() || bIsHoveringResize)
                        {
                            bIsHoveringNotify = true;
                        }
                    }
                    else
                    {
                        // Notify: 다이아몬드 + 박스 태그
                        const float BoxHeight = 18.0f;
                        const float BoxPadding = 4.0f;
                        float BoxWidth = TextSize.x + BoxPadding * 2;
                        float BoxY = CurrentY + (TrackHeight - BoxHeight) * 0.5f;

                        // 박스 (Track 라인 상단에 표시)
                        ImVec2 BoxMin = ImVec2(NotifyX + 8.0f, BoxY);
                        ImVec2 BoxMax = ImVec2(NotifyX + 8.0f + BoxWidth, BoxY + BoxHeight);
                        DrawList->AddRectFilled(BoxMin, BoxMax, IM_COL32(100, 150, 255, 200));
                        DrawList->AddRect(BoxMin, BoxMax, NotifyColor, 0.0f, 0, 2.0f);

                        // 텍스트 (박스 중앙)
                        ImVec2 TextPos = ImVec2(BoxMin.x + BoxPadding, BoxY + (BoxHeight - TextSize.y) * 0.5f);
                        DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), NotifyNameStr);

                        // 다이아몬드 (박스 왼쪽)
                        const float DiamondSize = 5.0f;
                        ImVec2 DiamondCenter = ImVec2(NotifyX, BoxY + BoxHeight * 0.5f);
                        DrawList->AddQuadFilled(
                            ImVec2(DiamondCenter.x, DiamondCenter.y - DiamondSize),
                            ImVec2(DiamondCenter.x + DiamondSize, DiamondCenter.y),
                            ImVec2(DiamondCenter.x, DiamondCenter.y + DiamondSize),
                            ImVec2(DiamondCenter.x - DiamondSize, DiamondCenter.y),
                            NotifyColor
                        );

                        // 드래그용 InvisibleButton (다이아몬드 + 박스)
                        ImGui::SetCursorScreenPos(ImVec2(NotifyX - DiamondSize, BoxY));
                        ImGui::PushID(NotifyIndex * 1000 + TrackIndex);
                        ImGui::InvisibleButton("##NotifyMarker", ImVec2(BoxWidth + 8.0f + DiamondSize, BoxHeight));

                        // 일반 Notify 드래그 처리
                        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                        {
                            ImVec2 MousePos = ImGui::GetMousePos();
                            float MouseNormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
                            float MouseTime = FMath::Lerp(StartTime, EndTime, FMath::Clamp(MouseNormalizedX, 0.0f, 1.0f));

                            // Playback Range로 제한
                            UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
                            if (DataModel)
                            {
                                const FFrameRate& FrameRate = DataModel->GetFrameRate();
                                float TimePerFrame = 1.0f / FrameRate.AsDecimal();

                                // Playback Range 시간 범위 계산
                                int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
                                int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? DataModel->GetNumberOfFrames() : State->PlaybackRangeEndFrame;
                                float PlaybackStartTime = static_cast<float>(PlaybackStartFrame) * TimePerFrame;
                                float PlaybackEndTime = static_cast<float>(PlaybackEndFrame) * TimePerFrame;

                                // 마우스 시간을 Playback Range 내로 클램프
                                MouseTime = FMath::Clamp(MouseTime, PlaybackStartTime, PlaybackEndTime);
                            }

                            Notify.TriggerTime = MouseTime;
                            State->SelectedNotifyIndex = NotifyIndex;
                        }

                        // Hover 시 플래그 설정 (Timeline 우클릭 메뉴 방지용)
                        if (ImGui::IsItemHovered())
                        {
                            bIsHoveringNotify = true;
                        }
                    }

                    // 클릭 시 선택
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        State->SelectedNotifyIndex = NotifyIndex;
                    }

                    // 우클릭 시 Edit 메뉴
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                    {
                        State->SelectedNotifyIndex = NotifyIndex;
                        ImGui::OpenPopup("##NotifyContextMenu");
                    }

                    // Notify 컨텍스트 메뉴 (PopID 전에 BeginPopup 호출해야 ID 스택 일치)
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
                    if (ImGui::BeginPopup("##NotifyContextMenu"))
                    {
                        // 헤더: Notify 정보 표시
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "NOTIFY");

                        ImGui::Separator();

                        // Begin Time 수정 (초 단위)
                        ImGui::Text("Begin Time");
                        ImGui::SameLine();
                        float TriggerTime = Notify.TriggerTime;
                        ImGui::SetNextItemWidth(100.0f);
                        if (ImGui::DragFloat("##BeginTime", &TriggerTime, 0.01f, 0.0f, FLT_MAX, "%.3f"))
                        {
                            // Playback Range로 제한
                            UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
                            if (DataModel)
                            {
                                const FFrameRate& FrameRate = DataModel->GetFrameRate();
                                float TimePerFrame = 1.0f / FrameRate.AsDecimal();
                                int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
                                int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? DataModel->GetNumberOfFrames() : State->PlaybackRangeEndFrame;
                                float PlaybackStartTime = static_cast<float>(PlaybackStartFrame) * TimePerFrame;
                                float PlaybackEndTime = static_cast<float>(PlaybackEndFrame) * TimePerFrame;
                                Notify.TriggerTime = FMath::Clamp(TriggerTime, PlaybackStartTime, PlaybackEndTime);
                            }
                            else
                            {
                                Notify.TriggerTime = FMath::Max(0.0f, TriggerTime);
                            }
                        }

                        // Notify Frame 수정 (프레임 단위)
                        ImGui::Text("Notify Frame");
                        ImGui::SameLine();
                        if (State->CurrentAnimation && State->CurrentAnimation->GetDataModel())
                        {
                            const FFrameRate& FR = State->CurrentAnimation->GetDataModel()->GetFrameRate();
                            float TimePerFrame = 1.0f / FR.AsDecimal();
                            int32 NotifyFrame = static_cast<int32>(Notify.TriggerTime * FR.AsDecimal());

                            ImGui::SetNextItemWidth(100.0f);
                            if (ImGui::DragInt("##NotifyFrame", &NotifyFrame, 1.0f, 0, INT_MAX))
                            {
                                // 프레임을 시간으로 변환
                                float NewTime = static_cast<float>(NotifyFrame) * TimePerFrame;

                                // Playback Range로 제한
                                int32 PlaybackStartFrame = State->PlaybackRangeStartFrame;
                                int32 PlaybackEndFrame = (State->PlaybackRangeEndFrame < 0) ? State->CurrentAnimation->GetDataModel()->GetNumberOfFrames() : State->PlaybackRangeEndFrame;
                                float PlaybackStartTime = static_cast<float>(PlaybackStartFrame) * TimePerFrame;
                                float PlaybackEndTime = static_cast<float>(PlaybackEndFrame) * TimePerFrame;
                                Notify.TriggerTime = FMath::Clamp(NewTime, PlaybackStartTime, PlaybackEndTime);
                            }
                        }

                        // Duration 수정 (AnimNotifyState만 해당)
                        if (Notify.Duration > 0.0f)
                        {
                            ImGui::Text("Duration");
                            ImGui::SameLine();
                            float Duration = Notify.Duration;
                            ImGui::SetNextItemWidth(100.0f);
                            if (ImGui::DragFloat("##Duration", &Duration, 0.01f, 0.01f, FLT_MAX, "%.3f"))
                            {
                                Notify.Duration = FMath::Max(0.01f, Duration);
                            }
                        }

                        ImGui::Separator();

                        // EDIT 섹션
                        ImGui::TextDisabled("EDIT");

                        // Copy
                        if (ImGui::MenuItem("Copy", "CTRL+C"))
                        {
                            State->NotifyClipboard = Notify;
                            State->bHasNotifyClipboard = true;
                        }

                        // Paste
                        if (ImGui::MenuItem("Paste", "CTRL+V", false, State->bHasNotifyClipboard))
                        {
                            if (State->bHasNotifyClipboard)
                            {
                                FAnimNotifyEvent PastedNotify = State->NotifyClipboard;
                                // 현재 재생 시간 위치에 붙여넣기
                                PastedNotify.TriggerTime = State->CurrentAnimationTime;
                                // TrackIndex는 현재 선택된 Notify와 동일하게 유지
                                PastedNotify.TrackIndex = Notify.TrackIndex;
                                Notifies.push_back(PastedNotify);
                                State->SelectedNotifyIndex = Notifies.Num() - 1;
                            }
                        }

                        // Delete
                        if (ImGui::MenuItem("Delete", "DEL"))
                        {
                            // Notify 제거 (루프 밖에서 처리하도록 플래그 설정)
                            State->SelectedNotifyIndex = -1;
                            Notifies.erase(Notifies.begin() + NotifyIndex);
                        }

                        // Cut
                        if (ImGui::MenuItem("Cut", "CTRL+X"))
                        {
                            State->NotifyClipboard = Notify;
                            State->bHasNotifyClipboard = true;
                            State->SelectedNotifyIndex = -1;
                            Notifies.erase(Notifies.begin() + NotifyIndex);
                        }

                        // Replace with Notify... (하위 메뉴)
                        if (ImGui::BeginMenu("Replace with Notify..."))
                        {
                            // Notify 라이브러리 자동 스캔
                            ScanNotifyLibrary();

                            for (const FString& NotifyClass : AvailableNotifyClasses)
                            {
                                if (ImGui::MenuItem(NotifyClass.c_str()))
                                {
                                    Notify.NotifyName = FName(NotifyClass);
                                    Notify.Duration = 0.0f;  // Notify는 Duration 없음
                                }
                            }
                            ImGui::EndMenu();
                        }

                        // Replace with Notify State... (하위 메뉴)
                        if (ImGui::BeginMenu("Replace with Notify State..."))
                        {
                            // NotifyState 라이브러리 자동 스캔
                            ScanNotifyLibrary();

                            for (const FString& NotifyStateClass : AvailableNotifyStateClasses)
                            {
                                if (ImGui::MenuItem(NotifyStateClass.c_str()))
                                {
                                    Notify.NotifyName = FName(NotifyStateClass);
                                    if (Notify.Duration == 0.0f)
                                    {
                                        Notify.Duration = 0.1f;  // NotifyState는 기본 Duration 설정
                                    }
                                }
                            }
                            ImGui::EndMenu();
                        }

                        ImGui::Separator();

                        // Min Trigger Weight
                        ImGui::Text("Min Trigger Weight");
                        ImGui::SetNextItemWidth(150.0f);
                        ImGui::DragFloat("##MinTriggerWeight", &Notify.TriggerWeightThreshold, 0.01f, 0.0f, 1.0f, "%.5f");

                        // Edit Script (Lua 파일 열기)
                        ImGui::Separator();
                        if (ImGui::MenuItem("Edit Script"))
                        {
                            FString NotifyClassName = Notify.NotifyName.ToString();
                            bool bIsNotifyState = (Notify.Duration > 0.0f);
                            OpenNotifyScriptInEditor(NotifyClassName, bIsNotifyState);
                        }

                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar(2);

                    ImGui::PopID();

                    // Duration이 있으면 연장선 표시
                    if (Notify.Duration > 0.0f)
                    {
                        float EndTimeNotify = Notify.TriggerTime + Notify.Duration;
                        if (EndTimeNotify <= EndTime)
                        {
                            float NormalizedEndTime = (EndTimeNotify - StartTime) / (EndTime - StartTime);
                            float EndX = ScrollTimelineMin.x + NormalizedEndTime * ScrollTimelineWidth;

                            ImVec2 DurationMin = ImVec2(NotifyX, CurrentY + 2);
                            ImVec2 DurationMax = ImVec2(EndX, CurrentY + TrackHeight - 2);
                            DrawList->AddRectFilled(DurationMin, DurationMax, IM_COL32(100, 150, 255, 100));
                            DrawList->AddRect(DurationMin, DurationMax, IM_COL32(100, 150, 255, 200));
                        }
                    }
                }
            }
        }

        CurrentY += TrackHeight;
    }

    // 재생 범위 렌더링 (녹색 반투명 영역, Ruler 위치 기준)
    ImVec2 PlaybackRangeMin = ImVec2(RulerMin.x + NotifyPanelWidth, RulerMin.y);
    ImVec2 PlaybackRangeMax = ImVec2(RulerMax.x, RulerMax.y);
    DrawPlaybackRange(DrawList, PlaybackRangeMin, PlaybackRangeMax, StartTime, EndTime, State);

    // Playhead 렌더링 (빨간 세로 바) - 세로 그리드와 동일하게 스크롤 영역 전체 높이
    ImVec2 PlayheadMin = ScrollTimelineMin;
    ImVec2 PlayheadMax = ImVec2(ScrollTimelineMax.x, ScrollCursor.y + FMath::Max(ScrollAreaSize.y, ContentHeight));
    DrawTimelinePlayhead(DrawList, PlayheadMin, PlayheadMax, State->CurrentAnimationTime, StartTime, EndTime);

    // 마우스 입력 처리 (InvisibleButton으로 영역 확보)
    ImGui::SetCursorScreenPos(ScrollTimelineMin);
    ImGui::InvisibleButton("##Timeline", ImVec2(ScrollTimelineWidth, ScrollAreaSize.y));

    // Timeline 빈 공간 클릭 시 Notify 선택 해제
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        State->SelectedNotifyIndex = -1;
    }

    // 재생 범위 핸들 드래그 감지 (Shift + 드래그, 프레임 단위)
    static bool bDraggingRangeStart = false;
    static bool bDraggingRangeEnd = false;
    bool bShiftHeld = ImGui::GetIO().KeyShift;

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 MousePos = ImGui::GetMousePos();
        float NormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
        float MouseTime = FMath::Lerp(StartTime, EndTime, NormalizedX);
        int32 MouseFrame = (FrameRate > 0.0f) ? static_cast<int32>(MouseTime * FrameRate) : 0;

        if (bShiftHeld && !bDraggingRangeStart && !bDraggingRangeEnd)
        {
            // Shift 누른 상태에서 드래그 시작: 가까운 핸들 선택
            int32 RangeEndFrame = (State->PlaybackRangeEndFrame < 0) ? TotalFrames : State->PlaybackRangeEndFrame;
            int32 DistToStart = FMath::Abs(MouseFrame - State->PlaybackRangeStartFrame);
            int32 DistToEnd = FMath::Abs(MouseFrame - RangeEndFrame);

            if (DistToStart < DistToEnd && DistToStart < 5)
            {
                bDraggingRangeStart = true;
            }
            else if (DistToEnd < 5)
            {
                bDraggingRangeEnd = true;
            }
        }

        if (bDraggingRangeStart)
        {
            // 범위 시작 드래그 (프레임 단위)
            int32 RangeEndFrame = (State->PlaybackRangeEndFrame < 0) ? TotalFrames : State->PlaybackRangeEndFrame;
            State->PlaybackRangeStartFrame = FMath::Clamp(MouseFrame, 0, RangeEndFrame - 1);
        }
        else if (bDraggingRangeEnd)
        {
            // 범위 끝 드래그 (프레임 단위)
            State->PlaybackRangeEndFrame = FMath::Clamp(MouseFrame, State->PlaybackRangeStartFrame + 1, TotalFrames);
        }
        else if (!bShiftHeld)
        {
            // 일반 드래그: 시간 스크러빙
            State->CurrentAnimationTime = FMath::Clamp(MouseTime, 0.0f, MaxTime);
            RefreshAnimationFrame(State);
        }
    }
    else
    {
        // 마우스 버튼 릴리즈
        bDraggingRangeStart = false;
        bDraggingRangeEnd = false;
    }

    // 우클릭 컨텍스트 메뉴 (Notify 추가 - Notify 마커 위가 아닐 때만)
    static float RightClickTime = 0.0f;
    static int32 RightClickTrackIndex = 0;

    if (ImGui::IsItemHovered())
    {
        // 우클릭 위치 저장 (Notify 위가 아닐 때만)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !bIsHoveringNotify)
        {
            ImVec2 MousePos = ImGui::GetMousePos();

            // 클릭한 시간 계산 (스크롤 영역 기준)
            float NormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
            float MouseTime = FMath::Lerp(StartTime, EndTime, FMath::Clamp(NormalizedX, 0.0f, 1.0f));

            // 클릭한 Track 계산 (스크롤 영역 기준, 첫 줄은 Notifies 헤더이므로 -1)
            float RelativeY = MousePos.y - ScrollCursor.y;
            int32 RowIndex = static_cast<int32>(RelativeY / TrackHeight);
            int32 TrackIndex = RowIndex - 1;  // 첫 줄(0)은 Notifies 헤더

            // Notifies 헤더 줄(TrackIndex < 0)이 아닌 경우만 처리
            // Animation이 없으면 컨텍스트 메뉴 열지 않음
            if (State->CurrentAnimation && TrackIndex >= 0 && TrackIndex < State->NotifyTrackNames.Num())
            {
                RightClickTime = MouseTime;
                RightClickTrackIndex = TrackIndex;
                ImGui::OpenPopup("##TimelineContextMenu");
            }
        }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    if (ImGui::BeginPopup("##TimelineContextMenu"))
    {
        // Animation이 없으면 메뉴 닫기 (안전장치)
        if (!State->CurrentAnimation)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            ImGui::PopStyleVar(2);
            return;
        }

        // Paste (클립보드에 Notify가 있을 때만 활성화)
        if (ImGui::MenuItem("Paste", "CTRL+V", false, State->bHasNotifyClipboard))
        {
            if (State->bHasNotifyClipboard)
            {
                FAnimNotifyEvent PastedNotify = State->NotifyClipboard;
                PastedNotify.TriggerTime = RightClickTime;
                PastedNotify.TrackIndex = RightClickTrackIndex;
                State->CurrentAnimation->AddNotify(PastedNotify);
                State->SelectedNotifyIndex = State->CurrentAnimation->Notifies.Num() - 1;
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Add Notify..."))
        {
            // 메뉴 열릴 때마다 Notify 라이브러리 자동 스캔 (파일 추가/삭제 반영)
            ScanNotifyLibrary();

            for (const FString& NotifyClass : AvailableNotifyClasses)
            {
                if (ImGui::MenuItem(NotifyClass.c_str()))
                {
                    FAnimNotifyEvent NewNotify;
                    NewNotify.NotifyName = FName(NotifyClass);
                    NewNotify.TriggerTime = RightClickTime;
                    NewNotify.Duration = 0.0f;
                    NewNotify.TrackIndex = RightClickTrackIndex;
                    State->CurrentAnimation->AddNotify(NewNotify);
                }
            }

            ImGui::Separator();

            // New Notify Script (하위 메뉴로 텍스트 입력 제공)
            if (ImGui::BeginMenu("New Notify Script..."))
            {
                ImGui::Text("Enter script name (empty for auto-numbering):");
                ImGui::SetNextItemWidth(250.0f);
                bool bEnterPressed = ImGui::InputTextWithHint("##NewNotifyName", "e.g., FootStep", NewNotifyNameBuffer, sizeof(NewNotifyNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::Spacing();

                if (ImGui::Button("Create", ImVec2(120, 0)) || bEnterPressed)
                {
                    FString ScriptName(NewNotifyNameBuffer);
                    CreateNewNotifyScript(ScriptName, false);
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add Notify State..."))
        {
            // 메뉴 열릴 때마다 Notify 라이브러리 자동 스캔 (파일 추가/삭제 반영)
            ScanNotifyLibrary();

            for (const FString& StateClass : AvailableNotifyStateClasses)
            {
                if (ImGui::MenuItem(StateClass.c_str()))
                {
                    FAnimNotifyEvent NewNotify;
                    NewNotify.NotifyName = FName(StateClass);
                    NewNotify.TriggerTime = RightClickTime;
                    NewNotify.Duration = 1.0f;
                    NewNotify.TrackIndex = RightClickTrackIndex;
                    State->CurrentAnimation->AddNotify(NewNotify);
                }
            }

            ImGui::Separator();

            // New Notify State Script (하위 메뉴로 텍스트 입력 제공)
            if (ImGui::BeginMenu("New Notify State Script..."))
            {
                ImGui::Text("Enter script name (empty for auto-numbering):");
                ImGui::SetNextItemWidth(250.0f);
                bool bEnterPressed = ImGui::InputTextWithHint("##NewNotifyStateName", "e.g., TrailEffect", NewNotifyNameBuffer, sizeof(NewNotifyNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::Spacing();

                if (ImGui::Button("Create", ImVec2(120, 0)) || bEnterPressed)
                {
                    FString ScriptName(NewNotifyNameBuffer);
                    CreateNewNotifyScript(ScriptName, true);
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                {
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);  // WindowPadding + ItemSpacing

    // Shift + 우클릭: 재생 범위 초기화
    if (ImGui::IsItemHovered() && bShiftHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        State->PlaybackRangeStartFrame = 0;
        State->PlaybackRangeEndFrame = -1; // 전체 범위
    }

    // 키보드 단축키 (Preview 윈도우에 포커스가 있고 Timeline 영역이 호버되어 있을 때만)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsItemHovered())
    {
        bool bCtrlHeld = ImGui::GetIO().KeyCtrl;

        // Ctrl+C: Copy (Animation이 있을 때만)
        if (State->CurrentAnimation && bCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_C) && State->SelectedNotifyIndex >= 0)
        {
            TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
            if (State->SelectedNotifyIndex < Notifies.Num())
            {
                State->NotifyClipboard = Notifies[State->SelectedNotifyIndex];
                State->bHasNotifyClipboard = true;
            }
        }

        // Ctrl+V: Paste (Animation이 있을 때만)
        if (State->CurrentAnimation && bCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_V) && State->bHasNotifyClipboard)
        {
            FAnimNotifyEvent PastedNotify = State->NotifyClipboard;
            PastedNotify.TriggerTime = State->CurrentAnimationTime;
            // 선택된 Notify가 있으면 같은 트랙에, 없으면 Track 0에 붙여넣기
            if (State->SelectedNotifyIndex >= 0 && State->SelectedNotifyIndex < State->CurrentAnimation->Notifies.Num())
            {
                PastedNotify.TrackIndex = State->CurrentAnimation->Notifies[State->SelectedNotifyIndex].TrackIndex;
            }
            else
            {
                PastedNotify.TrackIndex = 0;
            }
            State->CurrentAnimation->AddNotify(PastedNotify);
            State->SelectedNotifyIndex = State->CurrentAnimation->Notifies.Num() - 1;
        }

        // Ctrl+X: Cut (Animation이 있을 때만)
        if (State->CurrentAnimation && bCtrlHeld && ImGui::IsKeyPressed(ImGuiKey_X) && State->SelectedNotifyIndex >= 0)
        {
            TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
            if (State->SelectedNotifyIndex < Notifies.Num())
            {
                State->NotifyClipboard = Notifies[State->SelectedNotifyIndex];
                State->bHasNotifyClipboard = true;
                Notifies.erase(Notifies.begin() + State->SelectedNotifyIndex);
                State->SelectedNotifyIndex = -1;
            }
        }

        // Delete: Delete
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && State->SelectedNotifyIndex >= 0)
        {
            TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
            if (State->SelectedNotifyIndex < Notifies.Num())
            {
                Notifies.erase(Notifies.begin() + State->SelectedNotifyIndex);
                State->SelectedNotifyIndex = -1;
            }
        }
    }

    // 스크롤 영역 내부 콘텐츠 크기 설정
    // Track이 적으면 스크롤 불필요, 많으면 정확히 ContentHeight만큼만
    if (ContentHeight > ScrollAreaSize.y)
    {
        // 현재 커서 위치를 기준으로 남은 높이만 Dummy로 채움
        float CurrentHeight = ImGui::GetCursorPosY();
        float RemainingHeight = ContentHeight - CurrentHeight;
        if (RemainingHeight > 0.0f)
        {
            ImGui::Dummy(ImVec2(0.0f, RemainingHeight));
        }
    }

    ImGui::EndChild();  // TrackScrollArea 종료
    ImGui::PopStyleVar();  // WindowPadding 복원
}

void SAnimationWindow::DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, FAnimationTabState* State)
{
    // 눈금자 배경
    DrawList->AddRectFilled(RulerMin, RulerMax, IM_COL32(50, 50, 50, 255));

    // Ruler 하단 테두리 (첫 가로줄)
    DrawList->AddLine(
        ImVec2(RulerMin.x, RulerMax.y),
        ImVec2(RulerMax.x, RulerMax.y),
        IM_COL32(60, 60, 60, 255),
        1.0f
    );

    if (!State->CurrentAnimation || !State->CurrentAnimation->GetDataModel())
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    float FrameRate = DataModel->GetFrameRate().AsDecimal();
    if (FrameRate <= 0.0f)
    {
        return;
    }

    // View Range 프레임 범위
    int32 ViewStartFrame = State->ViewRangeStartFrame;
    int32 ViewEndFrame = (State->ViewRangeEndFrame < 0) ? DataModel->GetNumberOfFrames() : State->ViewRangeEndFrame;

    if (ViewEndFrame <= ViewStartFrame)
    {
        return;
    }

    float RulerWidth = RulerMax.x - RulerMin.x;
    int32 NumFrames = ViewEndFrame - ViewStartFrame;

    // 프레임마다 세로 줄 그리기 (타임라인 전체 높이)
    // 타임라인 하단 좌표는 DrawTimelineRuler 함수에서 알 수 없으므로 충분한 높이로 설정
    float TimelineBottom = RulerMax.y + 500.0f; // 충분히 긴 세로줄

    for (int32 FrameIndex = ViewStartFrame; FrameIndex <= ViewEndFrame; ++FrameIndex)
    {
        float NormalizedX = static_cast<float>(FrameIndex - ViewStartFrame) / static_cast<float>(NumFrames);
        float ScreenX = RulerMin.x + NormalizedX * RulerWidth;

        // 세로 줄 (Ruler 하단부터 타임라인 맨 아래까지)
        DrawList->AddLine(
            ImVec2(ScreenX, RulerMax.y),
            ImVec2(ScreenX, TimelineBottom),
            IM_COL32(70, 70, 70, 255),
            1.0f
        );

        // 프레임 번호 표시 (일정 간격마다)
        float PixelsPerFrame = RulerWidth / NumFrames;
        int32 LabelInterval = 1;

        // 간격 조정 (픽셀당 프레임 수에 따라)
        if (PixelsPerFrame < 10.0f)
        {
            LabelInterval = 10;
        }
        else if (PixelsPerFrame < 20.0f)
        {
            LabelInterval = 5;
        }
        else if (PixelsPerFrame < 40.0f)
        {
            LabelInterval = 2;
        }

        // 라벨 표시 (Ruler 중앙)
        if (FrameIndex % LabelInterval == 0)
        {
            char FrameLabel[32];
            snprintf(FrameLabel, sizeof(FrameLabel), "%d", FrameIndex);

            ImVec2 TextSize = ImGui::CalcTextSize(FrameLabel);
            float RulerCenterY = (RulerMin.y + RulerMax.y) * 0.5f - TextSize.y * 0.5f;
            DrawList->AddText(
                ImVec2(ScreenX - TextSize.x * 0.5f, RulerCenterY),
                IM_COL32(220, 220, 220, 255),
                FrameLabel
            );
        }
    }
}

void SAnimationWindow::DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime)
{
    float Duration = EndTime - StartTime;
    if (Duration <= 0.0f)
    {
        return;
    }

    float NormalizedX = (CurrentTime - StartTime) / Duration;
    if (NormalizedX < 0.0f || NormalizedX > 1.0f)
    {
        return; // 화면 밖
    }

    float ScreenX = TimelineMin.x + NormalizedX * (TimelineMax.x - TimelineMin.x);

    // 빨간 세로 바
    DrawList->AddLine(
        ImVec2(ScreenX, TimelineMin.y),
        ImVec2(ScreenX, TimelineMax.y),
        IM_COL32(255, 50, 50, 255),
        2.0f
    );

    // 상단 삼각형 핸들
    float TriangleSize = 6.0f;
    DrawList->AddTriangleFilled(
        ImVec2(ScreenX, TimelineMin.y),
        ImVec2(ScreenX - TriangleSize, TimelineMin.y + TriangleSize * 1.5f),
        ImVec2(ScreenX + TriangleSize, TimelineMin.y + TriangleSize * 1.5f),
        IM_COL32(255, 50, 50, 255)
    );
}

void SAnimationWindow::DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    float MaxTime = DataModel->GetPlayLength();
    int32 TotalFrames = DataModel->GetNumberOfFrames();
    float FrameRate = DataModel->GetFrameRate().AsDecimal();

    float Duration = EndTime - StartTime;
    if (Duration <= 0.0f)
    {
        return;
    }

    // Playback Range는 항상 전체 범위 (0 ~ TotalFrames)
    int32 RangeStartFrame = 0;
    int32 RangeEndFrame = TotalFrames;

    float RangeStart = 0.0f;
    float RangeEnd = (FrameRate > 0.0f) ? (RangeEndFrame / FrameRate) : MaxTime;

    // 화면에 보이는 부분만 클리핑
    if (RangeEnd < StartTime || RangeStart > EndTime)
    {
        return; // 완전히 화면 밖
    }

    // 화면 범위에 맞춰 클램프
    float VisibleRangeStart = FMath::Max(RangeStart, StartTime);
    float VisibleRangeEnd = FMath::Min(RangeEnd, EndTime);

    float TimelineWidth = TimelineMax.x - TimelineMin.x;
    float TimelineHeight = TimelineMax.y - TimelineMin.y;

    // 시작/끝 위치 계산
    float NormalizedStart = (VisibleRangeStart - StartTime) / Duration;
    float NormalizedEnd = (VisibleRangeEnd - StartTime) / Duration;

    float ScreenStartX = TimelineMin.x + NormalizedStart * TimelineWidth;
    float ScreenEndX = TimelineMin.x + NormalizedEnd * TimelineWidth;

    // 녹색 반투명 영역 (상단 10픽셀만)
    const float RangeBarHeight = 10.0f;
    DrawList->AddRectFilled(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenEndX, TimelineMin.y + RangeBarHeight),
        IM_COL32(50, 200, 50, 120)
    );

    // 녹색 경계선 (In/Out 마커) - 작게
    DrawList->AddLine(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenStartX, TimelineMin.y + RangeBarHeight),
        IM_COL32(50, 255, 50, 255),
        2.0f
    );

    DrawList->AddLine(
        ImVec2(ScreenEndX, TimelineMin.y),
        ImVec2(ScreenEndX, TimelineMin.y + RangeBarHeight),
        IM_COL32(50, 255, 50, 255),
        2.0f
    );

    // In/Out 핸들 (상단 삼각형) - 작게
    float HandleSize = 4.0f;

    // In 핸들 (왼쪽)
    DrawList->AddTriangleFilled(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenStartX - HandleSize, TimelineMin.y + HandleSize * 1.2f),
        ImVec2(ScreenStartX + HandleSize, TimelineMin.y + HandleSize * 1.2f),
        IM_COL32(50, 255, 50, 255)
    );

    // Out 핸들 (오른쪽)
    DrawList->AddTriangleFilled(
        ImVec2(ScreenEndX, TimelineMin.y),
        ImVec2(ScreenEndX - HandleSize, TimelineMin.y + HandleSize * 1.2f),
        ImVec2(ScreenEndX + HandleSize, TimelineMin.y + HandleSize * 1.2f),
        IM_COL32(50, 255, 50, 255)
    );
}

void SAnimationWindow::DrawNotifyTracksPanel(FAnimationTabState* State, float StartTime, float EndTime)
{
    if (!State || !State->CurrentAnimation)
    {
        ImGui::TextDisabled("No animation selected");
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        ImGui::TextDisabled("No animation data");
        return;
    }

    const float AnimLength = DataModel->GetPlayLength();
    const float Duration = EndTime - StartTime;

    if (Duration <= 0.0f)
    {
        return;
    }

    // Notify 이벤트 가져오기
    UAnimSequenceBase* AnimBase = State->CurrentAnimation;
    TArray<const FAnimNotifyEvent*> Notifies;
    AnimBase->GetAnimNotifiesFromDeltaPositions(0.0f, AnimLength, Notifies);

    // Track이 없으면 안내 메시지
    if (State->NotifyTrackNames.Num() == 0)
    {
        ImGui::TextDisabled("No tracks. Click '+ Add Track' to create one.");
        return;
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 PanelMin = ImGui::GetWindowPos();
    ImVec2 PanelSize = ImGui::GetWindowSize();
    const float PanelWidth = PanelSize.x;
    const float TrackHeight = 45.0f;
    const float RulerHeight = 25.0f;

    // 타임라인 눈금자 그리기 (상단)
    ImVec2 RulerMin = PanelMin;
    ImVec2 RulerMax = ImVec2(PanelMin.x + PanelWidth, PanelMin.y + RulerHeight);
    DrawList->AddRectFilled(RulerMin, RulerMax, IM_COL32(40, 40, 45, 255));

    // 눈금 그리기
    const int32 NumTicks = 10;
    for (int32 i = 0; i <= NumTicks; ++i)
    {
        float T = static_cast<float>(i) / static_cast<float>(NumTicks);
        float Time = FMath::Lerp(StartTime, EndTime, T);
        float X = RulerMin.x + T * PanelWidth;

        DrawList->AddLine(ImVec2(X, RulerMax.y - 5), ImVec2(X, RulerMax.y), IM_COL32(180, 180, 180, 255), 1.0f);

        char Label[32];
        snprintf(Label, sizeof(Label), "%.1fs", Time);
        ImVec2 TextSize = ImGui::CalcTextSize(Label);
        DrawList->AddText(ImVec2(X - TextSize.x * 0.5f, RulerMin.y + 3), IM_COL32(200, 200, 200, 255), Label);
    }

    // Playhead (현재 시간 표시)
    float CurrentTime = State->CurrentAnimationTime;
    if (CurrentTime >= StartTime && CurrentTime <= EndTime)
    {
        float NormalizedX = (CurrentTime - StartTime) / Duration;
        float PlayheadX = RulerMin.x + NormalizedX * PanelWidth;
        DrawList->AddLine(ImVec2(PlayheadX, RulerMin.y), ImVec2(PlayheadX, PanelMin.y + PanelSize.y), IM_COL32(255, 100, 100, 255), 2.0f);
    }

    // 각 Track 렌더링
    float CurrentY = RulerMax.y;
    for (int32 TrackIndex = 0; TrackIndex < State->NotifyTrackNames.Num(); ++TrackIndex)
    {
        ImVec2 TrackMin = ImVec2(PanelMin.x, CurrentY);
        ImVec2 TrackMax = ImVec2(PanelMin.x + PanelWidth, CurrentY + TrackHeight);

        // Track 배경
        bool bIsSelected = (State->SelectedNotifyTrackIndex == TrackIndex);
        ImU32 TrackBgColor = bIsSelected ? IM_COL32(60, 60, 70, 255) : IM_COL32(45, 45, 50, 255);
        DrawList->AddRectFilled(TrackMin, TrackMax, TrackBgColor);
        DrawList->AddRect(TrackMin, TrackMax, IM_COL32(80, 80, 90, 255), 0.0f, 0, 1.0f);

        // Track 이름
        const char* TrackName = State->NotifyTrackNames[TrackIndex].c_str();
        DrawList->AddText(ImVec2(TrackMin.x + 5, TrackMin.y + 3), IM_COL32(180, 180, 180, 255), TrackName);

        // Track 메뉴 버튼 (오른쪽)
        ImGui::SetCursorScreenPos(ImVec2(TrackMax.x - 50, TrackMin.y + 2));
        char MenuButtonID[64];
        snprintf(MenuButtonID, sizeof(MenuButtonID), "...##TrackMenu%d", TrackIndex);
        if (ImGui::SmallButton(MenuButtonID))
        {
            char PopupID[64];
            snprintf(PopupID, sizeof(PopupID), "TrackMenu%d", TrackIndex);
            ImGui::OpenPopup(PopupID);
        }

        char PopupID[64];
        snprintf(PopupID, sizeof(PopupID), "TrackMenu%d", TrackIndex);
        if (ImGui::BeginPopup(PopupID))
        {
            if (ImGui::BeginMenu("Add Notify..."))
            {
                // 메뉴 열릴 때마다 Notify 라이브러리 자동 스캔 (파일 추가/삭제 반영)
                ScanNotifyLibrary();

                for (const FString& NotifyClass : AvailableNotifyClasses)
                {
                    if (ImGui::MenuItem(NotifyClass.c_str()))
                    {
                        FAnimNotifyEvent NewNotify;
                        NewNotify.NotifyName = FName(NotifyClass);
                        NewNotify.TriggerTime = State->CurrentAnimationTime;
                        NewNotify.Duration = 0.0f;
                        NewNotify.TrackIndex = TrackIndex;
                        State->CurrentAnimation->AddNotify(NewNotify);
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("New Notify Script..."))
                {
                    bShowNewNotifyDialog = true;
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add Notify State..."))
            {
                // 메뉴 열릴 때마다 Notify 라이브러리 자동 스캔 (파일 추가/삭제 반영)
                ScanNotifyLibrary();

                for (const FString& StateClass : AvailableNotifyStateClasses)
                {
                    if (ImGui::MenuItem(StateClass.c_str()))
                    {
                        FAnimNotifyEvent NewNotify;
                        NewNotify.NotifyName = FName(StateClass);
                        NewNotify.TriggerTime = State->CurrentAnimationTime;
                        NewNotify.Duration = 1.0f;
                        NewNotify.TrackIndex = TrackIndex;
                        State->CurrentAnimation->AddNotify(NewNotify);
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("New Notify State Script..."))
                {
                    bShowNewNotifyStateDialog = true;
                    memset(NewNotifyNameBuffer, 0, sizeof(NewNotifyNameBuffer));
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Manage Notifies..."))
            {
                ScanNotifyLibrary();
            }

            ImGui::Separator();

            // 마지막 남은 Track은 삭제 불가
            // Notify가 있는 Track도 삭제 불가
            bool bHasNotifyInTrack = false;
            if (State->CurrentAnimation)
            {
                const TArray<FAnimNotifyEvent>& Notifies = State->CurrentAnimation->Notifies;
                for (const FAnimNotifyEvent& Notify : Notifies)
                {
                    if (Notify.TrackIndex == TrackIndex)
                    {
                        bHasNotifyInTrack = true;
                        break;
                    }
                }
            }
            bool bCanRemoveTrack = State->NotifyTrackNames.Num() > 1 && !bHasNotifyInTrack;

            if (ImGui::MenuItem("Remove Track", nullptr, false, bCanRemoveTrack))
            {
                State->NotifyTrackNames.erase(State->NotifyTrackNames.begin() + TrackIndex);
                if (State->SelectedNotifyTrackIndex == TrackIndex)
                {
                    State->SelectedNotifyTrackIndex = -1;
                }
            }
            ImGui::EndPopup();
        }

        // Track 클릭 감지
        ImVec2 MousePos = ImGui::GetMousePos();
        if (ImGui::IsMouseClicked(0))
        {
            if (MousePos.x >= TrackMin.x && MousePos.x <= TrackMax.x &&
                MousePos.y >= TrackMin.y && MousePos.y <= TrackMax.y)
            {
                State->SelectedNotifyTrackIndex = TrackIndex;
            }
        }

        for (int32 i = 0; i < Notifies.Num(); ++i)
        {
            const FAnimNotifyEvent* NotifyEvent = Notifies[i];
            if (!NotifyEvent)
                continue;

            if (NotifyEvent->TrackIndex != TrackIndex)
                continue;

            float NotifyTime = NotifyEvent->TriggerTime;
            if (NotifyTime < StartTime || NotifyTime > EndTime)
                continue;

            float NormalizedX = (NotifyTime - StartTime) / Duration;
            float NotifyX = TrackMin.x + NormalizedX * PanelWidth;

            ImVec2 MarkerMin(NotifyX - 5, TrackMin.y + 20);
            ImVec2 MarkerMax(NotifyX + 5, TrackMax.y - 5);

            bool bNotifySelected = (State->SelectedNotifyIndex == i);
            ImU32 MarkerColor = bNotifySelected ? IM_COL32(255, 200, 100, 255) : IM_COL32(100, 200, 255, 255);

            DrawList->AddRectFilled(MarkerMin, MarkerMax, MarkerColor);
            DrawList->AddRect(MarkerMin, MarkerMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.5f);

            const char* NotifyName = NotifyEvent->NotifyName.ToString().c_str();
            ImVec2 TextSize = ImGui::CalcTextSize(NotifyName);
            DrawList->AddText(ImVec2(NotifyX - TextSize.x * 0.5f, MarkerMin.y - TextSize.y - 2), IM_COL32(220, 220, 220, 255), NotifyName);

            bool bIsHovered = (MousePos.x >= MarkerMin.x && MousePos.x <= MarkerMax.x &&
                               MousePos.y >= MarkerMin.y && MousePos.y <= MarkerMax.y);

            if (bIsHovered && ImGui::IsMouseClicked(0))
            {
                State->SelectedNotifyIndex = i;
                State->bDraggingNotify = false;
                State->NotifyDragOffsetX = MousePos.x - NotifyX;
            }

            if (State->SelectedNotifyIndex == i && ImGui::IsMouseDragging(0, 2.0f))
            {
                if (!State->bDraggingNotify)
                {
                    State->bDraggingNotify = true;
                }

                float NewNotifyX = MousePos.x - State->NotifyDragOffsetX;
                float NewNormalizedX = (NewNotifyX - TrackMin.x) / PanelWidth;
                float NewTime = StartTime + NewNormalizedX * Duration;
                NewTime = FMath::Clamp(NewTime, 0.0f, AnimLength);

                State->CurrentAnimation->Notifies[i].TriggerTime = NewTime;
                State->CurrentAnimationTime = NewTime;
                RefreshAnimationFrame(State);
            }

            if (ImGui::IsMouseReleased(0) && State->SelectedNotifyIndex == i)
            {
                State->bDraggingNotify = false;
            }
        }

        CurrentY += TrackHeight;
    }

    // Spacer
    ImGui::Dummy(ImVec2(0, RulerHeight + (State->NotifyTrackNames.Num() * TrackHeight)));
}

void SAnimationWindow::DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FAnimationTabState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    float Duration = EndTime - StartTime;
    if (Duration <= 0.0f)
    {
        return;
    }

    float TimelineWidth = TimelineMax.x - TimelineMin.x;
    float MarkerHeight = TimelineMax.y - TimelineMin.y;

    // Notify 이벤트 가져오기
    UAnimSequenceBase* AnimBase = State->CurrentAnimation;
    TArray<const FAnimNotifyEvent*> Notifies;
    float MaxTime = DataModel->GetPlayLength();
    AnimBase->GetAnimNotifiesFromDeltaPositions(0.0f, MaxTime, Notifies);

    for (int32 i = 0; i < Notifies.Num(); ++i)
    {
        const FAnimNotifyEvent* Notify = Notifies[i];
        if (!Notify)
            continue;

        float TriggerTime = Notify->TriggerTime;
        if (TriggerTime < StartTime || TriggerTime > EndTime)
        {
            continue; // 화면 밖
        }

        float NormalizedX = (TriggerTime - StartTime) / Duration;
        float ScreenX = TimelineMin.x + NormalizedX * TimelineWidth;
        float ScreenY = TimelineMin.y + MarkerHeight * 0.5f;

        // 선택 여부 확인
        bool bIsSelected = (State->SelectedNotifyIndex == i);
        ImU32 MarkerColor = bIsSelected ? IM_COL32(255, 200, 100, 255) : IM_COL32(100, 200, 255, 255);

        // Notify 마커 (다이아몬드)
        float MarkerSize = 5.0f;
        DrawList->AddQuadFilled(
            ImVec2(ScreenX, ScreenY - MarkerSize),
            ImVec2(ScreenX + MarkerSize, ScreenY),
            ImVec2(ScreenX, ScreenY + MarkerSize),
            ImVec2(ScreenX - MarkerSize, ScreenY),
            MarkerColor
        );

        // Notify 이름 표시
        const char* NotifyName = Notify->NotifyName.ToString().c_str();
        ImVec2 TextSize = ImGui::CalcTextSize(NotifyName);
        DrawList->AddText(
            ImVec2(ScreenX - TextSize.x * 0.5f, ScreenY - MarkerSize - TextSize.y - 2),
            IM_COL32(220, 220, 220, 255),
            NotifyName
        );

        // 클릭 감지 (마우스가 마커 근처에 있는지)
        ImVec2 MousePos = ImGui::GetMousePos();
        if (ImGui::IsMouseClicked(0))
        {
            float DistToMarker = FMath::Sqrt(
                (MousePos.x - ScreenX) * (MousePos.x - ScreenX) +
                (MousePos.y - ScreenY) * (MousePos.y - ScreenY)
            );
            if (DistToMarker < MarkerSize * 2.0f)
            {
                State->SelectedNotifyIndex = i;
            }
        }
    }
}

// ============================================================================
// RenderTimelinePanel - Timeline 패널 전체 렌더링
// ============================================================================

void SAnimationWindow::RenderTimelinePanel()
{
    FAnimationTabState* State = ActiveState;
    if (!State)
    {
        return;
    }

    FRect PanelRect = TimelineRect;
    if (PanelRect.GetWidth() <= 0 || PanelRect.GetHeight() <= 0)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(PanelRect.Left, PanelRect.Top));
    ImGui::SetNextWindowSize(ImVec2(PanelRect.GetWidth(), PanelRect.GetHeight()));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Timeline##AnimTimeline", nullptr, flags))
    {
        RenderTimelineControls(State);
    }
    ImGui::End();
}
