#include "pch.h"
#include "ViewportToolbarWidget.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Runtime/Renderer/FViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/CameraComponent.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Slate/USlateManager.h"
#include "Source/Slate/Windows/ViewportWindow.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Engine/GameFramework/PlayerCameraManager.h"
#include "Source/Runtime/Engine/GameFramework/Camera/CamMod_DepthOfField.h"
#include "Source/Slate/StatsOverlayD2D.h"
#include "ImGui/imgui.h"

SViewportToolbarWidget::SViewportToolbarWidget() = default;

SViewportToolbarWidget::~SViewportToolbarWidget()
{
	// 아이콘 해제는 UObject GC에서 처리
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
	IconSkeletalMesh = nullptr;
	IconBillboard = nullptr;
	IconEditorIcon = nullptr;
	IconFog = nullptr;
	IconCollision = nullptr;
	IconAntiAliasing = nullptr;
	IconTile = nullptr;
	IconShadow = nullptr;
	IconShadowAA = nullptr;
	IconSkinning = nullptr;
	IconParticle = nullptr;
	IconSingleToMultiViewport = nullptr;
	IconMultiToSingleViewport = nullptr;
}

void SViewportToolbarWidget::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;
	LoadIcons(Device);
}

void SViewportToolbarWidget::LoadIcons(ID3D11Device* InDevice)
{
	if (!InDevice)
	{
		return;
	}

	// 기즈모 아이콘
	IconSelect = NewObject<UTexture>();
	IconSelect->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Select.dds", InDevice, false);

	IconMove = NewObject<UTexture>();
	IconMove->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Move.dds", InDevice, false);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Rotate.dds", InDevice, false);

	IconScale = NewObject<UTexture>();
	IconScale->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Scale.dds", InDevice, false);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load(GDataDir + "/Default/Icon/WorldSpace.dds", InDevice, false);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load(GDataDir + "/Default/Icon/LocalSpace.dds", InDevice, false);

	// 카메라/뷰포트 모드 아이콘
	IconCamera = NewObject<UTexture>();
	IconCamera->Load(GDataDir + "/Default/Icon/Viewport_Mode_Camera.dds", InDevice, false);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load(GDataDir + "/Default/Icon/Viewport_Mode_Perspective.dds", InDevice, false);

	IconTop = NewObject<UTexture>();
	IconTop->Load(GDataDir + "/Default/Icon/Viewport_Mode_Top.dds", InDevice, false);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load(GDataDir + "/Default/Icon/Viewport_Mode_Bottom.dds", InDevice, false);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load(GDataDir + "/Default/Icon/Viewport_Mode_Left.dds", InDevice, false);

	IconRight = NewObject<UTexture>();
	IconRight->Load(GDataDir + "/Default/Icon/Viewport_Mode_Right.dds", InDevice, false);

	IconFront = NewObject<UTexture>();
	IconFront->Load(GDataDir + "/Default/Icon/Viewport_Mode_Front.dds", InDevice, false);

	IconBack = NewObject<UTexture>();
	IconBack->Load(GDataDir + "/Default/Icon/Viewport_Mode_Back.dds", InDevice, false);

	// 카메라 설정 아이콘
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load(GDataDir + "/Default/Icon/CameraSpeed_16.dds", InDevice, false);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load(GDataDir + "/Default/Icon/Viewport_Setting_FOV.dds", InDevice, false);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_NearClip.dds", InDevice, false);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_FarClip.dds", InDevice, false);

	// 뷰모드 아이콘
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Lit.dds", InDevice, false);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Unlit.dds", InDevice, false);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load(GDataDir + "/Default/Icon/Icon_ViewMode_Wireframe.dds", InDevice, false);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_BufferVis.dds", InDevice, false);

	// ShowFlag 아이콘
	IconShowFlag = NewObject<UTexture>();
	IconShowFlag->Load(GDataDir + "/Default/Icon/Viewport_ShowFlag.dds", InDevice, false);

	IconRevert = NewObject<UTexture>();
	IconRevert->Load(GDataDir + "/Default/Icon/Viewport_Revert.png", InDevice);

	IconStats = NewObject<UTexture>();
	IconStats->Load(GDataDir + "/Default/Icon/Viewport_Stats.png", InDevice);

	IconHide = NewObject<UTexture>();
	IconHide->Load(GDataDir + "/Default/Icon/Viewport_Hide.png", InDevice);

	IconBVH = NewObject<UTexture>();
	IconBVH->Load(GDataDir + "/Default/Icon/Viewport_BVH.png", InDevice);

	IconGrid = NewObject<UTexture>();
	IconGrid->Load(GDataDir + "/Default/Icon/Viewport_Grid.png", InDevice);

	IconDecal = NewObject<UTexture>();
	IconDecal->Load(GDataDir + "/Default/Icon/Viewport_Decal.png", InDevice);

	IconStaticMesh = NewObject<UTexture>();
	IconStaticMesh->Load(GDataDir + "/Default/Icon/Viewport_StaticMesh.png", InDevice);

	IconSkeletalMesh = NewObject<UTexture>();
	IconSkeletalMesh->Load(GDataDir + "/Default/Icon/Viewport_SkeletalMesh.png", InDevice);

	IconBillboard = NewObject<UTexture>();
	IconBillboard->Load(GDataDir + "/Default/Icon/Viewport_Billboard.png", InDevice);

	IconEditorIcon = NewObject<UTexture>();
	IconEditorIcon->Load(GDataDir + "/Default/Icon/Viewport_EditorIcon.png", InDevice);

	IconFog = NewObject<UTexture>();
	IconFog->Load(GDataDir + "/Default/Icon/Viewport_Fog.png", InDevice);

	IconCollision = NewObject<UTexture>();
	IconCollision->Load(GDataDir + "/Default/Icon/Viewport_Collision.png", InDevice);

	IconAntiAliasing = NewObject<UTexture>();
	IconAntiAliasing->Load(GDataDir + "/Default/Icon/Viewport_AntiAliasing.png", InDevice);

	IconTile = NewObject<UTexture>();
	IconTile->Load(GDataDir + "/Default/Icon/Viewport_Tile.png", InDevice);

	IconShadow = NewObject<UTexture>();
	IconShadow->Load(GDataDir + "/Default/Icon/Viewport_Shadow.png", InDevice);

	IconShadowAA = NewObject<UTexture>();
	IconShadowAA->Load(GDataDir + "/Default/Icon/Viewport_ShadowAA.png", InDevice);

	IconSkinning = NewObject<UTexture>();
	IconSkinning->Load(GDataDir + "/Default/Icon/Viewport_Skinning.png", InDevice);

	IconParticle = NewObject<UTexture>();
	IconParticle->Load(GDataDir + "/Default/Icon/Viewport_Particle.dds", InDevice);

	// 뷰포트 레이아웃 아이콘
	IconSingleToMultiViewport = NewObject<UTexture>();
	IconSingleToMultiViewport->Load(GDataDir + "/Default/Icon/Viewport_SingleToMultiViewport.dds", InDevice, false);

	IconMultiToSingleViewport = NewObject<UTexture>();
	IconMultiToSingleViewport->Load(GDataDir + "/Default/Icon/Viewport_MultiToSingleViewport.dds", InDevice, false);
}

