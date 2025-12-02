#pragma once
#include "Object.h"
#include "Windows/Window.h"
#include "Windows/SplitterV.h"
#include "Windows/SplitterH.h"
#include "Windows/ViewportWindow.h"
#include "Windows/DynamicEditorWindow.h"
#include "Windows/ParticleEditorWindow.h"
#include "Windows/SPhysicsAssetEditorWindow.h"

class SSceneIOWindow;
class SDetailsWindow;
class UMainToolbarWidget;
class UConsoleWindow;
class UContentBrowserWindow;

class USlateManager : public UObject
{
public:
    DECLARE_CLASS(USlateManager, UObject)

    static USlateManager& GetInstance();

    void SaveSplitterConfig();
    void LoadSplitterConfig();

    USlateManager();
    virtual ~USlateManager() override;

    USlateManager(const USlateManager&) = delete;
    USlateManager& operator=(const USlateManager&) = delete;

    void Initialize(ID3D11Device* Device, UWorld* World, const FRect& InRect);
    void SwitchLayout(EViewportLayoutMode NewMode);
    EViewportLayoutMode GetCurrentLayoutMode() const { return CurrentMode; }

    void SwitchPanel(SWindow* SwitchPanel);

    void Render();
    void RenderAfterUI();
    void Update(float deltaSecond);
    void ProcessInput();

    void OnMouseMove(FVector2D MousePos);
    void OnMouseDown(FVector2D MousePos, uint32 Button);
    void OnMouseUp(FVector2D MousePos, uint32 Button);

    void OnShutdown();
    void Shutdown();

    static SViewportWindow* ActiveViewport;

    void SetRect(const FRect& InRect) { Rect = InRect; }
    const FRect& GetRect() const { return Rect; }

    void SetWorld(UWorld* InWorld) { World = InWorld; }
    void SetPIEWorld(UWorld* InWorld);

    void ToggleConsole();
    bool IsConsoleVisible() const { return bIsConsoleVisible; }
    void ForceOpenConsole();

    void ToggleContentBrowser();
    void OpenContentBrowser(const FString& InitialPath = "");
    bool IsContentBrowserVisible() const;

    void OpenDynamicEditor();
    void OpenDynamicEditorWithFile(const char* FilePath);
    void OpenDynamicEditorWithAnimation(const char* AnimFilePath);
    void CloseDynamicEditor();
    bool IsDynamicEditorOpen() const { return DynamicEditorWindow != nullptr; }
    SDynamicEditorWindow* GetDynamicEditorWindow() const { return DynamicEditorWindow; }

    void OpenSkeletalMeshViewer() { OpenDynamicEditor(); }
    void OpenSkeletalMeshViewerWithFile(const char* FilePath) { OpenDynamicEditorWithFile(FilePath); }
    void CloseSkeletalMeshViewer() { CloseDynamicEditor(); }
    bool IsSkeletalMeshViewerOpen() const
    {
        return DynamicEditorWindow != nullptr &&
               DynamicEditorWindow->GetEditorMode() == EEditorMode::Skeletal;
    }

    void OpenBlendSpace2DEditor(UBlendSpace2D* BlendSpace = nullptr);
    void CloseBlendSpace2DEditor();
    bool IsBlendSpace2DEditorOpen() const
    {
        return DynamicEditorWindow != nullptr &&
               DynamicEditorWindow->GetEditorMode() == EEditorMode::BlendSpace2D;
    }

    void OpenAnimStateMachineWindow();
    void OpenAnimStateMachineWindowWithFile(const char* FilePath);
    void CloseAnimStateMachineWindow();
    bool IsAnimStateMachineWindowOpen() const
    {
        return DynamicEditorWindow != nullptr &&
               DynamicEditorWindow->GetEditorMode() == EEditorMode::AnimGraph;
    }

    void OpenParticleEditorWindow();
    void OpenParticleEditorWindowWithSystem(class UParticleSystem* System);
    void OpenParticleEditorWindowWithFile(const char* FilePath);
    void CloseParticleEditorWindow();
    bool IsParticleEditorWindowOpen() const { return ParticleEditorWindow != nullptr; }

    void OpenPhysicsAssetEditorWindow();
    void ClosePhysicsAssetEditorWindow();
    bool IsPhysicsAssetEditorWindowOpen() const;

    void RequestSceneLoad(const FString& ScenePath);

