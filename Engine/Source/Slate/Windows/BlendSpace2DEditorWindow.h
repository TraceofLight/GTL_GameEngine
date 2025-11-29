#pragma once
#include "Window.h"
#include "Source/Runtime/Core/Math/Vector.h"

class UBlendSpace2D;
class UAnimSequence;
class UWorld;
class FViewport;
class ASkeletalMeshActor;
class USkeletalMeshComponent;
class ViewerState;

/**
 * @brief Blend Space 2D 에디터 윈도우
 *
 * 2D 그리드 상에서 애니메이션 샘플 포인트를 시각적으로 배치하고 편집할 수 있는 에디터입니다.
 */
class SBlendSpace2DEditorWindow : public SWindow
{
public:
	SBlendSpace2DEditorWindow();
	virtual ~SBlendSpace2DEditorWindow();

	/**
	 * @brief 윈도우 초기화
	 */
	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	/**
	 * @brief 편집할 BlendSpace 설정
	 */
	void SetBlendSpace(UBlendSpace2D* InBlendSpace);

	/**
	 * @brief 현재 편집 중인 BlendSpace 반환
	 */
	UBlendSpace2D* GetBlendSpace() const { return EditingBlendSpace; }

	/**
	 * @brief 렌더링
	 */
	virtual void OnRender() override;

	/**
	 * @brief 뷰포트 렌더링
	 */
	void OnRenderViewport();

	/**
	 * @brief 업데이트
	 */
	void OnUpdate(float DeltaSeconds);

	/**
	 * @brief 마우스 입력 처리
	 */
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	/**
	 * @brief 윈도우가 열려있는지 확인
	 */
	bool IsOpen() const { return bIsOpen; }

	/**
	 * @brief 윈도우 닫기
	 */
	void Close() { bIsOpen = false; }

private:
	// ===== UI 렌더링 =====
	void RenderPreviewViewport();      // 상단: 애니메이션 프리뷰
	void RenderGridEditor();           // 하단 왼쪽: 2D 그리드
	void RenderAnimationList();        // 하단 오른쪽: 애니메이션 시퀀스 목록

	void RenderGrid();
	void RenderSamplePoints();
	void RenderSamplePoints_Enhanced(const TArray<int32>& InSampleIndices, const TArray<float>& InWeights);
	void RenderPreviewMarker();
	void RenderAxisLabels();
	void RenderTriangulation();  // Delaunay 삼각분할 시각화
	void RenderTriangulation_Enhanced(int32 InActiveTriangle);
	void RenderToolbar();
	void RenderSampleList();
	void RenderProperties();

	// ===== 타임라인 렌더링 =====
	void RenderTimelineControls();
	void RenderTimeline();
	void DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime);
	void DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime);

	// ===== 타임라인 컨트롤 =====
	void TimelineToFront();
	void TimelineToPrevious();
	void TimelineReverse();
	void TimelinePlay();
	void TimelineToNext();
	void TimelineToEnd();

	// ===== 좌표 변환 =====
	ImVec2 ParamToScreen(FVector2D Param) const;
	FVector2D ScreenToParam(ImVec2 ScreenPos) const;

	// ===== 입력 처리 =====
	void HandleMouseInput();
	void HandleKeyboardInput();

	// ===== 샘플 관리 =====
	void AddSampleAtPosition(FVector2D Position);
	void RemoveSelectedSample();
	void SelectSample(int32 Index);

	// ===== 데이터 =====
	UBlendSpace2D* EditingBlendSpace = nullptr;
	ID3D11Device* Device = nullptr;

	// ===== 프리뷰 뷰포트 (ViewerState 재활용) =====
	ViewerState* PreviewState = nullptr;

	// ===== 애니메이션 시퀀스 목록 =====
	TArray<UAnimSequence*> AvailableAnimations;  // 로드된 모든 애니메이션
	int32 SelectedAnimationIndex = -1;           // 선택된 애니메이션 인덱스

	// ===== UI 상태 =====
	bool bIsOpen = true;
	ImVec2 CanvasPos;
	ImVec2 CanvasSize;
	int32 SelectedSampleIndex = -1;
	bool bDraggingSample = false;
	bool bDraggingPreviewMarker = false;
	FVector2D PreviewParameter = FVector2D(0.0f, 0.0f);
	FRect PreviewViewportRect = FRect(0, 0, 0, 0);  // 프리뷰 뷰포트 영역

	// ===== Grid Snapping =====
	bool bEnableGridSnapping = true;
	float GridSnapSize = 10.0f;  // 스냅 간격 (파라미터 단위)

	// ===== Zoom & Pan =====
	float ZoomLevel = 1.0f;       // 줌 레벨 (1.0 = 100%)
	FVector2D PanOffset = FVector2D(0.0f, 0.0f);  // 팬 오프셋
	bool bPanning = false;        // 팬 드래그 중
	ImVec2 PanStartMousePos;      // 팬 시작 마우스 위치

	// ===== Sample Duplication =====
	bool bDuplicatingMode = false;  // Ctrl 키 누른 상태로 드래그 중

	// ===== Context Menu =====
	bool bShowContextMenu = false;
	ImVec2 ContextMenuPos;
	int32 ContextMenuSampleIndex = -1;

	// ===== 애니메이션 재생 상태 =====
	bool bIsPlaying = true;           // 재생 중인지 여부
	float PlaybackSpeed = 1.0f;       // 재생 속도 배율
	bool bLoopAnimation = true;       // 루프 재생 여부
	float CurrentAnimationTime = 0.0f;  // 현재 애니메이션 시간

	// ===== 타임라인 아이콘 =====
	class UTexture* IconGoToFront = nullptr;
	class UTexture* IconGoToFrontOff = nullptr;
	class UTexture* IconStepBackwards = nullptr;
	class UTexture* IconStepBackwardsOff = nullptr;
	class UTexture* IconBackwards = nullptr;
	class UTexture* IconBackwardsOff = nullptr;
	class UTexture* IconRecord = nullptr;
	class UTexture* IconPause = nullptr;
	class UTexture* IconPauseOff = nullptr;
	class UTexture* IconPlay = nullptr;
	class UTexture* IconPlayOff = nullptr;
	class UTexture* IconStepForward = nullptr;
	class UTexture* IconStepForwardOff = nullptr;
	class UTexture* IconGoToEnd = nullptr;
	class UTexture* IconGoToEndOff = nullptr;
	class UTexture* IconLoop = nullptr;
	class UTexture* IconLoopOff = nullptr;

	// ===== UI 설정 =====
	static constexpr float GridCellSize = 40.0f;
	static constexpr float SamplePointRadius = 8.0f;
	static constexpr float PreviewMarkerRadius = 12.0f;

	// ===== 색상 =====
	ImU32 GridColor = IM_COL32(80, 80, 80, 255);
	ImU32 AxisColor = IM_COL32(150, 150, 150, 255);
	ImU32 SampleColor = IM_COL32(255, 200, 0, 255);
	ImU32 SelectedSampleColor = IM_COL32(255, 100, 0, 255);
	ImU32 PreviewColor = IM_COL32(0, 255, 0, 255);

	// ===== 전용 렌더 타겟 (프리뷰용) =====
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;

	// 렌더 타겟 관리
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();

public:
	// 렌더 타겟 접근자
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }
};
