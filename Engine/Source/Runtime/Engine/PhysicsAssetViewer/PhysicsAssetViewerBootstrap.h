#pragma once

class PhysicsAssetViewerState;
class UWorld;
struct ID3D11Device;

class PhysicsAssetViewerBootstrap
{
public:
    static PhysicsAssetViewerState* CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice);
    static void DestroyViewerState(PhysicsAssetViewerState*& State);
};
