#pragma once
#include "SWindow.h"

class FViewport;
class FViewportClient;
class UWorld;
class UParticleSystem;
class AParticleSystemActor;
struct ID3D11Device;

/**
 * @brief ParticleSystem 프리뷰 윈도우
 * @details ContentBrowser에서 .psys 더블클릭 시 열리는 파티클 프리뷰어
 */
class SParticlePreviewWindow : public SWindow
{
public:
	SParticlePreviewWindow();
	virtual ~SParticlePreviewWindow();

	bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

	// SWindow overrides
	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void OnMouseMove(FVector2D MousePos) override;
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

	// ParticleSystem 로드
	void LoadParticleSystem(const FString& Path);

	// ParticleSystem 설정 (직접 Template 전달)
	void SetParticleSystem(UParticleSystem* InTemplate);

	// Getters
	FViewport* GetViewport() const { return Viewport; }
	FViewportClient* GetViewportClient() const { return ViewportClient; }
	bool IsOpen() const { return bIsOpen; }
	void Close() { bIsOpen = false; }

private:
	void OnRenderViewport();
	void OnRenderControlPanel();

	// 프리뷰 제어
	void RestartParticleSystem();

private:
	FViewport* Viewport = nullptr;
	FViewportClient* ViewportClient = nullptr;
	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;

	// 프리뷰용 Actor
	AParticleSystemActor* PreviewActor = nullptr;

	// UI 상태
	bool bIsOpen = true;
	bool bInitialPlacementDone = false;

	// 뷰포트 영역
	FRect ViewportRect;
};
