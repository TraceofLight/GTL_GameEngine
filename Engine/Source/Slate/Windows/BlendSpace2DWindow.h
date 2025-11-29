#pragma once
#include "Window.h"
#include "Source/Runtime/Core/Math/Vector.h"

class FViewport;
class FViewportClient;
class UWorld;
class USkeletalMesh;
class UAnimSequence;
class UBlendSpace2D;
class UTexture;
class AGizmoActor;
class ASkeletalMeshActor;
class SSplitterH;
class SSplitterV;
struct ID3D11Device;
struct ImDrawList;

/**
 * @brief BlendSpace2D 에디터 탭 상태
 * @details 각 탭의 고유 상태를 관리 (BlendSpace, 뷰포트, 그리드 등)
 *
 * @param TabName 탭 표시 이름
 * @param TabId 고유 탭 ID
 * @param FilePath 현재 편집 중인 파일 경로
 * @param BlendSpace 편집 중인 BlendSpace2D
 * @param PreviewParameter 프리뷰 위치 파라미터
 */
struct FBlendSpace2DTabState
{
	// === 기본 정보 ===
	FName TabName;
	int32 TabId = 0;
	FString FilePath;

	// === 월드 및 뷰포트 ===
	UWorld* World = nullptr;
	FViewport* Viewport = nullptr;
	FViewportClient* Client = nullptr;

	// === 에셋 참조 ===
	ASkeletalMeshActor* PreviewActor = nullptr;
	USkeletalMesh* CurrentMesh = nullptr;
	FString LoadedMeshPath;
	UBlendSpace2D* BlendSpace = nullptr;

	// === 애니메이션 시퀀스 목록 ===
	TArray<UAnimSequence*> AvailableAnimations;
	int32 SelectedAnimationIndex = -1;

	// === 프리뷰 상태 ===
	FVector2D PreviewParameter = FVector2D(0.0f, 0.0f);
	bool bIsPlaying = true;
	float PlaybackSpeed = 1.0f;
	bool bLoopAnimation = true;
	float CurrentAnimationTime = 0.0f;

	// === 그리드 에디터 상태 ===
	ImVec2 CanvasPos = ImVec2(0, 0);
	ImVec2 CanvasSize = ImVec2(400, 400);
	int32 SelectedSampleIndex = -1;
	bool bDraggingSample = false;
	bool bDraggingPreviewMarker = false;

	// === Grid Snapping ===
	bool bEnableGridSnapping = true;
	float GridSnapSize = 10.0f;

	// === Zoom & Pan ===
	float ZoomLevel = 1.0f;
	FVector2D PanOffset = FVector2D(0.0f, 0.0f);
	bool bPanning = false;
	ImVec2 PanStartMousePos = ImVec2(0, 0);

	// === Context Menu ===
	bool bShowContextMenu = false;
	ImVec2 ContextMenuPos = ImVec2(0, 0);
	int32 ContextMenuSampleIndex = -1;

	// === Display Options ===
	bool bShowMesh = true;
	bool bShowBones = false;
};

/**
 * @brief BlendSpace2D 에디터 윈도우
 * @details SSplitter 기반 레이아웃 (UE5 스타일)
 *          - Details Panel (좌측): BlendSpace 속성/샘플 편집
 *          - Viewport Panel (우측 상단): 3D 프리뷰
 *          - Grid Panel (우측 하단): 2D 블렌드 그리드 + 타임라인
 */
