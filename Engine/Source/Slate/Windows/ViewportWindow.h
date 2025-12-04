#pragma once
#include "Window.h"
#include "Enums.h"

class FViewport;
class FViewportClient;
class UTexture;
class SViewportToolbarWidget;

class SViewportWindow : public SWindow
{
public:
    SViewportWindow();
    virtual ~SViewportWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType);

    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;

    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    FViewport* GetViewport() const { return Viewport; }
    FViewportClient* GetViewportClient() const { return ViewportClient; }
    void SetVClientWorld(UWorld* InWorld);

    // 뷰포트 타입 전환 (ViewportToolbarWidget에서 호출)
    void SwitchToViewportType(EViewportType NewType, const char* NewName);
    EViewportType GetViewportType() const { return ViewportType; }
    const char* GetViewportName() const { return ViewportName.ToString().c_str(); }

    // 선택된 액터에 카메라 포커싱
    void FocusOnSelectedActor();
    void UpdateCameraAnimation(float DeltaTime);

    // Scene 로드 모달 요청
    void RequestSceneLoad(const FString& ScenePath)
    {
        bShowSceneLoadModal = true;
        PendingScenePath = ScenePath;
    }

private:
    void RenderToolbar();
    void LoadToolbarIcons(ID3D11Device* Device);

    // 드래그 앤 드롭 처리
    void HandleDropTarget();
    void SpawnActorFromFile(const char* FilePath, const FVector& WorldLocation);

private:
    FViewport* Viewport = nullptr;
    FViewportClient* ViewportClient = nullptr;

    EViewportType ViewportType;
    FName ViewportName;

    bool bIsActive;
    bool bIsMouseDown;

    // 공용 툴바 위젯
    SViewportToolbarWidget* ViewportToolbar = nullptr;

    // 카메라 옵션 드롭다운 아이콘 (ViewportWindow 전용)
    UTexture* IconCamera = nullptr;
    UTexture* IconPerspective = nullptr;
    UTexture* IconTop = nullptr;
    UTexture* IconBottom = nullptr;
    UTexture* IconLeft = nullptr;
    UTexture* IconRight = nullptr;
    UTexture* IconFront = nullptr;
    UTexture* IconBack = nullptr;
    UTexture* IconFOV = nullptr;
    UTexture* IconNearClip = nullptr;
    UTexture* IconFarClip = nullptr;

    // 뷰포트 레이아웃 전환 아이콘 (ViewportWindow 전용)
    UTexture* IconSingleToMultiViewport = nullptr;
    UTexture* IconMultiToSingleViewport = nullptr;

    // Scene 로드 확인 모달
    bool bShowSceneLoadModal = false;
    FString PendingScenePath;

    // 카메라 포커싱 애니메이션
    bool bIsCameraAnimating = false;
    float CameraAnimationTime = 0.0f;
    FVector CameraStartLocation;
    FVector CameraTargetLocation;
    static constexpr float CAMERA_ANIMATION_DURATION = 0.35f;
};
