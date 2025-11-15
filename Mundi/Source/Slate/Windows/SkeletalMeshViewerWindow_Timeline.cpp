#include "pch.h"

#include "BoneAnchorComponent.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshViewerWindow.h"
#include "ImGui/imgui.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"

// Timeline 컨트롤 UI 렌더링
void SSkeletalMeshViewerWindow::RenderTimelineControls(ViewerState* State)
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

    // Play/Pause (AnimInstance 컨트롤)
    if (State->bIsPlaying)
    {
        if (IconPause && IconPause->GetShaderResourceView())
        {
            if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
            {
                UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
                if (AnimInst)
                {
                    AnimInst->StopAnimation();
                    State->bIsPlaying = false;
                }
            }
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
    float PrevTime = State->CurrentAnimationTime;
    if (ImGui::SliderFloat("##Timeline", &State->CurrentAnimationTime, 0.0f, MaxTime, "%.2fs"))
    {
        // 슬라이더가 변경되면 AnimInstance에 시간 설정
        if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
        {
            UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
            if (AnimInst)
            {
                AnimInst->SetPosition(State->CurrentAnimationTime);
            }
        }
    }

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

    // 새로운 타임라인 렌더링
    RenderTimeline(State);
}

// Timeline 헬퍼: 프레임 변경 시 공통 갱신 로직
void SSkeletalMeshViewerWindow::RefreshAnimationFrame(ViewerState* State)
{
    if (!State)
    {
        return;
    }

    // 편집된 bone transform 캐시 초기화 (새 프레임의 애니메이션 포즈 적용)
    State->EditedBoneTransforms.clear();

    // AnimInstance를 통해 포즈 업데이트 (EvaluateAnimation이 자동 호출됨)
    if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
    {
        UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
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

        // BoneAnchor 위치 동기화 (본이 선택되어 있으면)
        if (State->SelectedBoneIndex >= 0)
        {
            State->PreviewActor->RepositionAnchorToBone(State->SelectedBoneIndex);
        }
    }
}

void SSkeletalMeshViewerWindow::TimelineToFront(ViewerState* State)
{
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    State->CurrentAnimationTime = 0.0f;

    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineToPrevious(ViewerState* State)
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

    // 프레임당 시간 (30fps 기준)
    float FrameTime = 1.0f / 30.0f;
    State->CurrentAnimationTime = FMath::Max(0.0f, State->CurrentAnimationTime - FrameTime);

    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineReverse(ViewerState* State)
{
    if (!State)
    {
        return;
    }

    // 역재생 (음수 속도) - AnimInstance를 통해 설정
    State->PlaybackSpeed = -std::abs(State->PlaybackSpeed);

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
    State->PlaybackSpeed = -FMath::Abs(State->PlaybackSpeed);
    State->bIsPlaying = true;
}

void SSkeletalMeshViewerWindow::TimelineRecord(ViewerState* State)
{
    // TODO: 녹화 기능 구현
}

void SSkeletalMeshViewerWindow::TimelinePlay(ViewerState* State)
{
    if (!State)
    {
        return;
    }

    // 정방향 재생 - AnimInstance를 통해 설정
    State->PlaybackSpeed = std::abs(State->PlaybackSpeed);

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
    State->PlaybackSpeed = FMath::Abs(State->PlaybackSpeed);
    State->bIsPlaying = true;
}

void SSkeletalMeshViewerWindow::TimelineToNext(ViewerState* State)
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

    // 프레임당 시간 (30fps 기준)
    float FrameTime = 1.0f / 30.0f;
    float MaxTime = DataModel->GetPlayLength();
    State->CurrentAnimationTime = FMath::Min(MaxTime, State->CurrentAnimationTime + FrameTime);

    RefreshAnimationFrame(State);
}

void SSkeletalMeshViewerWindow::TimelineToEnd(ViewerState* State)
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

void SSkeletalMeshViewerWindow::RenderTimeline(ViewerState* State)
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

    // 타임라인 영역 크기 설정
    float TimelineHeight = 60.0f;
    ImVec2 TimelineSize = ImVec2(ImGui::GetContentRegionAvail().x, TimelineHeight);

    ImVec2 CursorPos = ImGui::GetCursorScreenPos();
    ImVec2 TimelineMin = CursorPos;
    ImVec2 TimelineMax = ImVec2(CursorPos.x + TimelineSize.x, CursorPos.y + TimelineSize.y);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // 타임라인 배경
    DrawList->AddRectFilled(TimelineMin, TimelineMax, IM_COL32(40, 40, 40, 255));

    // 가시 범위 계산 (현재 시간 중심으로 표시)
    float VisibleDuration = MaxTime / State->TimelineZoom;
    float HalfDuration = VisibleDuration * 0.5f;

    // Playhead를 화면 중앙에 고정, 타임라인이 스크롤됨
    float StartTime = State->CurrentAnimationTime - HalfDuration;
    float EndTime = State->CurrentAnimationTime + HalfDuration;

    // 범위 제한 (애니메이션 시작/끝을 벗어나지 않도록)
    if (StartTime < 0.0f)
    {
        EndTime -= StartTime;
        StartTime = 0.0f;
    }
    if (EndTime > MaxTime)
    {
        StartTime -= (EndTime - MaxTime);
        EndTime = MaxTime;
        if (StartTime < 0.0f)
        {
            StartTime = 0.0f;
        }
    }

    // 눈금자 영역 (상단 30픽셀)
    ImVec2 RulerMin = TimelineMin;
    ImVec2 RulerMax = ImVec2(TimelineMax.x, TimelineMin.y + 30.0f);

    // 타임라인 트랙 영역 (하단 30픽셀)
    ImVec2 TrackMin = ImVec2(TimelineMin.x, RulerMax.y);
    ImVec2 TrackMax = TimelineMax;

    // 눈금자 렌더링
    DrawTimelineRuler(DrawList, RulerMin, RulerMax, StartTime, EndTime, State);

    // 재생 범위 렌더링 (녹색 반투명 영역)
    DrawPlaybackRange(DrawList, TimelineMin, TimelineMax, StartTime, EndTime, State);

    // 키프레임 마커 렌더링
    DrawKeyframeMarkers(DrawList, TrackMin, TrackMax, StartTime, EndTime, State);

    // Playhead 렌더링 (빨간 세로 바)
    DrawTimelinePlayhead(DrawList, TimelineMin, TimelineMax, State->CurrentAnimationTime, StartTime, EndTime);

    // 마우스 입력 처리 (InvisibleButton으로 영역 확보)
    // SetCursorScreenPos 대신 상대 좌표 사용하여 ImGui assertion 회피
    ImGui::InvisibleButton("##Timeline", TimelineSize);

    // 재생 범위 핸들 드래그 감지 (Shift + 드래그)
    static bool bDraggingRangeStart = false;
    static bool bDraggingRangeEnd = false;
    bool bShiftHeld = ImGui::GetIO().KeyShift;

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        ImVec2 MousePos = ImGui::GetMousePos();
        float NormalizedX = (MousePos.x - TimelineMin.x) / TimelineSize.x;
        float MouseTime = FMath::Lerp(StartTime, EndTime, NormalizedX);

        if (bShiftHeld && !bDraggingRangeStart && !bDraggingRangeEnd)
        {
            // Shift 누른 상태에서 드래그 시작: 가까운 핸들 선택
            float RangeEnd = (State->PlaybackRangeEnd < 0.0f) ? MaxTime : State->PlaybackRangeEnd;
            float DistToStart = FMath::Abs(MouseTime - State->PlaybackRangeStart);
            float DistToEnd = FMath::Abs(MouseTime - RangeEnd);

            if (DistToStart < DistToEnd && DistToStart < 0.5f)
            {
                bDraggingRangeStart = true;
            }
            else if (DistToEnd < 0.5f)
            {
                bDraggingRangeEnd = true;
            }
        }

        if (bDraggingRangeStart)
        {
            // 범위 시작 드래그
            float RangeEnd = (State->PlaybackRangeEnd < 0.0f) ? MaxTime : State->PlaybackRangeEnd;
            State->PlaybackRangeStart = FMath::Clamp(MouseTime, 0.0f, RangeEnd - 0.01f);
        }
        else if (bDraggingRangeEnd)
        {
            // 범위 끝 드래그
            State->PlaybackRangeEnd = FMath::Clamp(MouseTime, State->PlaybackRangeStart + 0.01f, MaxTime);
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

    if (ImGui::IsItemHovered())
    {
        // 마우스 휠로 줌
        float Wheel = ImGui::GetIO().MouseWheel;
        if (Wheel != 0.0f)
        {
            State->TimelineZoom = FMath::Clamp(State->TimelineZoom + Wheel * 0.1f, 0.5f, 10.0f);
        }

        // Shift + 우클릭: 재생 범위 초기화
        if (bShiftHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            State->PlaybackRangeStart = 0.0f;
            State->PlaybackRangeEnd = -1.0f; // 전체 범위
        }
    }

    // InvisibleButton이 이미 TimelineHeight만큼 영역을 차지했으므로 추가 간격만 더함
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
}

void SSkeletalMeshViewerWindow::DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, ViewerState* State)
{
    // 눈금자 배경
    DrawList->AddRectFilled(RulerMin, RulerMax, IM_COL32(50, 50, 50, 255));

    float RulerWidth = RulerMax.x - RulerMin.x;
    float Duration = EndTime - StartTime;

    if (Duration <= 0.0f)
    {
        return;
    }

    // 눈금 간격 결정 (픽셀 기준 100px 간격)
    float PixelsPerSecond = RulerWidth / Duration;
    float DesiredTickSpacing = 100.0f; // 픽셀
    float TimePerTick = DesiredTickSpacing / PixelsPerSecond;

    // 적절한 눈금 간격으로 반올림 (0.1s, 0.5s, 1s, 5s, 10s 등)
    float TickIntervals[] = {0.1f, 0.5f, 1.0f, 5.0f, 10.0f, 30.0f, 60.0f};
    float TickInterval = 1.0f;
    for (float Interval : TickIntervals)
    {
        if (TimePerTick <= Interval)
        {
            TickInterval = Interval;
            break;
        }
    }

    // 눈금 그리기
    float FirstTick = std::ceil(StartTime / TickInterval) * TickInterval;
    for (float Time = FirstTick; Time <= EndTime; Time += TickInterval)
    {
        float NormalizedX = (Time - StartTime) / Duration;
        float ScreenX = RulerMin.x + NormalizedX * RulerWidth;

        // 눈금선
        DrawList->AddLine(
            ImVec2(ScreenX, RulerMax.y - 10.0f),
            ImVec2(ScreenX, RulerMax.y),
            IM_COL32(180, 180, 180, 255),
            1.0f
        );

        // 시간 텍스트
        char TimeLabel[32];
        if (State->bShowFrameNumbers)
        {
            int32 Frame = static_cast<int32>(Time * 30.0f); // 30fps 가정
            snprintf(TimeLabel, sizeof(TimeLabel), "%d", Frame);
        }
        else
        {
            snprintf(TimeLabel, sizeof(TimeLabel), "%.2fs", Time);
        }

        ImVec2 TextSize = ImGui::CalcTextSize(TimeLabel);
        DrawList->AddText(
            ImVec2(ScreenX - TextSize.x * 0.5f, RulerMin.y + 5.0f),
            IM_COL32(220, 220, 220, 255),
            TimeLabel
        );
    }
}

