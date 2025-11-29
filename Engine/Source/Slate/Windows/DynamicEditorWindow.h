#pragma once
#include "Window.h"

class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class UAnimSequence;
class UBlendSpace2D;
class UAnimStateMachine;
struct ID3D11Device;
class AGizmoActor;
class UTexture;
struct ImDrawList;
class SAnimationWindow;

class SDynamicEditorWindow;

/**
 * @brief 에디터 모드 열거형
 * @details 동적 에디터 윈도우에서 지원하는 편집 모드
 */
enum class EEditorMode : uint8
{
	Skeletal,      // 스켈레탈 메시 편집 (본 구조, 메시)
	Animation,     // 애니메이션 편집 (타임라인, 키프레임)
	AnimGraph,     // 애니메이션 그래프 (스테이트 머신)
	BlendSpace2D   // 블렌드 스페이스 2D 편집
};

/**
 * @brief 에디터 탭 상태
 * @details 각 탭의 고유 상태를 관리 (모드, 에셋, 뷰포트 등)
 */
struct FEditorTabState
{
	// 기본 정보
	FName Name;
	int32 TabId = 0;
	EEditorMode Mode = EEditorMode::Skeletal;

	// 월드 및 뷰포트
	UWorld* World = nullptr;
	FViewport* Viewport = nullptr;
	FViewportClient* Client = nullptr;

	// 공통 에셋 참조
	class ASkeletalMeshActor* PreviewActor = nullptr;
	USkeletalMesh* CurrentMesh = nullptr;
	FString LoadedMeshPath;

	// Skeletal 모드 상태
	int32 SelectedBoneIndex = -1;
	bool bShowMesh = true;
	bool bShowBones = true;
	bool bBoneLinesDirty = true;
	int32 LastSelectedBoneIndex = -1;
	char MeshPathBuffer[260] = {0};
	std::set<int32> ExpandedBoneIndices;

	// 본 트랜스폼 편집
	FVector EditBoneLocation;
	FVector EditBoneRotation;
	FVector EditBoneScale;
	bool bBoneTransformChanged = false;
	bool bBoneRotationEditing = false;
	bool bWasGizmoDragging = false;
	int32 DragStartBoneIndex = -1;

	// Animation 모드 상태
	int32 SelectedAnimationIndex = -1;
	UAnimSequence* CurrentAnimation = nullptr;
	bool bIsPlaying = false;
	float CurrentAnimationTime = 0.0f;
	bool bLoopAnimation = true;
	float PlaybackSpeed = 1.0f;

	// 타임라인 뷰
	float TimelineZoom = 1.0f;
	float TimelineScroll = 0.0f;
	bool bShowFrameNumbers = false;
	int32 PlaybackRangeStartFrame = 0;
	int32 PlaybackRangeEndFrame = -1;
	int32 WorkingRangeStartFrame = 0;
	int32 WorkingRangeEndFrame = -1;
	int32 ViewRangeStartFrame = 0;
	int32 ViewRangeEndFrame = -1;

	// Notify Track
	bool bNotifiesExpanded = false;
	int32 SelectedNotifyTrackIndex = -1;
	int32 SelectedNotifyIndex = -1;
	TArray<FString> NotifyTrackNames;
	std::set<int32> UsedTrackNumbers;
	bool bDraggingNotify = false;
	float NotifyDragOffsetX = 0.0f;

	// Notify 클립보드
	bool bHasNotifyClipboard = false;
	struct FAnimNotifyEvent NotifyClipboard;

	// Animation Recording
	bool bIsRecording = false;
	bool bShowRecordDialog = false;
	char RecordFileNameBuffer[128] = "";
	FString RecordedFileName = "";
	TArray<TMap<int32, FTransform>> RecordedFrames;
	float RecordStartTime = 0.0f;
	float RecordFrameRate = 30.0f;

	// Detail Panel
	int32 DetailPanelTabIndex = 0;
	uint32 BoneTransformModeFlags = 0x7;
	FVector ReferenceBoneLocation;
	FVector ReferenceBoneRotation;
	FVector ReferenceBoneScale;
	FVector MeshRelativeBoneLocation;
	FVector MeshRelativeBoneRotation;
	FVector MeshRelativeBoneScale;

	// AnimGraph 모드 상태
	UAnimStateMachine* StateMachine = nullptr;
	FWideString StateMachineFilePath;

	// BlendSpace2D 모드 상태
	UBlendSpace2D* BlendSpace = nullptr;
	FWideString BlendSpaceFilePath;
	FVector2D PreviewBlendPosition;
	int32 SelectedSampleIndex = -1;
	bool bIsDraggingSample = false;
};

