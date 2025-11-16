#include "pch.h"
#include "SkeletalViewerBootstrap.h"
#include "CameraActor.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "FViewport.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

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

    State->World->GetGizmoActor()->SetSpace(EGizmoSpace::Local);

    // Viewport + client per tab
    State->Viewport = new FViewport();
    // 프레임 마다 initial size가 바꿜 것이다
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor (mesh can be set later from UI)
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        if (Preview)
        {
            // Enable tick in editor for preview world
            Preview->SetTickInEditor(true);
        }
        State->PreviewActor = Preview;
    }

    return State;
}

void SkeletalViewerBootstrap::DestroyViewerState(ViewerState*& State)
{
    if (!State) return;

    // CurrentAnimation이 임포트한 애니메이션 중 하나인지 확인
    bool bCurrentAnimIsImported = false;
    for (UAnimSequence* AnimSeq : State->ImportedAnimSequences)
    {
        if (AnimSeq == State->CurrentAnimation)
        {
            bCurrentAnimIsImported = true;
            break;
        }
    }

    // 임포트한 애니메이션이면 포인터 무효화
    if (bCurrentAnimIsImported)
    {
        State->CurrentAnimation = nullptr;
        State->SelectedAnimationIndex = -1;
    }

    // 이 Viewer에서 임포트한 AnimSequence들만 삭제
    // 먼저 모든 메시에서 해당 애니메이션 제거 (dangling pointer 방지)
    for (UAnimSequence* AnimSeq : State->ImportedAnimSequences)
    {
        if (AnimSeq)
        {
            // ResourceManager의 모든 SkeletalMesh에서 이 애니메이션 제거
            UResourceManager& ResourceManager = UResourceManager::GetInstance();
            TArray<USkeletalMesh*> AllMeshes = ResourceManager.GetAll<USkeletalMesh>();
            for (USkeletalMesh* Mesh : AllMeshes)
            {
                if (Mesh)
                {
                    Mesh->RemoveAnimation(AnimSeq);
                }
            }

            // AnimSequence와 DataModel 삭제
            ObjectFactory::DeleteObject(AnimSeq);
        }
    }
    State->ImportedAnimSequences.Empty();

    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}
