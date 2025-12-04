#include "pch.h"
#include "ViewportToolbarWidget.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Runtime/Renderer/FViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/CameraComponent.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Slate/USlateManager.h"
#include "ImGui/imgui.h"

SViewportToolbarWidget::SViewportToolbarWidget()
{
}

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
	IconSelect->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Select.png", InDevice);

	IconMove = NewObject<UTexture>();
	IconMove->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Move.png", InDevice);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Rotate.png", InDevice);

	IconScale = NewObject<UTexture>();
	IconScale->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_Scale.png", InDevice);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_WorldSpace.png", InDevice);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_LocalSpace.png", InDevice);

	// 카메라/뷰포트 모드 아이콘
	IconCamera = NewObject<UTexture>();
	IconCamera->Load(GDataDir + "/Default/Icon/Viewport_Mode_Camera.png", InDevice);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load(GDataDir + "/Default/Icon/Viewport_Mode_Perspective.png", InDevice);

	IconTop = NewObject<UTexture>();
	IconTop->Load(GDataDir + "/Default/Icon/Viewport_Mode_Top.png", InDevice);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load(GDataDir + "/Default/Icon/Viewport_Mode_Bottom.png", InDevice);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load(GDataDir + "/Default/Icon/Viewport_Mode_Left.png", InDevice);

	IconRight = NewObject<UTexture>();
	IconRight->Load(GDataDir + "/Default/Icon/Viewport_Mode_Right.png", InDevice);

	IconFront = NewObject<UTexture>();
	IconFront->Load(GDataDir + "/Default/Icon/Viewport_Mode_Front.png", InDevice);

	IconBack = NewObject<UTexture>();
	IconBack->Load(GDataDir + "/Default/Icon/Viewport_Mode_Back.png", InDevice);

	// 카메라 설정 아이콘
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load(GDataDir + "/Default/Icon/CameraSpeed_16.dds", InDevice, false);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load(GDataDir + "/Default/Icon/Viewport_Setting_FOV.png", InDevice);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_NearClip.png", InDevice);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load(GDataDir + "/Default/Icon/Viewport_Setting_FarClip.png", InDevice);

	// 뷰모드 아이콘
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Lit.png", InDevice);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_Unlit.png", InDevice);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load(GDataDir + "/Default/Icon/Viewport_Toolbar_WorldSpace.png", InDevice);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load(GDataDir + "/Default/Icon/Viewport_ViewMode_BufferVis.png", InDevice);

	// ShowFlag 아이콘
	IconShowFlag = NewObject<UTexture>();
	IconShowFlag->Load(GDataDir + "/Default/Icon/Viewport_ShowFlag.png", InDevice);

	IconRevert = NewObject<UTexture>();
	IconRevert->Load(GDataDir + "/Default/Icon/Viewport_Revert.png", InDevice);

	IconStats = NewObject<UTexture>();
	IconStats->Load(GDataDir + "/Default/Icon/Viewport_Stats.png", InDevice);

	IconHide = NewObject<UTexture>();
	IconHide->Load(GDataDir + "/Default/Icon/Viewport_Hide.png", InDevice);

	IconBVH = NewObject<UTexture>();
	IconBVH->Load(GDataDir + "/Default/Icon/ShowFlag_BVH.png", InDevice);

	IconGrid = NewObject<UTexture>();
	IconGrid->Load(GDataDir + "/Default/Icon/ShowFlag_Grid.png", InDevice);

	IconDecal = NewObject<UTexture>();
	IconDecal->Load(GDataDir + "/Default/Icon/ShowFlag_Decal.png", InDevice);

	IconStaticMesh = NewObject<UTexture>();
	IconStaticMesh->Load(GDataDir + "/Default/Icon/ShowFlag_StaticMesh.png", InDevice);

	IconSkeletalMesh = NewObject<UTexture>();
	IconSkeletalMesh->Load(GDataDir + "/Default/Icon/ShowFlag_SkeletalMesh.png", InDevice);

	IconBillboard = NewObject<UTexture>();
	IconBillboard->Load(GDataDir + "/Default/Icon/ShowFlag_Billboard.png", InDevice);

	IconEditorIcon = NewObject<UTexture>();
	IconEditorIcon->Load(GDataDir + "/Default/Icon/ShowFlag_EditorIcon.png", InDevice);

	IconFog = NewObject<UTexture>();
	IconFog->Load(GDataDir + "/Default/Icon/ShowFlag_Fog.png", InDevice);

	IconCollision = NewObject<UTexture>();
	IconCollision->Load(GDataDir + "/Default/Icon/ShowFlag_Collision.png", InDevice);

	IconAntiAliasing = NewObject<UTexture>();
	IconAntiAliasing->Load(GDataDir + "/Default/Icon/ShowFlag_AntiAliasing.png", InDevice);

	IconTile = NewObject<UTexture>();
	IconTile->Load(GDataDir + "/Default/Icon/ShowFlag_Tile.png", InDevice);

	IconShadow = NewObject<UTexture>();
	IconShadow->Load(GDataDir + "/Default/Icon/ShowFlag_Shadow.png", InDevice);

	IconShadowAA = NewObject<UTexture>();
	IconShadowAA->Load(GDataDir + "/Default/Icon/ShowFlag_ShadowAA.png", InDevice);

	IconSkinning = NewObject<UTexture>();
	IconSkinning->Load(GDataDir + "/Default/Icon/ShowFlag_Skinning.png", InDevice);

	IconParticle = NewObject<UTexture>();
	IconParticle->Load(GDataDir + "/Default/Icon/ShowFlag_Particle.png", InDevice);

	// 뷰포트 레이아웃 아이콘
	IconSingleToMultiViewport = NewObject<UTexture>();
	IconSingleToMultiViewport->Load(GDataDir + "/Default/Icon/Viewport_SingleToMulti.png", InDevice);

	IconMultiToSingleViewport = NewObject<UTexture>();
	IconMultiToSingleViewport->Load(GDataDir + "/Default/Icon/Viewport_MultiToSingle.png", InDevice);
}

