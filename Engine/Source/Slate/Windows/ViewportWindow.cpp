#include "pch.h"
#include "ViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include "USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"

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

	IconSelect = nullptr;
	IconMove = nullptr;
	IconRotate = nullptr;
	IconScale = nullptr;
	IconWorldSpace = nullptr;
	IconLocalSpace = nullptr;

	IconCamera = nullptr;
	IconPerspective = nullptr;
	IconTop = nullptr;
	IconBottom = nullptr;
	IconLeft = nullptr;
	IconRight = nullptr;
	IconFront = nullptr;
	IconBack = nullptr;

	IconSpeed = nullptr;
	IconFOV = nullptr;
	IconNearClip = nullptr;
	IconFarClip = nullptr;

	IconViewMode_Lit = nullptr;
	IconViewMode_Unlit = nullptr;
	IconViewMode_Wireframe = nullptr;
	IconViewMode_BufferVis = nullptr;

	IconShowFlag = nullptr;
	IconRevert = nullptr;
	IconStats = nullptr;
	IconHide = nullptr;
	IconBVH = nullptr;
	IconGrid = nullptr;
	IconDecal = nullptr;
	IconStaticMesh = nullptr;
	IconBillboard = nullptr;
	IconEditorIcon = nullptr;
	IconFog = nullptr;
	IconCollision = nullptr;
	IconAntiAliasing = nullptr;
	IconTile = nullptr;

	IconSingleToMultiViewport = nullptr;
	IconMultiToSingleViewport = nullptr;

	IconParticle = nullptr;
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

	// 툴바 아이콘 로드
	LoadToolbarIcons(Device);

	return true;
}

void SViewportWindow::OnRender()
{
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

void SViewportWindow::RenderToolbar()
{
	if (!Viewport) return;

	// 툴바 영역 크기
	float ToolbarHeight = 35.0f;
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
		// 기즈모 버튼 스타일 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));      // 간격 좁히기
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);            // 모서리 둥글게
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // 배경 투명
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // 호버 배경

		// 기즈모 모드 버튼들 렌더링
		RenderGizmoModeButtons();

		// 구분선
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
		ImGui::SameLine();

		// 기즈모 스페이스 버튼 렌더링
		RenderGizmoSpaceButton();

		// 기즈모 버튼 스타일 복원
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);

		// === 오른쪽 정렬 버튼들 ===
		// Switch, ShowFlag, ViewMode는 오른쪽 고정, Camera는 ViewMode 너비에 따라 왼쪽으로 밀림

		const float SwitchButtonWidth = 33.0f;  // Switch 버튼
		const float ShowFlagButtonWidth = 50.0f;  // ShowFlag 버튼 (대략)
		const float ButtonSpacing = 8.0f;

		// 현재 ViewMode 이름으로 실제 너비 계산
		const char* CurrentViewModeName = "뷰모드";
		if (ViewportClient)
		{
			EViewMode CurrentViewMode = ViewportClient->GetViewMode();
			switch (CurrentViewMode)
			{
			case EViewMode::VMI_Lit_Gouraud:
			case EViewMode::VMI_Lit_Lambert:
			case EViewMode::VMI_Lit_Phong:
				CurrentViewModeName = "라이팅 포함";
				break;
			case EViewMode::VMI_Unlit:
				CurrentViewModeName = "언릿";
				break;
			case EViewMode::VMI_Wireframe:
				CurrentViewModeName = "와이어프레임";
				break;
			case EViewMode::VMI_WorldNormal:
				CurrentViewModeName = "월드 노멀";
				break;
			case EViewMode::VMI_SceneDepth:
				CurrentViewModeName = "씬 뎁스";
				break;
			}
		}

		// ViewMode 버튼의 실제 너비 계산
		char viewModeText[64];
		sprintf_s(viewModeText, "%s %s", CurrentViewModeName, "∨");
		ImVec2 viewModeTextSize = ImGui::CalcTextSize(viewModeText);
		const float ViewModeButtonWidth = 17.0f + 4.0f + viewModeTextSize.x + 16.0f;

		// Camera 버튼 너비 계산
		char cameraText[64];
		sprintf_s(cameraText, "%s %s", ViewportName.ToString().c_str(), "∨");
		ImVec2 cameraTextSize = ImGui::CalcTextSize(cameraText);
		const float CameraButtonWidth = 17.0f + 4.0f + cameraTextSize.x + 16.0f;

		// 사용 가능한 전체 너비와 현재 커서 위치
		float AvailableWidth = ImGui::GetContentRegionAvail().x;
		float CursorStartX = ImGui::GetCursorPosX();
		ImVec2 CurrentCursor = ImGui::GetCursorPos();

		// 오른쪽부터 역순으로 위치 계산
		// Switch는 오른쪽 끝
		float SwitchX = CursorStartX + AvailableWidth - SwitchButtonWidth;

		// ShowFlag는 Switch 왼쪽
		float ShowFlagX = SwitchX - ButtonSpacing - ShowFlagButtonWidth;

		// ViewMode는 ShowFlag 왼쪽 (실제 너비 사용)
		float ViewModeX = ShowFlagX - ButtonSpacing - ViewModeButtonWidth;

		// Camera는 ViewMode 왼쪽 (ViewMode 너비에 따라 위치 변동)
		float CameraX = ViewModeX - ButtonSpacing - CameraButtonWidth;

		// 버튼들을 순서대로 그리기 (Y 위치는 동일하게 유지)
		ImGui::SetCursorPos(ImVec2(CameraX, CurrentCursor.y));
		RenderCameraOptionDropdownMenu();

		ImGui::SetCursorPos(ImVec2(ViewModeX, CurrentCursor.y));
		RenderViewModeDropdownMenu();

		ImGui::SetCursorPos(ImVec2(ShowFlagX, CurrentCursor.y));
		RenderShowFlagDropdownMenu();

		ImGui::SetCursorPos(ImVec2(SwitchX, CurrentCursor.y));
		RenderViewportLayoutSwitchButton();
	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 기즈모 아이콘 텍스처 생성 및 로드
	IconSelect = NewObject<UTexture>();
	IconSelect->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Select.png", Device);

	IconMove = NewObject<UTexture>();
	IconMove->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Move.png", Device);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Rotate.png", Device);

	IconScale = NewObject<UTexture>();
	IconScale->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Scale.png", Device);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_LocalSpace.png", Device);

	// 뷰포트 모드 아이콘 텍스처 로드
	IconCamera = NewObject<UTexture>();
	IconCamera->Load(GDataDir + "/Default/Icon/Viewport_Mode_Camera.png", Device);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load(GDataDir + "/Default/Icon/Viewport_Mode_Perspective.png", Device);

	IconTop = NewObject<UTexture>();
	IconTop->Load(GDataDir + "/Default/Icon/Viewport_Mode_Top.png", Device);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load(GDataDir + "/Default/Icon/Viewport_Mode_Bottom.png", Device);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load(GDataDir + "/Default/Icon/Viewport_Mode_Left.png", Device);

	IconRight = NewObject<UTexture>();
	IconRight->Load(GDataDir + "/Default/Icon/Viewport_Mode_Right.png", Device);

	IconFront = NewObject<UTexture>();
	IconFront->Load(GDataDir + "/Default/Icon/Viewport_Mode_Front.png", Device);

	IconBack = NewObject<UTexture>();
	IconBack->Load(GDataDir + "/Default/Icon/Viewport_Mode_Back.png", Device);

	// 뷰포트 설정 아이콘 텍스처 로드
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load(GDataDir + "/Default/Icon/Viewport_Mode_Camera.png", Device);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load(GDataDir + "/Default/Icon/Viewport_Setting_FOV.png", Device);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_NearClip.png", Device);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_FarClip.png", Device);

	// 뷰모드 아이콘 텍스처 로드
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Lit.png", Device);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Unlit.png", Device);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_BufferVis.png", Device);

	// ShowFlag 아이콘 텍스처 로드
	IconShowFlag = NewObject<UTexture>();
	IconShowFlag->Load(GDataDir + "/Default/Icon/Viewport_ShowFlag.png", Device);

	IconRevert = NewObject<UTexture>();
	IconRevert->Load(GDataDir + "/Default/Icon/Viewport_Revert.png", Device);

	IconStats = NewObject<UTexture>();
	IconStats->Load(GDataDir + "/Default/Icon/Viewport_Stats.png", Device);

	IconHide = NewObject<UTexture>();
	IconHide->Load(GDataDir + "/Default/Icon/Viewport_Hide.png", Device);

	IconBVH = NewObject<UTexture>();
	IconBVH->Load(GDataDir + "/Default/Icon/Viewport_BVH.png", Device);

	IconGrid = NewObject<UTexture>();
	IconGrid->Load(GDataDir + "/Default/Icon/Viewport_Grid.png", Device);

	IconDecal = NewObject<UTexture>();
	IconDecal->Load(GDataDir + "/Default/Icon/Viewport_Decal.png", Device);

	IconStaticMesh = NewObject<UTexture>();
	IconStaticMesh->Load(GDataDir + "/Default/Icon/Viewport_StaticMesh.png", Device);

	IconSkeletalMesh = NewObject<UTexture>();
	IconSkeletalMesh->Load(GDataDir + "/Default/Icon/Viewport_SkeletalMesh.png", Device);

	IconBillboard = NewObject<UTexture>();
	IconBillboard->Load(GDataDir + "/Default/Icon/Viewport_Billboard.png", Device);

	IconEditorIcon = NewObject<UTexture>();
	IconEditorIcon->Load(GDataDir + "/Default/Icon/Viewport_EditorIcon.png", Device);

	IconFog = NewObject<UTexture>();
	IconFog->Load(GDataDir + "/Default/Icon/Viewport_Fog.png", Device);

	IconCollision = NewObject<UTexture>();
	IconCollision->Load(GDataDir + "/Default/Icon/Viewport_Collision.png", Device);

	IconAntiAliasing = NewObject<UTexture>();
	IconAntiAliasing->Load(GDataDir + "/Default/Icon/Viewport_AntiAliasing.png", Device);

	IconTile = NewObject<UTexture>();
	IconTile->Load(GDataDir + "/Default/Icon/Viewport_Tile.png", Device);

	IconShadow = NewObject<UTexture>();
	IconShadow->Load(GDataDir + "/Default/Icon/Viewport_Shadow.png", Device);

	IconShadowAA = NewObject<UTexture>();
	IconShadowAA->Load(GDataDir + "/Default/Icon/Viewport_ShadowAA.png", Device);

	IconSkinning = NewObject<UTexture>();
	IconSkinning->Load(GDataDir + "/Default/Icon/Viewport_Skinning.png", Device);

	IconParticle = NewObject<UTexture>();
	IconParticle->Load(GDataDir + "/Default/Icon/Viewport_Particle.dds", Device);

	// 뷰포트 레이아웃 전환 아이콘 로드
	IconSingleToMultiViewport = NewObject<UTexture>();
	IconSingleToMultiViewport->Load(GDataDir + "/Default/Icon/Viewport_SingleToMultiViewport.png", Device);
	IconMultiToSingleViewport = NewObject<UTexture>();
	IconMultiToSingleViewport->Load(GDataDir + "/Default/Icon/Viewport_MultiToSingleViewport.png", Device);
}

