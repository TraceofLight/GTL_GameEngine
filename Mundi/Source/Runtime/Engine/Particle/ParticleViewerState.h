#pragma once

class UWorld;
class FViewport;
class FViewportClient;
class AParticleSystemActor;
class UParticleSystem;
class UParticleEmitter;
class UParticleModule;

/**
 * @brief 파티클 에디터 상태 클래스
 * @details 파티클 에디터 윈도우의 탭별 상태를 관리
 */
class ParticleViewerState
{
public:
	FName Name;
	UWorld* World = nullptr;
	FViewport* Viewport = nullptr;
	FViewportClient* Client = nullptr;

	// 프리뷰 액터 및 파티클 시스템
	AParticleSystemActor* PreviewActor = nullptr;
	UParticleSystem* CurrentSystem = nullptr;
	FString LoadedSystemPath;

	// 선택 상태
	int32 SelectedEmitterIndex = -1;
	int32 SelectedModuleIndex = -1;
	UParticleEmitter* SelectedEmitter = nullptr;
	UParticleModule* SelectedModule = nullptr;

	// UI 상태
	bool bShowEmitterPanel = true;
	bool bShowDetailsPanel = true;
	bool bShowCurveEditor = true;

	// 시뮬레이션 제어
	bool bIsSimulating = true;
	float SimulationSpeed = 1.0f;
	float AccumulatedTime = 0.0f;

	// 뷰포트 설정
	bool bShowBounds = false;
	bool bShowOriginAxis = true;
	bool bShowGrid = true;
	float BackgroundColor[3] = { 0.1f, 0.1f, 0.1f };

	// 타임라인/커브 에디터 상태
	float TimelineZoom = 1.0f;
	float TimelineScroll = 0.0f;

	// 커브 에디터 선택
	int32 SelectedCurveIndex = -1;
	FString SelectedCurveName;

	// 이름 변경 상태
	bool bRenamingEmitter = false;
	int32 RenamingEmitterIndex = -1;
	char RenameBuffer[256] = "";

	// LOD 상태
	int32 CurrentLODIndex = 0;
};
