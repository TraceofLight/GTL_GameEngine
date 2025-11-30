#pragma once
#include "Window.h"

class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class UAnimSequence;
class UTexture;
class AGizmoActor;
class ASkeletalMeshActor;
class SSplitterH;
class SSplitterV;
struct ID3D11Device;
struct ImDrawList;

/**
 * @brief Animation 에디터 탭 상태
 * @details 각 탭의 고유 상태를 관리 (애니메이션, 뷰포트, 타임라인 등)
 */
struct FAnimationTabState
{
	// === 기본 정보 ===
	FName TabName;                          // 탭 표시 이름 (파일명 기반)
	int32 TabId = 0;                        // 고유 탭 ID
	FString FilePath;                       // 현재 편집 중인 파일 경로

	// === 월드 및 뷰포트 ===
	UWorld* World = nullptr;
	FViewport* Viewport = nullptr;
	FViewportClient* Client = nullptr;

	// === 에셋 참조 ===
	ASkeletalMeshActor* PreviewActor = nullptr;
	USkeletalMesh* CurrentMesh = nullptr;
	FString LoadedMeshPath;

	// === Bone 관련 ===
	int32 SelectedBoneIndex = -1;
	bool bShowMesh = true;
	bool bShowBones = true;
	bool bBoneLinesDirty = true;
	int32 LastSelectedBoneIndex = -1;
	char MeshPathBuffer[260] = {0};
	std::set<int32> ExpandedBoneIndices;

	// === Bone Transform 편집 ===
	FVector EditBoneLocation;
	FVector EditBoneRotation;
	FVector EditBoneScale;
	bool bBoneTransformChanged = false;
	bool bBoneRotationEditing = false;
	bool bWasGizmoDragging = false;
	int32 DragStartBoneIndex = -1;

	// === Animation 재생 ===
	int32 SelectedAnimationIndex = -1;
	UAnimSequence* CurrentAnimation = nullptr;
	bool bIsPlaying = false;
	float CurrentAnimationTime = 0.0f;
	bool bLoopAnimation = true;
	float PlaybackSpeed = 1.0f;

	// === Timeline 뷰 ===
	float TimelineZoom = 1.0f;
	float TimelineScroll = 0.0f;
	bool bShowFrameNumbers = false;
	int32 PlaybackRangeStartFrame = 0;
	int32 PlaybackRangeEndFrame = -1;
	int32 WorkingRangeStartFrame = 0;
	int32 WorkingRangeEndFrame = -1;
	int32 ViewRangeStartFrame = 0;
	int32 ViewRangeEndFrame = -1;

	// === Notify Track ===
	bool bNotifiesExpanded = false;
	int32 SelectedNotifyTrackIndex = -1;
	int32 SelectedNotifyIndex = -1;
	TArray<FString> NotifyTrackNames;
	std::set<int32> UsedTrackNumbers;
	bool bDraggingNotify = false;
	float NotifyDragOffsetX = 0.0f;

	// === Notify 클립보드 ===
	bool bHasNotifyClipboard = false;
	struct FAnimNotifyEvent NotifyClipboard;

	// === Animation Recording ===
	bool bIsRecording = false;
	bool bShowRecordDialog = false;
	char RecordFileNameBuffer[128] = "";
	FString RecordedFileName;
	TArray<TMap<int32, FTransform>> RecordedFrames;
	float RecordStartTime = 0.0f;
	float RecordFrameRate = 30.0f;

	// === Properties Panel ===
	int32 DetailPanelTabIndex = 0;
	uint32 BoneTransformModeFlags = 0x7;
	FVector ReferenceBoneLocation;
	FVector ReferenceBoneRotation;
	FVector ReferenceBoneScale;
	FVector MeshRelativeBoneLocation;
	FVector MeshRelativeBoneRotation;
	FVector MeshRelativeBoneScale;
};

/**
 * @brief Animation 에디터 윈도우
 * @details SSplitter 기반 레이아웃으로 4개 패널 지원
 *          - Viewport Panel: 3D 프리뷰
 *          - Properties Panel: Bone 트랜스폼 편집
 *          - Animation List Panel: 애니메이션 목록
 *          - Timeline Panel: 타임라인 컨트롤 및 Notify
 */
class SAnimationWindow : public SWindow
{
public:
	SAnimationWindow();
	virtual ~SAnimationWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height,
	                UWorld* InWorld, ID3D11Device* InDevice);

	// === SWindow overrides ===
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// === 내장 모드 (DynamicEditorWindow 내부에서 사용) ===
	// 별도 ImGui 윈도우 생성 없이, 현재 ImGui 컨텍스트 내에서 SSplitter 레이아웃만 렌더링
	void RenderEmbedded(const FRect& ContentRect);
	void SetEmbeddedMode(bool bEmbedded) { bIsEmbeddedMode = bEmbedded; }

	// === 탭 관리 ===
	void OpenNewTab(const FString& FilePath);
	void OpenNewTabWithAnimation(UAnimSequence* Animation, const FString& FilePath);
	void OpenNewTabWithMesh(USkeletalMesh* Mesh, const FString& MeshPath);
	void CloseTab(int32 Index);
	int32 GetTabCount() const { return Tabs.Num(); }

	// === 에셋 로드/저장 ===
	void LoadSkeletalMesh(const FString& Path);
	void LoadAnimation(const FString& Path);
	void LoadAnimationFile(const char* FilePath);  // DynamicEditorWindow에서 호출
	void SaveCurrentAnimation();                   // DynamicEditorWindow에서 호출

	// === Accessors ===
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	AGizmoActor* GetGizmoActor() const;
	FAnimationTabState* GetActiveState() const { return ActiveState; }
	UWorld* GetWorld() const { return World; }
	ID3D11Device* GetDevice() const { return Device; }

	// === 상태 ===
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }
	bool IsFocused() const { return bIsFocused; }
	bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

	// === 뷰포트 영역 ===
	FRect GetViewportRect() const { return ViewportRect; }

	// === 렌더 타겟 ===
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

