#include "pch.h"
#include "PhysicsAssetViewerBootstrap.h"
#include "PhysicsAssetViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsManager.h"
#include "Source/Editor/Gizmo/GizmoActor.h"

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

    // Physics Scene 생성 (시뮬레이션용)
    State->World->GetPhysicsSceneHandle() = PHYSICS.CreateScene();

    // Viewport + Client (SkeletalMeshViewer와 동일한 ViewportClient 사용)
    State->Viewport = new FViewport();
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->SetPickingEnabled(true);
    // 카메라 초기 위치/회전은 FSkeletalViewerViewportClient 생성자에서 설정됨

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);
    State->World->SetEditorCameraActor(Client->GetCamera());

    // Gizmo Actor
    AGizmoActor* Gizmo = State->World->SpawnActor<AGizmoActor>();
    if (Gizmo)
    {
        Gizmo->SetEditorCameraActor(Client->GetCamera());
    }
    State->GizmoActor = Gizmo;

    // Floor Actor (바닥) - 시뮬레이션 시작 시 Static Physics Body가 됨
    AStaticMeshActor* Floor = State->World->SpawnActor<AStaticMeshActor>();
    if (Floor && Floor->GetStaticMeshComponent())
    {
        Floor->GetStaticMeshComponent()->SetStaticMesh("Data/Default/StaticMesh/Cube.obj");
        Floor->GetStaticMeshComponent()->SetVisibility(true);

        // 바닥 위치 및 크기 설정 (넓고 얇은 평면)
        Floor->SetActorLocation(FVector(0.0f, 0.0f, -0.5f));
        Floor->SetActorScale(FVector(500.0f, 500.0f, 1.0f));
    }
    State->FloorActor = Floor;

    // Preview Actor (스켈레탈 메쉬)
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

    // Physics Scene 정리
    if (State->World && State->World->GetPhysicsScene())
    {
        PHYSICS.DestroyScene(State->World->GetPhysicsSceneHandle());
    }

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State;
    State = nullptr;
}