void SViewportWindow::RenderGizmoModeButtons()
{
	ImVec2 switchCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(switchCursorPos.y - 2.0f);

	const ImVec2 IconSize(17, 17);

	// GizmoActor에서 직접 현재 모드 가져오기
	EGizmoMode CurrentGizmoMode = EGizmoMode::Select;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoMode = GizmoActor->GetMode();
		}
	}

	// Select 버튼
	bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택합니다. [Q]");
	}
	ImGui::SameLine();

	// Move 버튼
	bool bIsMoveActive = (CurrentGizmoMode == EGizmoMode::Translate);
	ImVec4 MoveTintColor = bIsMoveActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##MoveBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), MoveTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 이동시킵니다. [W]");
	}
	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 회전시킵니다. [E]");
	}
	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 스케일을 조절합니다. [R]");
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderGizmoSpaceButton()
{
	const ImVec2 IconSize(17, 17);

	// GizmoActor에서 직접 현재 스페이스 가져오기
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoSpace = GizmoActor->GetSpace();
		}
	}

	// 현재 스페이스에 따라 적절한 아이콘 표시
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "월드 스페이스 좌표 [Tab]" : "로컬 스페이스 좌표 [Tab]";

	// 선택 상태 tint (월드/로컬 모두 동일하게 흰색)
	ImVec4 TintColor = ImVec4(1, 1, 1, 1);

	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor))
		{
			// 버튼 클릭 시 스페이스 전환
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}
	else
	{
		// 아이콘이 없는 경우 텍스트 버튼
		const char* ButtonText = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(ButtonText, ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", TooltipText);
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderCameraOptionDropdownMenu()
{
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 0.7f);

	const ImVec2 IconSize(17, 17);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", ViewportName.ToString().c_str(), "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float HorizontalPadding = 8.0f;
	const float CameraDropdownWidth = IconSize.x + 4.0f + TextSize.x + HorizontalPadding * 2.0f;

	// 드롭다운 버튼 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (카메라 아이콘 + 현재 모드명 + 화살표)
	ImVec2 ButtonSize(CameraDropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewportModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewportModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("카메라 옵션");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰포트 모드에 따라 아이콘 선택
	UTexture* CurrentModeIcon = nullptr;
	switch (ViewportType)
	{
	case EViewportType::Perspective:
		CurrentModeIcon = IconCamera;
		break;
	case EViewportType::Orthographic_Top:
		CurrentModeIcon = IconTop;
		break;
	case EViewportType::Orthographic_Bottom:
		CurrentModeIcon = IconBottom;
		break;
	case EViewportType::Orthographic_Left:
		CurrentModeIcon = IconLeft;
		break;
	case EViewportType::Orthographic_Right:
		CurrentModeIcon = IconRight;
		break;
	case EViewportType::Orthographic_Front:
		CurrentModeIcon = IconFront;
		break;
	case EViewportType::Orthographic_Back:
		CurrentModeIcon = IconBack;
		break;
	default:
		CurrentModeIcon = IconCamera;
		break;
	}

	if (CurrentModeIcon && CurrentModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰포트 모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewportModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션 1: 원근 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "원근");
		ImGui::Separator();

		bool bIsPerspective = (ViewportType == EViewportType::Perspective);
		const char* RadioIcon = bIsPerspective ? "●" : "○";

		// 원근 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
		ImVec2 SelectableSize(180, 20);
		ImVec2 SelectableCursorPos = ImGui::GetCursorPos();

		if (ImGui::Selectable("##Perspective", bIsPerspective, 0, SelectableSize))
		{
			ViewportType = EViewportType::Perspective;
			ViewportName = "원근";
			if (ViewportClient)
			{
				ViewportClient->SetViewportType(ViewportType);
				ViewportClient->SetupCameraMode();
			}
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트를 원근 보기로 전환합니다.");
		}

		// Selectable 위에 내용 렌더링
		ImVec2 ContentPos = ImVec2(SelectableCursorPos.x + 4, SelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
		ImGui::SetCursorPos(ContentPos);

		ImGui::Text("%s", RadioIcon);
		ImGui::SameLine(0, 4);

		if (IconPerspective && IconPerspective->GetShaderResourceView())
		{
			ImGui::Image((void*)IconPerspective->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("원근");

		// --- 섹션 2: 직교 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "직교");
		ImGui::Separator();

		// 직교 모드 목록
		struct ViewportModeEntry {
			EViewportType type;
			const char* koreanName;
			UTexture** icon;
			const char* tooltip;
		};

		ViewportModeEntry orthographicModes[] = {
			{ EViewportType::Orthographic_Top, "상단", &IconTop, "뷰포트를 상단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Bottom, "하단", &IconBottom, "뷰포트를 하단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Left, "왼쪽", &IconLeft, "뷰포트를 왼쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Right, "오른쪽", &IconRight, "뷰포트를 오른쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Front, "정면", &IconFront, "뷰포트를 정면 보기로 전환합니다." },
			{ EViewportType::Orthographic_Back, "후면", &IconBack, "뷰포트를 후면 보기로 전환합니다." }
		};

		for (int i = 0; i < 6; i++)
		{
			const auto& mode = orthographicModes[i];
			bool bIsSelected = (ViewportType == mode.type);
			const char* RadioIcon = bIsSelected ? "●" : "○";

			// 직교 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
			char SelectableID[32];
			sprintf_s(SelectableID, "##Ortho%d", i);

			ImVec2 OrthoSelectableCursorPos = ImGui::GetCursorPos();

			if (ImGui::Selectable(SelectableID, bIsSelected, 0, SelectableSize))
			{
				ViewportType = mode.type;
				ViewportName = mode.koreanName;
				if (ViewportClient)
				{
					ViewportClient->SetViewportType(ViewportType);
					ViewportClient->SetupCameraMode();
				}
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", mode.tooltip);
			}

			// Selectable 위에 내용 렌더링
			ImVec2 OrthoContentPos = ImVec2(OrthoSelectableCursorPos.x + 4, OrthoSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
			ImGui::SetCursorPos(OrthoContentPos);

			ImGui::Text("%s", RadioIcon);
			ImGui::SameLine(0, 4);

			if (*mode.icon && (*mode.icon)->GetShaderResourceView())
			{
				ImGui::Image((void*)(*mode.icon)->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}

			ImGui::Text("%s", mode.koreanName);
		}

		// --- 섹션 3: 이동 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "이동");
		ImGui::Separator();

		ACameraActor* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
		if (Camera)
		{
			if (IconSpeed && IconSpeed->GetShaderResourceView())
			{
				ImGui::Image((void*)IconSpeed->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("카메라 이동 속도");

			float speed = Camera->GetCameraSpeed();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##CameraSpeed", &speed, 1.0f, 100.0f, "%.1f"))
			{
				Camera->SetCameraSpeed(speed);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("WASD 키로 카메라를 이동할 때의 속도 (1-100)");
			}
		}

		// --- 섹션 4: 뷰 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰");
		ImGui::Separator();

		if (Camera && Camera->GetCameraComponent())
		{
			UCameraComponent* camComp = Camera->GetCameraComponent();

			// FOV
			if (IconFOV && IconFOV->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFOV->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("필드 오브 뷰");

			float fov = camComp->GetFOV();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FOV", &fov, 30.0f, 120.0f, "%.1f"))
			{
				camComp->SetFOV(fov);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라 시야각 (30-120도)\n값이 클수록 넓은 범위가 보입니다");
			}

			// 근평면
			if (IconNearClip && IconNearClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconNearClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("근평면");

			float NearClip = camComp->GetNearClip();
			float FarClip = camComp->GetFarClip();
			ImGui::SetNextItemWidth(180);

			float NearMax = std::max(1.0f, FarClip * 0.5f);
			if (ImGui::SliderFloat("##NearClip", &NearClip, 0.1f, NearMax, "%.2f"))
			{
				camComp->SetClipPlanes(NearClip, FarClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 가까운 렌더링 거리\n이 값보다 가까운 오브젝트는 보이지 않습니다");
			}

			// 원평면
			if (IconFarClip && IconFarClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFarClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("원평면");

			ImGui::SetNextItemWidth(180);
			float FarMin = NearClip * 2.0f;
			if (ImGui::DragFloat("##FarClip", &FarClip, 100.0f, FarMin, 100000.0f, "%.0f"))
			{
				camComp->SetClipPlanes(NearClip, FarClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 먼 렌더링 거리 (드래그로 조절)\n이 값보다 먼 오브젝트는 보이지 않습니다");
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderViewModeDropdownMenu()
{
	if (!ViewportClient) return;

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(17, 17);

	// 현재 뷰모드 이름 및 아이콘 가져오기
	EViewMode CurrentViewMode = ViewportClient->GetViewMode();
	const char* CurrentViewModeName = "뷰모드";
	UTexture* CurrentViewModeIcon = nullptr;

	switch (CurrentViewMode)
	{
	case EViewMode::VMI_Lit_Gouraud:
	case EViewMode::VMI_Lit_Lambert:
	case EViewMode::VMI_Lit_Phong:
		CurrentViewModeName = "라이팅 포함";
		CurrentViewModeIcon = IconViewMode_Lit;
		break;
	case EViewMode::VMI_Unlit:
		CurrentViewModeName = "언릿";
		CurrentViewModeIcon = IconViewMode_Unlit;
		break;
	case EViewMode::VMI_Wireframe:
		CurrentViewModeName = "와이어프레임";
		CurrentViewModeIcon = IconViewMode_Wireframe;
		break;
	case EViewMode::VMI_WorldNormal:
		CurrentViewModeName = "월드 노멀";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	case EViewMode::VMI_SceneDepth:
		CurrentViewModeName = "씬 뎁스";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	}

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", CurrentViewModeName, "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (아이콘 + 텍스트)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("뷰모드 선택");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰모드 아이콘 표시
	if (CurrentViewModeIcon && CurrentViewModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentViewModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션: 뷰모드 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰모드");
		ImGui::Separator();

		// ===== Lit 메뉴 (서브메뉴 포함) =====
		bool bIsLitMode = (CurrentViewMode == EViewMode::VMI_Lit_Phong ||
			CurrentViewMode == EViewMode::VMI_Lit_Gouraud ||
			CurrentViewMode == EViewMode::VMI_Lit_Lambert);

		const char* LitRadioIcon = bIsLitMode ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 LitCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 LitSelectableSize(180, IconSize.y);

		// 호버 감지용 투명 Selectable
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		bool bLitHovered = ImGui::Selectable("##LitHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, LitSelectableSize);
		ImGui::PopStyleColor();

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(LitCursorPos);

		ImGui::Text("%s", LitRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Lit && IconViewMode_Lit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Lit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("라이팅포함"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "셰이딩 모델");
			ImGui::Separator();

			// PHONG
			bool bIsPhong = (CurrentViewMode == EViewMode::VMI_Lit_Phong);
			const char* PhongIcon = bIsPhong ? "●" : "○";
			char PhongLabel[32];
			sprintf_s(PhongLabel, "%s PHONG", PhongIcon);
			if (ImGui::MenuItem(PhongLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Phong);
				CurrentLitSubMode = 3;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("픽셀 단위 셰이딩 (Per-Pixel)\n스페큘러 하이라이트 포함");
			}

			// GOURAUD
			bool bIsGouraud = (CurrentViewMode == EViewMode::VMI_Lit_Gouraud);
			const char* GouraudIcon = bIsGouraud ? "●" : "○";
			char GouraudLabel[32];
			sprintf_s(GouraudLabel, "%s GOURAUD", GouraudIcon);
			if (ImGui::MenuItem(GouraudLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Gouraud);
				CurrentLitSubMode = 1;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("정점 단위 셰이딩 (Per-Vertex)\n부드러운 색상 보간");
			}

			// LAMBERT
			bool bIsLambert = (CurrentViewMode == EViewMode::VMI_Lit_Lambert);
			const char* LambertIcon = bIsLambert ? "●" : "○";
			char LambertLabel[32];
			sprintf_s(LambertLabel, "%s LAMBERT", LambertIcon);
			if (ImGui::MenuItem(LambertLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Lambert);
				CurrentLitSubMode = 2;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Diffuse만 계산하는 간단한 셰이딩\n스페큘러 하이라이트 없음");
			}

			ImGui::EndMenu();
		}

		// ===== Unlit 메뉴 =====
		bool bIsUnlit = (CurrentViewMode == EViewMode::VMI_Unlit);
		const char* UnlitRadioIcon = bIsUnlit ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 UnlitCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 UnlitSelectableSize(180, IconSize.y);

		if (ImGui::Selectable("##UnlitSelectableArea", false, 0, UnlitSelectableSize))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Unlit);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("조명 계산 없이 텍스처와 색상만 표시");
		}

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(UnlitCursorPos);

		ImGui::Text("%s", UnlitRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Unlit && IconViewMode_Unlit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Unlit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("언릿");

		// ===== Wireframe 메뉴 =====
		bool bIsWireframe = (CurrentViewMode == EViewMode::VMI_Wireframe);
		const char* WireframeRadioIcon = bIsWireframe ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 WireframeCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 WireframeSelectableSize(180, IconSize.y);

		if (ImGui::Selectable("##WireframeSelectableArea", false, 0, WireframeSelectableSize))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Wireframe);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("메시의 외곽선(에지)만 표시");
		}

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(WireframeCursorPos);

		ImGui::Text("%s", WireframeRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_Wireframe && IconViewMode_Wireframe->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Wireframe->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("와이어프레임");

		// ===== Buffer Visualization 메뉴 (서브메뉴 포함) =====
		bool bIsBufferVis = (CurrentViewMode == EViewMode::VMI_WorldNormal ||
			CurrentViewMode == EViewMode::VMI_SceneDepth);

		const char* BufferVisRadioIcon = bIsBufferVis ? "●" : "○";

		// Selectable로 감싸서 전체 호버링 영역 확보
		ImVec2 BufferVisCursorPos = ImGui::GetCursorScreenPos();
		ImVec2 BufferVisSelectableSize(180, IconSize.y);

		// 호버 감지용 투명 Selectable
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		bool bBufferVisHovered = ImGui::Selectable("##BufferVisHoverArea", false, ImGuiSelectableFlags_AllowItemOverlap, BufferVisSelectableSize);
		ImGui::PopStyleColor();

		// Selectable 위에 내용 렌더링 (순서: ● + [텍스처] + 텍스트)
		ImGui::SetCursorScreenPos(BufferVisCursorPos);

		ImGui::Text("%s", BufferVisRadioIcon);
		ImGui::SameLine(0, 4);

		if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("버퍼 시각화"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "버퍼 시각화");
			ImGui::Separator();

			// Scene Depth
			bool bIsSceneDepth = (CurrentViewMode == EViewMode::VMI_SceneDepth);
			const char* SceneDepthIcon = bIsSceneDepth ? "●" : "○";
			char SceneDepthLabel[32];
			sprintf_s(SceneDepthLabel, "%s 씬 뎁스", SceneDepthIcon);
			if (ImGui::MenuItem(SceneDepthLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_SceneDepth);
				CurrentBufferVisSubMode = 0;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("씬의 깊이 정보를 그레이스케일로 표시\n어두울수록 카메라에 가까움");
			}

			// World Normal
			bool bIsWorldNormal = (CurrentViewMode == EViewMode::VMI_WorldNormal);
			const char* WorldNormalIcon = bIsWorldNormal ? "●" : "○";
			char WorldNormalLabel[32];
			sprintf_s(WorldNormalLabel, "%s 월드 노멀", WorldNormalIcon);
			if (ImGui::MenuItem(WorldNormalLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_WorldNormal);
				CurrentBufferVisSubMode = 1;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("월드 공간의 노멀 벡터를 RGB로 표시\nR=X, G=Y, B=Z 축 방향");
			}

			ImGui::EndMenu();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderShowFlagDropdownMenu()
{
	if (!ViewportClient) return;

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(20, 20);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s", "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 드롭다운 버튼 생성 (아이콘 + 텍스트)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ShowFlagBtn", ButtonSize))
	{
		ImGui::OpenPopup("ShowFlagPopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Show Flag 설정");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// ShowFlag 아이콘 표시
	if (IconShowFlag && IconShowFlag->GetShaderResourceView())
	{
		ImGui::Image((void*)IconShowFlag->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== ShowFlag 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ShowFlagPopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		URenderSettings& RenderSettings = ViewportClient->GetWorld()->GetRenderSettings();

		// --- 디폴트 사용 (Reset) ---
		ImVec2 ResetCursorPos = ImGui::GetCursorScreenPos();
		if (ImGui::Selectable("##ResetDefault", false, 0, ImVec2(0, IconSize.y)))
		{
			// 기본 설정으로 복원
			RenderSettings.SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("모든 Show Flag를 기본 설정으로 복원합니다.");
		}

		// Selectable 위에 아이콘과 텍스트 그리기
		ImGui::SetCursorScreenPos(ResetCursorPos);
		if (IconRevert && IconRevert->GetShaderResourceView())
		{
			ImGui::Image((void*)IconRevert->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text("디폴트 사용");

		ImGui::Separator();

		// --- 뷰포트 통계 (Viewport Stats with Submenu) ---
		// 아이콘
		if (IconStats && IconStats->GetShaderResourceView())
		{
			ImGui::Image((void*)IconStats->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		if (ImGui::BeginMenu("  뷰포트 통계"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰포트 통계");
			ImGui::Separator();

			// 모두 숨김
			ImVec2 HideAllCursorPos = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##HideAllStats", false, 0, ImVec2(0, IconSize.y)))
			{
				UStatsOverlayD2D::Get().SetShowFPS(false);
				UStatsOverlayD2D::Get().SetShowMemory(false);
				UStatsOverlayD2D::Get().SetShowPicking(false);
				UStatsOverlayD2D::Get().SetShowDecal(false);
				UStatsOverlayD2D::Get().SetShowTileCulling(false);
				UStatsOverlayD2D::Get().SetShowLights(false);
				UStatsOverlayD2D::Get().SetShowShadow(false);
				UStatsOverlayD2D::Get().SetShowSkinning(false);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("모든 뷰포트 통계를 숨깁니다.");
			}

			// Selectable 위에 아이콘과 텍스트 그리기
			ImGui::SetCursorScreenPos(HideAllCursorPos);
			if (IconHide && IconHide->GetShaderResourceView())
			{
				ImGui::Image((void*)IconHide->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}
			ImGui::Text(" 모두 숨김");

			ImGui::Separator();

			// Individual stats checkboxes
			bool bFPS = UStatsOverlayD2D::Get().IsFPSVisible();
			if (ImGui::Checkbox(" FPS", &bFPS))
			{
				UStatsOverlayD2D::Get().ToggleFPS();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("프레임 속도와 프레임 시간을 표시합니다.");
			}

			bool bMemory = UStatsOverlayD2D::Get().IsMemoryVisible();
			if (ImGui::Checkbox(" MEMORY", &bMemory))
			{
				UStatsOverlayD2D::Get().ToggleMemory();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("메모리 사용량과 할당 횟수를 표시합니다.");
			}

			bool bPicking = UStatsOverlayD2D::Get().IsPickingVisible();
			if (ImGui::Checkbox(" PICKING", &bPicking))
			{
				UStatsOverlayD2D::Get().TogglePicking();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("오브젝트 선택 성능 통계를 표시합니다.");
			}

			bool bDecalStats = UStatsOverlayD2D::Get().IsDecalVisible();
			if (ImGui::Checkbox(" DECAL", &bDecalStats))
			{
				UStatsOverlayD2D::Get().ToggleDecal();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("데칼 렌더링 성능 통계를 표시합니다.");
			}

			bool bTileCullingStats = UStatsOverlayD2D::Get().IsTileCullingVisible();
			if (ImGui::Checkbox(" TILE CULLING", &bTileCullingStats))
			{
				UStatsOverlayD2D::Get().ToggleTileCulling();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 기반 라이트 컵링 통계를 표시합니다.");
			}

			bool bLightStats = UStatsOverlayD2D::Get().IsLightsVisible();
			if (ImGui::Checkbox(" LIGHTS", &bLightStats))
			{
				UStatsOverlayD2D::Get().ToggleLights();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("라이트 타입별 개수를 표시합니다.");
			}

			bool bShadowStats = UStatsOverlayD2D::Get().IsShadowVisible();
			if (ImGui::Checkbox(" SHADOWS", &bShadowStats))
			{
				UStatsOverlayD2D::Get().ToggleShadow();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("섀도우 맵 통계를 표시합니다. (섀도우 라이트 개수, 아틀라스 크기, 메모리 사용량)");
			}

			bool bSkinningStats = UStatsOverlayD2D::Get().IsSkinningVisible();
			if (ImGui::Checkbox(" SKINNING", &bSkinningStats))
			{
				UStatsOverlayD2D::Get().ToggleSkinning();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("스키닝 통계를 표시합니다.");
			}

			bool bParticle = UStatsOverlayD2D::Get().IsParticlesVisible();
			if (ImGui::Checkbox(" PARTICLE", &bParticle))
			{
				UStatsOverlayD2D::Get().ToggleParticles();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("입자 효과 통계를 표시합니다.");
			}

			ImGui::EndMenu();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트 성능 통계 표시 설정");
		}

		// --- 섹션: 일반 표시 플래그 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "일반 표시 플래그");
		ImGui::Separator();

		// BVH
		bool bBVH = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug);
		if (ImGui::Checkbox("##BVH", &bBVH))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BVHDebug);
		}
		ImGui::SameLine();
		if (IconBVH && IconBVH->GetShaderResourceView())
		{
			ImGui::Image((void*)IconBVH->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" BVH");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("BVH(Bounding Volume Hierarchy) 디버그 시각화를 표시합니다.");
		}

		// Grid
		bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
		if (ImGui::Checkbox("##Grid", &bGrid))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
		}
		ImGui::SameLine();
		if (IconGrid && IconGrid->GetShaderResourceView())
		{
			ImGui::Image((void*)IconGrid->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 그리드");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("월드 그리드를 표시합니다.");
		}

		// Decal
		bool bDecal = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
		if (ImGui::Checkbox("##Decal", &bDecal))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Decals);
		}
		ImGui::SameLine();
		if (IconDecal && IconDecal->GetShaderResourceView())
		{
			ImGui::Image((void*)IconDecal->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 데칼");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("데칼 렌더링을 표시합니다.");
		}

		// Static Mesh
		bool bStaticMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
		if (ImGui::Checkbox("##StaticMesh", &bStaticMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_StaticMeshes);
		}
		ImGui::SameLine();
		if (IconStaticMesh && IconStaticMesh->GetShaderResourceView())
		{
			ImGui::Image((void*)IconStaticMesh->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 스태틱 메시");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("스태틱 메시 렌더링을 표시합니다.");
			}

		// Skeletal Mesh
		bool bSkeletalMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_SkeletalMeshes);
		if (ImGui::Checkbox("##SkeletalMesh", &bSkeletalMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_SkeletalMeshes);
		}
		ImGui::SameLine();
		if (IconSkeletalMesh && IconSkeletalMesh->GetShaderResourceView())
		{
			ImGui::Image((void*)IconSkeletalMesh->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 스켈레탈 메시");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("스텔레탈 메시 렌더링을 표시합니다.");
		}

		// Particle
		bool bParticle = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Particles);
		if (ImGui::Checkbox("##Particle", &bParticle))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Particles);
		}
		ImGui::SameLine();
		if (IconParticle && IconParticle->GetShaderResourceView())
		{
			ImGui::Image((void*)IconParticle->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 파티클");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 시스템 렌더링을 표시합니다.");
		}

		// Billboard
		bool bBillboard = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Billboard);
		if (ImGui::Checkbox("##Billboard", &bBillboard))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Billboard);
		}
		ImGui::SameLine();
		if (IconBillboard && IconBillboard->GetShaderResourceView())
		{
			ImGui::Image((void*)IconBillboard->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 빌보드");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("빌보드 텍스트를 표시합니다.");
		}

		// EditorIcon
		bool bIcon = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_EditorIcon);
		if (ImGui::Checkbox("##Icon", &bIcon))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_EditorIcon);
		}
		ImGui::SameLine();
		if (IconEditorIcon && IconEditorIcon->GetShaderResourceView())
		{
			ImGui::Image((void*)IconEditorIcon->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 아이콘");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("에디터 전용 아이콘을 표시합니다.");
		}

		// Fog
		bool bFog = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
		if (ImGui::Checkbox("##Fog", &bFog))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Fog);
		}
		ImGui::SameLine();
		if (IconFog && IconFog->GetShaderResourceView())
		{
			ImGui::Image((void*)IconFog->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 포그");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("포그 효과를 표시합니다.");
		}

		// Bounds
		bool bBounds = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
		if (ImGui::Checkbox("##Bounds", &bBounds))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BoundingBoxes);
		}
		ImGui::SameLine();
		if (IconCollision && IconCollision->GetShaderResourceView())
		{
			ImGui::Image((void*)IconCollision->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 바운딩 박스");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("바운딩 박스를 표시합니다.");
		}

		// 그림자
		bool bShadows = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Shadows);
		if (ImGui::Checkbox("##Shadows", &bShadows))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Shadows);
		}
		ImGui::SameLine();
		if (IconShadow && IconShadow->GetShaderResourceView())
		{
			ImGui::Image((void*)IconShadow->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" 그림자");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("그림자를 표시합니다.");
		}

		// --- 섹션: 그래픽스 기능 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "그래픽스 기능");
		ImGui::Separator();

		// Depth of Field
		bool bDoF = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_DepthOfField);
		if (ImGui::Checkbox("##DoF", &bDoF))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_DepthOfField);
		}
		ImGui::SameLine();
		ImGui::Text(" 피사계 심도 (DoF)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("피사계 심도 효과를 적용합니다.");
		}

		// DoF가 활성화되면 파라미터 UI 표시
		if (bDoF && ViewportClient)
		{
			auto& DoFSettings = ViewportClient->GetEditorDoFSettings();
			DoFSettings.bEnabled = true;  // ShowFlag와 동기화

			ImGui::Indent(20.0f);
			ImGui::PushItemWidth(150.0f);

			// 모드 선택 (0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus, 4: ScreenPointFocus)
			const char* modeItems[] = { "Cinematic (시네마틱)", "Physical (물리 기반)", "Tilt-Shift (미니어처)", "Point Focus (World)", "Screen Point Focus (Local)" };
			ImGui::Combo("모드##DoF", &DoFSettings.DoFMode, modeItems, IM_ARRAYSIZE(modeItems));
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cinematic: 아티스트 친화적 선형 모델\nPhysical: 렌즈 물리학 기반 (과초점 거리 적용)\nTilt-Shift: 화면 위치 기반 (미니어처 효과)\nPoint Focus (World): 3D 월드 좌표 중심 구형 초점\nScreen Point Focus (Local): 화면 좌표 기반 초점");

			ImGui::Separator();

			if (DoFSettings.DoFMode == 0)  // Cinematic 모드
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Cinematic 모드 설정");

				ImGui::SliderFloat("초점 거리##DoF", &DoFSettings.FocusDistance, 10.0f, 2000.0f, "%.1f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("카메라로부터의 초점 거리입니다.");

				ImGui::SliderFloat("초점 범위##DoF", &DoFSettings.FocusRange, 1.0f, 500.0f, "%.1f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("선명하게 보이는 범위입니다.");

				ImGui::SliderFloat("근거리 블러##DoF", &DoFSettings.NearBlurScale, 0.0f, 0.1f, "%.3f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점보다 가까운 물체의 블러 강도입니다.");

				ImGui::SliderFloat("원거리 블러##DoF", &DoFSettings.FarBlurScale, 0.0f, 0.1f, "%.3f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점보다 먼 물체의 블러 강도입니다.");
			}
			else if (DoFSettings.DoFMode == 1)  // Physical 모드
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Physical 모드 설정");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "실제 카메라 렌즈 물리학 기반");

				ImGui::SliderFloat("초점 거리##DoFPhys", &DoFSettings.FocusDistance, 0.1f, 100.0f, "%.2f m");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점이 맞는 피사체까지의 거리 (미터)");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "렌즈 파라미터");

				// 초점 거리 (Focal Length) - 프리셋 사용
				const char* focalItems[] = { "24mm (광각)", "35mm (광각)", "50mm (표준)", "85mm (인물)", "135mm (망원)" };
				int focalIndex = 2;  // 기본값 50mm
				if (DoFSettings.FocalLength <= 24.0f) focalIndex = 0;
				else if (DoFSettings.FocalLength <= 35.0f) focalIndex = 1;
				else if (DoFSettings.FocalLength <= 50.0f) focalIndex = 2;
				else if (DoFSettings.FocalLength <= 85.0f) focalIndex = 3;
				else focalIndex = 4;

				if (ImGui::Combo("렌즈 초점거리##DoF", &focalIndex, focalItems, IM_ARRAYSIZE(focalItems)))
				{
					const float focalValues[] = { 24.0f, 35.0f, 50.0f, 85.0f, 135.0f };
					DoFSettings.FocalLength = focalValues[focalIndex];
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("렌즈의 초점거리 (mm)\n큰 값 = 배경 블러 증가");

				// F-Number (조리개)
				const char* fnumberItems[] = { "f/1.4 (밝음)", "f/2.0", "f/2.8", "f/4.0", "f/5.6", "f/8.0 (선명)" };
				int fnumberIndex = 2;  // 기본값 f/2.8
				if (DoFSettings.FNumber <= 1.4f) fnumberIndex = 0;
				else if (DoFSettings.FNumber <= 2.0f) fnumberIndex = 1;
				else if (DoFSettings.FNumber <= 2.8f) fnumberIndex = 2;
				else if (DoFSettings.FNumber <= 4.0f) fnumberIndex = 3;
				else if (DoFSettings.FNumber <= 5.6f) fnumberIndex = 4;
				else fnumberIndex = 5;

				if (ImGui::Combo("조리개 (F-Number)##DoF", &fnumberIndex, fnumberItems, IM_ARRAYSIZE(fnumberItems)))
				{
					const float fnumberValues[] = { 1.4f, 2.0f, 2.8f, 4.0f, 5.6f, 8.0f };
					DoFSettings.FNumber = fnumberValues[fnumberIndex];
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("조리개 값\n낮은 값 = 배경 블러 증가\n높은 값 = 전체적으로 선명 (과초점 거리 효과)");

				// 센서 크기
				const char* sensorItems[] = { "Full Frame (36mm)", "APS-C (23.6mm)", "Micro 4/3 (17.3mm)" };
				int sensorIndex = 0;  // 기본값 Full Frame
				if (DoFSettings.SensorWidth >= 36.0f) sensorIndex = 0;
				else if (DoFSettings.SensorWidth >= 23.6f) sensorIndex = 1;
				else sensorIndex = 2;

				if (ImGui::Combo("센서 크기##DoF", &sensorIndex, sensorItems, IM_ARRAYSIZE(sensorItems)))
				{
					const float sensorValues[] = { 36.0f, 23.6f, 17.3f };
					DoFSettings.SensorWidth = sensorValues[sensorIndex];
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("카메라 센서 크기\n큰 센서 = 배경 블러 증가");
			}
			else if (DoFSettings.DoFMode == 2)  // Tilt-Shift 모드
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tilt-Shift 모드 설정");

				ImGui::SliderFloat("중심 위치##DoF", &DoFSettings.TiltShiftCenterY, 0.0f, 1.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("선명한 띠의 중심 위치입니다.\n0 = 상단, 0.5 = 중앙, 1 = 하단");

				ImGui::SliderFloat("띠 너비##DoF", &DoFSettings.TiltShiftBandWidth, 0.05f, 0.8f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("선명하게 보이는 띠의 너비입니다.");

				ImGui::SliderFloat("블러 강도##DoF", &DoFSettings.TiltShiftBlurScale, 1.0f, 20.0f, "%.1f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("띠 바깥쪽의 블러 강도입니다.");
			}
			else if (DoFSettings.DoFMode == 3)  // PointFocus 모드 (World Space)
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Point Focus (World) 모드 설정");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "특정 3D 좌표 중심으로 구형 초점 영역");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "초점 지점 (World Space)");

				ImGui::SliderFloat("X##FocusPoint", &DoFSettings.FocusPoint.X, -100.0f, 100.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 지점의 X 좌표 (World Space)");

				ImGui::SliderFloat("Y##FocusPoint", &DoFSettings.FocusPoint.Y, -100.0f, 100.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 지점의 Y 좌표 (World Space)");

				ImGui::SliderFloat("Z##FocusPoint", &DoFSettings.FocusPoint.Z, -100.0f, 100.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 지점의 Z 좌표 (World Space)");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "초점 영역 설정");

				ImGui::SliderFloat("초점 반경##DoF", &DoFSettings.FocusRadius, 0.1f, 20.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 지점 주변에서 선명하게 보이는 구형 반경입니다.");

				ImGui::SliderFloat("블러 강도##DoFPF", &DoFSettings.PointFocusBlurScale, 0.1f, 5.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 반경 바깥쪽의 블러 강도입니다.");

				ImGui::SliderFloat("감쇠 곡선##DoF", &DoFSettings.PointFocusFalloff, 0.5f, 3.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("블러 증가 곡선입니다.\n1.0 = 선형\n2.0 = 제곱 (더 급격한 변화)\n0.5 = 루트 (더 완만한 변화)");
			}
			else  // ScreenPointFocus 모드 (Screen Space / Local)
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Screen Point Focus (Local) 모드 설정");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "화면 좌표 기반 초점 영역 (Screen Space)");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "초점 지점 (Screen Space)");

				ImGui::SliderFloat("X (화면)##ScreenFocus", &DoFSettings.ScreenFocusPoint.X, 0.0f, 1.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("화면 X 좌표 (0 = 왼쪽, 0.5 = 중앙, 1 = 오른쪽)");

				ImGui::SliderFloat("Y (화면)##ScreenFocus", &DoFSettings.ScreenFocusPoint.Y, 0.0f, 1.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("화면 Y 좌표 (0 = 상단, 0.5 = 중앙, 1 = 하단)");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "초점 영역 설정");

				ImGui::SliderFloat("초점 반경##ScreenFocus", &DoFSettings.ScreenFocusRadius, 0.01f, 0.5f, "%.3f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("화면 상의 초점 반경 (0~1, 화면 비율 기준)");

				ImGui::SliderFloat("깊이 범위##ScreenFocus", &DoFSettings.ScreenFocusDepthRange, 1.0f, 500.0f, "%.1f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 지점 주변의 깊이 허용 범위입니다.");

				ImGui::SliderFloat("블러 강도##ScreenFocus", &DoFSettings.ScreenFocusBlurScale, 0.1f, 5.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 영역 바깥쪽의 블러 강도입니다.");

				ImGui::SliderFloat("감쇠 곡선##ScreenFocus", &DoFSettings.ScreenFocusFalloff, 0.5f, 3.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("블러 증가 곡선입니다.\n1.0 = 선형\n2.0 = 제곱");

				ImGui::SliderFloat("종횡비##ScreenFocus", &DoFSettings.ScreenFocusAspectRatio, 0.5f, 2.0f, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("초점 영역 형태입니다.\n1.0 = 원형\n< 1 = 세로로 긴 타원\n> 1 = 가로로 긴 타원");
			}

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "공통 설정");

			ImGui::SliderFloat("최대 블러##DoF", &DoFSettings.MaxBlurRadius, 1.0f, 20.0f, "%.1f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("최대 블러 반경 (픽셀)입니다.");

			ImGui::SliderFloat("보케 크기##DoF", &DoFSettings.BokehSize, 0.5f, 3.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("보케 효과의 크기입니다.");

			// 블러 방식 선택
			const char* blurMethodItems[] = { "Disc 12 (빠름)", "Disc 24 (고품질)", "Gaussian (자연스러움)", "Hexagonal (6각형 보케)", "Circular Gather (최고품질)" };
			ImGui::Combo("블러 방식##DoF", &DoFSettings.BlurMethod, blurMethodItems, IM_ARRAYSIZE(blurMethodItems));
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Disc 12: 12샘플, 빠른 성능\nDisc 24: 24샘플, 고품질\nGaussian: 가중치 기반, 자연스러운 블러\nHexagonal: 6각형 보케, 아나모픽 렌즈 스타일\nCircular Gather: 48샘플, 최고 품질 (느림)");

			// 번짐 처리 방식 선택 (Bleeding Method)
			const char* bleedingMethodItems[] = { "None (기본)", "Scatter-as-Gather (물리 기반)" };
			ImGui::Combo("번짐 처리##DoF", &DoFSettings.BleedingMethod, bleedingMethodItems, IM_ARRAYSIZE(bleedingMethodItems));
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("None: 선명한 픽셀은 항상 선명 (빠름)\nScatter-as-Gather: 흐릿한 물체가 선명한 영역으로 번짐 (물리 기반, 느림)");

			ImGui::PopItemWidth();
			ImGui::Unindent(20.0f);
		}
		else if (ViewportClient)
		{
			ViewportClient->GetEditorDoFSettings().bEnabled = false;
		}

		ImGui::Separator();

		// FXAA (Anti-Aliasing)
		bool bFXAA = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);
		if (ImGui::Checkbox("##FXAA", &bFXAA))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_FXAA);
		}
		ImGui::SameLine();
		if (IconAntiAliasing && IconAntiAliasing->GetShaderResourceView())
		{
			ImGui::Image((void*)IconAntiAliasing->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴 (NVIDIA FXAA 3.11 style)
		if (ImGui::BeginMenu(" 안티 에일리어싱"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "안티 에일리어싱 (FXAA 3.11)");
			ImGui::Separator();

			// SpanMax 슬라이더 (최대 탐색 범위)
			float spanMax = RenderSettings.GetFXAASpanMax();
			ImGui::Text("최대 탐색 범위");
			if (ImGui::SliderFloat("##SpanMax", &spanMax, 2.0f, 16.0f, "%.1f"))
			{
				RenderSettings.SetFXAASpanMax(spanMax);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("엣지 탐색 최대 거리 (픽셀 단위)\n높을수록 긴 엣지를 더 잘 처리합니다. (권장: 8.0)");
			}

			// ReduceMul 슬라이더 (감쇠 배율)
			float reduceMul = RenderSettings.GetFXAAReduceMul();
			ImGui::Text("감쇠 배율");
			if (ImGui::SliderFloat("##ReduceMul", &reduceMul, 0.0f, 0.5f, "%.4f"))
			{
				RenderSettings.SetFXAAReduceMul(reduceMul);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("방향 벡터 감쇠 배율\n높을수록 더 안정적이지만 덜 정밀합니다. (권장: 0.125 = 1/8)");
			}

			// ReduceMin 슬라이더 (최소 감쇠값)
			float reduceMin = RenderSettings.GetFXAAReduceMin();
			ImGui::Text("최소 감쇠값");
			if (ImGui::SliderFloat("##ReduceMin", &reduceMin, 0.001f, 0.05f, "%.4f"))
			{
				RenderSettings.SetFXAAReduceMin(reduceMin);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("최소 감쇠 바닥값\n노이즈 영역에서 과민 반응을 방지합니다. (권장: 0.0078 = 1/128)");
			}

			ImGui::Separator();

			// 품질 프리셋 버튼
			ImGui::Text("품질 프리셋");

			// 고품질 버튼
			if (ImGui::Button("고품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAASpanMax(12.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 16.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 256.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("최고 품질 설정 (성능 낮음)\nSpanMax: 12, ReduceMul: 1/16, ReduceMin: 1/256");
			}

			ImGui::SameLine();

			// 중품질 버튼
			if (ImGui::Button("중품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAASpanMax(8.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 8.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 128.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("균형잡힌 설정 (기본값)\nSpanMax: 8, ReduceMul: 1/8, ReduceMin: 1/128");
			}

			ImGui::SameLine();

			// 저품질 버튼
			if (ImGui::Button("저품질", ImVec2(60, 0)))
			{
				RenderSettings.SetFXAASpanMax(4.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 4.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 64.0f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("빠른 처리 설정 (성능 높음)\nSpanMax: 4, ReduceMul: 1/4, ReduceMin: 1/64");
			}

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("FXAA 상세 설정");
		}

		// Tile-Based Light Culling
		bool bTileCulling = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCulling);
		if (ImGui::Checkbox("##TileCulling", &bTileCulling))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCulling);
		}
		ImGui::SameLine();
		if (IconTile && IconTile->GetShaderResourceView())
		{
			ImGui::Image((void*)IconTile->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴
		if (ImGui::BeginMenu(" 타일 기반 라이트 컬링"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "타일 기반 라이트 컬링");
			ImGui::Separator();

			// 디버그 시각화 체크박스
			bool bDebugVis = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug);
			if (ImGui::Checkbox(" 디버그 시각화", &bDebugVis))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCullingDebug);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 컬링 결과를 화면에 색상으로 시각화합니다.");
			}

			ImGui::Separator();

			// 타일 크기 입력
			static int tempTileSize = RenderSettings.GetTileSize();
			ImGui::Text("타일 크기 (픽셀)");
			ImGui::SetNextItemWidth(100);
			ImGui::InputInt("##TileSize", &tempTileSize, 1, 8);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일의 크기를 픽셀 단위로 설정합니다. (일반적: 8, 16, 32)");
			}

			// 적용 버튼
			ImGui::SameLine();
			if (ImGui::Button("적용"))
			{
				if (tempTileSize >= 4 && tempTileSize <= 64)
				{
					RenderSettings.SetTileSize(tempTileSize);
					// TileLightCuller는 매 프레임 생성되므로 다음 프레임에 자동 적용됨
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("타일 크기를 적용합니다. (4~64 사이 값)");
			}

			// 현재 설정값 표시
			ImGui::Text("현재 타일 크기: %d x %d", RenderSettings.GetTileSize(), RenderSettings.GetTileSize());

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("타일 기반 라이트 컬링 설정");
		}

		// ===== 그림자 안티 에일리어싱 =====
		bool bShadowAA = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_ShadowAntiAliasing);
		if (ImGui::Checkbox("##ShadowAA", &bShadowAA))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_ShadowAntiAliasing);
		}
		ImGui::SameLine();
		if (IconShadowAA && IconShadowAA->GetShaderResourceView())
		{
			ImGui::Image((void*)IconShadowAA->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		// 서브메뉴
		if (ImGui::BeginMenu(" 그림자 안티 에일리어싱"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "그림자 안티 에일리어싱");
			ImGui::Separator();

			// RadioButton과 바인딩할 int 변수 준비
			EShadowAATechnique currentTechnique = RenderSettings.GetShadowAATechnique();
			int techniqueInt = static_cast<int>(currentTechnique);
			const int oldTechniqueInt = techniqueInt; // 변경 감지를 위한 원본 값

			// RadioButton으로 변경 (int 값 0에 바인딩)
			ImGui::RadioButton(" PCF (Percentage-Closer Filtering)", &techniqueInt, static_cast<int>(EShadowAATechnique::PCF));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("부드러운 그림자 가장자리를 생성합니다. (기본값)");
			}

			// RadioButton으로 변경 (int 값 1에 바인딩)
			ImGui::RadioButton(" VSM (Variance Shadow Maps)", &techniqueInt, static_cast<int>(EShadowAATechnique::VSM));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("매우 부드러운 그림자를 생성하지만, 라이트 블리딩 문제가 발생할 수 있습니다.");
			}

			// RadioButton 클릭으로 int 값이 변경되었다면 RenderSettings 업데이트
			if (techniqueInt != oldTechniqueInt)
			{
				RenderSettings.SetShadowAATechnique(static_cast<EShadowAATechnique>(techniqueInt));
			}

			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("그림자 안티 에일리어싱 기술 설정");
		}

		// ===== 스키닝 모드 =====
		bool bIsGpuSkinning = (RenderSettings.GetSkinningMode() == ESkinningMode::GPU);
		if (ImGui::Checkbox("##GPUSkinning", &bIsGpuSkinning))
		{
			// 체크박스 상태에 따라 스키닝 모드 변경
			ESkinningMode newMode = bIsGpuSkinning ? ESkinningMode::GPU : ESkinningMode::CPU;
			RenderSettings.SetSkinningMode(newMode);
		}
		ImGui::SameLine();
		if (IconSkinning && IconSkinning->GetShaderResourceView())
		{
			ImGui::Image((void*)IconSkinning->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}
		ImGui::Text(" GPU 스키닝");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("CPU 또는 GPU 스키닝 방식을 선택합니다.");
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderViewportLayoutSwitchButton()
{
	ImVec2 switchCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(switchCursorPos.y - 0.7f);

	const ImVec2 IconSize(17, 17);

	// 현재 레이아웃 모드에 따라 아이콘 선택
	// FourSplit일 때 → SingleViewport 아이콘 표시 (클릭하면 단일로 전환)
	// SingleMain일 때 → MultiViewport 아이콘 표시 (클릭하면 멀티로 전환)
	EViewportLayoutMode CurrentMode = SLATE.GetCurrentLayoutMode();
	UTexture* SwitchIcon = (CurrentMode == EViewportLayoutMode::FourSplit) ? IconMultiToSingleViewport : IconSingleToMultiViewport;
	const char* TooltipText = (CurrentMode == EViewportLayoutMode::FourSplit) ? "단일 뷰포트로 전환" : "멀티 뷰포트로 전환";

	// 버튼 너비 계산 (아이콘 + 패딩)
	const float Padding = 8.0f;
	const float ButtonWidth = IconSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	// 버튼
	ImVec2 ButtonSize(ButtonWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	if (ImGui::Button("##SwitchLayout", ButtonSize))
	{
		SLATE.SwitchPanel(this);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(TooltipText);
	}

	// 버튼 위에 아이콘 렌더링 (중앙 정렬)
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - IconSize.x) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 아이콘 표시
	if (SwitchIcon && SwitchIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)SwitchIcon->GetShaderResourceView(), IconSize);
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);
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
