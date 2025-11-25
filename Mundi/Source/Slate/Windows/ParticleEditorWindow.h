#pragma once
#include "SWindow.h"

class FViewport;
class FViewportClient;
class UWorld;
class UParticleSystem;
class UParticleEmitter;
class UParticleModule;
class ParticleViewerState;
class UTexture;
struct ID3D11Device;

/**
 * @brief 파티클 시스템 에디터 윈도우
 * @details Cascade 스타일의 파티클 에디터 UI
 *          - Viewport: 파티클 프리뷰
 *          - Emitters: 이미터/모듈 스택
 *          - Details: 선택된 모듈 프로퍼티
 *          - Curve Editor: 커브 편집
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

private:
	// UI 렌더링
	void RenderToolbar();
	void RenderViewportPanel();
	void RenderEmittersPanel();
	void RenderDetailsPanel();
	void RenderCurveEditor();

	// 이미터 패널 헬퍼
	void RenderEmitterHeader(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderModuleStack(UParticleEmitter* Emitter, int32 EmitterIndex);
	void RenderModuleItem(UParticleModule* Module, int32 ModuleIndex, int32 EmitterIndex);

	// 모듈 프로퍼티 렌더링
	void RenderModuleProperties(UParticleModule* Module);

	// 시뮬레이션 제어
	void RestartSimulation();
	void ToggleSimulation();

private:
	// 탭 상태
	ParticleViewerState* ActiveState = nullptr;
	TArray<ParticleViewerState*> Tabs;
	int32 ActiveTabIndex = -1;

	// 레거시 참조 (탭 안정화 후 제거)
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 레이아웃 비율
	float ViewportWidthRatio = 0.4f;    // 뷰포트 너비 (40%)
	float EmittersPanelWidthRatio = 0.3f; // 이미터 패널 너비 (30%)
	float DetailsPanelWidthRatio = 0.3f;  // 디테일 패널 너비 (30%)
	float CurveEditorHeightRatio = 0.35f; // 커브 에디터 높이 (35%)

	// 뷰포트 영역 캐시
	FRect ViewportRect;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;
	bool bRequestFocus = false;

	// 툴바 아이콘
	UTexture* IconPlay = nullptr;
	UTexture* IconPause = nullptr;
	UTexture* IconRestart = nullptr;
	UTexture* IconBounds = nullptr;
	UTexture* IconOriginAxis = nullptr;
	UTexture* IconThumbnail = nullptr;
	UTexture* IconBackgroundColor = nullptr;

	// 에디터 상태
	float BackgroundColor[3] = { 0.1f, 0.1f, 0.1f };
};
