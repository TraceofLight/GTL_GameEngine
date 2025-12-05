#pragma once

#include "Actor.h"
#include "ACameraActor.generated.h"
class UCameraComponent;
class UUIManager;
class UInputManager;

UCLASS(DisplayName="카메라", Description="씬을 렌더링할 카메라 액터입니다")
class ACameraActor : public AActor
{
public:

    GENERATED_REFLECTION_BODY()

    ACameraActor();
   
    virtual void Tick(float DeltaSeconds) override;

    // 씬 로드 등 외부에서 카메라 각도를 즉시 세팅하고
    // 입력 경로와 동일한 방식으로 트랜스폼을 재조립
    void SetAnglesImmediate(float InPitchDeg, float InYawDeg);
    void SetRotationFromEulerAngles(FVector InAngles);

    // 선택: 스무딩/보간 캐시가 있다면 여기서 동기화
    void SyncRotationCache();

    void SetPerspectiveCameraInput(bool InPerspectiveCameraInput);

protected:
    ~ACameraActor() override;

public:
    UCameraComponent* GetCameraComponent() const { return CameraComponent; }
    
    // Matrices
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio) const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const;
    FMatrix GetViewProjectionMatrix() const;

    // Directions (world)
    void SetForward(FVector InForward);
    FVector GetForward() const;
    FVector GetRight() const;
    FVector GetUp() const;

    // Camera control methods
    void SetMouseSensitivity(float Sensitivity) { MouseSensitivity = Sensitivity; }
    void SetMoveSpeed(float Speed) { BaseCameraSpeed = Speed; }

    // Camera state getters
    float GetCameraYaw() const { return CameraYawDeg; }
    float GetCameraPitch() const { return CameraPitchDeg; }
    void SetCameraYaw(float Yaw) { CameraYawDeg = Yaw; }
    void SetCameraPitch(float Pitch) { CameraPitchDeg = Pitch; }

    // 전역 베이스 스피드 (모든 카메라가 공유, EditorINI에 저장)
    static float GetBaseCameraSpeed() { return BaseCameraSpeed; }
    static void SetBaseCameraSpeed(float InSpeed) { BaseCameraSpeed = InSpeed; EditorINI["CameraSpeed"] = std::to_string(BaseCameraSpeed); }

    // 전역 스피드 스칼라 (모든 카메라가 공유, 실시간 동기화)
    static float GetSpeedScalar() { return SpeedScalar; }
    static void SetSpeedScalar(float InScalar)
    {
        SpeedScalar = std::max(0.25f, std::min(128.0f, InScalar));
        EditorINI["CameraSpeedScalar"] = std::to_string(SpeedScalar);  // 마지막 값 저장
    }

    // 레거시 호환성 (기존 코드와의 호환)
    float GetCameraSpeed() { return BaseCameraSpeed * SpeedScalar; }
    void SetCameraSpeed(float InSpeed) { SetBaseCameraSpeed(InSpeed); }

    void ProcessEditorCameraInput(float DeltaSeconds);

    // ───── 직렬화 관련 ────────────────────────────
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

private:
    UCameraComponent* CameraComponent = nullptr;

    // Camera control parameters
    float MouseSensitivity = 0.1f;  // 기존 World에서 사용하던 값으로 조정
    static float BaseCameraSpeed;   // 전역 베이스 스피드 (모든 카메라 공유, EditorINI에 저장)
    static float SpeedScalar;       // 전역 스피드 스칼라 (모든 카메라 공유, 0.25 ~ 128.0)

    // Camera rotation state (cumulative)
    float CameraYawDeg = 0.0f;   // World Up(Y) based Yaw (unlimited accumulation)
    float CameraPitchDeg = 0.0f; // Local Right based Pitch (limited)

    bool PerspectiveCameraInput = false;
    

    // Camera input processing methods
    void ProcessCameraRotation(float DeltaSeconds);
    void ProcessCameraMovement(float DeltaSeconds);
};

