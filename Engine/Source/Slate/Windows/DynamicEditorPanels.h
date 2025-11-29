#pragma once
#include "DynamicEditorWindow.h"

class UTexture;

/**
 * @brief Asset Panel - FBX 로드 및 표시 옵션
 */
class SAssetPanel : public SEditorPanel
{
public:
	SAssetPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::Skeletal || Mode == EEditorMode::Animation;
	}
};

/**
 * @brief Viewport Panel - 3D 프리뷰 렌더링
 */
class SViewportPreviewPanel : public SEditorPanel
{
public:
	SViewportPreviewPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override { return true; }

	FRect ContentRect;
};

/**
 * @brief BoneTree Panel - 본 계층 구조
 */
class SBoneTreePanel : public SEditorPanel
{
public:
	SBoneTreePanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::Skeletal || Mode == EEditorMode::Animation;
	}

private:
	void RenderBoneTree(FEditorTabState* State);
};

/**
 * @brief Detail Panel - 선택된 본 속성
 */
class SBoneDetailPanel : public SEditorPanel
{
public:
	SBoneDetailPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::Skeletal || Mode == EEditorMode::Animation;
	}

private:
	void RenderAssetDetailsTab(FEditorTabState* State);
	void RenderDetailsTab(FEditorTabState* State);
	void RenderBoneSection(FEditorTabState* State);
	void RenderTransformsSection(FEditorTabState* State);
	void RenderMaterialSlotsSection(FEditorTabState* State);
	void RenderLODPickerSection(FEditorTabState* State);
	bool RenderTransformEditor(const char* Label, FVector& Location, FVector& Rotation, FVector& Scale, bool bReadOnly = false);
	void SyncBoneTransformFromViewport(FEditorTabState* State);
	void ApplyBoneTransformToViewport(FEditorTabState* State);
};

/**
 * @brief Timeline Panel - 애니메이션 타임라인 컨트롤
 */
class STimelinePanel : public SEditorPanel
{
public:
	STimelinePanel(SDynamicEditorWindow* InOwner);
	virtual ~STimelinePanel();
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::Animation;
	}

private:
	// Timeline 컨트롤 UI 렌더링
	void RenderTimelineControls(FEditorTabState* State);
	void RenderTimeline(FEditorTabState* State);

	// Timeline 컨트롤 기능
	void TimelineToFront(FEditorTabState* State);
	void TimelineToPrevious(FEditorTabState* State);
	void TimelineReverse(FEditorTabState* State);
	void TimelineRecord(FEditorTabState* State);
	void TimelinePlay(FEditorTabState* State);
	void TimelineToNext(FEditorTabState* State);
	void TimelineToEnd(FEditorTabState* State);

	// Timeline 헬퍼
	void RefreshAnimationFrame(FEditorTabState* State);

	// Timeline 렌더링 헬퍼
	void DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime);
	void DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawNotifyTracksPanel(FEditorTabState* State, float StartTime, float EndTime);

	// Notify Track 복원
	void RebuildNotifyTracks(FEditorTabState* State);

	// Notify 라이브러리 관리
	void ScanNotifyLibrary();
	void CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState);
	void OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState);
	TArray<FString> AvailableNotifyClasses;
	TArray<FString> AvailableNotifyStateClasses;

	// New Notify Script 다이얼로그 상태
	bool bShowNewNotifyDialog = false;
	bool bShowNewNotifyStateDialog = false;
	char NewNotifyNameBuffer[128] = "";

	// Timeline 아이콘
	UTexture* IconGoToFront = nullptr;
	UTexture* IconGoToFrontOff = nullptr;
	UTexture* IconStepBackwards = nullptr;
	UTexture* IconStepBackwardsOff = nullptr;
	UTexture* IconBackwards = nullptr;
	UTexture* IconBackwardsOff = nullptr;
	UTexture* IconRecord = nullptr;
	UTexture* IconPause = nullptr;
	UTexture* IconPauseOff = nullptr;
	UTexture* IconPlay = nullptr;
	UTexture* IconPlayOff = nullptr;
	UTexture* IconStepForward = nullptr;
	UTexture* IconStepForwardOff = nullptr;
	UTexture* IconGoToEnd = nullptr;
	UTexture* IconGoToEndOff = nullptr;
	UTexture* IconLoop = nullptr;
	UTexture* IconLoopOff = nullptr;
};

/**
 * @brief Animation List Panel - 사용 가능한 애니메이션 목록
 */
class SAnimationListPanel : public SEditorPanel
{
public:
	SAnimationListPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::Animation;
	}

private:
	void RenderAnimationList(FEditorTabState* State);
};

/**
 * @brief AnimGraph Node Editor Panel - 노드 기반 스테이트 머신 에디터
 */
class SAnimGraphNodePanel : public SEditorPanel
{
public:
	SAnimGraphNodePanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::AnimGraph;
	}
};

/**
 * @brief AnimGraph State List Panel - 좌측 상태 목록 패널
 */
class SAnimGraphStateListPanel : public SEditorPanel
{
public:
	SAnimGraphStateListPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::AnimGraph;
	}
};

/**
 * @brief AnimGraph Details Panel - 우측 상세 속성 패널
 */
class SAnimGraphDetailsPanel : public SEditorPanel
{
public:
	SAnimGraphDetailsPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::AnimGraph;
	}
};

/**
 * @brief BlendSpace Grid Panel - 블렌드 스페이스 2D 그리드
 */
class SBlendSpaceGridPanel : public SEditorPanel
{
public:
	SBlendSpaceGridPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::BlendSpace2D;
	}
};

/**
 * @brief BlendSpace Sample List Panel - 블렌드 스페이스 샘플 목록
 */
class SBlendSpaceSampleListPanel : public SEditorPanel
{
public:
	SBlendSpaceSampleListPanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner) {}
	virtual void OnRender() override;
	virtual bool IsActiveForMode(EEditorMode Mode) const override
	{
		return Mode == EEditorMode::BlendSpace2D;
	}
};