void SViewportToolbarWidget::Render(FViewportClient* ViewportClient, AGizmoActor* GizmoActor, bool bShowLayoutSwitch)
{
	if (!ViewportClient)
	{
		return;
	}

	// 기즈모 버튼 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

	// 수직 중앙 정렬
	const float ButtonHeight = 23.0f;
	float verticalPadding = (ToolbarHeight - ButtonHeight) * 0.5f;
	ImGui::SetCursorPosY(verticalPadding);

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

	// === 오른쪽 정렬 버튼들 ===
	const float ShowFlagButtonWidth = 50.0f;
	const float ButtonSpacing = 8.0f;

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
		CurrentViewModeName = "Wireframe";
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
	const float SpeedIconSize = 14.0f;
	const float SpeedIconTextSpacing = 4.0f;
	const float SpeedHorizontalPadding = 6.0f;
	const float SpeedButtonWidth = SpeedHorizontalPadding + SpeedIconSize + SpeedIconTextSpacing + speedTextSize.x + SpeedHorizontalPadding;

	float AvailableWidth = ImGui::GetContentRegionAvail().x;
	float CursorStartX = ImGui::GetCursorPosX();
	ImVec2 CurrentCursor = ImGui::GetCursorPos();

	// 오른쪽부터 역순으로 위치 계산
	float RightEdge = CursorStartX + AvailableWidth;
	float ShowFlagX = RightEdge - ShowFlagButtonWidth;
	float ViewModeX = ShowFlagX - ButtonSpacing - ViewModeButtonWidth;
	float SpeedX = ViewModeX - ButtonSpacing - SpeedButtonWidth;

	// 버튼들을 순서대로 그리기
	ImGui::SetCursorPos(ImVec2(SpeedX, CurrentCursor.y));
	RenderCameraSpeedButton(ViewportClient);

	ImGui::SetCursorPos(ImVec2(ViewModeX, CurrentCursor.y));
	RenderViewModeDropdownMenu(ViewportClient);

	ImGui::SetCursorPos(ImVec2(ShowFlagX, CurrentCursor.y));
	RenderShowFlagDropdownMenu(ViewportClient);
}

