#pragma once

class UWorld;
class FViewport;
class FViewportClient;
class ASkeletalMeshActor;
class USkeletalMesh;
class UPhysicsAsset;

class PhysicsAssetViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Preview
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    UPhysicsAsset* PhysicsAsset = nullptr;

    // Selection
    int32 SelectedBodyIndex = -1;
    int32 SelectedConstraintIndex = -1;
    FName SelectedBoneName;

    // Debug Draw Options
    bool bShowBodies = true;
    bool bShowConstraints = true;
    bool bShowBoneNames = false;

    // Simulation
    bool bSimulating = false;
};
