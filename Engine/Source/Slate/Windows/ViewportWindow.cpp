#include "pch.h"
#include "ViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include "USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"
#include "Source/Slate/Widgets/ViewportToolbarWidget.h"

#include "CameraComponent.h"
#include "CameraActor.h"
#include "StatsOverlayD2D.h"

#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMeshComponent.h"
#include "PrimitiveComponent.h"
#include "ParticleSystemActor.h"
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"
#include "Source/Runtime/Engine/Particle/ParticleSystem.h"
#include "ResourceManager.h"
#include "Source/Runtime/Core/Misc/PathUtils.h"
#include "SelectionManager.h"
#include "AABB.h"
#include <filesystem>

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow::SViewportWindow()
{
	ViewportType = EViewportType::Perspective;
	bIsActive = false;
	bIsMouseDown = false;
}

SViewportWindow::~SViewportWindow()
{
	if (Viewport)
	{
		delete Viewport;
		Viewport = nullptr;
	}

	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	if (ViewportToolbar)
	{
		delete ViewportToolbar;
		ViewportToolbar = nullptr;
	}

	// 카메라 옵션 아이콘 (ViewportWindow 전용)
	IconCamera = nullptr;
	IconPerspective = nullptr;
	IconTop = nullptr;
	IconBottom = nullptr;
	IconLeft = nullptr;
	IconRight = nullptr;
	IconFront = nullptr;
	IconBack = nullptr;
	IconFOV = nullptr;
	IconNearClip = nullptr;
	IconFarClip = nullptr;

	// 레이아웃 전환 아이콘 (ViewportWindow 전용)
	IconSingleToMultiViewport = nullptr;
	IconMultiToSingleViewport = nullptr;
}

bool SViewportWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType)
{
	ViewportType = InViewportType;

	// 이름 설정
	switch (ViewportType)
	{
	case EViewportType::Perspective:		ViewportName = "원근"; break;
	case EViewportType::Orthographic_Front: ViewportName = "정면"; break;
	case EViewportType::Orthographic_Left:  ViewportName = "왼쪽"; break;
	case EViewportType::Orthographic_Top:   ViewportName = "상단"; break;
	case EViewportType::Orthographic_Back:	ViewportName = "후면"; break;
	case EViewportType::Orthographic_Right:  ViewportName = "오른쪽"; break;
	case EViewportType::Orthographic_Bottom:   ViewportName = "하단"; break;
	}

	// FViewport 생성
	Viewport = new FViewport();
	if (!Viewport->Initialize(StartX, StartY, Width, Height, Device))
	{
		delete Viewport;
		Viewport = nullptr;
		return false;
	}

	// FViewportClient 생성
	ViewportClient = new FViewportClient();
	ViewportClient->SetViewportType(ViewportType);
	ViewportClient->SetWorld(World); // 전역 월드 연결 (이미 있다고 가정)

	// 양방향 연결
	Viewport->SetViewportClient(ViewportClient);

	// 공용 툴바 위젯 생성 및 초기화
	ViewportToolbar = new SViewportToolbarWidget();
	ViewportToolbar->Initialize(Device);

	// 툴바 아이콘 로드 (카메라/레이아웃 전용)
	LoadToolbarIcons(Device);

	return true;
}