void SViewportToolbarWidget::Render(FViewportClient* ViewportClient, AGizmoActor* GizmoActor,
	bool bShowCameraDropdown, bool bShowLayoutSwitch,
	EViewportType ViewportType, const char* ViewportName,
	SViewportWindow* OwnerViewport)
{
	if (!ViewportClient)
	{
		return;
	}

	// 기즈모 버튼 렌더링 (GizmoActor가 있을 때만)
	if (GizmoActor)
	{
		// 기즈모 버튼 스타일 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

		// 수직 중앙 정렬
		constexpr float ButtonHeight = 23.0f;
		float VerticalPadding = (ToolbarHeight - ButtonHeight) * 0.5f;
		ImGui::SetCursorPosY(VerticalPadding);

		// 기즈모 모드 버튼들
		RenderGizmoModeButtons(GizmoActor);

		// 구분선
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
		ImGui::SameLine();

		// 기즈모 스페이스 버튼
		RenderGizmoSpaceButton(GizmoActor);

		// 기즈모 버튼 스타일 복원
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	// === 오른쪽 정렬 버튼들 ===
	constexpr float ShowFlagButtonWidth = 50.0f;
	constexpr float ButtonSpacing = 8.0f;

	// 현재 ViewMode 이름으로 실제 너비 계산
	const char* CurrentViewModeName = "Lit";
	EViewMode CurrentViewMode = ViewportClient->GetViewMode();
	switch (CurrentViewMode)
	{
	case EViewMode::VMI_Lit_Gouraud:
	case EViewMode::VMI_Lit_Lambert:
	case EViewMode::VMI_Lit_Phong:
		CurrentViewModeName = "Lit";
		break;
	case EViewMode::VMI_Unlit:
		CurrentViewModeName = "Unlit";
		break;
	case EViewMode::VMI_Wireframe:
		CurrentViewModeName = "Wire";
		break;
	case EViewMode::VMI_WorldNormal:
		CurrentViewModeName = "Normal";
		break;
	case EViewMode::VMI_SceneDepth:
		CurrentViewModeName = "Depth";
		break;
	}

	char viewModeText[64];
	sprintf_s(viewModeText, "%s %s", CurrentViewModeName, "∨");
	ImVec2 viewModeTextSize = ImGui::CalcTextSize(viewModeText);
	const float ViewModeButtonWidth = 17.0f + 4.0f + viewModeTextSize.x + 16.0f;

	// Speed 버튼 너비 계산
	float finalSpeed = CalculateCameraSpeed(ViewportClient);
	char speedText[16];
	if (finalSpeed >= 100.0f)
	{
		sprintf_s(speedText, "%.0f", finalSpeed);
	}
	else if (finalSpeed >= 10.0f)
	{
		sprintf_s(speedText, "%.1f", finalSpeed);
	}
	else
	{
		sprintf_s(speedText, "%.2f", finalSpeed);
	}
	ImVec2 speedTextSize = ImGui::CalcTextSize(speedText);
	constexpr float SpeedIconSize = 14.0f;
	constexpr float SpeedIconTextSpacing = 4.0f;
	constexpr float SpeedHorizontalPadding = 6.0f;
	const float SpeedButtonWidth = SpeedHorizontalPadding + SpeedIconSize + SpeedIconTextSpacing + speedTextSize.x + SpeedHorizontalPadding;

	// Camera 버튼 너비 계산 (옵션)
	float CameraButtonWidth = 0.0f;
	if (bShowCameraDropdown)
	{
		char cameraText[64];
		sprintf_s(cameraText, "%s %s", ViewportName, "∨");
		ImVec2 cameraTextSize = ImGui::CalcTextSize(cameraText);
		CameraButtonWidth = 17.0f + 4.0f + cameraTextSize.x + 16.0f;
	}

	float AvailableWidth = ImGui::GetContentRegionAvail().x;
	float CursorStartX = ImGui::GetCursorPosX();

	// 수직 정렬을 위해 Y 위치 재설정
	constexpr float ButtonHeight = 23.0f;
	float VerticalPadding = (ToolbarHeight - ButtonHeight) * 0.5f;
	ImGui::SetCursorPosY(VerticalPadding);

	ImVec2 CurrentCursor = ImGui::GetCursorPos();

	// 오른쪽부터 역순으로 위치 계산
	float RightEdge = CursorStartX + AvailableWidth;
	float CurrentX = RightEdge;

	// Layout은 제일 오른쪽 (옵션)
	if (bShowLayoutSwitch)
	{
		constexpr float LayoutButtonWidth = 33.0f;
		CurrentX -= LayoutButtonWidth;
	}
	float LayoutX = CurrentX;

	// ShowFlag
	if (bShowLayoutSwitch)
	{
		CurrentX -= ButtonSpacing;
	}
	CurrentX -= ShowFlagButtonWidth;
	float ShowFlagX = CurrentX;

	// ViewMode
	CurrentX -= ButtonSpacing + ViewModeButtonWidth;
	float ViewModeX = CurrentX;

	// Speed
	CurrentX -= ButtonSpacing + SpeedButtonWidth;
	float SpeedX = CurrentX;

	// Camera (옵션)
	float CameraX = CurrentX;
	if (bShowCameraDropdown)
	{
		CurrentX -= ButtonSpacing + CameraButtonWidth;
		CameraX = CurrentX;
	}

	// 버튼들을 순서대로 그리기
	if (bShowCameraDropdown)
	{
		ImGui::SetCursorPos(ImVec2(CameraX, CurrentCursor.y));
		RenderCameraOptionDropdownMenu(ViewportClient, ViewportType, ViewportName, OwnerViewport);
	}

	ImGui::SetCursorPos(ImVec2(SpeedX, CurrentCursor.y));
	RenderCameraSpeedButton(ViewportClient);

	ImGui::SetCursorPos(ImVec2(ViewModeX, CurrentCursor.y));
	RenderViewModeDropdownMenu(ViewportClient);

	ImGui::SetCursorPos(ImVec2(ShowFlagX, CurrentCursor.y));
	RenderShowFlagDropdownMenu(ViewportClient);

	if (bShowLayoutSwitch)
	{
		ImGui::SetCursorPos(ImVec2(LayoutX, CurrentCursor.y));
		RenderViewportLayoutSwitchButton(OwnerViewport);
	}
}

void SViewportToolbarWidget::RenderGizmoModeButtons(AGizmoActor* GizmoActor) const
{
	constexpr ImVec2 IconSize(17, 17);

	EGizmoMode CurrentGizmoMode = GizmoActor ? GizmoActor->GetMode() : EGizmoMode::Select;

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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Select [Q]");
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Translate [W]");
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Rotate [E]");
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Scale [R]");
	}
	ImGui::SameLine();
}

