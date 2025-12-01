#pragma once
class ViewerState;
class UWorld;
class AStaticMeshActor;
class ASkeletalMeshActor;
class FViewportClient;
struct ID3D11Device;

// Minimal bootstrap helpers to construct/destroy per-tab viewer state.
class SkeletalViewerBootstrap
{
public:
    static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyViewerState(ViewerState*& State);

    // 공통 유틸리티 함수들
    // 바닥판 액터 생성
    static AStaticMeshActor* CreateFloorActor(UWorld* InWorld);

    // 바닥판 및 카메라 설정
    static void SetupFloorAndCamera(ASkeletalMeshActor* PreviewActor, AStaticMeshActor* FloorActor, FViewportClient* Client);
};