void SViewportWindow::OnRender()
{
	// 뷰포트가 너무 작으면 렌더링 건너뛰기 (애니메이션 중 깔끔한 전환을 위해)
	float ViewportWidth = Rect.Right - Rect.Left;
	float ViewportHeight = Rect.Bottom - Rect.Top;
	const float MinRenderSize = 50.0f;  // 최소 렌더링 크기
	if (ViewportWidth < MinRenderSize || ViewportHeight < MinRenderSize)
	{
		return;
	}

	// Slate(UI)만 처리하고 렌더는 FViewport에 위임
	RenderToolbar();

	if (Viewport)
		Viewport->Render();

	// 카메라 포커싱 애니메이션 업데이트
	UpdateCameraAnimation(ImGui::GetIO().DeltaTime);

	// 드래그 앤 드롭 타겟 영역 (뷰포트 전체)
	HandleDropTarget();

	// Scene 로드 확인 모달
	if (bShowSceneLoadModal)
	{
		ImGui::OpenPopup("Load Scene?");
		bShowSceneLoadModal = false; // OpenPopup 호출 후 플래그 리셋
	}

	// 모달 중앙 위치 설정
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Load Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("씬을 로드하시겠습니까?");
		ImGui::Separator();

		// 파일명만 표시
		std::filesystem::path scenePath(PendingScenePath);
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", scenePath.filename().string().c_str());

		ImGui::Spacing();
		ImGui::Text("현재 씬의 저장되지 않은 변경사항은 사라집니다.");
		ImGui::Spacing();

		float buttonWidth = 120.0f;
		float spacing = 10.0f;
		float totalWidth = buttonWidth * 2 + spacing;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

		if (ImGui::Button("Yes", ImVec2(buttonWidth, 0)))
		{
			// 씬 로드
			if (ViewportClient && ViewportClient->GetWorld())
			{
				// UTF-8 → UTF-16 변환 (한글 경로 지원)
				FWideString widePath = UTF8ToWide(PendingScenePath);
				if (ViewportClient->GetWorld()->LoadLevelFromFile(widePath))
				{
					UE_LOG("Scene loaded successfully: %s", PendingScenePath.c_str());
				}
				else
				{
					UE_LOG("ERROR: Failed to load scene: %s", PendingScenePath.c_str());
				}
			}
			PendingScenePath.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("No", ImVec2(buttonWidth, 0)))
		{
			PendingScenePath.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SViewportWindow::OnUpdate(float DeltaSeconds)
{
	if (!Viewport)
		return;

	if (!Viewport) return;

	// 툴바 높이만큼 뷰포트 영역 조정

	uint32 NewStartX = static_cast<uint32>(Rect.Left);
	uint32 NewStartY = static_cast<uint32>(Rect.Top);
	uint32 NewWidth = static_cast<uint32>(Rect.Right - Rect.Left);
	uint32 NewHeight = static_cast<uint32>(Rect.Bottom - Rect.Top);

	Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
	ViewportClient->Tick(DeltaSeconds);
}

void SViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
}

void SViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리s
	bIsMouseDown = true;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, Button);

}

void SViewportWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	bIsMouseDown = false;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, Button);
}

void SViewportWindow::SetVClientWorld(UWorld* InWorld)
{
	if (ViewportClient && InWorld)
	{
		ViewportClient->SetWorld(InWorld);
	}
}

void SViewportWindow::SwitchToViewportType(EViewportType NewType, const char* NewName)
{
	ViewportType = NewType;
	ViewportName = NewName;

	if (ViewportClient)
	{
		ViewportClient->SetViewportType(ViewportType);
		ViewportClient->SetupCameraMode();
	}
}

