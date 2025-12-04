#include "pch.h"
#include "ParticleViewerBootstrap.h"
#include "ParticleViewerState.h"
#include "ParticleSystemActor.h"
#include "ParticleSystemComponent.h"
#include "CameraActor.h"
#include "AmbientLightActor.h"
#include "DirectionalLightActor.h"
#include "Source/Runtime/Engine/Components/AmbientLightComponent.h"
#include "Source/Runtime/Engine/Components/DirectionalLightComponent.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Core/Object/ObjectFactory.h"
#include "Source/Editor/Grid/GridActor.h"

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
	Client->SetPickingEnabled(false);  // 플로팅 윈도우에서는 피킹 비활성화

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);

	// 카메라 설정
	ACameraActor* Camera = State->World->GetEditorCameraActor();
	if (Camera)
	{
		Camera->SetActorLocation(FVector(5, 0, 2));
	}

	// 프리뷰 라이팅 설정 (UE Cascade 스타일)
	// Ambient Light - 전역 조명
	AAmbientLightActor* AmbientLight = State->World->SpawnActor<AAmbientLightActor>();
	if (AmbientLight)
	{
		UAmbientLightComponent* AmbientComp = AmbientLight->GetLightComponent();
		if (AmbientComp)
		{
			AmbientComp->SetIntensity(0.5f);
			AmbientComp->SetLightColor(FLinearColor(0.3f, 0.35f, 0.4f, 1.0f));
		}
	}

	// Directional Light - 주 광원 (UE 기본 회전: Pitch=304.736, Yaw=39.84)
	ADirectionalLightActor* DirectionalLight = State->World->SpawnActor<ADirectionalLightActor>();
	if (DirectionalLight)
	{
		// 위에서 비스듬히 비추는 각도 설정 (Roll=-55도, Yaw=40도)
		FQuat LightRotation = FQuat::MakeFromEulerZYX(FVector(-55.0f, 40.0f, 0.0f));
		DirectionalLight->SetActorRotation(LightRotation);

		UDirectionalLightComponent* DirLightComp = DirectionalLight->GetLightComponent();
		if (DirLightComp)
		{
			DirLightComp->SetIntensity(1.5f);
			DirLightComp->SetLightColor(FLinearColor(1.0f, 0.98f, 0.95f, 1.0f));
			DirLightComp->SetCastShadows(false);  // 프리뷰에서는 그림자 비활성화
		}
	}

	// 프리뷰 ParticleSystemActor 스폰
	AParticleSystemActor* Preview = State->World->SpawnActor<AParticleSystemActor>();
	if (Preview)
	{
		Preview->SetTickInEditor(true);
	}
	State->PreviewActor = Preview;

	// Origin Axis용 GridActor 생성 (그리드는 표시하지 않고 축만 표시)
	AGridActor* GridActor = State->World->SpawnActor<AGridActor>();
	if (GridActor)
	{
		GridActor->Initialize();
		GridActor->ClearLines();
		GridActor->CreateAxisLines(5.0f, FVector::Zero()); // 원점에 5유닛 길이의 축 생성

		// LineComponent를 OriginAxis로 설정 (SF_OriginAxis 플래그로 제어)
		ULineComponent* LineComp = GridActor->GetLineComponent();
		if (LineComp)
		{
			LineComp->SetIsOriginAxis(true);
		}
	}

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
	// World가 이미 삭제되었는지 확인 (엔진 종료 시 DeleteAll에서 먼저 삭제될 수 있음)
	if (State->World && ObjectFactory::IsValidObject(State->World))
	{
		ObjectFactory::DeleteObject(State->World);
	}
	State->World = nullptr;

	delete State;
	State = nullptr;
}
