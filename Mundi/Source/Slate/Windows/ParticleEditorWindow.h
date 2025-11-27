#pragma once
#include "SWindow.h"
#include "SSplitterH.h"
#include "SSplitterV.h"

class FViewport;
class FViewportClient;
class UWorld;
class UParticleSystem;
class UParticleEmitter;
class UParticleModule;
class ParticleViewerState;
struct ID3D11Device;
class UTexture;

// ============================================================================
// Curve Editor Data Structures
// ============================================================================

/**
 * @brief 커브 보간 모드
 */
enum class EInterpMode : uint8
{
	Linear,      // 선형 보간
	Constant,    // 상수 (계단식)
	Cubic        // 3차 베지어 곡선
};

/**
 * @brief 탄젠트 모드
 */
enum class ETangentMode : uint8
{
	Auto,           // 자동 탄젠트 (부드러운 곡선)
	User,           // 사용자 정의
	Break,          // 탄젠트 분리 (입력/출력 독립)
	AutoClamped     // 클램핑된 자동 탄젠트
};

/**
 * @brief 커브 키프레임 데이터
 */
struct FCurveKey
{
	float Time;              // X축: 시간
	float Value;             // Y축: 값
	EInterpMode InterpMode;  // 보간 모드
	ETangentMode TangentMode; // 탄젠트 모드
	float ArriveTangent;     // 입력 탄젠트
	float LeaveTangent;      // 출력 탄젠트
	bool bSelected;          // 선택 상태

	FCurveKey()
		: Time(0.0f)
		, Value(0.0f)
		, InterpMode(EInterpMode::Cubic)
		, TangentMode(ETangentMode::Auto)
		, ArriveTangent(0.0f)
		, LeaveTangent(0.0f)
		, bSelected(false)
	{
	}

	FCurveKey(float InTime, float InValue)
		: Time(InTime)
		, Value(InValue)
		, InterpMode(EInterpMode::Cubic)
		, TangentMode(ETangentMode::Auto)
		, ArriveTangent(0.0f)
		, LeaveTangent(0.0f)
		, bSelected(false)
	{
	}
};

/**
 * @brief 커브 데이터 (속성 1개)
 */
struct FCurveData
{
	TArray<FCurveKey> Keys;      // 키프레임 배열 (시간 순 정렬)
	FLinearColor Color;          // 커브 색상
	FString PropertyName;        // 속성 이름 (예: "StartVelocityRadial")
	bool bVisible;               // 표시 여부
	bool bSelected;              // 선택 상태
	class UParticleModule* OwnerModule;  // 이 커브를 소유한 모듈 인스턴스

	FCurveData()
		: Color(1.0f, 1.0f, 1.0f, 1.0f)
		, bVisible(true)
		, bSelected(false)
		, OwnerModule(nullptr)
	{
	}

	FCurveData(const FString& InName, const FLinearColor& InColor)
		: Color(InColor)
		, PropertyName(InName)
		, bVisible(true)
		, bSelected(false)
		, OwnerModule(nullptr)
	{
	}
};

/**
 * @brief 커브 에디터 선택 상태
 */
struct FCurveEditorSelection
{
	int32 CurveIndex;            // 선택된 커브 인덱스 (-1이면 없음)
	TArray<int32> SelectedKeys;  // 선택된 키 인덱스들

	// 드래그 상태
	enum class EDragType : uint8
	{
		None,
		Key,              // 키 드래그
		ArriveTangent,    // 입력 탄젠트 핸들
		LeaveTangent      // 출력 탄젠트 핸들
	};

	EDragType DragType;
	int32 DragKeyIndex;          // 드래그 중인 키 인덱스
	ImVec2 DragStartPos;         // 드래그 시작 위치 (스크린 좌표)

	FCurveEditorSelection()
		: CurveIndex(-1)
		, DragType(EDragType::None)
		, DragKeyIndex(-1)
		, DragStartPos(0, 0)
	{
	}

	void ClearSelection()
	{
		CurveIndex = -1;
		SelectedKeys.clear();
	}

	bool IsKeySelected(int32 KeyIndex) const
	{
		for (int32 Idx : SelectedKeys)
		{
			if (Idx == KeyIndex)
			{
				return true;
			}
		}
		return false;
	}
};

// Forward declarations for panel classes
class SParticleViewportPanel;
class SParticleDetailPanel;
class SParticleEmittersPanel;
class SParticleCurveEditorPanel;

/**
 * @brief 파티클 시스템 에디터 윈도우
 * @details 스플리터 기반 4분할 레이아웃
 *          - 좌상단: Viewport (파티클 프리뷰)
 *          - 좌하단: Detail (선택된 모듈 프로퍼티)
 *          - 우상단: Emitters (이미터/모듈 스택)
 *          - 우하단: Curve Editor (커브 편집)
 */
class SParticleEditorWindow : public SWindow
{
public:
	SParticleEditorWindow();
	virtual ~SParticleEditorWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	// SWindow overrides
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// Viewport 렌더링 (SlateManager에서 호출)
	void OnRenderViewport();