void SViewportWindow::RenderToolbar()
{
	if (!Viewport || !ViewportToolbar) return;

	// 툴바 영역 크기
	float ToolbarHeight = ViewportToolbar->GetToolbarHeight();
	ImVec2 ToolbarPosition(Rect.Left, Rect.Top);
	ImVec2 ToolbarSize(Rect.Right - Rect.Left, ToolbarHeight);

	// 툴바 위치 지정
	ImGui::SetNextWindowPos(ToolbarPosition);
	ImGui::SetNextWindowSize(ToolbarSize);

	// 뷰포트별 고유한 윈도우 ID
	char WindowId[64];
	sprintf_s(WindowId, "ViewportToolbar_%p", this);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin(WindowId, nullptr, WindowFlags))
	{
		// GizmoActor 가져오기
		AGizmoActor* GizmoActor = nullptr;
		if (ViewportClient && ViewportClient->GetWorld() && !GEngine.IsPIEActive())
		{
			GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		}

		// PIE 힌트 표시
		if (GEngine.IsPIEActive() && GEngine.IsPIEInputCaptured())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
			const float ButtonHeight = 23.0f;
			float verticalPadding = (ToolbarHeight - ButtonHeight) * 0.5f;
			ImGui::SetCursorPosY(verticalPadding);

			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Press Shift + F1 to detach");
			ImGui::SameLine();

			ImGui::PopStyleVar();
		}

		// ViewportToolbar 렌더링 (기즈모 + 카메라 + 속도 + 뷰모드 + ShowFlag + 레이아웃)
		ViewportToolbar->Render(ViewportClient, GizmoActor, true, true, ViewportType, ViewportName.ToString().c_str(), this);
	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 카메라 옵션 드롭다운 아이콘 (ViewportWindow 전용)
	IconCamera = NewObject<UTexture>();
	IconCamera->Load(GDataDir + "/Default/Icon/Viewport_Mode_Camera.dds", Device, false);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load(GDataDir + "/Default/Icon/Viewport_Mode_Perspective.dds", Device, false);

	IconTop = NewObject<UTexture>();
	IconTop->Load(GDataDir + "/Default/Icon/Viewport_Mode_Top.dds", Device, false);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load(GDataDir + "/Default/Icon/Viewport_Mode_Bottom.dds", Device, false);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load(GDataDir + "/Default/Icon/Viewport_Mode_Left.dds", Device, false);

	IconRight = NewObject<UTexture>();
	IconRight->Load(GDataDir + "/Default/Icon/Viewport_Mode_Right.dds", Device, false);

	IconFront = NewObject<UTexture>();
	IconFront->Load(GDataDir + "/Default/Icon/Viewport_Mode_Front.dds", Device, false);

	IconBack = NewObject<UTexture>();
	IconBack->Load(GDataDir + "/Default/Icon/Viewport_Mode_Back.dds", Device, false);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load(GDataDir + "/Default/Icon/Viewport_Setting_FOV.dds", Device, false);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_NearClip.dds", Device, false);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_FarClip.dds", Device, false);

	// 뷰포트 레이아웃 전환 아이콘 (ViewportWindow 전용)
	IconSingleToMultiViewport = NewObject<UTexture>();
	IconSingleToMultiViewport->Load(GDataDir + "/Default/Icon/Viewport_SingleToMultiViewport.dds", Device, false);

	IconMultiToSingleViewport = NewObject<UTexture>();
	IconMultiToSingleViewport->Load(GDataDir + "/Default/Icon/Viewport_MultiToSingleViewport.dds", Device, false);
}