    // Viewport layout animation
    void StartLayoutAnimation(bool bSingleToQuad, int32 ViewportIndex = -1);
    bool IsLayoutAnimating() const { return ViewportAnimation.bIsAnimating; }

private:
    FRect Rect;

    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    SSplitter* FourSplitLayout = nullptr;
    SSplitter* SingleLayout = nullptr;

    SViewportWindow* Viewports[4];
    SViewportWindow* MainViewport;

    SSplitterV* LeftTop;
    SSplitterV* LeftBottom;

    SWindow* ControlPanel = nullptr;
    SWindow* DetailPanel = nullptr;

    SSplitterH* TopPanel = nullptr;
    SSplitterH* LeftPanel = nullptr;
    SSplitterV* RightPanel = nullptr;

    EViewportLayoutMode CurrentMode = EViewportLayoutMode::FourSplit;

    UMainToolbarWidget* MainToolbar;

    UConsoleWindow* ConsoleWindow = nullptr;
    bool bIsConsoleVisible = false;
    bool bIsConsoleAnimating = false;
    bool bConsoleShouldFocus = false;
    float ConsoleAnimationProgress = 0.0f;
    const float ConsoleAnimationDuration = 0.25f;
    float ConsoleHeight = 300.0f;
    const float ConsoleMinHeight = 150.0f;
    const float ConsoleMaxHeightRatio = 0.8f;
    const float ConsoleHorizontalMargin = 10.0f;
    const float ConsoleStatusBarHeight = 6.0f;
    bool bIsConsoleDragging = false;
    float ConsoleDragStartY = 0.0f;
    float ConsoleDragStartHeight = 0.0f;

    SDynamicEditorWindow* DynamicEditorWindow = nullptr;
    bool bPendingCloseDynamicEditor = false;

    SParticleEditorWindow* ParticleEditorWindow = nullptr;

    UContentBrowserWindow* ContentBrowserWindow = nullptr;
    bool bIsContentBrowserVisible = false;
    bool bIsContentBrowserAnimating = false;
    bool bRequestContentBrowserFocus = false;
    float ContentBrowserAnimationProgress = 0.0f;
    const float ContentBrowserAnimationDuration = 0.25f;
    const float ContentBrowserHeightRatio = 0.35f;
    const float ContentBrowserHorizontalMargin = 10.0f;

    bool bIsShutdown = false;
    bool bWasLoadingLastFrame = false;

    void CreateAnimStateMachineWindowIfNeeded();

    // Layout transition animation helpers
    void UpdateViewportAnimation(float DeltaSeconds);
    static float EaseInOutCubic(float InT);
    void FinalizeSingleLayoutFromAnimation();
    void FinalizeFourSplitLayoutFromAnimation();
    void SyncRectsToViewports() const;

    // Viewport animation state
    struct FViewportAnimation
    {
        bool bIsAnimating = false;
        float AnimationTime = 0.0f;
        float AnimationDuration = 0.2f;
        bool bSingleToQuad = true;
        int32 PromotedViewportIndex = 0;

        float StartHRatio = 0.5f;
        float TargetHRatio = 0.5f;
        float StartVRatioTop = 0.5f;
        float TargetVRatioTop = 0.5f;
        float StartVRatioBottom = 0.5f;
        float TargetVRatioBottom = 0.5f;

        int32 SavedMinChildSizeH = 4;
        int32 SavedMinChildSizeVTop = 4;
        int32 SavedMinChildSizeVBottom = 4;
    };
    FViewportAnimation ViewportAnimation;

    float SavedHRatio = 0.5f;
    float SavedVRatioTop = 0.5f;
    float SavedVRatioBottom = 0.5f;
    // Currently promoted viewport index (for Single mode)
    int32 CurrentPromotedViewportIndex = 0;
    // Unified splitter control (Unreal-style)
    bool bIsDraggingCenterCross = false;     // Center cross drag state
    bool bIsDraggingVerticalLine = false;    // Vertical line drag (synced LeftTop+LeftBottom)
    FVector2D SplitterDragStartPos;          // Drag start position
    float SplitterDragStartHRatio = 0.5f;    // LeftPanel ratio at drag start
    float SplitterDragStartVRatio = 0.5f;    // Vertical ratio at drag start

    // Splitter helper functions
    FRect GetCenterCrossRect() const;
    FRect GetVerticalLineRect() const;
    bool IsMouseOnCenterCross(FVector2D MousePos) const;
    bool IsMouseOnVerticalLine(FVector2D MousePos) const;
    void SyncVerticalSplitters();            // Sync LeftTop and LeftBottom ratios
};
