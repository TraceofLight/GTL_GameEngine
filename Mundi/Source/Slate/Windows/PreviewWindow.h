#pragma once
#include "SWindow.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;

class SPreviewWindow : public SWindow
{
public:
    SPreviewWindow();
    virtual ~SPreviewWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void OnRenderViewport();

    // Accessors (active tab)
    FViewport* GetViewport() const { return ActiveState ? ActiveState->Viewport : nullptr; }
    FViewportClient* GetViewportClient() const { return ActiveState ? ActiveState->Client : nullptr; }

    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(const FString& Path);

private:
    // Tabs
    void OpenNewTab(const char* Name = "Viewer");
    void CloseTab(int Index);

private:
    // Per-tab state
    ViewerState* ActiveState = nullptr;
    TArray<ViewerState*> Tabs;
    int ActiveTabIndex = -1;

    // For legacy single-state flows; removed once tabs are stable
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    // Layout state
    float LeftPanelRatio = 0.25f;   // 25% of width
    float RightPanelRatio = 0.25f;  // 25% of width

    // Cached center region used for viewport sizing and input mapping
    FRect CenterRect;

    // Whether we've applied the initial ImGui window placement
    bool bInitialPlacementDone = false;

    // Request focus on first open
    bool bRequestFocus = false;

    // Window open state
    bool bIsOpen = true;

    // Timeline 아이콘
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

public:
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }

private:
    void UpdateBoneTransformFromSkeleton(ViewerState* State);
    void ApplyBoneTransform(ViewerState* State);
    void ExpandToSelectedBone(ViewerState* State, int32 BoneIndex);

    // Timeline 컨트롤 UI 렌더링
    void RenderTimelineControls(ViewerState* State);
    void RenderTimeline(ViewerState* State);

    // Timeline 컨트롤 기능
    void TimelineToFront(ViewerState* State);
    void TimelineToPrevious(ViewerState* State);
    void TimelineReverse(ViewerState* State);
    void TimelineRecord(ViewerState* State);
    void TimelinePlay(ViewerState* State);
    void TimelineToNext(ViewerState* State);
    void TimelineToEnd(ViewerState* State);

    // Timeline 헬퍼: 프레임 변경 시 공통 갱신 로직
    void RefreshAnimationFrame(ViewerState* State);

    // Timeline 렌더링 헬퍼
    void DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, ViewerState* State);
    void DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, ViewerState* State);
    void DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime);
    void DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, ViewerState* State);
    void DrawNotifyTracksPanel(ViewerState* State, float StartTime, float EndTime);

    // Notify Track 복원 (애니메이션 로드 시)
    void RebuildNotifyTracks(ViewerState* State);

    // Notify 라이브러리 관리
    void ScanNotifyLibrary();
    void CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState);
    void OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState);
    TArray<FString> AvailableNotifyClasses;
    TArray<FString> AvailableNotifyStateClasses;

    // New Notify Script 다이얼로그 상태
    bool bShowNewNotifyDialog = false;
    bool bShowNewNotifyStateDialog = false;
    char NewNotifyNameBuffer[128] = "";

    // 전용 렌더 타겟 (스켈레탈 메쉬 프리뷰용)
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