void SViewportWindow::HandleDropTarget()
{
	// 드래그 중일 때만 드롭 타겟 윈도우를 표시
	const ImGuiPayload* dragPayload = ImGui::GetDragDropPayload();
	bool isDragging = (dragPayload != nullptr && dragPayload->IsDataType("ASSET_FILE"));

	if (isDragging)
	{
		// 뷰포트 영역에 Invisible Button을 만들어 드롭 타겟으로 사용
		ImVec2 ViewportPos(Rect.Left, Rect.Top + 35.0f); // 툴바 높이(35) 제외
		ImVec2 ViewportSize(Rect.GetWidth(), Rect.GetHeight() - 35.0f);

		ImGui::SetNextWindowPos(ViewportPos);
		ImGui::SetNextWindowSize(ViewportSize);

		// Invisible overlay window for drop target
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoBringToFrontOnFocus
			| ImGuiWindowFlags_NoBackground;

		char WindowId[64];
		sprintf_s(WindowId, "ViewportDropTarget_%p", this);

		ImGui::Begin(WindowId, nullptr, flags);

		// Invisible button을 전체 영역에 만들어서 드롭 타겟으로 사용
		ImGui::InvisibleButton("ViewportDropArea", ViewportSize);

		// 드롭 타겟 처리
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				const char* filePath = (const char*)payload->Data;

				// 마우스 위치를 월드 좌표로 변환
				ImVec2 mousePos = ImGui::GetMousePos();

				// 뷰포트 내부의 상대 좌표 계산 (ViewportPos 기준)
				FVector2D screenPos(mousePos.x - ViewportPos.x, mousePos.y - ViewportPos.y);

				// 카메라 방향 기반으로 월드 좌표 계산
				FVector WorldLocation = FVector(0, 0, 0);

				if (ViewportClient && ViewportClient->GetCamera())
				{
					ACameraActor* Camera = ViewportClient->GetCamera();
					UCameraComponent* CameraComp = Camera->GetCameraComponent();

					// 뷰포트 크기
					float ViewportWidth = ViewportSize.x;
					float ViewportHeight = ViewportSize.y;

					// 스크린 좌표를 NDC(Normalized Device Coordinates)로 변환
					// NDC 범위: X [-1, 1], Y [-1, 1]
					float NdcX = (screenPos.X / ViewportWidth) * 2.0f - 1.0f;
					float NdcY = 1.0f - (screenPos.Y / ViewportHeight) * 2.0f; // Y축 반전

					// 카메라 정보
					FVector CameraPos = Camera->GetActorLocation();
					FVector CameraForward = Camera->GetForward();
					FVector CameraRight = Camera->GetRight();
					FVector CameraUp = Camera->GetUp();

					ECameraProjectionMode ProjMode = CameraComp->GetProjectionMode();

					if (ProjMode == ECameraProjectionMode::Orthographic)
					{
						// Ortho: 화면 좌표를 직접 월드 좌표로 deproject하여 ground plane(Z=0)에 스폰
						// OrthoZoom은 픽셀당 월드 유닛 (화면 절반 높이 = OrthoZoom * ViewportHeight / 2)
						float OrthoZoom = CameraComp->GetOrthoZoom();
						float HalfWidth = ViewportWidth * 0.5f * OrthoZoom;
						float HalfHeight = ViewportHeight * 0.5f * OrthoZoom;

						// 카메라 로컬 오프셋 (NDC * 화면 월드 크기)
						FVector LocalOffset = CameraRight * (NdcX * HalfWidth) + CameraUp * (NdcY * HalfHeight);

						// 카메라 위치 + 오프셋 = 뷰 평면 위의 월드 좌표
						FVector ViewPlanePos = CameraPos + LocalOffset;

						// Ground plane(Z=0)으로 레이캐스트
						// Ray: ViewPlanePos + T * CameraForward, 여기서 결과의 Z = 0
						// ViewPlanePos.Z + T * CameraForward.Z = 0
						// T = -ViewPlanePos.Z / CameraForward.Z
						if (std::abs(CameraForward.Z) > 0.001f)
						{
							float T = -ViewPlanePos.Z / CameraForward.Z;
							WorldLocation = ViewPlanePos + CameraForward * T;
						}
						else
						{
							// 카메라가 수평을 바라보는 경우 (Front/Right 뷰)
							// Y=0 또는 X=0 평면에 스폰
							if (std::abs(CameraForward.Y) > std::abs(CameraForward.X))
							{
								// Front 뷰 (Y 방향 바라봄) - Y=0 평면에 스폰
								float T = -ViewPlanePos.Y / CameraForward.Y;
								WorldLocation = ViewPlanePos + CameraForward * T;
							}
							else
							{
								// Right 뷰 (X 방향 바라봄) - X=0 평면에 스폰
								float T = -ViewPlanePos.X / CameraForward.X;
								WorldLocation = ViewPlanePos + CameraForward * T;
							}
						}
					}
					else
					{
						// Perspective: 기존 FOV 기반 레이 방향 계산
						float FOV = CameraComp->GetFOV();
						float AspectRatio = ViewportWidth / ViewportHeight;
						float TanHalfFOV = tan(FOV * 0.5f * 3.14159f / 180.0f);

						// 마우스 스크린 좌표에 해당하는 월드 방향 벡터 계산
						FVector RayDir = CameraForward
							+ CameraRight * (NdcX * TanHalfFOV * AspectRatio)
							+ CameraUp * (NdcY * TanHalfFOV);
						RayDir.Normalize();

						// Ground plane(Z=0)에 레이캐스트하여 스폰 위치 결정
						// Ray: CameraPos + T * RayDir, Z = 0
						if (std::abs(RayDir.Z) > 0.001f)
						{
							float T = -CameraPos.Z / RayDir.Z;
							if (T > 0)
							{
								WorldLocation = CameraPos + RayDir * T;
							}
							else
							{
								// 카메라가 ground 아래를 보지 않음 - 적당한 거리에 스폰
								WorldLocation = CameraPos + RayDir * 500.0f;
							}
						}
						else
						{
							// 수평 시야 - 적당한 거리에 스폰
							WorldLocation = CameraPos + RayDir * 500.0f;
						}
					}
				}

				// 액터 생성
				SpawnActorFromFile(filePath, WorldLocation);

				UE_LOG("Viewport: Dropped asset '%s' at world location (%f, %f, %f)",
					filePath, WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}
}

void SViewportWindow::SpawnActorFromFile(const char* FilePath, const FVector& WorldLocation)
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		UE_LOG("ERROR: ViewportClient or World is null");
		return;
	}

	UWorld* world = ViewportClient->GetWorld();
	std::filesystem::path path(FilePath);
	std::string extension = path.extension().string();

	// 소문자로 변환
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	UE_LOG("Spawning actor from file: %s (extension: %s)", FilePath, extension.c_str());

	if (extension == ".prefab")
	{
		// Prefab Actor 생성
		FWideString widePath = path.wstring();
		AActor* actor = world->SpawnPrefabActor(widePath);
		if (actor)
		{
			actor->SetActorLocation(WorldLocation);
			UE_LOG("Prefab actor spawned at (%f, %f, %f)",
				WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
		}
		else
		{
			UE_LOG("ERROR: Failed to spawn prefab actor from %s", FilePath);
		}
	}
	else if (extension == ".fbx")
	{
		// SkeletalMesh Actor 생성
		ASkeletalMeshActor* actor = world->SpawnActor<ASkeletalMeshActor>(FTransform(WorldLocation, FQuat::Identity(), FVector::One()));
		if (actor)
		{
			actor->GetSkeletalMeshComponent()->SetSkeletalMesh(FilePath);
			actor->ObjectName = path.filename().string().c_str();
			UE_LOG("SkeletalMeshActor spawned at (%f, %f, %f)",
				WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
		}
		else
		{
			UE_LOG("ERROR: Failed to spawn SkeletalMeshActor");
		}
	}
	else if (extension == ".obj")
	{
		// StaticMesh Actor 생성
		AStaticMeshActor* actor = world->SpawnActor<AStaticMeshActor>(FTransform(WorldLocation, FQuat::Identity(), FVector::One()));
		if (actor)
		{
			actor->GetStaticMeshComponent()->SetStaticMesh(FilePath);
			actor->ObjectName = path.filename().string().c_str();
			UE_LOG("StaticMeshActor spawned at (%f, %f, %f)",
				WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
		}
		else
		{
			UE_LOG("ERROR: Failed to spawn StaticMeshActor");
		}
	}
	else if (extension == ".psys")
	{
		// ParticleSystem Actor 생성
		AParticleSystemActor* Actor = world->SpawnActor<AParticleSystemActor>(FTransform(WorldLocation, FQuat::Identity(), FVector::One()));
		if (Actor)
		{
			UParticleSystem* LoadedPS = NewObject<UParticleSystem>();
			if (LoadedPS && LoadedPS->LoadFromFileInternal(FString(FilePath)))
			{
				Actor->SetParticleSystem(LoadedPS);
				Actor->ObjectName = path.filename().stem().string().c_str();
				UE_LOG("ParticleSystemActor spawned from %s at (%f, %f, %f)",
					FilePath, WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
			}
			else
			{
				UE_LOG("ERROR: Failed to load .psys file: %s", FilePath);
			}
		}
		else
		{
			UE_LOG("ERROR: Failed to spawn ParticleSystemActor");
		}
	}
	else if (extension == ".scene")
	{
		// Scene 파일은 모달로 로드 여부 확인
		bShowSceneLoadModal = true;
		PendingScenePath = FilePath;
		UE_LOG("Scene file dropped: %s - showing load confirmation modal", FilePath);
	}
	else
	{
		UE_LOG("WARNING: Unsupported file type: %s", extension.c_str());
	}
}

void SViewportWindow::FocusOnSelectedActor()
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	UWorld* World = ViewportClient->GetWorld();
	USelectionManager* SelectionMgr = World->GetSelectionManager();
	if (!SelectionMgr)
	{
		return;
	}

	// 에디터 카메라 가져오기
	ACameraActor* EditorCamera = World->GetEditorCameraActor();
	if (!EditorCamera)
	{
		return;
	}

	// 선택된 액터들의 바운딩 박스 계산
	FVector BoundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector BoundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool bHasValidBounds = false;

	// 컴포넌트 모드인 경우 선택된 컴포넌트 위치로 포커싱
	if (!SelectionMgr->IsActorMode())
	{
		USceneComponent* SelectedComp = SelectionMgr->GetSelectedComponent();
		if (SelectedComp)
		{
			FVector CompLocation = SelectedComp->GetWorldLocation();

			// 컴포넌트 바운딩 박스 계산 시도
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(SelectedComp))
			{
				FAABB Bounds = PrimComp->GetWorldAABB();
				// 유효한 AABB인지 확인 (Min < Max)
				if (Bounds.Min.X < Bounds.Max.X && Bounds.Min.Y < Bounds.Max.Y && Bounds.Min.Z < Bounds.Max.Z)
				{
					BoundsMin = Bounds.Min;
					BoundsMax = Bounds.Max;
					bHasValidBounds = true;
				}
			}

			// 바운딩 박스가 유효하지 않으면 기본 크기 사용
			if (!bHasValidBounds)
			{
				constexpr float DefaultFocusExtent = 2.0f;
				BoundsMin = CompLocation - FVector(DefaultFocusExtent, DefaultFocusExtent, DefaultFocusExtent);
				BoundsMax = CompLocation + FVector(DefaultFocusExtent, DefaultFocusExtent, DefaultFocusExtent);
				bHasValidBounds = true;
			}
		}
	}

	// 액터 모드이거나 컴포넌트 바운드 계산 실패 시 액터 기반 계산
	if (!bHasValidBounds)
	{
		const TArray<AActor*>& SelectedActors = SelectionMgr->GetSelectedActors();
		if (SelectedActors.empty())
		{
			return;
		}

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor)
			{
				continue;
			}

			// 액터의 바운딩 박스 계산
			FVector ActorMin, ActorMax;
			bool bActorHasBounds = false;

			// StaticMeshComponent 확인
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Actor->GetComponent(UStaticMeshComponent::StaticClass())))
			{
				if (SMC->GetStaticMesh())
				{
					FAABB Bounds = SMC->GetWorldAABB();
					ActorMin = Bounds.Min;
					ActorMax = Bounds.Max;
					bActorHasBounds = true;
				}
			}
			// SkeletalMeshComponent 확인
			else if (USkeletalMeshComponent* SkMC = Cast<USkeletalMeshComponent>(Actor->GetComponent(USkeletalMeshComponent::StaticClass())))
			{
				if (SkMC->GetSkeletalMesh())
				{
					FAABB Bounds = SkMC->GetWorldAABB();
					// 유효한 AABB인지 확인 (Min < Max)
					if (Bounds.Min.X < Bounds.Max.X && Bounds.Min.Y < Bounds.Max.Y && Bounds.Min.Z < Bounds.Max.Z)
					{
						ActorMin = Bounds.Min;
						ActorMax = Bounds.Max;
						bActorHasBounds = true;
					}
				}
			}

			// 바운딩 박스가 없으면 빌보드/아이콘 기반 크기 또는 기본값 사용
			if (!bActorHasBounds)
			{
				FVector Loc = Actor->GetActorLocation();

				// 빌보드 컴포넌트가 있으면 적절한 포커싱 거리 사용
				constexpr float DefaultFocusExtent = 2.0f;
				ActorMin = Loc - FVector(DefaultFocusExtent, DefaultFocusExtent, DefaultFocusExtent);
				ActorMax = Loc + FVector(DefaultFocusExtent, DefaultFocusExtent, DefaultFocusExtent);
			}

			// 전체 바운딩 박스 업데이트
			BoundsMin.X = std::min(BoundsMin.X, ActorMin.X);
			BoundsMin.Y = std::min(BoundsMin.Y, ActorMin.Y);
			BoundsMin.Z = std::min(BoundsMin.Z, ActorMin.Z);
			BoundsMax.X = std::max(BoundsMax.X, ActorMax.X);
			BoundsMax.Y = std::max(BoundsMax.Y, ActorMax.Y);
			BoundsMax.Z = std::max(BoundsMax.Z, ActorMax.Z);
			bHasValidBounds = true;
		}
	}

	if (!bHasValidBounds)
	{
		return;
	}

	// 바운딩 박스 중심과 크기 계산
	FVector Center = (BoundsMin + BoundsMax) * 0.5f;
	FVector Extent = (BoundsMax - BoundsMin) * 0.5f;
	float BoundsRadius = Extent.Size();

	// 거리 계산: min(radius * 1.2, radius + 5.0) - 작은 오브젝트도 적절한 거리, 큰 오브젝트는 과도하게 멀어지지 않게
	float ScaledDist = BoundsRadius * 1.2f;
	float AdditiveDist = BoundsRadius + 5.0f;
	float FocusDistance = std::max(std::min(ScaledDist, AdditiveDist), 3.0f);

	// 현재 카메라 방향 유지하면서 위치만 이동
	FVector CamForward = EditorCamera->GetForward();
	if (CamForward.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		CamForward = FVector(1.0f, 0.0f, 0.0f);
	}
	CamForward.Normalize();

	// 카메라 애니메이션 시작
	CameraStartLocation = EditorCamera->GetActorLocation();
	CameraTargetLocation = Center - CamForward * FocusDistance;
	CameraAnimationTime = 0.0f;
	bIsCameraAnimating = true;
}