void SSkeletalMeshViewerWindow::DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime)
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

void SSkeletalMeshViewerWindow::DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, ViewerState* State)
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
    float Duration = EndTime - StartTime;
    if (Duration <= 0.0f)
    {
        return;
    }

    // 재생 범위가 설정되지 않았으면 전체 범위 사용
    float RangeStart = State->PlaybackRangeStart;
    float RangeEnd = (State->PlaybackRangeEnd < 0.0f) ? MaxTime : State->PlaybackRangeEnd;

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

    // 녹색 반투명 영역
    DrawList->AddRectFilled(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenEndX, TimelineMax.y),
        IM_COL32(50, 200, 50, 80)
    );

    // 녹색 경계선 (In/Out 마커)
    DrawList->AddLine(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenStartX, TimelineMax.y),
        IM_COL32(50, 255, 50, 255),
        2.0f
    );

    DrawList->AddLine(
        ImVec2(ScreenEndX, TimelineMin.y),
        ImVec2(ScreenEndX, TimelineMax.y),
        IM_COL32(50, 255, 50, 255),
        2.0f
    );

    // In/Out 핸들 (상단 삼각형)
    float HandleSize = 5.0f;

    // In 핸들 (왼쪽)
    DrawList->AddTriangleFilled(
        ImVec2(ScreenStartX, TimelineMin.y),
        ImVec2(ScreenStartX - HandleSize, TimelineMin.y + HandleSize * 1.5f),
        ImVec2(ScreenStartX + HandleSize, TimelineMin.y + HandleSize * 1.5f),
        IM_COL32(50, 255, 50, 255)
    );

    // Out 핸들 (오른쪽)
    DrawList->AddTriangleFilled(
        ImVec2(ScreenEndX, TimelineMin.y),
        ImVec2(ScreenEndX - HandleSize, TimelineMin.y + HandleSize * 1.5f),
        ImVec2(ScreenEndX + HandleSize, TimelineMin.y + HandleSize * 1.5f),
        IM_COL32(50, 255, 50, 255)
    );
}