void SViewportToolbarWidget::RenderGizmoSpaceButton(AGizmoActor* GizmoActor) const
{
	constexpr ImVec2 IconSize(17, 17);

	EGizmoSpace CurrentGizmoSpace = GizmoActor ? GizmoActor->GetSpace() : EGizmoSpace::World;
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "World Space [Ctrl+`]" : "Local Space [Ctrl+`]";

	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetSpace(bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", TooltipText);
	}
	ImGui::SameLine();
}

float SViewportToolbarWidget::CalculateCameraSpeed(const FViewportClient* ViewportClient) const
{
	float Multiplier = SPEED_MULTIPLIERS[CameraSpeedSetting - 1];
	float Scalar = ViewportClient ? ViewportClient->GetCameraSpeedScalar() : 1.0f;
	return BASE_CAMERA_SPEED * Multiplier * Scalar;
}

void SViewportToolbarWidget::RenderCameraSpeedButton(FViewportClient* ViewportClient)
{
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 0.7f);

	float FinalSpeed = CalculateCameraSpeed(ViewportClient);
	char ButtonText[16];
	if (FinalSpeed >= 100.0f)
	{
		sprintf_s(ButtonText, "%.0f", FinalSpeed);
	}
	else if (FinalSpeed >= 10.0f)
	{
		sprintf_s(ButtonText, "%.1f", FinalSpeed);
	}
	else
	{
		sprintf_s(ButtonText, "%.2f", FinalSpeed);
	}

	constexpr float IconSizeF = 14.0f;
	constexpr float IconTextSpacing = 4.0f;
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	constexpr float HorizontalPadding = 6.0f;
	const float ButtonWidth = HorizontalPadding + IconSizeF + IconTextSpacing + TextSize.x + HorizontalPadding;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	ImVec2 ButtonSize(ButtonWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();
	ImVec2 ButtonScreenPos = ImGui::GetCursorScreenPos();

	if (ImGui::Button("##CameraSpeedBtn", ButtonSize))
	{
		ImGui::OpenPopup("CameraSpeedPopup_VTW");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Camera Speed: %.2f", FinalSpeed);
	}

	// 아이콘 렌더링
	if (IconSpeed && IconSpeed->GetShaderResourceView())
	{
		float IconY = ButtonScreenPos.y + (ButtonSize.y - IconSizeF) * 0.5f;
		float IconX = ButtonScreenPos.x + HorizontalPadding;
		ImGui::GetWindowDrawList()->AddImage(
			(ImTextureID)IconSpeed->GetShaderResourceView(),
			ImVec2(IconX, IconY),
			ImVec2(IconX + IconSizeF, IconY + IconSizeF)
		);
	}

	// 텍스트 렌더링
	float TextStartX = ButtonCursorPos.x + HorizontalPadding + IconSizeF + IconTextSpacing;
	float TextStartY = ButtonCursorPos.y + (ButtonSize.y - TextSize.y) * 0.5f;
	ImGui::SetCursorPos(ImVec2(TextStartX, TextStartY));
	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// 팝업
	if (ImGui::BeginPopup("CameraSpeedPopup_VTW", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));

		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Camera Speed");

		float CurrentMultiplier = SPEED_MULTIPLIERS[CameraSpeedSetting - 1];
		char MultiplierFormat[16];
		if (CurrentMultiplier >= 1.0f)
		{
			sprintf_s(MultiplierFormat, "%.1fx", CurrentMultiplier);
		}
		else
		{
			sprintf_s(MultiplierFormat, "%.4fx", CurrentMultiplier);
		}

		ImGui::SetNextItemWidth(200);
		if (ImGui::SliderInt("##SpeedSetting", &CameraSpeedSetting, 1, 8, MultiplierFormat))
		{
			if (ViewportClient)
			{
				ACameraActor* Camera = ViewportClient->GetCamera();
				if (Camera)
				{
					Camera->SetCameraSpeed(CalculateCameraSpeed(ViewportClient));
				}
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Speed Scalar");

		float CurrentScalar = ViewportClient ? ViewportClient->GetCameraSpeedScalar() : 1.0f;

		// Min 레이블
		ImGui::Text("0.25");
		ImGui::SameLine();

		// 슬라이더 바 숨기기 (투명하게)
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

		ImGui::SetNextItemWidth(140);
		if (ImGui::SliderFloat("##SpeedScalar", &CurrentScalar, 0.25f, 128.0f, "%.2f", ImGuiSliderFlags_Logarithmic))
		{
			CurrentScalar = std::max(0.25f, std::min(128.0f, CurrentScalar));
			if (ViewportClient)
			{
				ViewportClient->SetCameraSpeedScalar(CurrentScalar);
				ACameraActor* Camera = ViewportClient->GetCamera();
				if (Camera)
				{
					Camera->SetCameraSpeed(CalculateCameraSpeed(ViewportClient));
				}
			}
		}

		ImGui::PopStyleColor(3);

		// Max 레이블
		ImGui::SameLine();
		ImGui::Text("128");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 최종 속도 표시
		float FinalSpeedValue = CalculateCameraSpeed(ViewportClient);
		ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Final Speed: %.2f", FinalSpeedValue);

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderViewModeDropdownMenu(FViewportClient* ViewportClient) const
{
	if (!ViewportClient)
	{
		return;
	}

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	constexpr ImVec2 IconSize(17, 17);

	EViewMode CurrentViewMode = ViewportClient->GetViewMode();
	const char* CurrentViewModeName = "Lit";
	UTexture* CurrentViewModeIcon = nullptr;

	switch (CurrentViewMode)
	{
	case EViewMode::VMI_Lit_Gouraud:
	case EViewMode::VMI_Lit_Lambert:
	case EViewMode::VMI_Lit_Phong:
		CurrentViewModeName = "Lit";
		CurrentViewModeIcon = IconViewMode_Lit;
		break;
	case EViewMode::VMI_Unlit:
		CurrentViewModeName = "Unlit";
		CurrentViewModeIcon = IconViewMode_Unlit;
		break;
	case EViewMode::VMI_Wireframe:
		CurrentViewModeName = "Wire";
		CurrentViewModeIcon = IconViewMode_Wireframe;
		break;
	case EViewMode::VMI_WorldNormal:
		CurrentViewModeName = "Normal";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	case EViewMode::VMI_SceneDepth:
		CurrentViewModeName = "Depth";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	}

	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", CurrentViewModeName, "∨");

	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	constexpr float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	if (ImGui::Button("##ViewModeBtn_VTW", ButtonSize))
	{
		ImGui::OpenPopup("ViewModePopup_VTW");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("View Mode");
	}

	// 버튼 내용 렌더링
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	if (CurrentViewModeIcon && CurrentViewModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentViewModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// 팝업
	if (ImGui::BeginPopup("ViewModePopup_VTW", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		constexpr ImVec2 IconSize16(16, 16);
		const float IconTextOffsetY = (ImGui::GetTextLineHeight() - IconSize16.y) * 0.5f;

		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰 모드");
		ImGui::Separator();

		// Lit (서브메뉴)
		if (IconViewMode_Lit && IconViewMode_Lit->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconViewMode_Lit->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::BeginMenu("● Lit"))
		{
			// Phong
			bool bIsPhong = (CurrentViewMode == EViewMode::VMI_Lit_Phong);
			const char* PhongRadio = bIsPhong ? "●" : "○";
			char PhongLabel[64];
			sprintf_s(PhongLabel, "%s Phong (Per-Pixel Full)", PhongRadio);
			if (ImGui::MenuItem(PhongLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Phong);
			}

			// Lambert
			bool bIsLambert = (CurrentViewMode == EViewMode::VMI_Lit_Lambert);
			const char* LambertRadio = bIsLambert ? "●" : "○";
			char LambertLabel[64];
			sprintf_s(LambertLabel, "%s Lambert (Per-Pixel Diffuse)", LambertRadio);
			if (ImGui::MenuItem(LambertLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Lambert);
			}

			// Gouraud
			bool bIsGouraud = (CurrentViewMode == EViewMode::VMI_Lit_Gouraud);
			const char* GouraudRadio = bIsGouraud ? "●" : "○";
			char GouraudLabel[64];
			sprintf_s(GouraudLabel, "%s Gouraud (Per-Vertex)", GouraudRadio);
			if (ImGui::MenuItem(GouraudLabel))
			{
				ViewportClient->SetViewMode(EViewMode::VMI_Lit_Gouraud);
			}

			ImGui::EndMenu();
		}

		// Unlit
		if (IconViewMode_Unlit && IconViewMode_Unlit->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconViewMode_Unlit->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bIsUnlit = (CurrentViewMode == EViewMode::VMI_Unlit);
		const char* UnlitRadio = bIsUnlit ? "●" : "○";
		char UnlitLabel[32];
		sprintf_s(UnlitLabel, "%s Unlit", UnlitRadio);
		if (ImGui::Selectable(UnlitLabel, bIsUnlit))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Unlit);
		}

		// Wireframe
		if (IconViewMode_Wireframe && IconViewMode_Wireframe->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconViewMode_Wireframe->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bIsWireframe = (CurrentViewMode == EViewMode::VMI_Wireframe);
		const char* WireframeRadio = bIsWireframe ? "●" : "○";
		char WireframeLabel[32];
		sprintf_s(WireframeLabel, "%s Wireframe", WireframeRadio);
		if (ImGui::Selectable(WireframeLabel, bIsWireframe))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Wireframe);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "버퍼 시각화");
		ImGui::Separator();

		// World Normal
		if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bIsNormal = (CurrentViewMode == EViewMode::VMI_WorldNormal);
		const char* NormalRadio = bIsNormal ? "●" : "○";
		char NormalLabel[32];
		sprintf_s(NormalLabel, "%s World Normal", NormalRadio);
		if (ImGui::Selectable(NormalLabel, bIsNormal))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_WorldNormal);
		}

		// Scene Depth
		if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bIsDepth = (CurrentViewMode == EViewMode::VMI_SceneDepth);
		const char* DepthRadio = bIsDepth ? "●" : "○";
		char DepthLabel[32];
		sprintf_s(DepthLabel, "%s Scene Depth", DepthRadio);
		if (ImGui::Selectable(DepthLabel, bIsDepth))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_SceneDepth);
		}

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderShowFlagDropdownMenu(const FViewportClient* ViewportClient) const
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	constexpr ImVec2 IconSize(20, 20);

	char ButtonText[64];
	sprintf_s(ButtonText, "%s", "∨");

	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.16f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.21f, 1.00f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.28f, 0.26f, 1.00f));

	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	if (ImGui::Button("##ShowFlagBtn_VTW", ButtonSize))
	{
		ImGui::OpenPopup("ShowFlagPopup_VTW");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Show Flags");
	}

	// 버튼 내용 렌더링
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	if (IconShowFlag && IconShowFlag->GetShaderResourceView())
	{
		ImGui::Image((void*)IconShowFlag->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// 팝업
	if (ImGui::BeginPopup("ShowFlagPopup_VTW", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		URenderSettings& RenderSettings = ViewportClient->GetWorld()->GetRenderSettings();
		UStatsOverlayD2D& StatsOverlay = UStatsOverlayD2D::Get();

		constexpr ImVec2 IconSize16(16, 16);
		const float IconTextOffsetY = (ImGui::GetTextLineHeight() - IconSize16.y) * 0.5f;

		// Reset to Default
		if (IconRevert && IconRevert->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconRevert->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::MenuItem("기본값으로 복원"))
		{
			RenderSettings.SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
		}

		ImGui::Separator();

		// Stats 서브메뉴
		if (IconStats && IconStats->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconStats->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::BeginMenu("Stats"))
		{
			// Hide All 옵션
			if (IconHide && IconHide->GetShaderResourceView())
			{
				ImVec2 CursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
				ImGui::Image((void*)IconHide->GetShaderResourceView(), IconSize16);
				ImGui::SetCursorPosY(CursorPos.y);
				ImGui::SameLine();
			}
			if (ImGui::MenuItem("모두 숨기기"))
			{
				StatsOverlay.SetShowFPS(false);
				StatsOverlay.SetShowMemory(false);
				StatsOverlay.SetShowPicking(false);
				StatsOverlay.SetShowDecal(false);
				StatsOverlay.SetShowTileCulling(false);
				StatsOverlay.SetShowLights(false);
				StatsOverlay.SetShowShadow(false);
				StatsOverlay.SetShowGPU(false);
				StatsOverlay.SetShowSkinning(false);
				StatsOverlay.SetShowParticles(false);
			}

			ImGui::Separator();

			// 개별 Stats 옵션
			bool bShowFPS = StatsOverlay.IsFPSVisible();
			if (ImGui::Checkbox("FPS", &bShowFPS))
			{
				StatsOverlay.ToggleFPS();
			}

			bool bShowMemory = StatsOverlay.IsMemoryVisible();
			if (ImGui::Checkbox("MEMORY", &bShowMemory))
			{
				StatsOverlay.ToggleMemory();
			}

			bool bShowPicking = StatsOverlay.IsPickingVisible();
			if (ImGui::Checkbox("PICKING", &bShowPicking))
			{
				StatsOverlay.TogglePicking();
			}

			bool bShowDecal = StatsOverlay.IsDecalVisible();
			if (ImGui::Checkbox("DECAL", &bShowDecal))
			{
				StatsOverlay.ToggleDecal();
			}

			bool bShowTileCulling = StatsOverlay.IsTileCullingVisible();
			if (ImGui::Checkbox("TILE CULLING", &bShowTileCulling))
			{
				StatsOverlay.ToggleTileCulling();
			}

			bool bShowLights = StatsOverlay.IsLightsVisible();
			if (ImGui::Checkbox("LIGHTS", &bShowLights))
			{
				StatsOverlay.ToggleLights();
			}

			bool bShowShadows = StatsOverlay.IsShadowVisible();
			if (ImGui::Checkbox("SHADOWS", &bShowShadows))
			{
				StatsOverlay.ToggleShadow();
			}

			bool bShowSkinning = StatsOverlay.IsSkinningVisible();
			if (ImGui::Checkbox("SKINNING", &bShowSkinning))
			{
				StatsOverlay.ToggleSkinning();
			}

			bool bShowParticles = StatsOverlay.IsParticlesVisible();
			if (ImGui::Checkbox("PARTICLE", &bShowParticles))
			{
				StatsOverlay.ToggleParticles();
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "일반");
		ImGui::Separator();

		// BVH Debug
		if (IconBVH && IconBVH->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconBVH->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bBVH = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug);
		if (ImGui::Checkbox("BVH Debug", &bBVH))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BVHDebug);
		}

		// Grid
		if (IconGrid && IconGrid->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconGrid->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
		if (ImGui::Checkbox("Grid", &bGrid))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
		}

		// Decals
		if (IconDecal && IconDecal->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconDecal->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bDecals = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
		if (ImGui::Checkbox("Decals", &bDecals))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Decals);
		}

		// Static Mesh
		if (IconStaticMesh && IconStaticMesh->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconStaticMesh->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bStaticMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
		if (ImGui::Checkbox("Static Meshes", &bStaticMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_StaticMeshes);
		}

		// Skeletal Mesh
		if (IconSkeletalMesh && IconSkeletalMesh->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconSkeletalMesh->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bSkeletalMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_SkeletalMeshes);
		if (ImGui::Checkbox("Skeletal Meshes", &bSkeletalMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_SkeletalMeshes);
		}

		// Particles
		if (IconParticle && IconParticle->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconParticle->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bParticles = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Particles);
		if (ImGui::Checkbox("Particles", &bParticles))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Particles);
		}

		// Billboard
		if (IconBillboard && IconBillboard->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconBillboard->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bBillboard = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Billboard);
		if (ImGui::Checkbox("Billboards", &bBillboard))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Billboard);
		}

		// Editor Icon
		if (IconEditorIcon && IconEditorIcon->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconEditorIcon->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bEditorIcon = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_EditorIcon);
		if (ImGui::Checkbox("Editor Icons", &bEditorIcon))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_EditorIcon);
		}

		// Fog
		if (IconFog && IconFog->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconFog->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bFog = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
		if (ImGui::Checkbox("Fog", &bFog))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Fog);
		}

		// Bounding Boxes
		if (IconCollision && IconCollision->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconCollision->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bBounds = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
		if (ImGui::Checkbox("Bounding Boxes", &bBounds))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BoundingBoxes);
		}

		// Shadows
		if (IconShadow && IconShadow->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconShadow->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		bool bShadows = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Shadows);
		if (ImGui::Checkbox("Shadows", &bShadows))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Shadows);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "포스트 프로세스");
		ImGui::Separator();

		// 안티 앨리어싱 (FXAA) 서브메뉴
		if (IconAntiAliasing && IconAntiAliasing->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconAntiAliasing->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::BeginMenu("안티 앨리어싱"))
		{
			// Enable/Disable 체크박스
			bool bFXAA = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);
			if (ImGui::Checkbox("활성화", &bFXAA))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_FXAA);
			}

			ImGui::Separator();

			// Quality Presets
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "품질 프리셋:");
			ImGui::SameLine();
			if (ImGui::SmallButton("High"))
			{
				RenderSettings.SetFXAASpanMax(8.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 8.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 128.0f);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Medium"))
			{
				RenderSettings.SetFXAASpanMax(8.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 4.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 64.0f);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Low"))
			{
				RenderSettings.SetFXAASpanMax(4.0f);
				RenderSettings.SetFXAAReduceMul(1.0f / 2.0f);
				RenderSettings.SetFXAAReduceMin(1.0f / 32.0f);
			}

			// SpanMax
			float SpanMax = RenderSettings.GetFXAASpanMax();
			ImGui::SetNextItemWidth(150);
			if (ImGui::SliderFloat("SpanMax", &SpanMax, 4.0f, 16.0f, "%.1f"))
			{
				RenderSettings.SetFXAASpanMax(SpanMax);
			}

			// ReduceMul (역수로 표시: 2~16)
			float ReduceMul = RenderSettings.GetFXAAReduceMul();
			float ReduceMulReciprocal = (ReduceMul > 0.0001f) ? (1.0f / ReduceMul) : 16.0f;
			ImGui::SetNextItemWidth(150);
			if (ImGui::SliderFloat("ReduceMul", &ReduceMulReciprocal, 2.0f, 16.0f, "1/%.0f"))
			{
				RenderSettings.SetFXAAReduceMul(1.0f / ReduceMulReciprocal);
			}

			// ReduceMin (역수로 표시: 16~256)
			float ReduceMin = RenderSettings.GetFXAAReduceMin();
			float ReduceMinReciprocal = (ReduceMin > 0.0001f) ? (1.0f / ReduceMin) : 128.0f;
			ImGui::SetNextItemWidth(150);
			if (ImGui::SliderFloat("ReduceMin", &ReduceMinReciprocal, 16.0f, 256.0f, "1/%.0f"))
			{
				RenderSettings.SetFXAAReduceMin(1.0f / ReduceMinReciprocal);
			}

			ImGui::EndMenu();
		}

		// 피사계 심도 (DoF) 서브메뉴
		if (ImGui::BeginMenu("피사계 심도 (DoF)"))
		{
			// Enable/Disable 체크박스
			bool bDoF = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_DepthOfField);
			if (ImGui::Checkbox("활성화", &bDoF))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_DepthOfField);
			}

			ImGui::Separator();

			// DoF Modifier 가져오기 또는 생성
			APlayerCameraManager* PCM = ViewportClient->GetWorld()->GetPlayerCameraManager();
			UCamMod_DepthOfField* DoFMod = nullptr;
			if (PCM)
			{
				// 기존 DoF Modifier 찾기
				for (UCameraModifierBase* Modifier : PCM->ActiveModifiers)
				{
					DoFMod = dynamic_cast<UCamMod_DepthOfField*>(Modifier);
					if (DoFMod)
					{
						break;
					}
				}

				// 없으면 새로 생성하여 추가
				if (!DoFMod)
				{
					DoFMod = NewObject<UCamMod_DepthOfField>();
					if (DoFMod)
					{
						PCM->ActiveModifiers.Add(DoFMod);
					}
				}
			}

			if (DoFMod)
			{
				// 모드 선택
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "모드:");
				if (ImGui::RadioButton("Cinematic", DoFMod->DoFMode == 0))
				{
					DoFMod->DoFMode = 0;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("Physical", DoFMod->DoFMode == 1))
				{
					DoFMod->DoFMode = 1;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("TiltShift", DoFMod->DoFMode == 2))
				{
					DoFMod->DoFMode = 2;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("PointFocus", DoFMod->DoFMode == 3))
				{
					DoFMod->DoFMode = 3;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("ScreenPoint", DoFMod->DoFMode == 4))
				{
					DoFMod->DoFMode = 4;
				}

				ImGui::Spacing();

				// 공통 파라미터
				ImGui::SetNextItemWidth(150);
				ImGui::SliderFloat("Focus Distance", &DoFMod->FocusDistance, 10.0f, 2000.0f, "%.1f");

				ImGui::SetNextItemWidth(150);
				ImGui::SliderFloat("Max Blur Radius", &DoFMod->MaxBlurRadius, 0.0f, 20.0f, "%.1f");

				ImGui::SetNextItemWidth(150);
				ImGui::SliderFloat("Bokeh Size", &DoFMod->BokehSize, 0.1f, 5.0f, "%.2f");

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// 모드별 파라미터
				if (DoFMod->DoFMode == 0) // Cinematic
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Cinematic 설정:");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Focus Range", &DoFMod->FocusRange, 10.0f, 500.0f, "%.1f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Near Blur Scale", &DoFMod->NearBlurScale, 0.0f, 0.1f, "%.4f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Far Blur Scale", &DoFMod->FarBlurScale, 0.0f, 0.1f, "%.4f");
				}
				else if (DoFMod->DoFMode == 1) // Physical
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Physical 설정:");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Focal Length (mm)", &DoFMod->FocalLength, 10.0f, 200.0f, "%.1f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("F-Number", &DoFMod->FNumber, 1.0f, 22.0f, "%.1f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Sensor Width (mm)", &DoFMod->SensorWidth, 10.0f, 50.0f, "%.1f");
				}
				else if (DoFMod->DoFMode == 2) // Tilt-Shift
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Tilt-Shift 설정:");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Center Y", &DoFMod->TiltShiftCenterY, 0.0f, 1.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Band Width", &DoFMod->TiltShiftBandWidth, 0.0f, 1.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Blur Scale", &DoFMod->TiltShiftBlurScale, 0.0f, 10.0f, "%.1f");
				}
				else if (DoFMod->DoFMode == 3) // PointFocus
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Point Focus 설정:");
					ImGui::SetNextItemWidth(150);
					ImGui::InputFloat3("Focus Point", &DoFMod->FocusPoint.X);
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Focus Radius", &DoFMod->FocusRadius, 0.1f, 10.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Blur Scale", &DoFMod->PointFocusBlurScale, 0.0f, 2.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Falloff", &DoFMod->PointFocusFalloff, 0.5f, 3.0f, "%.2f");
				}
				else if (DoFMod->DoFMode == 4) // ScreenPointFocus
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Screen Point Focus 설정:");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat2("Screen Point", &DoFMod->ScreenFocusPoint.X, 0.0f, 1.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Screen Radius", &DoFMod->ScreenFocusRadius, 0.0f, 0.5f, "%.3f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Depth Range", &DoFMod->ScreenFocusDepthRange, 10.0f, 200.0f, "%.1f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Blur Scale", &DoFMod->ScreenFocusBlurScale, 0.0f, 5.0f, "%.2f");
					ImGui::SetNextItemWidth(150);
					ImGui::SliderFloat("Falloff", &DoFMod->ScreenFocusFalloff, 0.5f, 3.0f, "%.2f");
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// Blur Method
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Blur Method:");
				const char* BlurMethods[] = { "Disc12", "Disc24", "Gaussian", "Hexagonal", "CircularGather" };
				ImGui::SetNextItemWidth(150);
				ImGui::Combo("##BlurMethod", &DoFMod->BlurMethod, BlurMethods, 5);
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "DoF Modifier not found");
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "렌더링");
		ImGui::Separator();

		// 타일 기반 라이트 컬링 서브메뉴
		if (IconTile && IconTile->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconTile->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::BeginMenu("타일 기반 라이트 컬링"))
		{
			// Enable/Disable 체크박스
			bool bShowTileCullingDebug = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCulling);
			if (ImGui::Checkbox("활성화", &bShowTileCullingDebug))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCulling);
			}

			ImGui::Separator();

			uint32 TileSize = RenderSettings.GetTileSize();
			int32 TileSizeInt = static_cast<int32>(TileSize);
			ImGui::SetNextItemWidth(100);
			if (ImGui::InputInt("Tile Size", &TileSizeInt, 1, 8))
			{
				TileSizeInt = std::max(8, std::min(64, TileSizeInt));
				RenderSettings.SetTileSize(static_cast<uint32>(TileSizeInt));
			}

			ImGui::EndMenu();
		}

		// 기즈모 안티 앨리어싱 (Shadow AA) 서브메뉴
		if (IconShadowAA && IconShadowAA->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconShadowAA->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		if (ImGui::BeginMenu("기즈모 안티 앨리어싱"))
		{
			EShadowAATechnique CurrentShadowAA = RenderSettings.GetShadowAATechnique();

			bool bIsPCF = (CurrentShadowAA == EShadowAATechnique::PCF);
			if (ImGui::RadioButton("PCF", bIsPCF))
			{
				RenderSettings.SetShadowAATechnique(EShadowAATechnique::PCF);
			}

			ImGui::SameLine();

			bool bIsVSM = (CurrentShadowAA == EShadowAATechnique::VSM);
			if (ImGui::RadioButton("VSM", bIsVSM))
			{
				RenderSettings.SetShadowAATechnique(EShadowAATechnique::VSM);
			}

			ImGui::EndMenu();
		}

		// GPU Skinning
		if (IconSkinning && IconSkinning->GetShaderResourceView())
		{
			ImVec2 CursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(CursorPos.y + IconTextOffsetY);
			ImGui::Image((void*)IconSkinning->GetShaderResourceView(), IconSize16);
			ImGui::SetCursorPosY(CursorPos.y);
			ImGui::SameLine();
		}
		ESkinningMode CurrentSkinningMode = RenderSettings.GetSkinningMode();
		bool bGPUSkinning = (CurrentSkinningMode == ESkinningMode::GPU);
		if (ImGui::Checkbox("GPU Skinning", &bGPUSkinning))
		{
			RenderSettings.SetSkinningMode(bGPUSkinning ? ESkinningMode::GPU : ESkinningMode::CPU);
		}

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderCameraOptionDropdownMenu(const FViewportClient* ViewportClient, EViewportType ViewportType, const char* ViewportName, SViewportWindow* OwnerViewport)
{
	if (!ViewportClient || !OwnerViewport)
	{
		return;
	}

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 0.7f);

	constexpr ImVec2 IconSize(17, 17);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", ViewportName, "∨");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	constexpr float HorizontalPadding = 8.0f;
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
			OwnerViewport->SwitchToViewportType(EViewportType::Perspective, "원근");
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

		for (int i = 0; i < 6; ++i)
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
				OwnerViewport->SwitchToViewportType(mode.type, mode.koreanName);
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

		// --- 섹션 3: 뷰 ---
		ACameraActor* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
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

void SViewportToolbarWidget::RenderViewportLayoutSwitchButton(SViewportWindow* OwnerViewport)
{
	if (!OwnerViewport)
	{
		return;
	}

	ImVec2 switchCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(switchCursorPos.y - 0.7f);

	constexpr ImVec2 IconSize(17, 17);

	// 현재 레이아웃 모드에 따라 아이콘 선택
	// FourSplit일 때 → SingleViewport 아이콘 표시 (클릭하면 단일로 전환)
	// SingleMain일 때 → MultiViewport 아이콘 표시 (클릭하면 멀티로 전환)
	EViewportLayoutMode CurrentMode = SLATE.GetCurrentLayoutMode();
	UTexture* SwitchIcon = (CurrentMode == EViewportLayoutMode::FourSplit) ? IconMultiToSingleViewport : IconSingleToMultiViewport;
	const char* TooltipText = (CurrentMode == EViewportLayoutMode::FourSplit) ? "단일 뷰포트로 전환" : "멀티 뷰포트로 전환";

	// 버튼 너비 계산 (아이콘 + 패딩)
	constexpr float Padding = 8.0f;
	constexpr float ButtonWidth = IconSize.x + Padding * 2.0f;

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
		SLATE.SwitchPanel(OwnerViewport);
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
