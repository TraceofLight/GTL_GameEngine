#include "pch.h"
#include "SkeletalViewerBootstrap.h"
#include "CameraActor.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Collision/AABB.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/Gizmo/GizmoActor.h"

ViewerState* SkeletalViewerBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    ViewerState* State = new ViewerState();
    State->Name = Name ? Name : "Viewer";

    // Preview world 만들기
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);  // Set as preview world for memory optimization
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    AGizmoActor* Gizmo = State->World->GetGizmoActor();
    Gizmo->SetSpace(EGizmoSpace::Local);
    Gizmo->SetMode(EGizmoMode::Translate);
    Gizmo->SetbRender(false);

    // Viewport + client per tab
    State->Viewport = new FViewport();
    // 프레임 마다 initial size가 바꿜 것이다
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->SetPickingEnabled(false);  // 플로팅 윈도우에서는 피킹 비활성화
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    Gizmo->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor (mesh can be set later from UI)
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        if (Preview)
        {
            // Enable tick in editor for preview world
            Preview->SetTickInEditor(true);

            // Preview World에서는 BeginPlay가 호출되지 않으므로 명시적으로 Delegate 등록
            Preview->RegisterAnimNotifyDelegate();
        }
        State->PreviewActor = Preview;

        // 바닥판 액터 생성
        State->FloorActor = CreateFloorActor(State->World);
    }

    return State;
}

void SkeletalViewerBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}

AStaticMeshActor* SkeletalViewerBootstrap::CreateFloorActor(UWorld* InWorld)
{
    if (!InWorld)
    {
        return nullptr;
    }

    AStaticMeshActor* Floor = InWorld->SpawnActor<AStaticMeshActor>();
    if (Floor && Floor->GetStaticMeshComponent())
    {
        Floor->GetStaticMeshComponent()->SetStaticMesh("Data/Default/StaticMesh/Cube.obj");
        Floor->GetStaticMeshComponent()->SetVisibility(false);
    }
    return Floor;
}

void SkeletalViewerBootstrap::SetupFloorAndCamera(ASkeletalMeshActor* PreviewActor, AStaticMeshActor* FloorActor, FViewportClient* Client)
{
    if (!PreviewActor)
    {
        return;
    }

    USkeletalMeshComponent* SkelComp = PreviewActor->GetSkeletalMeshComponent();
    if (!SkelComp)
    {
        return;
    }

    FAABB Bounds = SkelComp->GetWorldAABB();
    FVector Center = Bounds.GetCenter();
    FVector HalfExtent = Bounds.GetHalfExtent();

    float MaxExtent = std::max({ HalfExtent.X, HalfExtent.Y, HalfExtent.Z });
    if (MaxExtent < 0.001f)
    {
        MaxExtent = 1.0f;
        Center = FVector(0, 0, 0);
        HalfExtent = FVector(1, 1, 1);
    }

    // 바닥판 설정
    if (FloorActor)
    {
        float FloorSize = std::max(HalfExtent.X, HalfExtent.Y) * 10.0f;
        FloorSize = std::max(FloorSize, 5.0f);
        float FloorThickness = 0.2f;

        FVector FloorPos(0.0f, 0.0f, -FloorThickness * 0.5f + 0.1f);

        FloorActor->SetActorLocation(FloorPos);
        FloorActor->SetActorScale(FVector(FloorSize, FloorSize, FloorThickness));

        if (UStaticMeshComponent* FloorMesh = FloorActor->GetStaticMeshComponent())
        {
            FloorMesh->SetVisibility(true);
            FloorMesh->SetMaterial(0, nullptr);
        }
    }

    // 카메라 거리 조정
    if (Client)
    {
        ACameraActor* Camera = Client->GetCamera();
        if (Camera)
        {
            float BoundingRadius = HalfExtent.Size();
            float MinDistance = 3.0f;
            float DesiredDistance = std::max(MinDistance, BoundingRadius * 1.0f);

            FVector CameraPos(DesiredDistance, 0.0f, DesiredDistance * 0.67f);
            Camera->SetActorLocation(CameraPos);

            FVector ToCenter = Center - CameraPos;
            float PitchRad = std::atan2(-ToCenter.Z, std::sqrt(ToCenter.X * ToCenter.X + ToCenter.Y * ToCenter.Y));
            float PitchDeg = RadiansToDegrees(PitchRad);

            // 카메라가 +X에서 원점(-X 방향)을 바라보므로 Yaw=180
            Camera->SetActorRotation(FVector(0.0f, PitchDeg, 180.0f));
            Camera->SetCameraPitch(PitchDeg);
            Camera->SetCameraYaw(180.0f);
        }
    }
}
