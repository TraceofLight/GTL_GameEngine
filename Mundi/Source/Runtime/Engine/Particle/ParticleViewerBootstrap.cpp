#include "pch.h"
#include "ParticleViewerBootstrap.h"
#include "ParticleViewerState.h"
#include "ParticleSystemActor.h"
#include "ParticleSystemComponent.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Core/Object/ObjectFactory.h"

ParticleViewerState* ParticleViewerBootstrap::CreateViewerState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
	if (!InDevice)
	{
		return nullptr;
	}

	ParticleViewerState* State = new ParticleViewerState();
	State->Name = Name ? Name : "Particle Editor";

	// Preview world 만들기
	State->World = NewObject<UWorld>();
	State->World->SetWorldType(EWorldType::PreviewMinimal);
	State->World->Initialize();
	State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

	// Viewport + client per tab
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0, 0, 1, 1, InDevice);

	FViewportClient* Client = new FViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);

	// 카메라 설정
	ACameraActor* Camera = State->World->GetEditorCameraActor();
	if (Camera)
	{
		Camera->SetActorLocation(FVector(5, 0, 2));
	}

	// 프리뷰 ParticleSystemActor 스폰
	AParticleSystemActor* Preview = State->World->SpawnActor<AParticleSystemActor>();
	if (Preview)
	{
		Preview->SetTickInEditor(true);
	}
	State->PreviewActor = Preview;

	return State;
}

void ParticleViewerBootstrap::DestroyViewerState(ParticleViewerState*& State)
{
	if (!State)
	{
		return;
	}

	if (State->Viewport)
	{
		delete State->Viewport;
		State->Viewport = nullptr;
	}
	if (State->Client)
	{
		delete State->Client;
		State->Client = nullptr;
	}
	if (State->World)
	{
		ObjectFactory::DeleteObject(State->World);
		State->World = nullptr;
	}

	delete State;
	State = nullptr;
}