void SSkeletalMeshViewerWindow::DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, ViewerState* State)
{
    // TODO: AnimNotify 시스템 구현 후 활성화
    // 현재는 키프레임 마커를 그리지 않음

    /*
    if (!State || !State->CurrentAnimation)
    {
        return;
    }

    UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
    if (!DataModel)
    {
        return;
    }

    const TArray<FAnimNotifyEvent>& Notifies = DataModel->GetAnimNotifyEvents();
    float Duration = EndTime - StartTime;
    if (Duration <= 0.0f)
    {
        return;
    }

    float TimelineWidth = TimelineMax.x - TimelineMin.x;
    float MarkerHeight = TimelineMax.y - TimelineMin.y;

    for (const FAnimNotifyEvent& Notify : Notifies)
    {
        float TriggerTime = Notify.TriggerTime;
        if (TriggerTime < StartTime || TriggerTime > EndTime)
        {
            continue; // 화면 밖
        }

        float NormalizedX = (TriggerTime - StartTime) / Duration;
        float ScreenX = TimelineMin.x + NormalizedX * TimelineWidth;

        // 키프레임 마커 (다이아몬드)
        float MarkerSize = 4.0f;
        DrawList->AddQuadFilled(
            ImVec2(ScreenX, TimelineMin.y + MarkerHeight * 0.5f - MarkerSize),
            ImVec2(ScreenX + MarkerSize, TimelineMin.y + MarkerHeight * 0.5f),
            ImVec2(ScreenX, TimelineMin.y + MarkerHeight * 0.5f + MarkerSize),
            ImVec2(ScreenX - MarkerSize, TimelineMin.y + MarkerHeight * 0.5f),
            IM_COL32(100, 200, 255, 255)
        );
    }
    */
}
