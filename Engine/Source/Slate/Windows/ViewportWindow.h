#pragma once
#include "Window.h"
#include "Enums.h"

class FViewport;
class FViewportClient;
class UTexture;

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
    void RenderGizmoModeButtons();
    void RenderGizmoSpaceButton();
    void RenderCameraOptionDropdownMenu();
    void RenderCameraSpeedButton();  // 카메라 속도 버튼
    void RenderViewModeDropdownMenu();
    void RenderShowFlagDropdownMenu();
    void RenderViewportLayoutSwitchButton();
    void LoadToolbarIcons(ID3D11Device* Device);

    // 카메라 속도 계산 헬퍼
    float CalculateCameraSpeed() const;
    void ApplyCameraSpeed();

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

    // ViewMode 관련 상태 저장
    int CurrentLitSubMode = 0; // 0=default(Phong) 1=Gouraud, 2=Lambert, 3=Phong [기본값: default(Phong)]
    int CurrentBufferVisSubMode = 1; // 0=SceneDepth, 1=WorldNormal (기본값: WorldNormal)

    // 툴바 아이콘 텍스처
    UTexture* IconSelect = nullptr;
    UTexture* IconMove = nullptr;
    UTexture* IconRotate = nullptr;
    UTexture* IconScale = nullptr;
    UTexture* IconWorldSpace = nullptr;
    UTexture* IconLocalSpace = nullptr;

    // 뷰포트 모드 아이콘 텍스처
    UTexture* IconCamera = nullptr;
    UTexture* IconPerspective = nullptr;
    UTexture* IconTop = nullptr;
    UTexture* IconBottom = nullptr;
    UTexture* IconLeft = nullptr;
    UTexture* IconRight = nullptr;
    UTexture* IconFront = nullptr;
    UTexture* IconBack = nullptr;

    // 뷰포트 설정 아이콘 텍스처
    UTexture* IconSpeed = nullptr;
    UTexture* IconFOV = nullptr;
    UTexture* IconNearClip = nullptr;
    UTexture* IconFarClip = nullptr;

    // 뷰모드 아이콘 텍스처
    UTexture* IconViewMode_Lit = nullptr;
    UTexture* IconViewMode_Unlit = nullptr;
    UTexture* IconViewMode_Wireframe = nullptr;
    UTexture* IconViewMode_BufferVis = nullptr;

    // ShowFlag 아이콘 텍스처
    UTexture* IconShowFlag = nullptr;
    UTexture* IconRevert = nullptr;
    UTexture* IconStats = nullptr;
    UTexture* IconHide = nullptr;
    UTexture* IconBVH = nullptr;
    UTexture* IconGrid = nullptr;
    UTexture* IconDecal = nullptr;
    UTexture* IconStaticMesh = nullptr;
    UTexture* IconSkeletalMesh = nullptr;
    UTexture* IconBillboard = nullptr;
    UTexture* IconEditorIcon = nullptr;
    UTexture* IconFog = nullptr;
    UTexture* IconCollision = nullptr;
    UTexture* IconAntiAliasing = nullptr;
    UTexture* IconTile = nullptr;
    UTexture* IconShadow = nullptr;
    UTexture* IconShadowAA = nullptr;
    UTexture* IconSkinning = nullptr;
    UTexture* IconParticle = nullptr;

    // 뷰포트 레이아웃 전환 아이콘
    UTexture* IconSingleToMultiViewport = nullptr;  // 단일 뷰포트 아이콘
    UTexture* IconMultiToSingleViewport = nullptr;   // 멀티 뷰포트 아이콘

    // Scene 로드 확인 모달
    bool bShowSceneLoadModal = false;
    FString PendingScenePath;

    // 카메라 포커싱 애니메이션
    bool bIsCameraAnimating = false;
    float CameraAnimationTime = 0.0f;
    FVector CameraStartLocation;
    FVector CameraTargetLocation;
    static constexpr float CAMERA_ANIMATION_DURATION = 0.35f;

    // 카메라 속도 설정 (UE 스타일)
    // SpeedSetting: 1-8 배율 (기본값 4)
    // SpeedScalar: ViewportClient에서 관리 (0.25 ~ 4.0, 휠로 조절 가능)
    int32 CameraSpeedSetting = 4;
    static constexpr float SPEED_MULTIPLIERS[8] = { 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
    static constexpr float BASE_CAMERA_SPEED = 10.0f;  // 기본 카메라 속도
};
