#pragma once
#include "Vector.h"
#include"Enums.h"

class FViewport;
class UWorld;
class UCameraComponent;


/**
 * @brief 뷰포트 클라이언트 - UE의 FViewportClient를 모방
 */
class FViewportClient
{
public:
    FViewportClient();
    virtual ~FViewportClient();

    // 렌더링
    virtual void Draw(FViewport* Viewport);
    virtual void Tick(float DeltaTime);

    // 입력 처리
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y);
    virtual void MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button);
    virtual void MouseWheel(float DeltaSeconds);
    virtual void KeyDown(FViewport* Viewport, int32 KeyCode) {}
    virtual void KeyUp(FViewport* Viewport, int32 KeyCode) {}

    // 뷰포트 설정
    void SetViewportType(EViewportType InType) { ViewportType = InType; }
    EViewportType GetViewportType() const { return ViewportType; }

    void SetWorld(UWorld* InWorld) { World = InWorld; }
    UWorld* GetWorld() const { return World; }

    void SetCamera(ACameraActor* InCamera) { Camera = InCamera; }
    ACameraActor* GetCamera() const { return Camera; }

    // 카메라 매트릭스 계산
    FMatrix GetViewMatrix() const;


    // 뷰포트별 카메라 설정
    void SetupCameraMode();
    void SetViewMode(EViewMode InViewModeIndex) { ViewMode = InViewModeIndex; }

    EViewMode GetViewMode() { return ViewMode;}

    // 피킹 활성화 / 비활성화 (플로팅 윈도우에서는 비활성화)
    void SetPickingEnabled(bool bEnabled) { bPickingEnabled = bEnabled; }
    bool IsPickingEnabled() const { return bPickingEnabled; }

    // 마우스 버튼 상태 (ImGui 기반 뷰포트에서 사용)
    bool IsMouseButtonDown() const { return bIsMouseButtonDown; }

protected:
    EViewportType ViewportType = EViewportType::Perspective;
    UWorld* World = nullptr;
    ACameraActor* Camera = nullptr;
    int32 MouseLastX{};
    int32 MouseLastY{};
    bool bIsMouseButtonDown = false;
    bool bIsMouseRightButtonDown = false;
    static FVector CameraAddPosition;


    // 직교 뷰용 카메라 설정
    uint32 OrthographicAddXPosition;
    uint32  OrthographicAddYPosition;
    float OrthographicZoom = 30.0f;
    //뷰모드
    EViewMode ViewMode = EViewMode::VMI_Lit_Phong;

    //원근 투영 기본값
    bool PerspectiveCameraInput = false;
    FVector PerspectiveCameraPosition = FVector(-5.0f, 5.0f, 5.0f);
    FVector PerspectiveCameraRotation = FVector(0.0f, 22.5f, -45.0f);
    float PerspectiveCameraFov = 90.0f;

    // 피킹 활성화 여부 (플로팅 윈도우에서는 false로 설정)
    bool bPickingEnabled = true;

public:
    // 에디터 DoF 설정 (에디터 뷰포트에서 사용)
    struct FEditorDoFSettings
    {
        bool bEnabled = false;
        int32 DoFMode = 0;  // 0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus, 4: ScreenPointFocus

        // 공통 파라미터
        float FocusDistance = 500.0f;
        float MaxBlurRadius = 8.0f;
        float BokehSize = 1.0f;

        // Cinematic 모드 파라미터 (선형 모델)
        float FocusRange = 100.0f;
        float NearBlurScale = 0.02f;
        float FarBlurScale = 0.01f;

        // Physical 모드 파라미터 (렌즈 물리학)
        float FocalLength = 50.0f;      // mm (35, 50, 85 등)
        float FNumber = 2.8f;           // F1.4, F2.8, F5.6 등
        float SensorWidth = 36.0f;      // mm (풀프레임: 36, APS-C: 23.6)

        // Tilt-Shift 모드 파라미터
        float TiltShiftCenterY = 0.5f;      // 선명한 띠의 중심 (0~1)
        float TiltShiftBandWidth = 0.3f;    // 선명한 띠의 너비 (0~1)
        float TiltShiftBlurScale = 5.0f;    // 블러 강도

        // PointFocus 모드 파라미터 (점 초점, 구형 초점 영역, World Space)
        FVector FocusPoint = FVector(0.0f, 0.0f, 0.0f);  // 초점 지점 (World Space)
        float FocusRadius = 2.0f;           // 초점 반경 (이 반경 내에서는 선명)
        float PointFocusBlurScale = 0.5f;   // 블러 강도 스케일
        float PointFocusFalloff = 1.0f;     // 블러 감쇠 (1=선형, 2=제곱)

        // ScreenPointFocus 모드 파라미터 (화면 좌표 + 깊이 기반, Screen Space)
        FVector2D ScreenFocusPoint = FVector2D(0.5f, 0.5f);  // 화면 좌표 (0~1, 0.5=중앙)
        float ScreenFocusRadius = 0.2f;         // 화면 상의 초점 반경 (0~1)
        float ScreenFocusDepthRange = 100.0f;   // 초점 깊이 범위
        float ScreenFocusBlurScale = 1.0f;      // 블러 강도 스케일
        float ScreenFocusFalloff = 1.0f;        // 블러 감쇠 곡선
        float ScreenFocusAspectRatio = 1.0f;    // 종횡비 보정 (1.0 = 원형, 다른 값 = 타원형)

        // 블러 방식 (0: Disc12, 1: Disc24, 2: Gaussian, 3: Hexagonal, 4: CircularGather)
        int32 BlurMethod = 0;
    } EditorDoFSettings;

    FEditorDoFSettings& GetEditorDoFSettings() { return EditorDoFSettings; }
};
