#include "pch.h"

#include "BoneAnchorComponent.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshViewerWindow.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"

// Timeline 컨트롤 UI 렌더링
void SSkeletalMeshViewerWindow::RenderTimelineControls(ViewerState* State)
{
    if (!State || !State->CurrentAnimation)
        return;

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
        return;

    float MaxTime = DataModel->GetPlayLength();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

    // === 타임라인 컨트롤 버튼들 ===
    float ButtonSize = 20.0f;
    ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);

    // ToFront |<<
    if (IconGoToFront && IconGoToFront->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##ToFront", IconGoToFront->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineToFront(State);
        }
    }
    else
    {
        if (ImGui::Button("|<<", ButtonSizeVec))
        {
            TimelineToFront(State);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("To Front");
    }

    ImGui::SameLine();

    // ToPrevious |<
    if (IconStepBackwards && IconStepBackwards->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##StepBackwards", IconStepBackwards->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineToPrevious(State);
        }
    }
    else
    {
        if (ImGui::Button("|<", ButtonSizeVec))
        {
            TimelineToPrevious(State);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Previous Frame");
    }

    ImGui::SameLine();

    // Reverse <<
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
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Reverse");
    }

    ImGui::SameLine();

    // Record
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
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Record");
    }

    ImGui::SameLine();

    // Play/Pause
    if (State->bIsPlaying)
    {
        if (IconPause && IconPause->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##Pause", IconPause->GetShaderResourceView(), ButtonSizeVec))
            {
                State->bIsPlaying = false;
            }
        }
        else
        {
            if (ImGui::Button("||", ButtonSizeVec))
            {
                State->bIsPlaying = false;
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Pause");
        }
    }
    else
    {
        if (IconPlay && IconPlay->GetShaderResourceView())
        {
            if (ImGui::ImageButton("##Play", IconPlay->GetShaderResourceView(), ButtonSizeVec))
            {
                TimelinePlay(State);
            }
        }
        else
        {
            if (ImGui::Button(">", ButtonSizeVec))
            {
                TimelinePlay(State);
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Play");
        }
    }

    ImGui::SameLine();

    // ToNext >|
    if (IconStepForward && IconStepForward->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##StepForward", IconStepForward->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineToNext(State);
        }
    }
    else
    {
        if (ImGui::Button(">|", ButtonSizeVec))
        {
            TimelineToNext(State);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Next Frame");
    }

    ImGui::SameLine();

    // ToEnd >>|
    if (IconGoToEnd && IconGoToEnd->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##ToEnd", IconGoToEnd->GetShaderResourceView(), ButtonSizeVec))
        {
            TimelineToEnd(State);
        }
    }
    else
    {
        if (ImGui::Button(">>|", ButtonSizeVec))
        {
            TimelineToEnd(State);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("To End");
    }

    ImGui::SameLine();

    // Loop 토글
    bool bWasLooping = State->bLoopAnimation;
    UTexture* LoopIcon = bWasLooping ? IconLoop : IconLoopOff;
    if (LoopIcon && LoopIcon->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##Loop", LoopIcon->GetShaderResourceView(), ButtonSizeVec))
        {
            State->bLoopAnimation = !State->bLoopAnimation;
        }
    }
    else
    {
        if (bWasLooping)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
        }
        if (ImGui::Button("Loop", ButtonSizeVec))
        {
            State->bLoopAnimation = !State->bLoopAnimation;
        }
        if (bWasLooping)
        {
            ImGui::PopStyleColor();
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Loop");
    }

    ImGui::SameLine();

    // Timeline 슬라이더
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("##Timeline", &State->CurrentAnimationTime, 0.0f, MaxTime, "%.2fs");

    ImGui::SameLine();

    // 시간 표시
    ImGui::Text("%.2f / %.2f s", State->CurrentAnimationTime, MaxTime);

    ImGui::SameLine();

    // 재생 속도
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##Speed", &State->PlaybackSpeed, 0.1f, 3.0f, "%.1fx");

    ImGui::PopStyleVar(2);
}

// Timeline 헬퍼: 프레임 변경 시 공통 갱신 로직
void SSkeletalMeshViewerWindow::RefreshAnimationFrame(ViewerState* State)
{
    if (!State) return;

    // 편집된 bone transform 캐시 초기화 (새 프레임의 애니메이션 포즈 적용)
    State->EditedBoneTransforms.clear();

    UpdateBonesFromAnimation(State);

    // Bone Line 강제 갱신 (모든 본 업데이트)
    State->bBoneLinesDirty = true;
    if (State->PreviewActor)
    {
        State->PreviewActor->RebuildBoneLines(State->SelectedBoneIndex, true);

        // BoneAnchor 위치 동기화 (본이 선택되어 있으면)
        if (State->SelectedBoneIndex >= 0)
        {
            State->PreviewActor->RepositionAnchorToBone(State->SelectedBoneIndex);
        }
    }
}

// Timeline 기능 구현
void SSkeletalMeshViewerWindow::TimelineToFront(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;
    State->CurrentAnimationTime = 0.0f;
    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineToPrevious(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel) return;

    // 프레임당 시간 (30fps 기준)
    float FrameTime = 1.0f / 30.0f;
    State->CurrentAnimationTime = FMath::Max(0.0f, State->CurrentAnimationTime - FrameTime);

    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineReverse(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    // 역재생 (음수 속도)
    State->PlaybackSpeed = -FMath::Abs(State->PlaybackSpeed);
    State->bIsPlaying = true;
}

void SSkeletalMeshViewerWindow::TimelineRecord(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    // TODO: 녹화 기능 구현 (나중에 추가)
    UE_LOG("Timeline: Record feature not implemented yet");
}

void SSkeletalMeshViewerWindow::TimelinePlay(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    // 정방향 재생
    State->PlaybackSpeed = FMath::Abs(State->PlaybackSpeed);
    State->bIsPlaying = true;
}

void SSkeletalMeshViewerWindow::TimelineToNext(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel) return;

    // 프레임당 시간 (30fps 기준)
    float FrameTime = 1.0f / 30.0f;
    float MaxTime = DataModel->GetPlayLength();
    State->CurrentAnimationTime = FMath::Min(MaxTime, State->CurrentAnimationTime + FrameTime);

    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineToEnd(ViewerState* State)
{
    if (!State || !State->CurrentAnimation) return;

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel) return;

    State->CurrentAnimationTime = DataModel->GetPlayLength();

    RefreshAnimationFrame(State);
}
