#pragma once

#include <d3d11.h>
#include <dxgi.h>

class FGPUTimer;

class UStatsOverlayD2D
{
public:
    static UStatsOverlayD2D& Get();

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain);
	void Shutdown();
    void Draw();

    void SetShowFPS(bool b);
    void SetShowMemory(bool b);
    void SetShowPicking(bool b);
    void SetShowDecal(bool b);
    void SetShowTileCulling(bool b);
    void SetShowLights(bool b);
    void SetShowShadow(bool b);
    void SetShowGPU(bool b);
    void ToggleFPS();
    void ToggleMemory();
    void TogglePicking();
    void ToggleDecal();
    void ToggleTileCulling();
    void ToggleLights();
    void ToggleShadow();
    void ToggleGPU();
    bool IsFPSVisible() const { return bShowFPS; }
    bool IsMemoryVisible() const { return bShowMemory; }
    bool IsPickingVisible() const { return bShowPicking; }
    bool IsDecalVisible() const { return bShowDecal; }
    bool IsTileCullingVisible() const { return bShowTileCulling; }
    bool IsLightsVisible() const { return bShowLights; }
    bool IsShadowVisible() const { return bShowShadow; }
    bool IsGPUVisible() const { return bShowGPU; }

    void SetGPUTimer(FGPUTimer* InGPUTimer) { GPUTimer = InGPUTimer; }

private:
    UStatsOverlayD2D() = default;
    ~UStatsOverlayD2D() = default;
    UStatsOverlayD2D(const UStatsOverlayD2D&) = delete;
    UStatsOverlayD2D& operator=(const UStatsOverlayD2D&) = delete;

    void EnsureInitialized();
    void ReleaseD2DTarget();

private:
    bool bInitialized = false;
    bool bShowFPS = false;
    bool bShowMemory = false;
    bool bShowPicking = false;
    bool bShowDecal = false;
    bool bShowTileCulling = false;
    bool bShowShadow = false;
    bool bShowLights = false;
    bool bShowGPU = false;

    ID3D11Device* D3DDevice = nullptr;
    ID3D11DeviceContext* D3DContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    FGPUTimer* GPUTimer = nullptr;
};