void SViewportWindow::UpdateCameraAnimation(float DeltaTime)
{
	if (!bIsCameraAnimating)
	{
		return;
	}

	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		bIsCameraAnimating = false;
		return;
	}

	// 우클릭 드래그 시작 시 애니메이션 중단
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
	{
		bIsCameraAnimating = false;
		return;
	}

	ACameraActor* EditorCamera = ViewportClient->GetWorld()->GetEditorCameraActor();
	if (!EditorCamera)
	{
		bIsCameraAnimating = false;
		return;
	}

	CameraAnimationTime += DeltaTime;
	float Progress = CameraAnimationTime / CAMERA_ANIMATION_DURATION;

	if (Progress >= 1.0f)
	{
		Progress = 1.0f;
		bIsCameraAnimating = false;
	}

	// Ease-in-out (quartic)
	float SmoothProgress;
	if (Progress < 0.5f)
	{
		SmoothProgress = 8.0f * Progress * Progress * Progress * Progress;
	}
	else
	{
		float ProgressFromEnd = Progress - 1.0f;
		SmoothProgress = 1.0f - 8.0f * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd;
	}

	// 보간된 위치 계산
	FVector CurrentLocation = FVector::Lerp(CameraStartLocation, CameraTargetLocation, SmoothProgress);
	EditorCamera->SetActorLocation(CurrentLocation);
}
