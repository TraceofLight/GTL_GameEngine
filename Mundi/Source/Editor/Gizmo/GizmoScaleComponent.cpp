#include "pch.h"
#include "GizmoScaleComponent.h"

IMPLEMENT_CLASS(UGizmoScaleComponent)

UGizmoScaleComponent::UGizmoScaleComponent()
{
    SetStaticMesh(GDataDir + "/Default/Gizmo/ScaleHandle.obj");
    SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");
}

UGizmoScaleComponent::~UGizmoScaleComponent()
{
}

void UGizmoScaleComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}