	// 파티클 시스템 로드
	void LoadParticleSystem(const FString& Path);
	void SetParticleSystem(UParticleSystem* InSystem);

	// Accessors
	FViewport* GetViewport() const;
	FViewportClient* GetViewportClient() const;
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }
	bool IsFocused() const { return bIsFocused; }
	bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

	// 패널 접근용 (친구 클래스나 콜백을 위해)
	ParticleViewerState* GetActiveState() const { return ActiveState; }
	SParticleCurveEditorPanel* GetCurveEditorPanel() const { return CurveEditorPanel; }

	// 렌더 타겟 관리
	void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
	void RenderToPreviewRenderTarget();

	// 렌더 타겟 접근자
	ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

	// 모듈 UI 아이콘 접근자
	UTexture* GetIconCurveEditor() const { return IconCurveEditor; }
	UTexture* GetIconCheckbox() const { return IconCheckbox; }
	UTexture* GetIconCheckboxChecked() const { return IconCheckboxChecked; }

	// 이미터 헤더 아이콘 접근자
	UTexture* GetIconEmitterSolo() const { return IconEmitterSolo; }
	UTexture* GetIconRenderModeNormal() const { return IconRenderModeNormal; }
	UTexture* GetIconRenderModeCross() const { return IconRenderModeCross; }

	// LOD 접근자
	int32 GetCurrentLODIndex() const { return CurrentLODIndex; }
	void SetCurrentLODIndex(int32 Index);
	int32 GetMaxLODCount() const;

private:
	// 툴바 렌더링
	void RenderToolbar();
	void LoadToolbarIcons();
	bool RenderIconButton(const char* id, UTexture* icon, const char* label, const char* tooltip, bool bActive = false);

	// 모달 다이얼로그 렌더링
	void RenderRenameEmitterDialog();

	// 바운딩 박스/스피어 렌더링
	void RenderParticleBounds();

	// 툴바 아이콘
	UTexture* IconSave = nullptr;
	UTexture* IconLoad = nullptr;
	UTexture* IconRestartSim = nullptr;
	UTexture* IconRestartLevel = nullptr;
	UTexture* IconUndo = nullptr;
	UTexture* IconRedo = nullptr;
	UTexture* IconThumbnail = nullptr;
	UTexture* IconBounds = nullptr;
	UTexture* IconOriginAxis = nullptr;
	UTexture* IconBackgroundColor = nullptr;
	UTexture* IconRegenLOD = nullptr;
	UTexture* IconLowestLOD = nullptr;
	UTexture* IconLowerLOD = nullptr;
	UTexture* IconHigherLOD = nullptr;
	UTexture* IconAddLOD = nullptr;

	// 모듈 UI 아이콘
	UTexture* IconCurveEditor = nullptr;
	UTexture* IconCheckbox = nullptr;
	UTexture* IconCheckboxChecked = nullptr;

	// 이미터 헤더 아이콘
	UTexture* IconEmitterSolo = nullptr;
	UTexture* IconRenderModeNormal = nullptr;   // Sprite
	UTexture* IconRenderModeCross = nullptr;

	// LOD 상태
	int32 CurrentLODIndex = 0;

	// 탭 상태
	ParticleViewerState* ActiveState = nullptr;
	TArray<ParticleViewerState*> Tabs;
	int32 ActiveTabIndex = -1;

	// 레거시 참조
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 스플리터 레이아웃
	// MainSplitter (H): Left | Right
	//   LeftSplitter (V): Viewport | Detail
	//   RightSplitter (V): Emitters | CurveEditor
	SSplitterH* MainSplitter = nullptr;
	SSplitterV* LeftSplitter = nullptr;
	SSplitterV* RightSplitter = nullptr;

	// 패널들
	SParticleViewportPanel* ViewportPanel = nullptr;
	SParticleDetailPanel* DetailPanel = nullptr;
	SParticleEmittersPanel* EmittersPanel = nullptr;
	SParticleCurveEditorPanel* CurveEditorPanel = nullptr;

	// 뷰포트 영역 캐시 (실제 3D 렌더링 영역)
	FRect ViewportRect;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;
	bool bIsFocused = false;

	// 전용 렌더 타겟 (파티클 프리뷰용)
	ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
	ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
	ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
	ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
	uint32 PreviewRenderTargetWidth = 0;
	uint32 PreviewRenderTargetHeight = 0;

	// 렌더 타겟 관리 (내부용)
	void CreateRenderTarget(uint32 Width, uint32 Height);
	void ReleaseRenderTarget();
};

// ============================================================================
// Panel Classes
// ============================================================================

/**
 * @brief 파티클 뷰포트 패널
 */
class SParticleViewportPanel : public SWindow
{
public:
	SParticleViewportPanel(SParticleEditorWindow* InOwner);
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;

	// 3D 콘텐츠 렌더링 영역 (실제 뷰포트 영역)
	FRect ContentRect;

private:
	SParticleEditorWindow* Owner = nullptr;
};

