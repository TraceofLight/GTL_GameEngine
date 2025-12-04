#pragma once

class PhysicsAssetViewerState;
class UWorld;
class ASkeletalMeshActor;
class AStaticMeshActor;
class FViewportClient;
struct ID3D11Device;

class PhysicsAssetViewerBootstrap
{
public:
    static PhysicsAssetViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyViewerState(PhysicsAssetViewerState*& State);

    // 메쉬 로드 후 Floor와 카메라 설정 (SkeletalViewerBootstrap과 동일 패턴)
    static void SetupFloorAndCamera(ASkeletalMeshActor* PreviewActor, AStaticMeshActor* FloorActor, FViewportClient* Client);
};
