#include "pch.h"
#include "GizmoRotateComponent.h"

IMPLEMENT_CLASS(UGizmoRotateComponent)

UGizmoRotateComponent::UGizmoRotateComponent()
{
    SetStaticMesh(GDataDir + "/Default/Gizmo/RotationHandle.obj");
    // 기즈모 셰이더로 설정
    GizmoMaterial = UResourceManager::GetInstance().Load<UMaterial>("Shaders/UI/Gizmo.hlsl");
}

UGizmoRotateComponent::~UGizmoRotateComponent()
{
}

void UGizmoRotateComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
