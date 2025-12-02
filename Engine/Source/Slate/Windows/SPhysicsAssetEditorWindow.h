#pragma once
#include "Window.h"
#include "SplitterH.h"
#include "SplitterV.h"

class PhysicsAssetViewerState;
class UWorld;
class UTexture;
class FViewport;
class FViewportClient;
class USkeletalMesh;
class UPhysicsAsset;
struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;

// Forward declarations for panel classes
class SPhysicsAssetToolbarPanel;
class SPhysicsAssetViewportPanel;
class SPhysicsAssetBodyListPanel;
class SPhysicsAssetPropertiesPanel;

/**
 * @brief Physics Asset Editor Window
 * @details SSplitter 기반 레이아웃으로 Physics Asset 편집 지원
 *          - Viewport Panel: 3D 프리뷰 (Bodies, Constraints 시각화)
 *          - Body List Panel: Physics Bodies 및 Constraints 목록
 *          - Properties Panel: 선택된 Body/Constraint 속성 편집
 */
class SPhysicsAssetEditorWindow : public SWindow
{
public:
    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice, bool bEmbedded = false);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // Embedded 모드 (DynamicEditorWindow 내부에서 사용)
    void RenderEmbedded(const FRect& ContentRect);
    void SetEmbeddedMode(bool bEmbedded) { bIsEmbeddedMode = bEmbedded; }
    bool IsEmbeddedMode() const { return bIsEmbeddedMode; }

    // Viewport rendering
    void OnRenderViewport();

    // Asset 로드
    void LoadSkeletalMesh(const FString& Path);
    void LoadPhysicsAsset(const FString& Path);

    // Accessors
    FViewport* GetViewport() const;
    FViewportClient* GetViewportClient() const;
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
    bool IsFocused() const { return bIsFocused; }
    bool ShouldBlockEditorInput() const { return bIsOpen && bIsFocused; }

    PhysicsAssetViewerState* GetActiveState() const { return ActiveState; }
    UWorld* GetWorld() const { return World; }
    ID3D11Device* GetDevice() const { return Device; }

    // 뷰포트 영역 (입력 처리용)
    FRect GetViewportRect() const { return ViewportRect; }

    // Render Target
    void UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight);
    void RenderToPreviewRenderTarget();
    ID3D11ShaderResourceView* GetPreviewShaderResourceView() const { return PreviewShaderResourceView; }

    // delete 전 호출하여 리소스 안전하게 해제 (ImGui 지연 렌더링 문제 방지)
    void PrepareForDelete();

    // Physics Simulation
    void StartSimulation();
    void StopSimulation();
    bool IsSimulating() const;

private:
    // UI Panels (Legacy - 독립 윈도우 모드용)
    void RenderToolbar();
    void RenderViewportPanel();
    void RenderBodyListPanel();
    void RenderSkeletonTreePanel();
    void RenderPropertiesPanel();

    // State
    PhysicsAssetViewerState* ActiveState = nullptr;
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;
    bool bIsOpen = true;
    bool bIsFocused = false;
    bool bInitialPlacementDone = false;
    bool bIsEmbeddedMode = false;

    // SSplitter 레이아웃
    // MainSplitter (H): Left(Viewport) | Right(RightSplitter)
    //   RightSplitter (V): Top(BodyList) | Bottom(Properties)
    SSplitterH* MainSplitter = nullptr;
    SSplitterV* RightSplitter = nullptr;

    // 패널들
    SPhysicsAssetToolbarPanel* ToolbarPanel = nullptr;
    SPhysicsAssetViewportPanel* ViewportPanelWidget = nullptr;
    SPhysicsAssetBodyListPanel* BodyListPanel = nullptr;
    SPhysicsAssetPropertiesPanel* PropertiesPanelWidget = nullptr;

    // 뷰포트 영역 캐시 (실제 3D 렌더링 영역)
    FRect ViewportRect;

    // Render Target
    ID3D11Texture2D* PreviewRenderTargetTexture = nullptr;
    ID3D11RenderTargetView* PreviewRenderTargetView = nullptr;
    ID3D11ShaderResourceView* PreviewShaderResourceView = nullptr;
    ID3D11Texture2D* PreviewDepthStencilTexture = nullptr;
    ID3D11DepthStencilView* PreviewDepthStencilView = nullptr;
    uint32 PreviewRenderTargetWidth = 0;
    uint32 PreviewRenderTargetHeight = 0;

    void CreateRenderTarget(uint32 Width, uint32 Height);
    void ReleaseRenderTarget();
};

// ============================================================================
// Panel Classes
// ============================================================================

/**
 * @brief Physics Asset 툴바 패널
 */
class SPhysicsAssetToolbarPanel : public SWindow
{
public:
    SPhysicsAssetToolbarPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
};

/**
 * @brief Physics Asset 뷰포트 패널
 */
class SPhysicsAssetViewportPanel : public SWindow
{
public:
    SPhysicsAssetViewportPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;

    // 3D 콘텐츠 렌더링 영역
    FRect ContentRect;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
};

// Forward declarations for body list panel
class UPhysicsAsset;
struct FSkeleton;

/**
 * @brief Physics Asset Body/Constraint 목록 패널
 */
class SPhysicsAssetBodyListPanel : public SWindow
{
public:
    SPhysicsAssetBodyListPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
    void RenderBodyTree(PhysicsAssetViewerState* State);
    void RenderConstraintList(PhysicsAssetViewerState* State);
    void RenderSkeletonBodyTree(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, const FSkeleton* Skeleton);
    void RenderBodySetupList(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset);
    void RenderBodyShapes(PhysicsAssetViewerState* State, UPhysicsAsset* PhysAsset, int32 BodyIndex);
};

/**
 * @brief Physics Asset Properties 패널
 */
class SPhysicsAssetPropertiesPanel : public SWindow
{
public:
    SPhysicsAssetPropertiesPanel(SPhysicsAssetEditorWindow* InOwner);
    virtual void OnRender() override;

private:
    SPhysicsAssetEditorWindow* Owner = nullptr;
    void RenderBodyProperties(PhysicsAssetViewerState* State);
    void RenderShapeProperties(PhysicsAssetViewerState* State);
    void RenderConstraintProperties(PhysicsAssetViewerState* State);
};
