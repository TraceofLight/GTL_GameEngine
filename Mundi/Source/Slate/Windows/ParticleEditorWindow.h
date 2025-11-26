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

private:
	// 툴바 렌더링
	void RenderToolbar();
	void LoadToolbarIcons();
	bool RenderIconButton(const char* id, UTexture* icon, const char* label, const char* tooltip, bool bActive = false);

	// 모달 다이얼로그 렌더링
	void RenderRenameEmitterDialog();

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
	bool bShowColorPicker = false;
	bool bIsFocused = false;

	// 에디터 상태
	float BackgroundColor[3] = { 0.1f, 0.1f, 0.1f };
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

private:
	SParticleEditorWindow* Owner = nullptr;
};
