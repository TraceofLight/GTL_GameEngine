#include "pch.h"
#include "PhysicsAssetViewerBootstrap.h"
#include "PhysicsAssetViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/DirectionalLightActor.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/DirectionalLightComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsManager.h"
#include "Source/Runtime/Engine/Collision/AABB.h"
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
    Client->SetPickingEnabled(false);  // PAE는 자체 피킹 로직 사용 (Shape/Constraint 피킹)
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
    // 초기에는 숨김, SetupFloorAndCamera에서 메쉬 로드 후 설정
    AStaticMeshActor* Floor = State->World->SpawnActor<AStaticMeshActor>();
    if (Floor && Floor->GetStaticMeshComponent())
    {
        Floor->GetStaticMeshComponent()->SetStaticMesh("Data/Default/StaticMesh/Cube.obj");
        Floor->GetStaticMeshComponent()->SetVisibility(false);
    }
    State->FloorActor = Floor;

    // Preview Actor (스켈레탈 메쉬)
    ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
    if (Preview)
    {
        Preview->SetTickInEditor(true);
    }
    State->PreviewActor = Preview;

    // Directional Light (그림자용)
    // 카메라가 +X에서 원점을 바라보므로 Light도 전면(+X 방향)에서 비치도록 설정
    ADirectionalLightActor* DirLight = State->World->SpawnActor<ADirectionalLightActor>();
    if (DirLight)
    {
        // 전면에서 약간 위에서 비스듬하게 (카메라 방향과 유사)
        // Pitch=-30 (위에서 아래로), Yaw=180 (+X에서 -X 방향으로)
        DirLight->SetActorRotation(FVector(-30.0f, 180.0f, 0.0f));

        UDirectionalLightComponent* LightComp = DirLight->GetLightComponent();
        if (LightComp)
        {
            LightComp->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f));  // 약간 따뜻한 백색
            LightComp->SetIntensity(3.0f);
            LightComp->SetCastShadows(true);
        }
    }

    return State;
}

void PhysicsAssetViewerBootstrap::DestroyViewerState(PhysicsAssetViewerState*& State)
{
    if (!State) return;

    // World가 이미 삭제되었는지 확인 (엔진 종료 시 DeleteAll에서 먼저 삭제될 수 있음)
    bool bWorldValid = State->World && ObjectFactory::IsValidObject(State->World);

    // Physics Scene 정리
    if (bWorldValid && State->World->GetPhysicsScene())
    {
        PHYSICS.DestroyScene(State->World->GetPhysicsSceneHandle());
    }

    // PreviewActor의 InternalClothComponent 먼저 정리 (World 삭제 전)
    // World 삭제 시 Actor 소멸자에서 Cast 크래시 방지
    if (State->PreviewActor && ObjectFactory::IsValidObject(State->PreviewActor))
    {
        if (USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent())
        {
            SkelComp->DestroyInternalClothComponent();
        }
    }

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (bWorldValid) { ObjectFactory::DeleteObject(State->World); }
    State->World = nullptr;
    delete State;
    State = nullptr;
}

void PhysicsAssetViewerBootstrap::SetupFloorAndCamera(ASkeletalMeshActor* PreviewActor, AStaticMeshActor* FloorActor, FViewportClient* Client)
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

    // 메쉬 위치 조정: 바닥이 Floor 위에 오도록 (발이 가려지지 않게)
    // AABB의 최소 Z 값을 Floor 상면 (Z=0.1) 위에 위치시킴
    float MeshBottomZ = Bounds.Min.Z;
    float FloorTopZ = 0.1f;  // Floor 상면
    float OffsetZ = FloorTopZ - MeshBottomZ;
    PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, OffsetZ));

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