/**
 * @brief 동적 에디터 윈도우
 * @details ImGui BeginChild 기반 레이아웃으로 여러 에디터 모드를 지원하는 통합 윈도우
 *          - Skeletal: 스켈레탈 메시 편집
 *          - Animation: 애니메이션 편집
 *          - AnimGraph: 스테이트 머신 편집
 *          - BlendSpace2D: 블렌드 스페이스 편집
 *
 * SPreviewWindow의 레이아웃 구조를 그대로 사용:
 * - Skeletal 모드: [Left: Asset+BoneTree] | [Center: Viewport] | [Right: BoneProperties]
 * - Animation 모드: [Center: Viewport] | [Right: Details] + [Bottom: Timeline | AnimList]
 */
class SDynamicEditorWindow : public SWindow
{
public:
	SDynamicEditorWindow();
	virtual ~SDynamicEditorWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	// SWindow overrides
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// Viewport 렌더링
	void OnRenderViewport();

	// 에셋 로드
	void LoadSkeletalMesh(const FString& Path);
	void LoadAnimation(const FString& Path);
	void LoadAnimGraph(const FString& Path);
	void LoadBlendSpace(const FString& Path);

	// 직접 에셋 설정 (이미 로드된 객체 전달용)
	void SetBlendSpace(UBlendSpace2D* InBlendSpace);
	void SetAnimStateMachine(UAnimStateMachine* InStateMachine);

	// 모드 전환
	void SetEditorMode(EEditorMode NewMode);
	EEditorMode GetEditorMode() const;

	// Accessors
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	AGizmoActor* GetGizmoActor() const;
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }
	bool IsFocused() const { return bIsFocused; }
	bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

	// 탭/상태 접근
	FEditorTabState* GetActiveState() const { return ActiveState; }
	UWorld* GetWorld() const { return World; }
	ID3D11Device* GetDevice() const { return Device; }

	// 렌더 타겟 관리
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

	// 뷰포트 영역 캐시
	FRect GetViewportRect() const { return CenterRect; }

private:
	// 탭 관리
	FEditorTabState* CreateNewTab(const char* Name, EEditorMode Mode);
	void DestroyTab(FEditorTabState* State);
	void CloseTab(int32 Index);

	// Bone Transform 헬퍼
	void UpdateBoneTransformFromSkeleton(FEditorTabState* State);
	void ApplyBoneTransform(FEditorTabState* State);
	void ExpandToSelectedBone(FEditorTabState* State, int32 BoneIndex);

	// Timeline 컨트롤 UI 렌더링 (PreviewWindow에서 복사)
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

	// Timeline 헬퍼: 프레임 변경 시 공통 갱신 로직
	void RefreshAnimationFrame(FEditorTabState* State);

	// Timeline 렌더링 헬퍼
	void DrawTimelineRuler(ImDrawList* DrawList, const struct ImVec2& RulerMin, const struct ImVec2& RulerMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawPlaybackRange(ImDrawList* DrawList, const struct ImVec2& TimelineMin, const struct ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawTimelinePlayhead(ImDrawList* DrawList, const struct ImVec2& TimelineMin, const struct ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime);
	void DrawKeyframeMarkers(ImDrawList* DrawList, const struct ImVec2& TimelineMin, const struct ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State);
	void DrawNotifyTracksPanel(FEditorTabState* State, float StartTime, float EndTime);

	// Notify Track 복원 (애니메이션 로드 시)
	void RebuildNotifyTracks(FEditorTabState* State);

	// Notify 라이브러리 관리
	void ScanNotifyLibrary();
	void CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState);
	void OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState);

	// 렌더 타겟 관리
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();

private:
	// 탭 상태
	FEditorTabState* ActiveState = nullptr;
	TArray<FEditorTabState*> Tabs;
	int32 ActiveTabIndex = -1;
	int32 NextTabId = 1;

	// 월드/디바이스 참조
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 레이아웃 비율 (SPreviewWindow와 동일)
	float LeftPanelRatio = 0.25f;   // 25% of width
	float RightPanelRatio = 0.25f;  // 25% of width

	// Cached center region used for viewport sizing and input mapping
	FRect CenterRect;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;
	bool bRequestTabSwitch = false;
	int32 RequestedTabIndex = -1;

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

	// Notify 라이브러리
	TArray<FString> AvailableNotifyClasses;
	TArray<FString> AvailableNotifyStateClasses;

	// New Notify Script 다이얼로그 상태
	bool bShowNewNotifyDialog = false;
	bool bShowNewNotifyStateDialog = false;
	char NewNotifyNameBuffer[128] = "";

	// 전용 렌더 타겟
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;

	// Animation 모드용 내장 에디터 (SSplitter 기반 4패널 레이아웃)
	SAnimationWindow* EmbeddedAnimationEditor = nullptr;
};