class SBlendSpace2DWindow : public SWindow
{
public:
	SBlendSpace2DWindow();
	virtual ~SBlendSpace2DWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height,
	                UWorld* InWorld, ID3D11Device* InDevice);

	// === SWindow overrides ===
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// === 내장 모드 (DynamicEditorWindow 내부에서 사용) ===
	void RenderEmbedded(const FRect& ContentRect);
	void SetEmbeddedMode(bool bEmbedded) { bIsEmbeddedMode = bEmbedded; }

	// === 탭 관리 ===
	void OpenNewTab(const FString& FilePath);
	void OpenNewTabWithBlendSpace(UBlendSpace2D* InBlendSpace, const FString& FilePath);
	void OpenNewTabWithMesh(USkeletalMesh* Mesh, const FString& MeshPath);
	void CloseTab(int32 Index);
	int32 GetTabCount() const { return Tabs.Num(); }

	// === 에셋 로드 ===
	void LoadBlendSpace(const FString& Path);
	void LoadSkeletalMesh(const FString& Path);

	// === Accessors ===
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	AGizmoActor* GetGizmoActor() const;
	FBlendSpace2DTabState* GetActiveState() const { return ActiveState; }
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
	FBlendSpace2DTabState* CreateTabState(const FString& FilePath);
	void DestroyTabState(FBlendSpace2DTabState* State);
	FString ExtractFileName(const FString& FilePath) const;

	// === 패널 렌더링 ===
	void RenderDetailsPanel();
	void RenderViewportPanel();
	void RenderGridPanel();

	// === 그리드 렌더링 헬퍼 ===
	void RenderGrid(FBlendSpace2DTabState* State);
	void RenderSamplePoints(FBlendSpace2DTabState* State);
	void RenderSamplePoints_Enhanced(FBlendSpace2DTabState* State, const TArray<int32>& InSampleIndices, const TArray<float>& InWeights);
	void RenderPreviewMarker(FBlendSpace2DTabState* State);
	void RenderAxisLabels(FBlendSpace2DTabState* State);
	void RenderTriangulation(FBlendSpace2DTabState* State);
	void RenderTriangulation_Enhanced(FBlendSpace2DTabState* State, int32 InActiveTriangle);

	// === 좌표 변환 ===
	ImVec2 ParamToScreen(FBlendSpace2DTabState* State, FVector2D Param) const;
	FVector2D ScreenToParam(FBlendSpace2DTabState* State, ImVec2 ScreenPos) const;

	// === 입력 처리 ===
	void HandleGridMouseInput(FBlendSpace2DTabState* State);
	void HandleKeyboardInput(FBlendSpace2DTabState* State);

	// === 샘플 관리 ===
	void AddSampleAtPosition(FBlendSpace2DTabState* State, FVector2D Position);
	void RemoveSelectedSample(FBlendSpace2DTabState* State);
	void SelectSample(FBlendSpace2DTabState* State, int32 Index);

	// === 타임라인 컨트롤 ===
	void RenderTimelineControls(FBlendSpace2DTabState* State);
	void TimelineToFront(FBlendSpace2DTabState* State);
	void TimelineToPrevious(FBlendSpace2DTabState* State);
	void TimelinePlay(FBlendSpace2DTabState* State);
	void TimelineToNext(FBlendSpace2DTabState* State);
	void TimelineToEnd(FBlendSpace2DTabState* State);

	// === 렌더 타겟 관리 ===
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();

private:
	// === 탭 상태 ===
	FBlendSpace2DTabState* ActiveState = nullptr;
	TArray<FBlendSpace2DTabState*> Tabs;
	int32 ActiveTabIndex = -1;
	int32 NextTabId = 1;

	// === 월드/디바이스 ===
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// === SSplitter 레이아웃 (UE5 스타일) ===
	SSplitterH* MainSplitter = nullptr;      // 좌우 분할: Left(Details) / Right(RightSplitter)
	SSplitterV* RightSplitter = nullptr;     // 상하 분할: Top(Viewport) / Bottom(Grid)

	// === 패널 Rect (SSplitter에서 계산) ===
	FRect DetailsRect;
	FRect ViewportRect;
	FRect GridRect;

	// === UI 상태 ===
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;
	bool bIsEmbeddedMode = false;

	// === 타임라인 아이콘 ===
	UTexture* IconGoToFront = nullptr;
	UTexture* IconGoToFrontOff = nullptr;
	UTexture* IconStepBackwards = nullptr;
	UTexture* IconStepBackwardsOff = nullptr;
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

	// === 그리드 설정 ===
	static constexpr float GridCellSize = 40.0f;
	static constexpr float SamplePointRadius = 8.0f;
	static constexpr float PreviewMarkerRadius = 12.0f;

	// === 색상 ===
	ImU32 GridColor = IM_COL32(80, 80, 80, 255);
	ImU32 AxisColor = IM_COL32(150, 150, 150, 255);
	ImU32 SampleColor = IM_COL32(255, 200, 0, 255);
	ImU32 SelectedSampleColor = IM_COL32(255, 100, 0, 255);
	ImU32 PreviewColor = IM_COL32(0, 255, 0, 255);

	// === 렌더 타겟 ===
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;
};
