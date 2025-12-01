#include "pch.h"
#include "GizmoScaleComponent.h"

IMPLEMENT_CLASS(UGizmoScaleComponent)

UGizmoScaleComponent::UGizmoScaleComponent()
{
    SetStaticMesh(GDataDir + "/Default/Gizmo/ScaleHandle.obj");
    // 기즈모 셰이더로 설정
    GizmoMaterial = UResourceManager::GetInstance().Load<UMaterial>("Shaders/UI/Gizmo.hlsl");
}

UGizmoScaleComponent::~UGizmoScaleComponent()
{
}

void UGizmoScaleComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
