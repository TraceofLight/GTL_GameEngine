#include "pch.h"
#include "CameraComponent.h"
#include "FViewport.h"
#include "PlayerCameraManager.h"
#include "StaticMeshComponent.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//
//BEGIN_PROPERTIES(UCameraComponent)
//	MARK_AS_COMPONENT("카메라 컴포넌트", "카메라를 렌더링하는 컴포넌트입니다.")
//	ADD_PROPERTY_RANGE(float, FieldOfView, "Camera", 1.0f, 179.0f, true, "시야각 (FOV, Degrees)입니다.")
//	ADD_PROPERTY_RANGE(float, AspectRatio, "Camera", 0.1f, 10.0f, true, "화면 비율입니다.")
//	ADD_PROPERTY_RANGE(float, NearClip, "Camera", 0.01f, 1000.0f, true, "근거리 클리핑 평면입니다.")
//	ADD_PROPERTY_RANGE(float, FarClip, "Camera", 1.0f, 100000.0f, true, "원거리 클리핑 평면입니다.")
//	ADD_PROPERTY_RANGE(float, ZoomFactor, "Camera", 0.1f, 10.0f, true, "줌 배율입니다.")
//END_PROPERTIES()

UCameraComponent::UCameraComponent()
    : FieldOfView(90.0f)
    , AspectRatio(1.0f / 1.0f)
    , NearClip(0.1f)
    , FarClip(50000.0f)
    , ProjectionMode(ECameraProjectionMode::Perspective)
    , ZoomFactor(1.0f)
    , OrthoZoom(0.1f)
{
}

UCameraComponent::~UCameraComponent() {}

void UCameraComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    if (InWorld)
    {
        if (APlayerCameraManager* PlayerCameraManager = InWorld->GetPlayerCameraManager())
        {
            // 만약 현재 월드에 카메라가 없었으면 이 카메라가 View로 등록됨
            PlayerCameraManager->RegisterView(this);
        }

        // Create Direction Gizmo if not already created
        if (!CameraGizmo && !InWorld->bPie)
        {
            CREATE_EDITOR_COMPONENT(CameraGizmo, UStaticMeshComponent);

            // Set gizmo mesh (using the same mesh as GizmoActor's arrow)
            CameraGizmo->SetStaticMesh(GDataDir + "/Default/Gizmo/Camera.obj");
            CameraGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

            // Set default scale
            CameraGizmo->SetWorldScale(FVector(0.2f, 0.2f, 0.2f));
        }
    }
}

void UCameraComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        // Shutdown 시 World가 파괴 중이면 Unregister 스킵
        if (!World->IsTearingDown())
        {
            if (APlayerCameraManager* PlayerCameraManager = World->GetPlayerCameraManager())
            {
                // 만약 이 카메라를 뷰로 사용 중이었다면 해제
                PlayerCameraManager->UnregisterView(this);
            }
        }
    }

    Super::OnUnregister();
}

FMatrix UCameraComponent::GetViewMatrix() const
{
    // View 행렬을 Y-Up에서 Z-Up으로 변환하기 위한 행렬
    static const FMatrix YUpToZUp(
        0, 1, 0, 0,
        0, 0, 1, 0,
        1, 0, 0, 0,
        0, 0, 0, 1
    );

	const FMatrix World = GetWorldTransform().ToMatrix();

	return (YUpToZUp * World).InverseAffine();
}


FMatrix UCameraComponent::GetProjectionMatrix() const
{
    // 기본 구현은 전체 클라이언트 aspect ratio 사용
    float aspect = CLIENTWIDTH / CLIENTHEIGHT;
    return GetProjectionMatrix(aspect);
}

FMatrix UCameraComponent::GetProjectionMatrix(float ViewportAspectRatio) const
{
    if (ProjectionMode == ECameraProjectionMode::Perspective)
    {
        return FMatrix::PerspectiveFovLH(FieldOfView * (PI / 180.0f),
            ViewportAspectRatio,
            NearClip, FarClip);
    }
    else
    {
        // Viewport 없이는 기본 크기 사용
        float orthoWidth = 100.0f * OrthoZoom;
        float orthoHeight = orthoWidth / ViewportAspectRatio;

        return FMatrix::OrthoLH(orthoWidth, orthoHeight,
            NearClip, FarClip);
    }
}
FMatrix UCameraComponent::GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const
{
    if (ProjectionMode == ECameraProjectionMode::Perspective)
    {
        return FMatrix::PerspectiveFovLH(
            DegreesToRadians(FieldOfView),
            ViewportAspectRatio,
            NearClip, FarClip);
    }
    else
    {
        // 뷰포트 크기 기반 Orthographic (언리얼 방식)
        // OrthoZoom = 픽셀당 월드 유닛 (값이 클수록 줌아웃)
        float orthoWidth = Viewport->GetSizeX() * OrthoZoom;
        float orthoHeight = Viewport->GetSizeY() * OrthoZoom;

        return FMatrix::OrthoLH(
            orthoWidth,
            orthoHeight,
            NearClip, FarClip);
    }
}

FVector UCameraComponent::GetForward() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(1, 0, 0)).GetNormalized();
}

FVector UCameraComponent::GetRight() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(0, 1, 0)).GetNormalized();
}

FVector UCameraComponent::GetUp() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(0, 0, 1)).GetNormalized();
}

void UCameraComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    ProjectionMode = ECameraProjectionMode::Perspective;
}

void UCameraComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    // ProjectionMode는 수동 직렬화 (Enum 타입)
    if (bInIsLoading)
    {
        int32 ModeInt = static_cast<int32>(ECameraProjectionMode::Perspective);
        FJsonSerializer::ReadInt32(InOutHandle, "ProjectionMode", ModeInt, 0);
        ProjectionMode = static_cast<ECameraProjectionMode>(ModeInt);
    }
    else
    {
        InOutHandle["ProjectionMode"] = static_cast<int32>(ProjectionMode);
    }
}