void SViewportToolbarWidget::RenderGizmoModeButtons(AGizmoActor* GizmoActor)
{
	const ImVec2 IconSize(17, 17);

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

void SViewportToolbarWidget::RenderGizmoSpaceButton(AGizmoActor* GizmoActor)
{
	const ImVec2 IconSize(17, 17);

	EGizmoSpace CurrentGizmoSpace = GizmoActor ? GizmoActor->GetSpace() : EGizmoSpace::World;
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "World Space [Tab]" : "Local Space [Tab]";

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

float SViewportToolbarWidget::CalculateCameraSpeed(FViewportClient* ViewportClient) const
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

	const float IconSizeF = 14.0f;
	const float IconTextSpacing = 4.0f;
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float HorizontalPadding = 6.0f;
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

		ImGui::SetNextItemWidth(200);
		float CurrentScalar = ViewportClient ? ViewportClient->GetCameraSpeedScalar() : 1.0f;
		if (ImGui::SliderFloat("##SpeedScalar", &CurrentScalar, 0.25f, 128.0f, "%.2f", ImGuiSliderFlags_Logarithmic))
		{
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

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderViewModeDropdownMenu(FViewportClient* ViewportClient)
{
	if (!ViewportClient)
	{
		return;
	}

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(17, 17);

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
	const float Padding = 8.0f;
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

		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "View Mode");
		ImGui::Separator();

		// Lit
		bool bIsLit = (CurrentViewMode == EViewMode::VMI_Lit_Phong ||
			CurrentViewMode == EViewMode::VMI_Lit_Gouraud ||
			CurrentViewMode == EViewMode::VMI_Lit_Lambert);
		const char* LitRadio = bIsLit ? "O" : " ";
		char LitLabel[32];
		sprintf_s(LitLabel, "%s Lit", LitRadio);
		if (ImGui::MenuItem(LitLabel))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Lit_Phong);
		}

		// Unlit
		bool bIsUnlit = (CurrentViewMode == EViewMode::VMI_Unlit);
		const char* UnlitRadio = bIsUnlit ? "O" : " ";
		char UnlitLabel[32];
		sprintf_s(UnlitLabel, "%s Unlit", UnlitRadio);
		if (ImGui::MenuItem(UnlitLabel))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Unlit);
		}

		// Wireframe
		bool bIsWireframe = (CurrentViewMode == EViewMode::VMI_Wireframe);
		const char* WireframeRadio = bIsWireframe ? "O" : " ";
		char WireframeLabel[32];
		sprintf_s(WireframeLabel, "%s Wireframe", WireframeRadio);
		if (ImGui::MenuItem(WireframeLabel))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_Wireframe);
		}

		ImGui::Separator();

		// World Normal
		bool bIsNormal = (CurrentViewMode == EViewMode::VMI_WorldNormal);
		const char* NormalRadio = bIsNormal ? "O" : " ";
		char NormalLabel[32];
		sprintf_s(NormalLabel, "%s World Normal", NormalRadio);
		if (ImGui::MenuItem(NormalLabel))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_WorldNormal);
		}

		// Scene Depth
		bool bIsDepth = (CurrentViewMode == EViewMode::VMI_SceneDepth);
		const char* DepthRadio = bIsDepth ? "O" : " ";
		char DepthLabel[32];
		sprintf_s(DepthLabel, "%s Scene Depth", DepthRadio);
		if (ImGui::MenuItem(DepthLabel))
		{
			ViewportClient->SetViewMode(EViewMode::VMI_SceneDepth);
		}

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderShowFlagDropdownMenu(FViewportClient* ViewportClient)
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 1.0f);

	const ImVec2 IconSize(20, 20);

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

		// Reset
		if (ImGui::MenuItem("Reset to Default"))
		{
			RenderSettings.SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
		}

		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Show Flags");
		ImGui::Separator();

		// Grid
		bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
		if (ImGui::Checkbox(" Grid", &bGrid))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
		}

		// Static Mesh
		bool bStaticMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
		if (ImGui::Checkbox(" Static Meshes", &bStaticMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_StaticMeshes);
		}

		// Skeletal Mesh
		bool bSkeletalMesh = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_SkeletalMeshes);
		if (ImGui::Checkbox(" Skeletal Meshes", &bSkeletalMesh))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_SkeletalMeshes);
		}

		// Particles
		bool bParticles = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Particles);
		if (ImGui::Checkbox(" Particles", &bParticles))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Particles);
		}

		// Decals
		bool bDecals = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
		if (ImGui::Checkbox(" Decals", &bDecals))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Decals);
		}

		// Billboard
		bool bBillboard = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Billboard);
		if (ImGui::Checkbox(" Billboards", &bBillboard))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Billboard);
		}

		// Shadows
		bool bShadows = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Shadows);
		if (ImGui::Checkbox(" Shadows", &bShadows))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Shadows);
		}

		// Fog
		bool bFog = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
		if (ImGui::Checkbox(" Fog", &bFog))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Fog);
		}

		// Anti-Aliasing (FXAA)
		bool bAA = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);
		if (ImGui::Checkbox(" Anti-Aliasing", &bAA))
		{
			RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_FXAA);
		}

		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportToolbarWidget::RenderCameraOptionDropdownMenu(FViewportClient* ViewportClient)
{
	// Dynamic Editor에서는 카메라 옵션은 기본 원근 모드로 고정되므로 간소화된 구현
	// 필요 시 확장 가능
}

void SViewportToolbarWidget::RenderViewportLayoutSwitchButton()
{
	// 뷰포트 레이아웃 전환은 메인 에디터에서만 사용
	// Dynamic Editor에서는 사용하지 않음
}