private:
	// === 탭 생성/삭제 ===
	FAnimationTabState* CreateTabState(const FString& FilePath);
	void DestroyTabState(FAnimationTabState* State);
	FString ExtractFileName(const FString& FilePath) const;

	// === 패널 렌더링 (별도 cpp 파일) ===
	void RenderViewportPanel();           // SAnimationWindow_Viewport.cpp
	void RenderPropertiesPanel();         // SAnimationWindow_Properties.cpp
	void RenderAnimationListPanel();      // SAnimationWindow_AnimList.cpp
	void RenderTimelinePanel();           // SAnimationWindow_Timeline.cpp
	void RenderTimelineControls(FAnimationTabState* State);  // Timeline 컨트롤 UI
	void RenderTimeline(FAnimationTabState* State);          // Timeline 본체 렌더링

	// === Bone Transform 헬퍼 ===
	void UpdateBoneTransformFromSkeleton(FAnimationTabState* State);
	void ApplyBoneTransform(FAnimationTabState* State);
	void ExpandToSelectedBone(FAnimationTabState* State, int32 BoneIndex);

	// === Timeline 컨트롤 ===
	void TimelineToFront(FAnimationTabState* State);
	void TimelineToPrevious(FAnimationTabState* State);
	void TimelineReverse(FAnimationTabState* State);
	void TimelineRecord(FAnimationTabState* State);
	void TimelinePlay(FAnimationTabState* State);
	void TimelineToNext(FAnimationTabState* State);
	void TimelineToEnd(FAnimationTabState* State);
	void RefreshAnimationFrame(FAnimationTabState* State);

	// === Timeline 렌더링 헬퍼 ===
	void DrawTimelineRuler(ImDrawList* DrawList, const struct ImVec2& RulerMin,
	                       const struct ImVec2& RulerMax, float StartTime,
	                       float EndTime, FAnimationTabState* State);
	void DrawPlaybackRange(ImDrawList* DrawList, const struct ImVec2& TimelineMin,
	                       const struct ImVec2& TimelineMax, float StartTime,
	                       float EndTime, FAnimationTabState* State);
	void DrawTimelinePlayhead(ImDrawList* DrawList, const struct ImVec2& TimelineMin,
	                          const struct ImVec2& TimelineMax, float CurrentTime,
	                          float StartTime, float EndTime);
	void DrawKeyframeMarkers(ImDrawList* DrawList, const struct ImVec2& TimelineMin,
	                         const struct ImVec2& TimelineMax, float StartTime,
	                         float EndTime, FAnimationTabState* State);
	void DrawNotifyTracksPanel(FAnimationTabState* State, float StartTime, float EndTime);

	// === Notify 관리 ===
	void RebuildNotifyTracks(FAnimationTabState* State);
	void ScanNotifyLibrary();
	void CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState);
	void OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState);

	// === 렌더 타겟 관리 ===
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();

private:
	// === 탭 상태 ===
	FAnimationTabState* ActiveState = nullptr;
	TArray<FAnimationTabState*> Tabs;
	int32 ActiveTabIndex = -1;
	int32 NextTabId = 1;

	// === 월드/디바이스 ===
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// === SSplitter 레이아웃 (UE 스타일) ===
	SSplitterH* MainSplitter = nullptr;      // Left(LeftSplitter) / Right(RightSplitter)
	SSplitterV* LeftSplitter = nullptr;      // Top(Viewport) / Bottom(Timeline)
	SSplitterV* RightSplitter = nullptr;     // Top(Properties) / Bottom(AnimList)

	// === 패널 Rect (SSplitter에서 계산) ===
	FRect ViewportRect;
	FRect PropertiesRect;
	FRect AnimListRect;
	FRect TimelineRect;

	// === UI 상태 ===
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;
	bool bIsEmbeddedMode = false;  // true면 별도 윈도우 생성하지 않고 내장 모드로 동작

	// === Timeline 아이콘 ===
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

	// === Notify 라이브러리 ===
	TArray<FString> AvailableNotifyClasses;
	TArray<FString> AvailableNotifyStateClasses;

	// === Notify 다이얼로그 ===
	bool bShowNewNotifyDialog = false;
	bool bShowNewNotifyStateDialog = false;
	char NewNotifyNameBuffer[128] = "";

	// === 렌더 타겟 ===
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;
};