/**
 * @brief 파티클 디테일 패널
 */
class SParticleDetailPanel : public SWindow
{
public:
	SParticleDetailPanel(SParticleEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SParticleEditorWindow* Owner = nullptr;
	void RenderModuleProperties(UParticleModule* Module);
};

/**
 * @brief 이미터 패널 (모듈 스택)
 */
class SParticleEmittersPanel : public SWindow
{
public:
	SParticleEmittersPanel(SParticleEditorWindow* InOwner);
	virtual void OnRender() override;

private:
	SParticleEditorWindow* Owner = nullptr;
	void RenderEmitterHeader(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderModuleStack(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderModuleItem(UParticleModule* Module, int32 ModuleIndex, int32 EmitterIndex);

	// 컨텍스트 메뉴 관련
	void AddNewEmitter(UParticleSystem* System);
	void RenderModuleContextMenu(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderEmitterContextMenu(UParticleEmitter* Emitter, int32 EmitterIndex);

	// 삭제 기능
	void DeleteEmitter(UParticleSystem* System, int32 EmitterIndex);
	void DeleteSelectedModule();

	// 헬퍼: 모듈 추가 후 EmitterInstance 업데이트
	void AddModuleAndUpdateInstances(class UParticleLODLevel* LODLevel, UParticleModule* Module);
	void RefreshEmitterInstances();
};

/**
 * @brief 커브 에디터 패널
 */
class SParticleCurveEditorPanel : public SWindow
{
public:
	SParticleCurveEditorPanel(SParticleEditorWindow* InOwner);
	virtual void OnRender() override;

	// 커브 관리
	void AddCurve(const FString& PropertyName, const FLinearColor& Color, class UParticleModule* OwnerModule = nullptr);
	void RemoveCurve(const FString& PropertyName);
	void RemoveAllCurves();
	void RemoveAllCurvesForModule(const FString& ModuleName);
	bool HasCurve(const FString& PropertyName) const;

	// 디테일 패널 → 커브 에디터 동기화
	void RefreshCurvesFromModule();

private:
	SParticleEditorWindow* Owner = nullptr;

	// 커브 데이터
	TArray<FCurveData> Curves;           // 등록된 커브 목록
	FCurveEditorSelection Selection;     // 선택 상태

	// 뷰 상태
	float ViewMinX = -0.1f;              // 보이는 시간 범위 (최소)
	float ViewMaxX = 1.1f;               // 보이는 시간 범위 (최대)
	float ViewMinY = -0.5f;              // 보이는 값 범위 (최소)
	float ViewMaxY = 1.5f;               // 보이는 값 범위 (최대)

	// 툴바 상태
	bool bPanMode = true;                // 패닝 모드 (기본 ON)
	bool bZoomMode = false;              // 줌 모드
	bool bShowAll = true;                // 모든 커브 표시

	// 마우스 입력 상태
	ImVec2 PanStartMousePos;             // 패닝 시작 위치
	float PanStartViewMinX, PanStartViewMaxX;
	float PanStartViewMinY, PanStartViewMaxY;
	bool bIsPanning = false;

	// 툴바 아이콘
	UTexture* IconHorizontal = nullptr;
	UTexture* IconVertical = nullptr;
	UTexture* IconFit = nullptr;
	UTexture* IconPan = nullptr;
	UTexture* IconZoom = nullptr;
	UTexture* IconAuto = nullptr;
	UTexture* IconUser = nullptr;
	UTexture* IconBreak = nullptr;
	UTexture* IconLinear = nullptr;
	UTexture* IconConstant = nullptr;
	UTexture* IconCubic = nullptr;
	UTexture* IconFlatten = nullptr;
	UTexture* IconStraighten = nullptr;
	UTexture* IconShowAll = nullptr;
	UTexture* IconCreate = nullptr;
	UTexture* IconDelete = nullptr;

	// 헬퍼 함수
	void LoadIcons();
	ImVec2 CurveToScreen(float Time, float Value, const ImVec2& CanvasPos, const ImVec2& CanvasSize) const;
	void ScreenToCurve(const ImVec2& ScreenPos, const ImVec2& CanvasPos, const ImVec2& CanvasSize, float& OutTime, float& OutValue) const;
	void FitView();
	void FitViewHorizontal();
	void FitViewVertical();
	void RenderToolbar();
	void RenderCurveList();
	void RenderCurveGrid();
	void HandleMouseInput(const ImVec2& CanvasPos, const ImVec2& CanvasSize);
	void RenderCurve(const FCurveData& Curve, const ImVec2& CanvasPos, const ImVec2& CanvasSize, ImDrawList* DrawList);
	void RenderKeys(const FCurveData& Curve, int32 CurveIndex, const ImVec2& CanvasPos, const ImVec2& CanvasSize, ImDrawList* DrawList);
	void UpdateModuleFromCurve(int32 CurveIndex);
	FLinearColor GetColorFromModuleInstance(class UParticleModule* Module);
};
