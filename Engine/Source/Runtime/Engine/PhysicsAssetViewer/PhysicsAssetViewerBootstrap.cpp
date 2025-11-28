#include "pch.h"
#include "PhysicsAssetViewerBootstrap.h"
#include "PhysicsAssetViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"

PhysicsAssetViewerState* PhysicsAssetViewerBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    PhysicsAssetViewerState* State = new PhysicsAssetViewerState();
    State->Name = Name ? Name : "PhysicsAsset";

    // Preview world
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    // Viewport + Client
    State->Viewport = new FViewport();
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->SetPickingEnabled(false);
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);
    State->World->SetEditorCameraActor(Client->GetCamera());

    // Preview Actor
    ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
    if (Preview)
    {
        Preview->SetTickInEditor(true);
    }
    State->PreviewActor = Preview;

    return State;
}

void PhysicsAssetViewerBootstrap::DestroyViewerState(PhysicsAssetViewerState*& State)
{
    if (!State) return;

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State;
    State = nullptr;
}
