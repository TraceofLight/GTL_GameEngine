#pragma once

class FGPUTimer;
class FCanvas;

/**
 * @brief 화면에 Stats를 표시하는 2D 오버레이 시스템
 * @details Canvas 시스템을 사용하여 렌더링 (FutureEngine 패턴 통합)
 */
class UStatsOverlayD2D
{
public:
    static UStatsOverlayD2D& Get();

    void Initialize();
	void Shutdown();
    void Draw(FCanvas& Canvas);

    void SetShowFPS(bool b) { bShowFPS = b; }
    void SetShowMemory(bool b) { bShowMemory = b; }
    void SetShowPicking(bool b)  { bShowPicking = b; }
    void SetShowDecal(bool b)  { bShowDecal = b; }
    void SetShowTileCulling(bool b)  { bShowTileCulling = b; }
    void SetShowLights(bool b) { bShowLights = b; }
    void SetShowShadow(bool b) { bShowShadow = b; }
    void SetShowGPU(bool b) { bShowGPU = b; }
    void SetShowSkinning(bool b) { bShowSkinning = b; }
    void SetShowParticles(bool b) { bShowParticles = b; }
    void SetShowPhysicsAsset(bool b) { bShowPhysicsAsset = b; }
    void ToggleFPS() { bShowFPS = !bShowFPS; }
    void ToggleMemory() { bShowMemory = !bShowMemory; }
    void TogglePicking() { bShowPicking = !bShowPicking; }
    void ToggleDecal() { bShowDecal = !bShowDecal; }
    void ToggleTileCulling() { bShowTileCulling = !bShowTileCulling; }
    void ToggleLights() { bShowLights = !bShowLights; }
    void ToggleShadow() { bShowShadow = !bShowShadow; }
    void ToggleGPU() { bShowGPU = !bShowGPU; }
    void ToggleSkinning() { bShowSkinning = !bShowSkinning; }
    void ToggleParticles() { bShowParticles = !bShowParticles; }
    void TogglePhysicsAsset() { bShowPhysicsAsset = !bShowPhysicsAsset; }
    bool IsFPSVisible() const { return bShowFPS; }
    bool IsMemoryVisible() const { return bShowMemory; }
    bool IsPickingVisible() const { return bShowPicking; }
    bool IsDecalVisible() const { return bShowDecal; }
    bool IsTileCullingVisible() const { return bShowTileCulling; }
    bool IsLightsVisible() const { return bShowLights; }
    bool IsShadowVisible() const { return bShowShadow; }
    bool IsGPUVisible() const { return bShowGPU; }
    bool IsSkinningVisible() const { return bShowSkinning; }
    bool IsParticlesVisible() const { return bShowParticles; }
    bool IsPhysicsAssetVisible() const { return bShowPhysicsAsset; }

    void SetGPUTimer(FGPUTimer* InGPUTimer) { GPUTimer = InGPUTimer; }

    // PhysicsAsset Stats 설정
    void SetPhysicsAssetStats(int32 Bodies, int32 Constraints, int32 Shapes, const char* Mode, bool bSimulating)
    {
        PhysicsAssetBodies = Bodies;
        PhysicsAssetConstraints = Constraints;
        PhysicsAssetShapes = Shapes;
        PhysicsAssetMode = Mode ? Mode : "";
        bPhysicsAssetSimulating = bSimulating;
    }

private:
    UStatsOverlayD2D() = default;
    ~UStatsOverlayD2D() = default;
    UStatsOverlayD2D(const UStatsOverlayD2D&) = delete;
    UStatsOverlayD2D& operator=(const UStatsOverlayD2D&) = delete;

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
    bool bShowSkinning = false;
    bool bShowParticles = false;
    bool bShowPhysicsAsset = false;

    // PhysicsAsset Stats 데이터
    int32 PhysicsAssetBodies = 0;
    int32 PhysicsAssetConstraints = 0;
    int32 PhysicsAssetShapes = 0;
    std::string PhysicsAssetMode;
    bool bPhysicsAssetSimulating = false;

    FGPUTimer* GPUTimer = nullptr;
};
