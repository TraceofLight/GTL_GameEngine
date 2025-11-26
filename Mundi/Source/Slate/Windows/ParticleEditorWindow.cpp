#include "pch.h"
#include "ParticleEditorWindow.h"
#include "Source/Runtime/Engine/Particle/ParticleViewerState.h"
#include "Source/Runtime/Engine/Particle/ParticleViewerBootstrap.h"
#include "Source/Runtime/Engine/Particle/ParticleSystem.h"
#include "Source/Runtime/Engine/Particle/ParticleEmitter.h"
#include "Source/Runtime/Engine/Particle/ParticleLODLevel.h"
#include "Source/Runtime/Engine/Particle/ParticleModule.h"
#include "Source/Runtime/Engine/Particle/ParticleModuleRequired.h"
#include "Source/Runtime/Engine/Particle/Spawn/ParticleModuleSpawn.h"
#include "Source/Runtime/Engine/Particle/Velocity/ParticleModuleVelocity.h"
#include "Source/Runtime/Engine/Particle/Size/ParticleModuleSize.h"
#include "Source/Runtime/Engine/Particle/Lifetime/ParticleModuleLifetime.h"
#include "Source/Runtime/Engine/Particle/Color/ParticleModuleColor.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotationRate.h"
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"
#include "Source/Runtime/Engine/GameFramework/ParticleSystemActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Runtime/RHI/D3D11RHI.h"
#include "Source/Runtime/Core/Misc/Base64.h"
#include "ThumbnailManager.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include <map>
#include "ImGui/imgui.h"
#include "Source/Editor/PlatformProcess.h"

#ifdef _EDITOR
#include "Source/Runtime/Engine/GameFramework/EditorEngine.h"
extern UEditorEngine GEngine;
#else
#include "Source/Runtime/Engine/GameFramework/GameEngine.h"
extern UGameEngine GEngine;
#endif

// 파티클 모듈 헤더
#include "Source/Runtime/Engine/Particle/Color/ParticleModuleColor.h"
#include "Source/Runtime/Engine/Particle/Lifetime/ParticleModuleLifetime.h"
#include "Source/Runtime/Engine/Particle/Location/ParticleModuleLocation.h"
#include "Source/Runtime/Engine/Particle/Size/ParticleModuleSize.h"
#include "Source/Runtime/Engine/Particle/Velocity/ParticleModuleVelocity.h"
#include "Source/Runtime/Engine/Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamSource.h"
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamTarget.h"
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamNoise.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotationRate.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleMeshRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleMeshRotationRate.h"
#include "Source/Runtime/Engine/Particle/SubUV/ParticleModuleSubUV.h"
#include "Source/Slate/Widgets/ParticleModuleDetailRenderer.h"
#include "Source/Slate/Widgets/PropertyRenderer.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

// ============================================================================
// SParticleEditorWindow
// ============================================================================

SParticleEditorWindow::SParticleEditorWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
}

SParticleEditorWindow::~SParticleEditorWindow()
{
	// 렌더 타겟 해제
	ReleaseRenderTarget();

	// 스플리터/패널 정리
	if (MainSplitter)
	{
		delete MainSplitter;
		MainSplitter = nullptr;
	}

	// 모든 탭 정리
	for (ParticleViewerState* State : Tabs)
	{
		ParticleViewerBootstrap::DestroyViewerState(State);
	}
	Tabs.clear();
	ActiveState = nullptr;
	ActiveTabIndex = -1;
}

bool SParticleEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
	Device = InDevice;
	World = InWorld;
	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 기본 탭 생성
	ParticleViewerState* State = ParticleViewerBootstrap::CreateViewerState("Particle Editor", InWorld, InDevice);
	if (State)
	{
		Tabs.Add(State);
		ActiveState = State;
		ActiveTabIndex = 0;
	}

	// 패널 생성
	ViewportPanel = new SParticleViewportPanel(this);
	DetailPanel = new SParticleDetailPanel(this);
	EmittersPanel = new SParticleEmittersPanel(this);
	CurveEditorPanel = new SParticleCurveEditorPanel(this);

	// 스플리터 계층 구조 생성
	// 좌측: Viewport(상) | Detail(하)
	LeftSplitter = new SSplitterV();
	LeftSplitter->SetSplitRatio(0.60f);
	LeftSplitter->SideLT = ViewportPanel;
	LeftSplitter->SideRB = DetailPanel;

	// 우측: Emitters(상) | CurveEditor(하)
	RightSplitter = new SSplitterV();
	RightSplitter->SetSplitRatio(0.45f);  // 커브 에디터 영역 확대 (0.55 -> 0.45)
	RightSplitter->SideLT = EmittersPanel;
	RightSplitter->SideRB = CurveEditorPanel;

	// 메인: Left(좌) | Right(우)
	MainSplitter = new SSplitterH();
	MainSplitter->SetSplitRatio(0.30f);
	MainSplitter->SideLT = LeftSplitter;
	MainSplitter->SideRB = RightSplitter;

	// 스플리터 초기 Rect 설정 (첫 OnUpdate 전에 유효한 값을 가지도록)
	MainSplitter->SetRect(StartX, StartY, StartX + Width, StartY + Height);

	bIsOpen = true;
	bRequestFocus = true;

	// 툴바 아이콘 로드
	LoadToolbarIcons();

	return ActiveState != nullptr;
}

void SParticleEditorWindow::OnRender()
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	// 리사이즈 가능하도록 size constraints 설정 (최소 400x300, 최대 무제한)
	ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(10000, 10000));

	// 리사이즈 그립 크기 증가
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

	// 초기 배치
	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	// 윈도우 타이틀
	FString WindowTitle = "Particle Editor";
	if (ActiveState->CurrentSystem)
	{
		const FString& Path = ActiveState->CurrentSystem->GetFilePath();
		if (!Path.empty())
		{
			std::filesystem::path fsPath(Path);
			WindowTitle += " - " + fsPath.filename().string();
		}
	}
	WindowTitle += "###ParticleEditor";

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	bIsFocused = false;

	if (ImGui::Begin(WindowTitle.c_str(), &bIsOpen, flags))
	{
		// 포커스 상태 확인
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 윈도우 위치/크기 추적
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		Rect = FRect(windowPos.x, windowPos.y, windowPos.x + windowSize.x, windowPos.y + windowSize.y);

		// ================================================================
		// 수동 리사이즈 핸들링 (우하단 코너)
		// ================================================================
		const float ResizeGripSize = 16.0f;
		ImVec2 resizeGripMin(windowPos.x + windowSize.x - ResizeGripSize, windowPos.y + windowSize.y - ResizeGripSize);
		ImVec2 resizeGripMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 mousePos = io.MousePos;

		// 리사이즈 그립 영역에 마우스가 있는지 확인
		bool bMouseInResizeGrip = (mousePos.x >= resizeGripMin.x && mousePos.x <= resizeGripMax.x &&
								   mousePos.y >= resizeGripMin.y && mousePos.y <= resizeGripMax.y);

		// 리사이즈 그립 그리기
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImU32 gripColor = bMouseInResizeGrip ? IM_COL32(150, 150, 150, 255) : IM_COL32(100, 100, 100, 200);
		drawList->AddTriangleFilled(
			ImVec2(resizeGripMax.x - 2, resizeGripMax.y - ResizeGripSize),
			ImVec2(resizeGripMax.x - 2, resizeGripMax.y - 2),
			ImVec2(resizeGripMax.x - ResizeGripSize, resizeGripMax.y - 2),
			gripColor
		);

		// 수동 리사이즈 처리
		static bool bResizing = false;
		static ImVec2 resizeStartSize;
		static ImVec2 resizeStartMouse;

		if (bMouseInResizeGrip && ImGui::IsMouseClicked(0))
		{
			bResizing = true;
			resizeStartSize = windowSize;
			resizeStartMouse = mousePos;
		}

		if (bResizing)
		{
			if (ImGui::IsMouseDown(0))
			{
				ImVec2 delta(mousePos.x - resizeStartMouse.x, mousePos.y - resizeStartMouse.y);
				float newWidth = std::max(400.0f, resizeStartSize.x + delta.x);
				float newHeight = std::max(300.0f, resizeStartSize.y + delta.y);
				ImGui::SetWindowSize(ImVec2(newWidth, newHeight));
			}
			else
			{
				bResizing = false;
			}
		}

		// 리사이즈 중일 때 커서 변경
		if (bMouseInResizeGrip || bResizing)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
		}

		// ================================================================
		// 툴바 렌더링 (Cascade 스타일)
		// ================================================================
		RenderToolbar();

		// 콘텐츠 영역 계산 (리사이즈 핸들을 위해 우/하단에 패딩 추가)
		const float ResizeHandlePadding = 16.0f;
		ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
		ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
		float toolbarHeight = ImGui::GetCursorPosY() - contentMin.y;
		FRect contentRect(
			windowPos.x + contentMin.x,
			windowPos.y + ImGui::GetCursorPosY(),
			windowPos.x + contentMax.x - ResizeHandlePadding,
			windowPos.y + contentMax.y - ResizeHandlePadding
		);

		// 스플리터에 영역 설정 및 렌더링
		if (MainSplitter)
		{
			MainSplitter->SetRect(contentRect.Left, contentRect.Top, contentRect.Right, contentRect.Bottom);
			MainSplitter->OnRender();
		}

		// 뷰포트 영역 캐시 (ViewportPanel의 실제 콘텐츠 영역)
		if (ViewportPanel)
		{
			// ContentRect가 유효하면 사용, 아니면 패널 Rect 사용
			if (ViewportPanel->ContentRect.GetWidth() > 0 && ViewportPanel->ContentRect.GetHeight() > 0)
			{
				ViewportRect = ViewportPanel->ContentRect;
			}
			else
			{
				ViewportRect = ViewportPanel->Rect;
			}
			ViewportRect.UpdateMinMax();
		}

		// 패널들을 항상 앞으로 가져오기 (메인 윈도우 클릭 시에도 패널 유지)
		ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##ParticleViewport");
		ImGuiWindow* DetailWin = ImGui::FindWindowByName("Details##ParticleDetail");
		ImGuiWindow* EmittersWin = ImGui::FindWindowByName("Emitters##ParticleEmitters");
		ImGuiWindow* CurveWin = ImGui::FindWindowByName("Curve Editor##ParticleCurve");

		if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
		if (DetailWin) ImGui::BringWindowToDisplayFront(DetailWin);
		if (EmittersWin) ImGui::BringWindowToDisplayFront(EmittersWin);
		if (CurveWin) ImGui::BringWindowToDisplayFront(CurveWin);

		// 열려있는 모든 팝업을 패널들보다 위로 (BG Color, 모듈 우클릭 등)
		ImGuiContext* g = ImGui::GetCurrentContext();
		if (g && g->OpenPopupStack.Size > 0)
		{
			for (int i = 0; i < g->OpenPopupStack.Size; ++i)
			{
				ImGuiPopupData& PopupData = g->OpenPopupStack[i];
				if (PopupData.Window)
				{
					ImGui::BringWindowToDisplayFront(PopupData.Window);
				}
			}
		}

		// 컬러피커 팝업 렌더링 (메인 윈도우 내부, 패널들 위에 오버레이)
		if (ImGui::BeginPopup("##BgColorPopup", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{

			if (ActiveState)
			{
				ImGui::Text("Background Color");
				ImGui::Separator();

				ImGui::ColorPicker3("##BgColorPicker", ActiveState->BackgroundColor,
					ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);

				ImGui::Separator();
				ImGui::Text("RGB: %.3f, %.3f, %.3f",
					ActiveState->BackgroundColor[0],
					ActiveState->BackgroundColor[1],
					ActiveState->BackgroundColor[2]);

				ImGui::Separator();

				if (ImGui::Button("OK", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(2);  // WindowRounding, WindowBorderSize

	// Rename 다이얼로그 렌더링 (최상위 모달)
	RenderRenameEmitterDialog();
}

void SParticleEditorWindow::OnUpdate(float DeltaSeconds)
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	// 스플리터 업데이트
	if (MainSplitter)
	{
		MainSplitter->OnUpdate(DeltaSeconds);
	}

	// ViewportClient Tick (카메라 입력 처리)
	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}

	// View 설정을 World RenderSettings에 동기화
	if (ActiveState->World)
	{
		URenderSettings& RenderSettings = ActiveState->World->GetRenderSettings();

		// Background Color 동기화
		RenderSettings.SetBackgroundColor(
			ActiveState->BackgroundColor[0],
			ActiveState->BackgroundColor[1],
			ActiveState->BackgroundColor[2]
		);

		// Grid 토글
		if (ActiveState->bShowGrid)
		{
			RenderSettings.EnableShowFlag(EEngineShowFlags::SF_Grid);
		}
		else
		{
			RenderSettings.DisableShowFlag(EEngineShowFlags::SF_Grid);
		}

		// Post Process 토글 (Fog, FXAA)
		if (ActiveState->bShowPostProcess)
		{
			RenderSettings.EnableShowFlag(EEngineShowFlags::SF_Fog);
			RenderSettings.EnableShowFlag(EEngineShowFlags::SF_FXAA);
		}
		else
		{
			RenderSettings.DisableShowFlag(EEngineShowFlags::SF_Fog);
			RenderSettings.DisableShowFlag(EEngineShowFlags::SF_FXAA);
		}
	}

	// 시뮬레이션 업데이트
	if (ActiveState->bIsSimulating && ActiveState->World)
	{
		ActiveState->AccumulatedTime += DeltaSeconds * ActiveState->SimulationSpeed;
		ActiveState->World->Tick(DeltaSeconds * ActiveState->SimulationSpeed);
	}
}

void SParticleEditorWindow::OnMouseMove(FVector2D MousePos)
{
	if (MainSplitter)
	{
		MainSplitter->OnMouseMove(MousePos);
	}

	// 팝업/모달이 열려있으면 뷰포트 입력 무시 (드롭다운 메뉴 방해 방지)
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// 뷰포트 영역 내에서 마우스 이벤트 전달 (카메라 컨트롤용)
	if (ActiveState && ActiveState->Viewport && ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseMove(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y));
	}
}

void SParticleEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (MainSplitter)
	{
		MainSplitter->OnMouseDown(MousePos, Button);
	}

	// 팝업/모달이 열려있으면 뷰포트 입력 무시 (드롭다운 메뉴 방해 방지)
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// 뷰포트 영역 내에서 마우스 이벤트 전달 (카메라 컨트롤용)
	if (ActiveState && ActiveState->Viewport && ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y), static_cast<int32>(Button));
	}
}

void SParticleEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (MainSplitter)
	{
		MainSplitter->OnMouseUp(MousePos, Button);
	}

	// 팝업/모달이 열려있으면 뷰포트 입력 무시 (드롭다운 메뉴 방해 방지)
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// 뷰포트 영역 내에서 마우스 이벤트 전달 (카메라 컨트롤용)
	if (ActiveState && ActiveState->Viewport && ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y), static_cast<int32>(Button));
	}
}

void SParticleEditorWindow::OnRenderViewport()
{
	if (!bIsOpen || !ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 뷰포트 영역이 유효한 경우에만 렌더링
	if (ViewportRect.GetWidth() > 0 && ViewportRect.GetHeight() > 0)
	{
		ActiveState->Viewport->Resize(
			static_cast<uint32>(ViewportRect.Left),
			static_cast<uint32>(ViewportRect.Top),
			static_cast<uint32>(ViewportRect.GetWidth()),
			static_cast<uint32>(ViewportRect.GetHeight())
		);

		if (ActiveState->Client)
		{
			ActiveState->Client->Draw(ActiveState->Viewport);
		}
	}
}

void SParticleEditorWindow::LoadParticleSystem(const FString& Path)
{
	if (!ActiveState)
	{
		return;
	}

	// TODO: 파일에서 파티클 시스템 로드
	UParticleSystem* System = UResourceManager::GetInstance().Load<UParticleSystem>(Path);
	if (System)
	{
		SetParticleSystem(System);
	}
}

void SParticleEditorWindow::SetParticleSystem(UParticleSystem* InSystem)
{
	if (!ActiveState)
	{
		return;
	}

	ActiveState->CurrentSystem = InSystem;
	ActiveState->SelectedEmitterIndex = -1;
	ActiveState->SelectedModuleIndex = -1;
	ActiveState->SelectedEmitter = nullptr;
	ActiveState->SelectedModule = nullptr;

	// 프리뷰 액터에 설정
	if (ActiveState->PreviewActor && InSystem)
	{
		ActiveState->PreviewActor->SetParticleSystem(InSystem);
	}
}

FViewport* SParticleEditorWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SParticleEditorWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}

void SParticleEditorWindow::LoadToolbarIcons()
{
	// 파일 관련
	IconSave = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Toolbar_Save.dds");
	IconLoad = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Toolbar_Load.dds");

	// 시뮬레이션 제어
	IconRestartSim = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_RestartSim.dds");
	IconRestartLevel = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_RestartLevel.dds");

	// 편집
	IconUndo = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_Undo.dds");
	IconRedo = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_Redo.dds");

	// 뷰 옵션
	IconThumbnail = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_Thumbnail.dds");
	IconBounds = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_Bounds.dds");
	IconOriginAxis = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_OriginAxis.dds");
	IconBackgroundColor = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_BgColor.dds");

	// LOD 관련
	IconRegenLOD = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_RegenLOD.dds");
	IconLowestLOD = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_LowestLOD.dds");
	IconLowerLOD = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_LowerLOD.dds");
	IconHigherLOD = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_HigherLOD.dds");
	IconAddLOD = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Particle_AddLOD.dds");

	// 모듈 UI 아이콘
	IconCurveEditor = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/CurveEditor_32x.dds");
	IconCheckbox = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/check_box_hovered.dds");
	IconCheckboxChecked = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/check_box_checked.dds");

	// 이미터 헤더 아이콘
	IconEmitterSolo = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Emitter_Solo.dds");
	IconRenderModeNormal = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/ParticleSprite_16x.dds");
	IconRenderModeCross = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/RenderMode_Cross.dds");
}

bool SParticleEditorWindow::RenderIconButton(const char* id, UTexture* icon, const char* label, const char* tooltip, bool bActive)
{
	const float IconSize = 24.0f;
	const float ButtonWidth = 75.0f;
	const float ButtonHeight = 48.0f;

	bool bClicked = false;

	ImGui::BeginGroup();
	ImGui::PushID(id);

	// 버튼 배경
	ImVec2 buttonPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// 활성화 상태 배경
	if (bActive)
	{
		drawList->AddRectFilled(
			buttonPos,
			ImVec2(buttonPos.x + ButtonWidth, buttonPos.y + ButtonHeight),
			IM_COL32(60, 100, 140, 255),
			4.0f
		);
	}

	// 투명 버튼 (클릭 감지용)
	ImGui::InvisibleButton("##btn", ImVec2(ButtonWidth, ButtonHeight));
	if (ImGui::IsItemClicked())
	{
		bClicked = true;
	}

	// 호버 효과
	if (ImGui::IsItemHovered())
	{
		drawList->AddRectFilled(
			buttonPos,
			ImVec2(buttonPos.x + ButtonWidth, buttonPos.y + ButtonHeight),
			IM_COL32(80, 80, 80, 100),
			4.0f
		);
		if (tooltip)
		{
			ImGui::SetTooltip("%s", tooltip);
		}
	}

	// 아이콘 중앙 배치
	float iconX = buttonPos.x + (ButtonWidth - IconSize) * 0.5f;
	float iconY = buttonPos.y + 2.0f;

	if (icon && icon->GetShaderResourceView())
	{
		drawList->AddImage(
			(void*)icon->GetShaderResourceView(),
			ImVec2(iconX, iconY),
			ImVec2(iconX + IconSize, iconY + IconSize)
		);
	}
	else
	{
		// 아이콘이 없으면 플레이스홀더
		drawList->AddRect(
			ImVec2(iconX, iconY),
			ImVec2(iconX + IconSize, iconY + IconSize),
			IM_COL32(100, 100, 100, 255)
		);
	}

	// 텍스트 중앙 배치 (아이콘 아래)
	ImVec2 textSize = ImGui::CalcTextSize(label);
	float textX = buttonPos.x + (ButtonWidth - textSize.x) * 0.5f;
	float textY = iconY + IconSize + 4.0f;
	drawList->AddText(ImVec2(textX, textY), IM_COL32(200, 200, 200, 255), label);

	ImGui::PopID();
	ImGui::EndGroup();

	return bClicked;
}

void SParticleEditorWindow::RenderToolbar()
{
	if (!ActiveState)
	{
		return;
	}

	// 툴바 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

	// 수직 구분선 그리기 헬퍼 람다
	auto DrawVerticalSeparator = [&]()
	{
		ImGui::SameLine();
		ImVec2 sepPos = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(sepPos.x + 4, sepPos.y + 4),
			ImVec2(sepPos.x + 4, sepPos.y + 38),
			IM_COL32(80, 80, 80, 255),
			2.0f
		);
		ImGui::Dummy(ImVec2(12, 0));
		ImGui::SameLine();
	};

	// ================================================================
	// 파일: Save, Load
	// ================================================================
	if (RenderIconButton("Save", IconSave, "Save", "파티클 시스템을 .psys 파일로 저장\n기본 경로: Data/Particle/"))
	{
		if (ActiveState->CurrentSystem)
		{
			FString FilePath = ActiveState->CurrentSystem->GetFilePath();
			if (FilePath.empty() || FilePath == "NewParticleSystem.psys")
			{
				// 경로가 없으면 다이얼로그 열기
				std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
					L"Data/Particle",
					L".psys",
					L"Particle System",
					L"NewParticleSystem.psys"
				);
				if (!SavePath.empty())
				{
					FilePath = SavePath.string();
				}
			}

			if (!FilePath.empty())
			{
				if (ActiveState->CurrentSystem->SaveToFile(FilePath))
				{
					ActiveState->LoadedSystemPath = FilePath;

					// ResourceManager에 등록 (새 파일인 경우)
					FString NormalizedPath = NormalizePath(FilePath);
					UResourceManager::GetInstance().Add<UParticleSystem>(NormalizedPath, ActiveState->CurrentSystem);

					// PropertyRenderer 캐시 클리어 (드롭다운 갱신)
					UPropertyRenderer::ClearResourcesCache();
				}
			}
		}
	}

	ImGui::SameLine();
	if (RenderIconButton("Load", IconLoad, "Load", ".psys 파일에서 파티클 시스템 로드\n기본 경로: Data/Particle/"))
	{
		std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
			L"Data/Particle",
			L".psys",
			L"Particle System"
		);
		if (!LoadPath.empty())
		{
			LoadParticleSystem(LoadPath.string());
		}
	}

	DrawVerticalSeparator();

	// ================================================================
	// 시뮬레이션: Restart Sim, Restart Level
	// ================================================================
	if (RenderIconButton("RestartSim", IconRestartSim, "Restart Sim", "파티클 시뮬레이션 재시작\n모든 파티클과 누적 시간 리셋"))
	{
		if (ActiveState->PreviewActor)
		{
			UParticleSystemComponent* PSC = ActiveState->PreviewActor->GetParticleSystemComponent();
			if (PSC)
			{
				PSC->ActivateSystem(true);
			}
		}
		ActiveState->AccumulatedTime = 0.0f;
	}

	ImGui::SameLine();
	if (RenderIconButton("RestartLevel", IconRestartLevel, "Restart Level", "Actor 위치/회전 리셋 및 시뮬레이션 재시작\n원점 (0, 0, 0)으로 초기화"))
	{
		if (ActiveState->PreviewActor)
		{
			ActiveState->PreviewActor->SetActorLocation(FVector::Zero());
			ActiveState->PreviewActor->SetActorRotation(FQuat::Identity());
		}
		ActiveState->AccumulatedTime = 0.0f;
		if (ActiveState->PreviewActor)
		{
			UParticleSystemComponent* PSC = ActiveState->PreviewActor->GetParticleSystemComponent();
			if (PSC)
			{
				PSC->ActivateSystem(true);
			}
		}
	}

	DrawVerticalSeparator();

	// ================================================================
	// 편집: Undo, Redo
	// ================================================================
	if (RenderIconButton("Undo", IconUndo, "Undo", "마지막 작업 실행 취소 (Ctrl+Z)\n(TBD)"))
	{
		// TODO: Undo 구현
	}

	ImGui::SameLine();
	if (RenderIconButton("Redo", IconRedo, "Redo", "실행 취소한 작업 다시 실행 (Ctrl+Y)\n(TBD)"))
	{
		// TODO: Redo 구현
	}

	DrawVerticalSeparator();

	// ================================================================
	// Thumbnail - 선택된 이미터의 썸네일 캡처
	// ================================================================
	if (RenderIconButton("Thumbnail", IconThumbnail, "Thumbnail", "선택된 Emitter의 128x128 썸네일 캡처 (.psys에 인라인 저장)"))
	{
		// 선택된 이미터가 있으면 썸네일 캡처
		if (ActiveState->SelectedEmitter && ActiveState->CurrentSystem)
		{
			D3D11RHI* RHI = GEngine.GetRHIDevice();
			if (RHI && PreviewRenderTargetTexture)
			{
				// 전용 렌더 타겟에서 썸네일 캡처 (128x128)
				std::vector<uint8_t> DDSBuffer;
				if (RHI->CaptureRenderTargetToMemory(PreviewRenderTargetTexture, DDSBuffer, 128, 128))
				{
					// Base64 인코딩
					FString Base64Data = FBase64::Encode(DDSBuffer.data(), DDSBuffer.size());
					ActiveState->SelectedEmitter->ThumbnailData = Base64Data;

					// 기존 캐시 무효화 (다시 촬영 시 즉시 반영)
					FString CacheKey = "ParticleEmitter_" + std::to_string(ActiveState->SelectedEmitter->UUID);
					FThumbnailManager::GetInstance().InvalidateThumbnail(CacheKey);

					// .psys 파일 캐시도 무효화 (ContentBrowser 썸네일 갱신)
					if (!ActiveState->CurrentSystem->GetFilePath().empty())
					{
						FThumbnailManager::GetInstance().InvalidateThumbnail(ActiveState->CurrentSystem->GetFilePath());
					}

					UE_LOG("ParticleEditor: Thumbnail captured (128x128) for emitter: %s", ActiveState->SelectedEmitter->EmitterName.c_str());
				}
			}
		}
	}

	DrawVerticalSeparator();

	// ================================================================
	// 뷰 옵션: Bounds (토글)
	// ================================================================
	if (RenderIconButton("Bounds", IconBounds, "Bounds", "바운딩 박스 표시 토글\n뷰포트에 파티클 시스템 경계 표시", ActiveState->bShowBounds))
	{
		ActiveState->bShowBounds = !ActiveState->bShowBounds;
	}

	DrawVerticalSeparator();

	// ================================================================
	// Origin Axis (토글)
	// ================================================================
	if (RenderIconButton("OriginAxis", IconOriginAxis, "Origin Axis", "원점 축 기즈모 표시 토글\nEmitter 원점에 X(빨강) Y(초록) Z(파랑) 축 표시", ActiveState->bShowOriginAxis))
	{
		ActiveState->bShowOriginAxis = !ActiveState->bShowOriginAxis;
	}

	DrawVerticalSeparator();

	// ================================================================
	// Background Color (팝업)
	// ================================================================
	if (RenderIconButton("BgColor", IconBackgroundColor, "BG Color", "뷰포트 배경색 변경\n클릭하여 컬러피커 열기", false))
	{
		ImGui::OpenPopup("##BgColorPopup");
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 관련: Regen LOD 버튼들
	// ================================================================
	if (RenderIconButton("RegenLODDup", IconRegenLOD, "Regen LOD", "최상위 LOD를 복제하여 최하위 LOD 재생성\n(TBD)"))
	{
		// TODO: LOD 재생성 (최하위, 최상위 복제)
	}

	ImGui::SameLine();
	if (RenderIconButton("RegenLOD", IconRegenLOD, "Regen LOD", "최하위 LOD 자동 재생성\n(TBD)"))
	{
		// TODO: LOD 재생성 (최하위)
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 네비게이션: Lowest, Lower
	// ================================================================
	if (RenderIconButton("LowestLOD", IconLowestLOD, "Lowest LOD", "최하위 디테일 LOD로 이동 (LOD 0)"))
	{
		CurrentLODIndex = 0;
	}

	ImGui::SameLine();
	if (RenderIconButton("LowerLOD", IconLowerLOD, "Lower LOD", "낮은 디테일 LOD로 이동"))
	{
		if (CurrentLODIndex > 0)
		{
			--CurrentLODIndex;
		}
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 추가 및 선택
	// ================================================================
	if (RenderIconButton("AddLODBefore", IconAddLOD, "Add LOD", "현재 LOD 앞에 새 LOD 추가\n(TBD)"))
	{
		// TODO: LOD 추가 (현재 앞에)
	}

	ImGui::SameLine();

	// LOD 입력창
	ImGui::BeginGroup();
	{
		ImGui::Text("LOD");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("현재 LOD 인덱스 (0 = 최고 디테일)\n수동 편집으로 특정 LOD로 이동");
		}

		ImGui::SetNextItemWidth(40);
		ImGui::InputInt("##LODIndex", &CurrentLODIndex, 0, 0);
		if (CurrentLODIndex < 0)
		{
			CurrentLODIndex = 0;
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();
	if (RenderIconButton("AddLODAfter", IconAddLOD, "Add LOD", "현재 LOD 뒤에 새 LOD 추가\n(TBD)"))
	{
		// TODO: LOD 추가 (현재 뒤에)
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 네비게이션: Higher
	// ================================================================
	if (RenderIconButton("HigherLOD", IconHigherLOD, "Higher LOD", "높은 디테일 LOD로 이동"))
	{
		++CurrentLODIndex;
		// TODO: 최대 LOD 수에 맞게 클램프
	}

	ImGui::PopStyleVar();

	// 툴바와 콘텐츠 사이 간격
	ImGui::Spacing();
	ImGui::Separator();
}

void SParticleEditorWindow::RenderRenameEmitterDialog()
{
	if (!ActiveState || !ActiveState->bRenamingEmitter)
	{
		return;
	}

	// 모달 팝업 열기
	if (!ImGui::IsPopupOpen("Rename Emitter##Modal"))
	{
		ImGui::OpenPopup("Rename Emitter##Modal");
	}

	// 화면 중앙에 위치
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::BeginPopupModal("Rename Emitter##Modal", &ActiveState->bRenamingEmitter, modalFlags))
	{
		ImGui::Text("Enter new emitter name:");
		ImGui::Spacing();

		// 첫 프레임에 포커스 설정
		static bool bFirstFrame = true;
		if (bFirstFrame)
		{
			ImGui::SetKeyboardFocusHere();
			bFirstFrame = false;
		}

		bool bConfirm = ImGui::InputText("##RenameInput", ActiveState->RenameBuffer, sizeof(ActiveState->RenameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::Spacing();

		if (bConfirm || ImGui::Button("OK", ImVec2(120, 0)))
		{
			if (ActiveState->CurrentSystem &&
				ActiveState->RenamingEmitterIndex >= 0 &&
				ActiveState->RenamingEmitterIndex < ActiveState->CurrentSystem->GetNumEmitters())
			{
				UParticleEmitter* Emitter = ActiveState->CurrentSystem->GetEmitter(ActiveState->RenamingEmitterIndex);
				if (Emitter)
				{
					Emitter->EmitterName = ActiveState->RenameBuffer;
				}
			}
			ActiveState->bRenamingEmitter = false;
			ActiveState->RenamingEmitterIndex = -1;
			bFirstFrame = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ActiveState->bRenamingEmitter = false;
			ActiveState->RenamingEmitterIndex = -1;
			bFirstFrame = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
	else
	{
		// 팝업이 닫혔으면 상태 초기화
		ActiveState->bRenamingEmitter = false;
		ActiveState->RenamingEmitterIndex = -1;
	}

	// 다이얼로그를 최상위로
	ImGui::SetWindowFocus("Rename Emitter##Modal");
}

// ============================================================================
// SParticleViewportPanel
// ============================================================================

SParticleViewportPanel::SParticleViewportPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
	ContentRect = FRect(0, 0, 0, 0);
}

void SParticleViewportPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("##ParticleViewport", nullptr, flags))
	{
		ParticleViewerState* State = Owner->GetActiveState();

		// ================================================================
		// View 드롭다운 메뉴
		// ================================================================
		if (ImGui::Button("View"))
		{
			ImGui::OpenPopup("ViewPopup");
		}
		if (ImGui::BeginPopup("ViewPopup"))
		{
			if (State)
			{
				// View Overlays 서브메뉴
				if (ImGui::BeginMenu("View Overlays"))
				{
					ImGui::MenuItem("Particle Counts", nullptr, &State->bShowParticleCounts);
					ImGui::MenuItem("Particle Event Counts", nullptr, &State->bShowParticleEventCounts);
					ImGui::MenuItem("Particle Times", nullptr, &State->bShowParticleTimes);
					ImGui::MenuItem("Particle Memory", nullptr, &State->bShowParticleMemory);
					ImGui::MenuItem("System Completed", nullptr, &State->bShowSystemCompleted);
					ImGui::MenuItem("Emitter Tick Times", nullptr, &State->bShowEmitterTickTimes);
					ImGui::EndMenu();
				}

				ImGui::Separator();

				ImGui::MenuItem("Orbit Mode", nullptr, &State->bOrbitMode);
				ImGui::MenuItem("Vector Fields", nullptr, &State->bShowVectorFields);
				ImGui::MenuItem("Grid", nullptr, &State->bShowGrid);
				ImGui::MenuItem("Wireframe Sphere", nullptr, &State->bShowWireframeSphere);
				ImGui::MenuItem("Post Process", nullptr, &State->bShowPostProcess);
				ImGui::MenuItem("Motion", nullptr, &State->bShowMotion);
				ImGui::MenuItem("Motion Radius", nullptr, &State->bShowMotionRadius);
				ImGui::MenuItem("Geometry", nullptr, &State->bShowGeometry);
				ImGui::MenuItem("Geometry Properties", nullptr, &State->bShowGeometryProperties);
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();

		// ================================================================
		// Time 드롭다운 메뉴
		// ================================================================
		if (ImGui::Button("Time"))
		{
			ImGui::OpenPopup("TimePopup");
		}
		if (ImGui::BeginPopup("TimePopup"))
		{
			if (State)
			{
				ImGui::MenuItem("Play/Pause", nullptr, &State->bIsSimulating);
				ImGui::MenuItem("Realtime", nullptr, &State->bRealtime);
				ImGui::MenuItem("Loop", nullptr, &State->bLooping);

				ImGui::Separator();

				// AnimSpeed 서브메뉴
				if (ImGui::BeginMenu("AnimSpeed"))
				{
					if (ImGui::MenuItem("100%", nullptr, State->SimulationSpeed == 1.0f))
					{
						State->SimulationSpeed = 1.0f;
					}
					if (ImGui::MenuItem("50%", nullptr, State->SimulationSpeed == 0.5f))
					{
						State->SimulationSpeed = 0.5f;
					}
					if (ImGui::MenuItem("25%", nullptr, State->SimulationSpeed == 0.25f))
					{
						State->SimulationSpeed = 0.25f;
					}
					if (ImGui::MenuItem("10%", nullptr, State->SimulationSpeed == 0.1f))
					{
						State->SimulationSpeed = 0.1f;
					}
					if (ImGui::MenuItem("1%", nullptr, State->SimulationSpeed == 0.01f))
					{
						State->SimulationSpeed = 0.01f;
					}
					ImGui::EndMenu();
				}
			}
			ImGui::EndPopup();
		}

		// 뷰포트 렌더링 영역
		ImGui::BeginChild("ParticleViewportRenderArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
		{
			// 뷰포트 크기 가져오기
			const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
			const uint32 NewWidth = static_cast<uint32>(ViewportSize.x);
			const uint32 NewHeight = static_cast<uint32>(ViewportSize.y);

			// 렌더 타겟 업데이트 (크기 변경 시)
			if (NewWidth > 0 && NewHeight > 0)
			{
				Owner->UpdateViewportRenderTarget(NewWidth, NewHeight);

				// 파티클 씬을 전용 렌더 타겟에 렌더링
				Owner->RenderToPreviewRenderTarget();

				// ImGui에 렌더 결과 표시
				ID3D11ShaderResourceView* PreviewSRV = Owner->GetPreviewShaderResourceView();
				if (PreviewSRV)
				{
					// 이미지 렌더링 전 위치 저장
					ImVec2 ImagePos = ImGui::GetCursorScreenPos();

					ImTextureID TextureID = reinterpret_cast<ImTextureID>(PreviewSRV);
					ImGui::Image(TextureID, ViewportSize);

					// ContentRect 업데이트 (이미지 실제 렌더링 영역)
					ContentRect.Left = ImagePos.x;
					ContentRect.Top = ImagePos.y;
					ContentRect.Right = ImagePos.x + ViewportSize.x;
					ContentRect.Bottom = ImagePos.y + ViewportSize.y;
					ContentRect.UpdateMinMax();

					// 팝업/모달이 열려있지 않을 때만 입력 처리
					// (드롭다운 메뉴나 모달 다이얼로그가 뷰포트에 가려지는 것을 방지)
					if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
					{
						// 마우스가 이미지 위에 있는지 확인하여 뷰포트 입력으로 간주
						ImVec2 MousePos = ImGui::GetMousePos();
						if (MousePos.x >= ImagePos.x && MousePos.x <= ImagePos.x + ViewportSize.x &&
							MousePos.y >= ImagePos.y && MousePos.y <= ImagePos.y + ViewportSize.y)
						{
							// ImGui가 입력을 캡처하지 않도록 설정 (뷰포트 카메라 컨트롤 활성화)
							ImGui::SetItemAllowOverlap();
						}
					}
				}
			}
			else
			{
				// 유효하지 않은 크기일 경우 ContentRect 초기화
				ContentRect = FRect(0, 0, 0, 0);
				ContentRect.UpdateMinMax();
			}
		}
		ImGui::EndChild();
	}
	else
	{
		// 윈도우가 표시되지 않으면 ContentRect 초기화
		ContentRect = FRect(0, 0, 0, 0);
		ContentRect.UpdateMinMax();
	}
	ImGui::End();
}

void SParticleViewportPanel::OnUpdate(float DeltaSeconds)
{
	// 뷰포트 업데이트는 메인 윈도우에서 처리
}

// ============================================================================
// SParticleDetailPanel
// ============================================================================

SParticleDetailPanel::SParticleDetailPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
}

void SParticleDetailPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Details##ParticleDetail", nullptr, flags))
	{
		ParticleViewerState* State = Owner->GetActiveState();
		if (!State)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No active state");
			ImGui::End();
			return;
		}

		if (!State->SelectedModule)
		{
			// 모듈이 선택되지 않았으면 에미터 정보 표시
			UParticleModuleDetailRenderer::RenderEmitterDetails(State->SelectedEmitter);
		}
		else
		{
			// 선택된 모듈의 디테일 렌더링
			UParticleModuleDetailRenderer::RenderModuleDetails(State->SelectedModule);
		}
	}
	ImGui::End();
}

void SParticleDetailPanel::RenderModuleProperties(UParticleModule* Module)
{
	// ParticleModuleDetailRenderer로 이동됨
	UParticleModuleDetailRenderer::RenderModuleDetails(Module);
}

// ============================================================================
// SParticleEmittersPanel
// ============================================================================

SParticleEmittersPanel::SParticleEmittersPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
}

void SParticleEmittersPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_HorizontalScrollbar;

	if (ImGui::Begin("Emitters##ParticleEmitters", nullptr, flags))
	{
		ParticleViewerState* State = Owner->GetActiveState();
		if (!State)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No active state");
			ImGui::End();
			return;
		}

		// 파티클 시스템이 없으면 자동 생성
		if (!State->CurrentSystem)
		{
			State->CurrentSystem = new UParticleSystem();
			State->CurrentSystem->SetFilePath("NewParticleSystem.psys");

			// PreviewActor에도 설정하여 렌더링되도록 함
			if (State->PreviewActor)
			{
				State->PreviewActor->SetParticleSystem(State->CurrentSystem);
			}
		}

		UParticleSystem* System = State->CurrentSystem;

		// 이미터들을 수평으로 배치 (Cascade 스타일)
		const float EmitterColumnWidth = 180.0f;

		for (int32 i = 0; i < System->GetNumEmitters(); ++i)
		{
			UParticleEmitter* Emitter = System->GetEmitter(i);
			if (!Emitter)
			{
				continue;
			}

			ImGui::PushID(i);
			ImGui::BeginGroup();

			// 이미터 헤더
			RenderEmitterHeader(Emitter, i);

			// 모듈 스택
			ImGui::BeginChild(("EmitterModules" + std::to_string(i)).c_str(),
				ImVec2(EmitterColumnWidth, 0), true);
			RenderModuleStack(Emitter, i);

			// 에미터 블록 내 우클릭 컨텍스트 메뉴 (모듈 추가 + 에미터 조작)
			if (ImGui::BeginPopupContextWindow(("ModuleContext" + std::to_string(i)).c_str(), ImGuiPopupFlags_MouseButtonRight))
			{
				RenderModuleContextMenu(Emitter, i);
				ImGui::EndPopup();
			}

			ImGui::EndChild();

			ImGui::EndGroup();
			ImGui::PopID();

			ImGui::SameLine();
		}

		// 에미터가 없을 때 안내 텍스트
		if (System->GetNumEmitters() == 0)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Right-click to add emitter");
		}

		// 빈 영역 우클릭 → 새 이미터 추가 (에미터 블록 외 영역)
		if (ImGui::BeginPopupContextWindow("EmittersAreaContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Sprite Emitter"))
			{
				AddNewEmitter(System);
			}
			ImGui::EndPopup();
		}

		// Delete 키 처리 (윈도우 포커스 시)
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				DeleteSelectedModule();
			}
		}
	}
	ImGui::End();
}

void SParticleEmittersPanel::RenderEmitterHeader(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	bool bSelected = (State->SelectedEmitterIndex == EmitterIndex && State->SelectedModuleIndex == -1);

	// 시스템에 솔로가 있는지 확인 (솔로가 있으면 솔로가 아닌 이미터는 비활성화 표시)
	bool bSystemHasSolo = false;
	if (State && State->CurrentSystem)
	{
		for (int32 i = 0; i < State->CurrentSystem->GetNumEmitters(); ++i)
		{
			UParticleEmitter* E = State->CurrentSystem->GetEmitter(i);
			if (E && E->bIsSoloing)
			{
				bSystemHasSolo = true;
				break;
			}
		}
	}
	// 실질적으로 비활성화 상태인지 (Enable이 꺼져있거나, 솔로모드에서 솔로가 아닌 경우)
	bool bEffectivelyDisabled = !Emitter->bIsEnabled || (bSystemHasSolo && !Emitter->bIsSoloing);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 CursorPos = ImGui::GetCursorScreenPos();

	// 이미터 색상
	ImVec4 headerColor(
		Emitter->EmitterEditorColor.R,
		Emitter->EmitterEditorColor.G,
		Emitter->EmitterEditorColor.B,
		1.0f
	);

	const float HeaderWidth = 180.0f;
	const float HeaderHeight = 60.0f;
	const float ColorBarHeight = 3.0f;
	const float ThumbnailSize = 36.0f;
	const float IconSize = 20.0f;
	const float Padding = 4.0f;

	// ============ 1) 상단 색상 바 ============
	DrawList->AddRectFilled(
		CursorPos,
		ImVec2(CursorPos.x + HeaderWidth, CursorPos.y + ColorBarHeight),
		IM_COL32(
			static_cast<int>(headerColor.x * 255),
			static_cast<int>(headerColor.y * 255),
			static_cast<int>(headerColor.z * 255),
			255
		)
	);

	// ============ 2) 헤더 배경 ============
	ImU32 bgColor = bSelected ? IM_COL32(60, 90, 130, 255) : IM_COL32(50, 50, 50, 255);
	DrawList->AddRectFilled(
		ImVec2(CursorPos.x, CursorPos.y + ColorBarHeight),
		ImVec2(CursorPos.x + HeaderWidth, CursorPos.y + HeaderHeight),
		bgColor
	);

	// 헤더 영역 저장 (나중에 클릭 처리용)
	ImVec2 HeaderMin = CursorPos;
	ImVec2 HeaderMax = ImVec2(CursorPos.x + HeaderWidth, CursorPos.y + HeaderHeight);

	// ============ 3) 썸네일 영역 (우측) ============
	float ThumbX = CursorPos.x + HeaderWidth - ThumbnailSize - Padding;
	float ThumbY = CursorPos.y + ColorBarHeight + (HeaderHeight - ColorBarHeight - ThumbnailSize) * 0.5f;

	// 썸네일 배경 (어두운 박스)
	DrawList->AddRectFilled(
		ImVec2(ThumbX, ThumbY),
		ImVec2(ThumbX + ThumbnailSize, ThumbY + ThumbnailSize),
		IM_COL32(20, 20, 20, 255)
	);
	DrawList->AddRect(
		ImVec2(ThumbX, ThumbY),
		ImVec2(ThumbX + ThumbnailSize, ThumbY + ThumbnailSize),
		IM_COL32(70, 70, 70, 255)
	);

	// 썸네일 텍스처 로드 (Base64 DDS 데이터로부터)
	ID3D11ShaderResourceView* ThumbSRV = nullptr;
	if (!Emitter->ThumbnailData.empty())
	{
		FString CacheKey = "ParticleEmitter_" + std::to_string(Emitter->UUID);
		ThumbSRV = FThumbnailManager::GetInstance().GetThumbnailFromBase64(
			Emitter->ThumbnailData,
			CacheKey
		);
	}

	if (ThumbSRV)
	{
		// 썸네일 텍스처 표시
		DrawList->AddImage(
			(ImTextureID)ThumbSRV,
			ImVec2(ThumbX, ThumbY),
			ImVec2(ThumbX + ThumbnailSize, ThumbY + ThumbnailSize)
		);
	}
	else
	{
		// 기본 아이콘 표시
		UTexture* ThumbIcon = Owner->GetIconRenderModeNormal();
		if (ThumbIcon && ThumbIcon->GetShaderResourceView())
		{
			float iconSize = 24.0f;
			float iconX = ThumbX + (ThumbnailSize - iconSize) * 0.5f;
			float iconY = ThumbY + (ThumbnailSize - iconSize) * 0.5f;
			DrawList->AddImage(
				(ImTextureID)ThumbIcon->GetShaderResourceView(),
				ImVec2(iconX, iconY),
				ImVec2(iconX + iconSize, iconY + iconSize)
			);
		}
	}


	// ============ 4) 이미터 이름 (좌상단) ============
	float TextX = CursorPos.x + Padding;
	float TextY = CursorPos.y + ColorBarHeight + Padding;

	ImGui::SetCursorScreenPos(ImVec2(TextX, TextY));
	ImGui::PushStyleColor(ImGuiCol_Text, bEffectivelyDisabled ? ImVec4(0.5f, 0.5f, 0.5f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	ImGui::TextUnformatted(Emitter->EmitterName.c_str());
	ImGui::PopStyleColor();

	// ============ 5) 아이콘 버튼 행 (좌하단) ============
	float IconY = TextY + 24;  // 이미터 이름과 버튼 사이 간격 증가
	float IconX = TextX;
	const float IconSpacing = 4.0f;
	const float BtnSize = IconSize + 6;

	ImGui::PushID(("EmitterBtns" + std::to_string(EmitterIndex)).c_str());

	// 투명 버튼 스타일
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

	// 5-1) Enable/Disable 토글 (체크박스)
	ImGui::SetCursorScreenPos(ImVec2(IconX, IconY));
	ImGui::PushID("enable");
	bool bEnableClicked = ImGui::Button("##e", ImVec2(BtnSize, BtnSize));
	bool bEnableHovered = ImGui::IsItemHovered();
	ImGui::PopID();

	// 체크박스 그리기
	{
		ImVec2 BtnMin = ImVec2(IconX, IconY);
		ImVec2 BtnMax = ImVec2(IconX + BtnSize, IconY + BtnSize);
		ImVec2 BtnCenter = ImVec2((BtnMin.x + BtnMax.x) * 0.5f, (BtnMin.y + BtnMax.y) * 0.5f);
		float boxSize = 12.0f;
		ImVec2 BoxMin = ImVec2(BtnCenter.x - boxSize * 0.5f, BtnCenter.y - boxSize * 0.5f);
		ImVec2 BoxMax = ImVec2(BtnCenter.x + boxSize * 0.5f, BtnCenter.y + boxSize * 0.5f);

		// 비활성화 상태면 흐린 색상
		ImU32 boxColor = bEffectivelyDisabled ? IM_COL32(80, 80, 80, 255) : IM_COL32(150, 150, 150, 255);
		ImU32 checkColor = bEffectivelyDisabled ? IM_COL32(60, 100, 60, 255) : IM_COL32(100, 200, 100, 255);

		DrawList->AddRect(BoxMin, BoxMax, boxColor, 2.0f);
		if (Emitter->bIsEnabled)
		{
			DrawList->AddLine(ImVec2(BoxMin.x + 2, BtnCenter.y), ImVec2(BtnCenter.x - 1, BoxMax.y - 3), checkColor, 2.0f);
			DrawList->AddLine(ImVec2(BtnCenter.x - 1, BoxMax.y - 3), ImVec2(BoxMax.x - 2, BoxMin.y + 2), checkColor, 2.0f);
		}
	}
	if (bEnableClicked)
	{
		Emitter->bIsEnabled = !Emitter->bIsEnabled;
		// LODLevel->bEnabled 업데이트
		ParticleViewerState* State = Owner->GetActiveState();
		if (State && State->CurrentSystem)
		{
			State->CurrentSystem->SetupSoloing();
		}
	}
	if (bEnableHovered)
	{
		ImGui::SetTooltip(Emitter->bIsEnabled ? "Disable Emitter" : "Enable Emitter");
	}
	IconX += BtnSize + IconSpacing;

	// 5-2) RenderMode 토글
	ImGui::SetCursorScreenPos(ImVec2(IconX, IconY));
	ImGui::PushID("rendermode");
	bool bRenderModeClicked = ImGui::Button("##r", ImVec2(BtnSize, BtnSize));
	bool bRenderModeHovered = ImGui::IsItemHovered();
	ImGui::PopID();

	// RenderMode 아이콘 그리기
	{
		ImVec2 BtnMin = ImVec2(IconX, IconY);
		ImVec2 BtnMax = ImVec2(IconX + BtnSize, IconY + BtnSize);
		ImVec2 BtnCenter = ImVec2((BtnMin.x + BtnMax.x) * 0.5f, (BtnMin.y + BtnMax.y) * 0.5f);

		UTexture* RenderModeIcon = nullptr;
		const char* RenderModeTooltip = "Normal";

		switch (Emitter->EmitterRenderMode)
		{
		case EEmitterRenderMode::Normal:
			RenderModeIcon = Owner->GetIconRenderModeNormal();
			RenderModeTooltip = "Render Mode: Normal (Sprite)";
			if (RenderModeIcon && RenderModeIcon->GetShaderResourceView())
			{
				DrawList->AddImage((ImTextureID)RenderModeIcon->GetShaderResourceView(),
					ImVec2(BtnMin.x + 3, BtnMin.y + 3), ImVec2(BtnMin.x + 3 + IconSize, BtnMin.y + 3 + IconSize));
			}
			break;
		case EEmitterRenderMode::Point:
			RenderModeTooltip = "Render Mode: Point";
			DrawList->AddCircleFilled(BtnCenter, 4.0f, IM_COL32(200, 200, 200, 255));
			break;
		case EEmitterRenderMode::Cross:
			RenderModeIcon = Owner->GetIconRenderModeCross();
			RenderModeTooltip = "Render Mode: Cross";
			if (RenderModeIcon && RenderModeIcon->GetShaderResourceView())
			{
				DrawList->AddImage((ImTextureID)RenderModeIcon->GetShaderResourceView(),
					ImVec2(BtnMin.x + 3, BtnMin.y + 3), ImVec2(BtnMin.x + 3 + IconSize, BtnMin.y + 3 + IconSize));
			}
			break;
		case EEmitterRenderMode::None:
			RenderModeTooltip = "Render Mode: None (Hidden)";
			{
				float cs = 6.0f;
				DrawList->AddLine(ImVec2(BtnCenter.x - cs, BtnCenter.y - cs), ImVec2(BtnCenter.x + cs, BtnCenter.y + cs), IM_COL32(180, 80, 80, 255), 2.0f);
				DrawList->AddLine(ImVec2(BtnCenter.x + cs, BtnCenter.y - cs), ImVec2(BtnCenter.x - cs, BtnCenter.y + cs), IM_COL32(180, 80, 80, 255), 2.0f);
			}
			break;
		}

		if (bRenderModeClicked)
		{
			int32 mode = static_cast<int32>(Emitter->EmitterRenderMode);
			mode = (mode + 1) % 4;
			Emitter->EmitterRenderMode = static_cast<EEmitterRenderMode>(mode);
		}
		if (bRenderModeHovered)
		{
			ImGui::SetTooltip("%s", RenderModeTooltip);
		}
	}
	IconX += BtnSize + IconSpacing;

	// 5-3) Solo 토글
	ImGui::SetCursorScreenPos(ImVec2(IconX, IconY));

	// Solo 활성화 시 노란색 배경
	if (Emitter->bIsSoloing)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
	}

	ImGui::PushID("solo");
	bool bSoloClicked = ImGui::Button("##s", ImVec2(BtnSize, BtnSize));
	bool bSoloHovered = ImGui::IsItemHovered();
	ImGui::PopID();

	if (Emitter->bIsSoloing)
	{
		ImGui::PopStyleColor(3);
	}

	// Solo 아이콘 그리기
	{
		ImVec2 BtnMin = ImVec2(IconX, IconY);
		ImVec2 BtnMax = ImVec2(IconX + BtnSize, IconY + BtnSize);
		ImVec2 BtnCenter = ImVec2((BtnMin.x + BtnMax.x) * 0.5f, (BtnMin.y + BtnMax.y) * 0.5f);

		UTexture* SoloIcon = Owner->GetIconEmitterSolo();
		if (SoloIcon && SoloIcon->GetShaderResourceView())
		{
			DrawList->AddImage((ImTextureID)SoloIcon->GetShaderResourceView(),
				ImVec2(BtnMin.x + 3, BtnMin.y + 3), ImVec2(BtnMin.x + 3 + IconSize, BtnMin.y + 3 + IconSize));
		}
		else
		{
			const char* label = "S";
			ImVec2 textSize = ImGui::CalcTextSize(label);
			ImVec2 textPos = ImVec2(BtnCenter.x - textSize.x * 0.5f, BtnCenter.y - textSize.y * 0.5f);
			DrawList->AddText(textPos, Emitter->bIsSoloing ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 255), label);
		}
	}

	if (bSoloClicked)
	{
		ParticleViewerState* State = Owner->GetActiveState();

		if (!Emitter->bIsSoloing)
		{
			// 다른 모든 이미터의 솔로 끄기 (하나만 솔로 가능)
			if (State && State->CurrentSystem)
			{
				for (int32 i = 0; i < State->CurrentSystem->GetNumEmitters(); ++i)
				{
					UParticleEmitter* E = State->CurrentSystem->GetEmitter(i);
					if (E && E != Emitter && E->bIsSoloing)
					{
						E->bIsSoloing = false;
						E->bIsEnabled = E->bWasEnabledBeforeSolo;
					}
				}
			}

			Emitter->bWasEnabledBeforeSolo = Emitter->bIsEnabled;
			Emitter->bIsSoloing = true;
		}
		else
		{
			Emitter->bIsSoloing = false;
			Emitter->bIsEnabled = Emitter->bWasEnabledBeforeSolo;
		}

		// LODLevel->bEnabled 업데이트
		if (State && State->CurrentSystem)
		{
			State->CurrentSystem->SetupSoloing();
		}
	}
	if (bSoloHovered)
	{
		ImGui::SetTooltip(Emitter->bIsSoloing ? "Disable Solo" : "Enable Solo");
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
	ImGui::PopID();

	// ============ 6) 헤더 배경 클릭으로 이미터 선택 ============
	// 아이콘 버튼 영역 외 클릭 시 선택 처리
	ImVec2 MousePos = ImGui::GetMousePos();
	bool bMouseInHeader = (MousePos.x >= HeaderMin.x && MousePos.x <= HeaderMax.x &&
	                       MousePos.y >= HeaderMin.y && MousePos.y <= HeaderMax.y);

	if (bMouseInHeader && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = -1;
		State->SelectedEmitter = Emitter;
		State->SelectedModule = nullptr;
	}

	// 커서 위치 복원 (헤더 아래로)
	ImGui::SetCursorScreenPos(ImVec2(CursorPos.x, CursorPos.y + HeaderHeight + 2));
}

void SParticleEmittersPanel::RenderModuleStack(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
	if (!LODLevel)
	{
		return;
	}

	// Required 모듈
	if (LODLevel->RequiredModule)
	{
		RenderModuleItem(static_cast<UParticleModule*>(LODLevel->RequiredModule), -1, EmitterIndex);
	}

	// Spawn 모듈
	if (LODLevel->SpawnModule)
	{
		RenderModuleItem(static_cast<UParticleModule*>(LODLevel->SpawnModule), -2, EmitterIndex);
	}

	// TypeData 모듈 (Mesh, Beam 등)
	if (LODLevel->TypeDataModule)
	{
		RenderModuleItem(static_cast<UParticleModule*>(LODLevel->TypeDataModule), -3, EmitterIndex);
	}

	ImGui::Separator();

	// 일반 모듈들
	for (int32 i = 0; i < static_cast<int32>(LODLevel->Modules.size()); ++i)
	{
		UParticleModule* Module = LODLevel->Modules[i];
		if (Module)
		{
			RenderModuleItem(Module, i, EmitterIndex);
		}
	}
}

// 모듈 클래스 이름으로부터 일관된 색상 생성 (정적 헬퍼)
static FLinearColor GetModuleColor(const char* className)
{
	if (!className)
	{
		return FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // 기본 회색
	}

	if (strstr(className, "Required"))
	{
		return FLinearColor(0.8f, 0.6f, 0.2f, 1.0f); // 노란색
	}
	else if (strstr(className, "Spawn"))
	{
		return FLinearColor(0.7f, 0.3f, 0.3f, 1.0f); // 빨간색
	}
	else if (strstr(className, "Lifetime"))
	{
		return FLinearColor(0.4f, 0.6f, 0.4f, 1.0f); // 녹색
	}
	else if (strstr(className, "Size"))
	{
		return FLinearColor(0.5f, 0.5f, 0.7f, 1.0f); // 파란색
	}
	else if (strstr(className, "Velocity") || strstr(className, "Acceleration"))
	{
		return FLinearColor(0.6f, 0.4f, 0.6f, 1.0f); // 보라색
	}
	else if (strstr(className, "Color"))
	{
		return FLinearColor(0.7f, 0.5f, 0.3f, 1.0f); // 주황색
	}
	else if (strstr(className, "TypeData"))
	{
		return FLinearColor(0.3f, 0.7f, 0.7f, 1.0f); // 청록색 (TypeData)
	}
	else
	{
		return FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // 기본 회색
	}
}

void SParticleEmittersPanel::RenderModuleItem(UParticleModule* Module, int32 ModuleIndex, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	bool bSelected = (State->SelectedEmitterIndex == EmitterIndex && State->SelectedModuleIndex == ModuleIndex);

	// 모듈 타입에 따른 색상
	const char* className = Module->GetClass() ? Module->GetClass()->Name : "Unknown";
	FLinearColor moduleColorLinear = GetModuleColor(className);
	ImVec4 moduleColor = ImVec4(moduleColorLinear.R, moduleColorLinear.G, moduleColorLinear.B, moduleColorLinear.A);

	if (bSelected)
	{
		moduleColor = ImVec4(0.3f, 0.5f, 0.8f, 1.0f); // 선택 시 파란색
	}

	ImGui::PushStyleColor(ImGuiCol_Header, moduleColor);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(moduleColor.x * 1.2f, moduleColor.y * 1.2f, moduleColor.z * 1.2f, 1.0f));

	// 모듈 이름 추출 (UParticleModule 접두사 제거)
	FString ModuleName = className;
	if (ModuleName.find("UParticleModule") == 0)
	{
		ModuleName = ModuleName.substr(15);
	}

	ImGui::PushID(ModuleIndex + EmitterIndex * 1000);

	// ============ 모듈 이름 + 우측 아이콘 UI ============
	const float IconSize = 14.0f;
	const float IconPadding = 4.0f;
	const float IconRightMargin = 16.0f;  // 오른쪽 여백 (하이라이트와 아이콘 사이 간격)
	float AvailWidth = ImGui::GetContentRegionAvail().x;
	bool bHasCurves = Module->ModuleHasCurves();

	// 아이콘 영역 너비 계산 (항상 2버튼 공간 확보 - 정렬 일관성)
	// ImageButton(FramePadding=0) = IconSize, 버튼2개 + 패딩 + 여백
	float IconAreaWidth = IconSize * 2 + IconPadding + IconRightMargin;

	// 비활성화된 모듈은 이름을 흐리게
	if (!Module->IsEnabled())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.7f));
	}

	// 커브 에디터에 추가된 경우 색상 띠 공간 확보 (6px 인덴트)
	if (Module->HasCurvesInEditor())
	{
		ImGui::Indent(6.0f);
	}

	// 모듈 이름 (Selectable) - 우측에 아이콘 공간 확보
	ImVec2 SelectableStartPos = ImGui::GetCursorScreenPos();
	float SelectableWidth = Module->HasCurvesInEditor() ? (AvailWidth - IconAreaWidth - 6.0f) : (AvailWidth - IconAreaWidth);
	if (ImGui::Selectable(ModuleName.c_str(), bSelected, 0, ImVec2(SelectableWidth, 20)))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = ModuleIndex;
		State->SelectedModule = Module;

		if (EmitterIndex >= 0 && State->CurrentSystem && EmitterIndex < State->CurrentSystem->GetNumEmitters())
		{
			State->SelectedEmitter = State->CurrentSystem->GetEmitter(EmitterIndex);
		}
	}

	// 커브 에디터에 추가된 모듈이면 좌측에 색상 라인 렌더링 (4px 너비)
	if (Module->HasCurvesInEditor())
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImVec2 LineMin = ImVec2(SelectableStartPos.x - 6.0f, SelectableStartPos.y);
		ImVec2 LineMax = ImVec2(SelectableStartPos.x - 2.0f, SelectableStartPos.y + 20);
		ImU32 LineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(moduleColorLinear.R, moduleColorLinear.G, moduleColorLinear.B, 1.0f));
		DrawList->AddRectFilled(LineMin, LineMax, LineColor);

		ImGui::Unindent(6.0f);
	}

	// 드래그 앤 드롭 - 일반 모듈만 (Required/Spawn/TypeData 제외)
	// ModuleIndex: -1 = Required, -2 = Spawn, -3 = TypeData, 0+ = 일반 모듈
	// Selectable 바로 다음에 호출해야 Selectable에 드래그 소스/타겟이 붙음
	if (ModuleIndex >= 0)
	{
		// 드래그 소스
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			// 페이로드: EmitterIndex와 ModuleIndex
			struct ModuleDragPayload
			{
				int32 EmitterIndex;
				int32 ModuleIndex;
			};
			ModuleDragPayload payload = { EmitterIndex, ModuleIndex };
			ImGui::SetDragDropPayload("MODULE_REORDER", &payload, sizeof(payload));

			// 드래그 프리뷰
			ImGui::Text("Move: %s", ModuleName.c_str());
			ImGui::EndDragDropSource();
		}

		// 드롭 타겟
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("MODULE_REORDER"))
			{
				struct ModuleDragPayload
				{
					int32 EmitterIndex;
					int32 ModuleIndex;
				};
				ModuleDragPayload* data = (ModuleDragPayload*)imguiPayload->Data;

				// 같은 에미터 내에서만 순서 변경 가능
				if (data->EmitterIndex == EmitterIndex && data->ModuleIndex != ModuleIndex)
				{
					UParticleEmitter* Emitter = State->CurrentSystem->GetEmitter(EmitterIndex);
					if (Emitter)
					{
						// 모든 LOD 레벨에서 모듈 순서 변경
						for (int32 lod = 0; lod < Emitter->GetNumLODs(); ++lod)
						{
							UParticleLODLevel* LODLevel = Emitter->GetLODLevel(lod);
							if (LODLevel && data->ModuleIndex < static_cast<int32>(LODLevel->Modules.size()) &&
								ModuleIndex < static_cast<int32>(LODLevel->Modules.size()))
							{
								// 모듈 swap
								UParticleModule* DraggedModule = LODLevel->Modules[data->ModuleIndex];
								LODLevel->Modules.erase(LODLevel->Modules.begin() + data->ModuleIndex);

								// 삽입 위치 조정 (삭제 후 인덱스 보정)
								int32 InsertIndex = ModuleIndex;
								if (data->ModuleIndex < ModuleIndex)
								{
									--InsertIndex;
								}
								LODLevel->Modules.insert(LODLevel->Modules.begin() + InsertIndex, DraggedModule);
							}
						}

						// 선택 상태 업데이트 (드래그한 모듈 선택 유지)
						State->SelectedModuleIndex = ModuleIndex;
						if (data->ModuleIndex < ModuleIndex)
						{
							--State->SelectedModuleIndex;
						}

						// EmitterInstance 업데이트 (페이로드 오프셋 재계산)
						RefreshEmitterInstances();
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	// 비활성화 텍스트 색상 복원
	if (!Module->IsEnabled())
	{
		ImGui::PopStyleColor();
	}

	// ============ 우측 정렬 아이콘 (커브 + 체크박스) ============
	// ImageButton frame padding 제거로 일관된 크기 유지
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

	// Selectable 높이(20px)와 아이콘 크기 차이를 계산하여 수직 중앙 정렬
	float SelectableHeight = 20.0f;
	float IconVerticalOffset = (SelectableHeight - IconSize) * 0.5f;

	ImGui::SameLine(AvailWidth - IconAreaWidth + 16);  // 아이콘 시작 위치
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + IconVerticalOffset); // 세로 중앙 정렬

	// 1) 커브 버튼 (커브가 있는 모듈만 표시)
	if (bHasCurves)
	{
		UTexture* CurveIcon = Owner->GetIconCurveEditor();
		if (CurveIcon && CurveIcon->GetShaderResourceView())
		{
			bool bCurvesInEditor = Module->HasCurvesInEditor();

			if (bCurvesInEditor)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
			}
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.4f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.7f, 0.5f, 1.0f));

			ImGui::PushID("curve");
			if (ImGui::ImageButton("##c", (void*)CurveIcon->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
			{
				bool bNewState = !bCurvesInEditor;
				Module->SetCurvesInEditor(bNewState);

				// 커브 에디터 패널에 추가/제거
				SParticleCurveEditorPanel* CurvePanel = Owner->GetCurveEditorPanel();
				if (CurvePanel)
				{
					const char* ClassName = Module->GetClass()->Name;
					FLinearColor BaseColor = GetModuleColor(ClassName);

					if (bNewState)
					{
						// 모듈 타입별로 여러 커브 추가 (UE 스타일)
						if (strstr(ClassName, "Velocity"))
						{
							// StartVelocity: X, Y, Z
							CurvePanel->AddCurve(FString(ClassName) + ".StartVelocity.X", FLinearColor(1.0f, 0.3f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartVelocity.Y", FLinearColor(0.3f, 1.0f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartVelocity.Z", FLinearColor(0.3f, 0.3f, 1.0f, 1.0f), Module);
							// StartVelocityRadial
							CurvePanel->AddCurve(FString(ClassName) + ".StartVelocityRadial", FLinearColor(1.0f, 1.0f, 0.3f, 1.0f), Module);
						}
						else if (strstr(ClassName, "Size"))
						{
							// StartSize: X, Y, Z
							CurvePanel->AddCurve(FString(ClassName) + ".StartSize.X", FLinearColor(1.0f, 0.3f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartSize.Y", FLinearColor(0.3f, 1.0f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartSize.Z", FLinearColor(0.3f, 0.3f, 1.0f, 1.0f), Module);
						}
						else if (strstr(ClassName, "Color"))
						{
							// StartColor: R, G, B, A
							CurvePanel->AddCurve(FString(ClassName) + ".StartColor.R", FLinearColor(1.0f, 0.3f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartColor.G", FLinearColor(0.3f, 1.0f, 0.3f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartColor.B", FLinearColor(0.3f, 0.3f, 1.0f, 1.0f), Module);
							CurvePanel->AddCurve(FString(ClassName) + ".StartAlpha", FLinearColor(0.8f, 0.8f, 0.8f, 1.0f), Module);
						}
						else if (strstr(ClassName, "Lifetime"))
						{
							// Lifetime (단일 float)
							CurvePanel->AddCurve(FString(ClassName) + ".Lifetime", BaseColor, Module);
						}
						else if (strstr(ClassName, "RotationRate") || strstr(ClassName, "MeshRotationRate"))
						{
							// RotationRate (단일 float - Degrees/Second)
							CurvePanel->AddCurve(FString(ClassName) + ".StartRotationRate", BaseColor, Module);
						}
						else if (strstr(ClassName, "Rotation") || strstr(ClassName, "MeshRotation"))
						{
							// StartRotation (단일 float - Degrees)
							CurvePanel->AddCurve(FString(ClassName) + ".StartRotation", BaseColor, Module);
						}
						else
						{
							// 기본: 모듈 이름만
							CurvePanel->AddCurve(FString(ClassName), BaseColor, Module);
						}
					}
					else
					{
						// 커브 에디터에서 제거 (해당 모듈의 모든 커브 제거)
						CurvePanel->RemoveAllCurvesForModule(FString(ClassName));
					}
				}
			}
			ImGui::PopID();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(bCurvesInEditor ? "Remove from Curve Editor" : "Add to Curve Editor");
			}
			ImGui::PopStyleColor(3);
		}
		else
		{
			// 아이콘 없으면 Dummy로 공간 확보
			ImGui::Dummy(ImVec2(IconSize, IconSize));
		}
	}
	else
	{
		// 커브 버튼 없을 때 동일 공간 확보 (정렬 유지)
		ImGui::Dummy(ImVec2(IconSize, IconSize));
	}

	ImGui::SameLine(0, IconPadding);

	// 2) 체크박스 (모듈 활성화)
	UTexture* CheckboxIcon = Module->IsEnabled() ? Owner->GetIconCheckboxChecked() : Owner->GetIconCheckbox();
	if (CheckboxIcon && CheckboxIcon->GetShaderResourceView())
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

		ImGui::PushID("enable");
		if (ImGui::ImageButton("##e", (void*)CheckboxIcon->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
		{
			Module->SetEnabled(!Module->IsEnabled());
		}
		ImGui::PopID();
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(Module->IsEnabled() ? "Disable Module" : "Enable Module");
		}
		ImGui::PopStyleColor(3);
	}

	ImGui::PopStyleVar();  // FramePadding 복원

	ImGui::PopID();

	ImGui::PopStyleColor(2);

	// 3D 드로우 모드 표시
	if (Module->IsSupported3DDrawMode())
	{
		ImGui::SameLine(150);
		ImGui::TextColored(
			Module->Is3DDrawMode() ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
			"3D");
	}
}

void SParticleEmittersPanel::AddNewEmitter(UParticleSystem* System)
{
	if (!System)
	{
		return;
	}

	// 새 Emitter 생성
	UParticleEmitter* NewEmitter = new UParticleEmitter();
	NewEmitter->EmitterName = "New Sprite Emitter";

	// 에디터 색상 랜덤 설정 (파스텔 톤)
	NewEmitter->EmitterEditorColor.R = 0.4f + (rand() % 60) / 100.0f;
	NewEmitter->EmitterEditorColor.G = 0.4f + (rand() % 60) / 100.0f;
	NewEmitter->EmitterEditorColor.B = 0.4f + (rand() % 60) / 100.0f;
	NewEmitter->EmitterEditorColor.A = 1.0f;

	// LOD Level 0 생성
	UParticleLODLevel* LODLevel = new UParticleLODLevel();

	// 기본 Required 모듈
	UParticleModuleRequired* RequiredModule = new UParticleModuleRequired();
	LODLevel->RequiredModule = RequiredModule;

	// 기본 Spawn 모듈
	UParticleModuleSpawn* SpawnModule = new UParticleModuleSpawn();
	LODLevel->SpawnModule = SpawnModule;

	// LOD Level 추가
	NewEmitter->LODLevels.Add(LODLevel);

	// 시스템에 이미터 추가
	System->Emitters.Add(NewEmitter);

	// 시스템 빌드 (EmitterInstance 생성을 위해 필요)
	System->BuildEmitters();

	// 선택 상태 업데이트
	ParticleViewerState* State = Owner->GetActiveState();
	if (State)
	{
		State->SelectedEmitterIndex = System->GetNumEmitters() - 1;
		State->SelectedModuleIndex = -1;
		State->SelectedEmitter = NewEmitter;
		State->SelectedModule = nullptr;

		// Preview Actor의 EmitterInstance 업데이트
		if (State->PreviewActor)
		{
			UParticleSystemComponent* PSC = State->PreviewActor->GetParticleSystemComponent();
			if (PSC)
			{
				PSC->UpdateInstances(true);
			}
		}
	}
}

void SParticleEmittersPanel::RenderModuleContextMenu(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	if (!Emitter)
	{
		return;
	}

	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
	if (!LODLevel)
	{
		return;
	}

	ParticleViewerState* State = Owner->GetActiveState();

	// Emitter 서브메뉴 (Cascade 스타일)
	if (ImGui::BeginMenu("Emitter"))
	{
		RenderEmitterContextMenu(Emitter, EmitterIndex);
		ImGui::EndMenu();
	}

	// TypeData 서브메뉴
	if (ImGui::BeginMenu("TypeData"))
	{
		// 현재 TypeData가 있는지 확인
		bool bHasTypeData = (LODLevel->TypeDataModule != nullptr);
		FString CurrentTypeStr = "None";
		if (bHasTypeData)
		{
			if (LODLevel->TypeDataModule->IsA<UParticleModuleTypeDataMesh>())
			{
				CurrentTypeStr = "Mesh";
			}
			else if (LODLevel->TypeDataModule->IsA<UParticleModuleTypeDataBeam>())
			{
				CurrentTypeStr = "Beam";
			}
			else
			{
				CurrentTypeStr = "Sprite";
			}
		}
		ImGui::Text("Current: %s", CurrentTypeStr.c_str());
		ImGui::Separator();

		if (ImGui::MenuItem("Sprite (Default)"))
		{
			// TypeData 제거 (기본 스프라이트로)
			if (LODLevel->TypeDataModule)
			{
				delete LODLevel->TypeDataModule;
				LODLevel->TypeDataModule = nullptr;
			}
			RefreshEmitterInstances();
		}
		if (ImGui::MenuItem("Mesh"))
		{
			// 기존 TypeData 제거
			if (LODLevel->TypeDataModule)
			{
				delete LODLevel->TypeDataModule;
				LODLevel->TypeDataModule = nullptr;
			}
			// 새 Mesh TypeData 생성
			UParticleModuleTypeDataMesh* MeshTypeData = new UParticleModuleTypeDataMesh();
			MeshTypeData->SetOwnerSystem(State->CurrentSystem);
			LODLevel->TypeDataModule = MeshTypeData;
			RefreshEmitterInstances();
		}
		ImGui::Separator();
		ImGui::MenuItem("GPU Sprites", nullptr, false, false); // TODO
		if (ImGui::MenuItem("Beam"))
		{
			// 기존 TypeData 제거
			if (LODLevel->TypeDataModule)
			{
				delete LODLevel->TypeDataModule;
				LODLevel->TypeDataModule = nullptr;
			}
			// 새 Beam TypeData 생성
			UParticleModuleTypeDataBeam* BeamTypeData = new UParticleModuleTypeDataBeam();
			LODLevel->TypeDataModule = BeamTypeData;
			RefreshEmitterInstances();
		}
		ImGui::MenuItem("Ribbon", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	ImGui::Separator();

	// Acceleration 서브메뉴
	if (ImGui::BeginMenu("Acceleration"))
	{
		ImGui::MenuItem("Const Acceleration", nullptr, false, false); // TODO
		ImGui::MenuItem("Drag", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Attraction 서브메뉴
	if (ImGui::BeginMenu("Attraction"))
	{
		ImGui::MenuItem("Line Attractor", nullptr, false, false); // TODO
		ImGui::MenuItem("Point Attractor", nullptr, false, false); // TODO
		ImGui::MenuItem("Point Gravity", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Collision 서브메뉴
	if (ImGui::BeginMenu("Collision"))
	{
		ImGui::MenuItem("Collision", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Color 서브메뉴
	if (ImGui::BeginMenu("Color"))
	{
		if (ImGui::MenuItem("Initial Color"))
		{
			UParticleModuleColor* Module = new UParticleModuleColor();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("Color Over Life", nullptr, false, false); // TODO
		ImGui::MenuItem("Scale Color/Life", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Event 서브메뉴
	if (ImGui::BeginMenu("Event"))
	{
		ImGui::MenuItem("Event Generator", nullptr, false, false); // TODO
		ImGui::MenuItem("Event Receiver Kill All", nullptr, false, false); // TODO
		ImGui::MenuItem("Event Receiver Spawn", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Lifetime 서브메뉴
	if (ImGui::BeginMenu("Lifetime"))
	{
		if (ImGui::MenuItem("Lifetime"))
		{
			UParticleModuleLifetime* Module = new UParticleModuleLifetime();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::EndMenu();
	}

	// Location 서브메뉴
	if (ImGui::BeginMenu("Location"))
	{
		if (ImGui::MenuItem("Initial Location"))
		{
			UParticleModuleLocation* Module = new UParticleModuleLocation();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("World Offset", nullptr, false, false); // TODO
		ImGui::MenuItem("Bone/Socket Location", nullptr, false, false); // TODO
		ImGui::MenuItem("Direct Location", nullptr, false, false); // TODO
		ImGui::MenuItem("Cylinder", nullptr, false, false); // TODO
		ImGui::MenuItem("Sphere", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Rotation 서브메뉴
	if (ImGui::BeginMenu("Rotation"))
	{
		if (ImGui::MenuItem("Initial Rotation"))
		{
			UParticleModuleRotation* Module = new UParticleModuleRotation();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		if (ImGui::MenuItem("Mesh Rotation (3D)"))
		{
			UParticleModuleMeshRotation* Module = new UParticleModuleMeshRotation();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("Rotation Over Life", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Rotation Rate 서브메뉴
	if (ImGui::BeginMenu("Rotation Rate"))
	{
		if (ImGui::MenuItem("Initial Rotation Rate"))
		{
			UParticleModuleRotationRate* Module = new UParticleModuleRotationRate();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		if (ImGui::MenuItem("Mesh Rotation Rate (3D)"))
		{
			UParticleModuleMeshRotationRate* Module = new UParticleModuleMeshRotationRate();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("Rotation Rate Over Life", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Orbit 서브메뉴
	if (ImGui::BeginMenu("Orbit"))
	{
		ImGui::MenuItem("Orbit", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Orientation 서브메뉴
	if (ImGui::BeginMenu("Orientation"))
	{
		ImGui::MenuItem("Axis Lock", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Size 서브메뉴
	if (ImGui::BeginMenu("Size"))
	{
		if (ImGui::MenuItem("Initial Size"))
		{
			UParticleModuleSize* Module = new UParticleModuleSize();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("Size By Life", nullptr, false, false); // TODO
		ImGui::MenuItem("Size Scale", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Spawn 서브메뉴
	if (ImGui::BeginMenu("Spawn"))
	{
		ImGui::MenuItem("Spawn Per Unit", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// SubUV 서브메뉴
	if (ImGui::BeginMenu("SubUV"))
	{
		if (ImGui::MenuItem("SubImage Index"))
		{
			UParticleModuleSubUV* Module = new UParticleModuleSubUV();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("SubUV Movie", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Vector Field 서브메뉴
	if (ImGui::BeginMenu("Vector Field"))
	{
		ImGui::MenuItem("VF Global", nullptr, false, false); // TODO
		ImGui::MenuItem("VF Local", nullptr, false, false); // TODO
		ImGui::MenuItem("VF Rotation", nullptr, false, false); // TODO
		ImGui::MenuItem("VF Rotation Rate", nullptr, false, false); // TODO
		ImGui::MenuItem("VF Scale", nullptr, false, false); // TODO
		ImGui::MenuItem("VF Scale Over Life", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Velocity 서브메뉴
	if (ImGui::BeginMenu("Velocity"))
	{
		if (ImGui::MenuItem("Initial Velocity"))
		{
			UParticleModuleVelocity* Module = new UParticleModuleVelocity();
			AddModuleAndUpdateInstances(LODLevel, Module);
		}
		ImGui::MenuItem("Velocity Over Life", nullptr, false, false); // TODO
		ImGui::MenuItem("Inherit Parent Velocity", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Beam 서브메뉴 (TypeData Beam이 있을 때만 표시)
	bool bHasTypeDataBeam = LODLevel->TypeDataModule && Cast<UParticleModuleTypeDataBeam>(LODLevel->TypeDataModule);
	if (bHasTypeDataBeam)
	{
		if (ImGui::BeginMenu("Beam"))
		{
			if (ImGui::MenuItem("Source"))
			{
				UParticleModuleBeamSource* Module = new UParticleModuleBeamSource();
				AddModuleAndUpdateInstances(LODLevel, Module);
			}
			if (ImGui::MenuItem("Target"))
			{
				UParticleModuleBeamTarget* Module = new UParticleModuleBeamTarget();
				AddModuleAndUpdateInstances(LODLevel, Module);
			}
			if (ImGui::MenuItem("Noise"))
			{
				UParticleModuleBeamNoise* Module = new UParticleModuleBeamNoise();
				AddModuleAndUpdateInstances(LODLevel, Module);
			}
			ImGui::EndMenu();
		}
	}
}

void SParticleEmittersPanel::RenderEmitterContextMenu(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	if (!State || !State->CurrentSystem)
	{
		return;
	}

	UParticleSystem* System = State->CurrentSystem;

	// Rename Emitter
	if (ImGui::MenuItem("Rename Emitter"))
	{
		State->bRenamingEmitter = true;
		State->RenamingEmitterIndex = EmitterIndex;
		strncpy_s(State->RenameBuffer, Emitter->EmitterName.c_str(), sizeof(State->RenameBuffer) - 1);
	}

	// Duplicate Emitter
	if (ImGui::MenuItem("Duplicate Emitter"))
	{
		UParticleEmitter* NewEmitter = new UParticleEmitter();
		NewEmitter->EmitterName = Emitter->EmitterName + " (Copy)";
		NewEmitter->EmitterEditorColor = Emitter->EmitterEditorColor;

		for (int32 lod = 0; lod < Emitter->GetNumLODs(); ++lod)
		{
			UParticleLODLevel* SourceLOD = Emitter->GetLODLevel(lod);
			if (SourceLOD)
			{
				UParticleLODLevel* NewLOD = new UParticleLODLevel();
				if (SourceLOD->RequiredModule)
				{
					UParticleModuleRequired* NewRequired = new UParticleModuleRequired();
					NewLOD->RequiredModule = NewRequired;
				}
				if (SourceLOD->SpawnModule)
				{
					UParticleModuleSpawn* NewSpawn = new UParticleModuleSpawn();
					NewLOD->SpawnModule = NewSpawn;
				}
				NewEmitter->LODLevels.Add(NewLOD);
			}
		}
		System->Emitters.Add(NewEmitter);
	}

	// Duplicate and Share Emitter
	if (ImGui::MenuItem("Duplicate and Share Emitter"))
	{
		// TODO: 모듈 공유 복제
	}

	// Delete Emitter
	if (ImGui::MenuItem("Delete Emitter"))
	{
		DeleteEmitter(System, EmitterIndex);
	}

	ImGui::Separator();

	// Export Emitter
	if (ImGui::MenuItem("Export Emitter"))
	{
		// TODO: 이미터 익스포트
	}

	// Export All
	if (ImGui::MenuItem("Export All"))
	{
		// TODO: 전체 익스포트
	}
}

void SParticleEmittersPanel::DeleteEmitter(UParticleSystem* System, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	if (!State || !System)
	{
		return;
	}

	if (EmitterIndex >= 0 && EmitterIndex < static_cast<int32>(System->Emitters.size()))
	{
		delete System->Emitters[EmitterIndex];
		System->Emitters.erase(System->Emitters.begin() + EmitterIndex);

		// 선택 상태 초기화
		State->SelectedEmitterIndex = -1;
		State->SelectedModuleIndex = -1;
		State->SelectedEmitter = nullptr;
		State->SelectedModule = nullptr;

		// EmitterInstance 업데이트
		RefreshEmitterInstances();
	}
}

void SParticleEmittersPanel::DeleteSelectedModule()
{
	ParticleViewerState* State = Owner->GetActiveState();
	if (!State || !State->CurrentSystem)
	{
		return;
	}

	// 에미터가 선택된 경우 (모듈이 아닌)
	if (State->SelectedEmitterIndex >= 0 && State->SelectedModuleIndex < 0)
	{
		DeleteEmitter(State->CurrentSystem, State->SelectedEmitterIndex);
		return;
	}

	// 모듈이 선택된 경우
	if (State->SelectedEmitterIndex < 0 || !State->SelectedEmitter)
	{
		return;
	}

	// LOD 0에서만 삭제 가능 (UE 규칙)
	if (State->CurrentLODIndex != 0)
	{
		// TODO: 경고 메시지 표시
		return;
	}

	// Required/Spawn 모듈은 삭제 불가 (UE 규칙)
	// ModuleIndex: -1 = Required, -2 = Spawn, 0+ = 일반 모듈
	if (State->SelectedModuleIndex == -1 || State->SelectedModuleIndex == -2)
	{
		// TODO: "Required and Spawn modules may not be deleted" 메시지
		return;
	}

	UParticleLODLevel* LODLevel = State->SelectedEmitter->GetLODLevel(0);
	if (!LODLevel)
	{
		return;
	}

	// 모든 LOD 레벨에서 모듈 삭제 (UE 규칙)
	int32 ModuleIndex = State->SelectedModuleIndex;
	for (int32 lod = 0; lod < State->SelectedEmitter->GetNumLODs(); ++lod)
	{
		UParticleLODLevel* Level = State->SelectedEmitter->GetLODLevel(lod);
		if (Level && ModuleIndex >= 0 && ModuleIndex < static_cast<int32>(Level->Modules.size()))
		{
			delete Level->Modules[ModuleIndex];
			Level->Modules.erase(Level->Modules.begin() + ModuleIndex);
		}
	}

	// 선택 상태 초기화
	State->SelectedModuleIndex = -1;
	State->SelectedModule = nullptr;

	// EmitterInstance 업데이트
	RefreshEmitterInstances();
}

void SParticleEmittersPanel::AddModuleAndUpdateInstances(UParticleLODLevel* LODLevel, UParticleModule* Module)
{
	if (!LODLevel || !Module)
	{
		return;
	}

	LODLevel->Modules.Add(Module);
	RefreshEmitterInstances();
}

void SParticleEmittersPanel::RefreshEmitterInstances()
{
	ParticleViewerState* State = Owner->GetActiveState();
	if (!State)
	{
		return;
	}

	// 시스템 리빌드
	if (State->CurrentSystem)
	{
		State->CurrentSystem->BuildEmitters();
	}

	// Preview Actor의 EmitterInstance 업데이트
	if (State->PreviewActor)
	{
		UParticleSystemComponent* PSC = State->PreviewActor->GetParticleSystemComponent();
		if (PSC)
		{
			PSC->UpdateInstances(true);
		}
	}
}

// ============================================================================
// SParticleCurveEditorPanel
// ============================================================================
//
// [UE 레퍼런스 기반 커브 에디터 구현 TODO]
//
// ============================================================================
// 1. 데이터 구조 정의 (FCurveEditorData)
// ============================================================================
// TODO: FCurveKey 구조체 정의
//   - float Time (X축: 시간)
//   - float Value (Y축: 값)
//   - EInterpMode InterpMode (Linear, Constant, Cubic 등)
//   - float ArriveTangent, LeaveTangent (탄젠트 값)
//   - ETangentMode TangentMode (Auto, User, Break 등)
//
// TODO: FCurveData 구조체 정의
//   - TArray<FCurveKey> Keys (키프레임 배열)
//   - FLinearColor Color (커브 색상)
//   - FString PropertyName (속성 이름)
//   - bool bVisible, bSelected
//
// TODO: FCurveEditorSelection 구조체 정의
//   - 선택된 커브 인덱스
//   - 선택된 키 인덱스들
//   - 드래그 중인 핸들 타입 (Key, ArriveTangent, LeaveTangent)
//
// ============================================================================
// 2. 상단 툴바 버튼 구현
// ============================================================================
// TODO: 뷰 조작 버튼 (아이콘 버튼으로)
//   - Horizontal: X축(시간)에 맞춰 뷰 범위 자동 조절
//   - Vertical: Y축(값)에 맞춰 뷰 범위 자동 조절
//   - Fit: 전체 커브가 보이도록 뷰 조절
//   - Pan: 패닝 모드 토글 (선택 시 주황색 배경)
//   - Zoom: 줌 모드 토글
//
// TODO: 탄젠트/보간 모드 버튼 (구분선 후)
//   - Auto: 자동 탄젠트 계산
//   - Auto/Clamped: 클램핑된 자동 탄젠트
//   - User: 사용자 정의 탄젠트
//   - Break: 탄젠트 분리 (도착/출발 독립)
//   - Linear: 선형 보간
//   - Constant: 상수 보간 (계단식)
//
// TODO: 키프레임 조작 버튼
//   - Flatten: 선택된 키의 탄젠트를 수평으로
//   - Straighten: 선택된 키의 탄젠트를 직선으로
//   - Show All: 모든 커브 표시 토글
//   - Create: 현재 시간에 키 생성
//   - Delete: 선택된 키 삭제
//
// TODO: Current Tab 드롭다운
//   - Default, Custom 등 탭 선택
//
// ============================================================================
// 3. 좌측 커브 리스트 UI
// ============================================================================
// TODO: 커브 항목 렌더링
//   - 좌측 세로 색상 바 (속성 전체 색상)
//   - 그 옆에 채널별 색상 박스 (R/G/B 또는 X/Y/Z)
//   - 속성 이름 텍스트 (예: "StartVelocityRadial")
//   - 선택 상태 표시 (배경 하이라이트)
//
// TODO: 커브 리스트 우클릭 컨텍스트 메뉴
//   - "Remove" - 단일 커브 제거
//   - "Remove All" - 전체 커브 제거
//
// TODO: 이미터 패널에서 "Add Curve" 버튼 연동
//   - 모듈의 커브 속성을 커브 에디터에 추가
//   - 중복 추가 방지
//
// ============================================================================
// 4. 커브 그리드 뷰 상태
// ============================================================================
// TODO: 뷰 상태 변수 추가
//   - float ViewMinX, ViewMaxX (보이는 시간 범위)
//   - float ViewMinY, ViewMaxY (보이는 값 범위)
//   - ImVec2 PanOffset (패닝 오프셋)
//   - float ZoomLevel (줌 레벨)
//
// TODO: 좌표 변환 함수
//   - CurveToScreen(float time, float value) -> ImVec2
//   - ScreenToCurve(ImVec2 screenPos) -> (float time, float value)
//
// ============================================================================
// 5. 커브 그리드 마우스 컨트롤
// ============================================================================
// TODO: 마우스 패닝 (중클릭 드래그 또는 Pan 모드)
//   - 드래그 시작 위치 저장
//   - 드래그 델타만큼 ViewMin/Max 이동
//
// TODO: 마우스 휠 줌
//   - 마우스 위치를 중심으로 줌
//   - Ctrl+휠: X축만 줌
//   - Shift+휠: Y축만 줌
//   - 휠: 양축 동시 줌
//
// TODO: 박스 선택 (좌클릭 드래그, 빈 공간에서)
//   - 드래그 박스 렌더링
//   - 박스 내 키 선택
//
// ============================================================================
// 6. 그리드 및 축 레이블 렌더링
// ============================================================================
// TODO: 동적 그리드 라인
//   - 줌 레벨에 따라 그리드 간격 조절
//   - 주요 라인 (굵게), 보조 라인 (얇게)
//
// TODO: X축 레이블 (하단)
//   - 시간 값 표시 (0.00, 0.05, 0.10, ...)
//   - 줌에 따라 표시 간격 조절
//
// TODO: Y축 레이블 (좌측)
//   - 값 표시 (-0.50, 0.00, 0.50, 1.00, ...)
//   - 줌에 따라 표시 간격 조절
//
// TODO: 0 라인 강조
//   - Y=0 라인 (마젠타/분홍색)
//   - X=0 라인 (선택적)
//
// ============================================================================
// 7. 커브 라인 렌더링
// ============================================================================
// TODO: 베지어/선형 커브 렌더링
//   - 키 사이 보간 방식에 따라 렌더링
//   - Linear: 직선
//   - Constant: 계단식
//   - Cubic: 베지어 곡선 (탄젠트 사용)
//
// TODO: 커브 색상
//   - 각 커브별 고유 색상
//   - 선택 시 밝은 색상
//
// ============================================================================
// 8. 키프레임 노드 렌더링 및 선택
// ============================================================================
// TODO: 키프레임 노드 렌더링
//   - 사각형 또는 다이아몬드 모양
//   - 선택 상태 표시 (테두리, 색상 변경)
//   - 호버 시 하이라이트
//
// TODO: 키프레임 선택
//   - 클릭으로 단일 선택
//   - Ctrl+클릭으로 다중 선택
//   - Shift+클릭으로 범위 선택
//
// TODO: 키프레임 드래그 이동
//   - 선택된 키 드래그
//   - Shift: X축만 이동 (시간)
//   - Ctrl: Y축만 이동 (값)
//   - 스냅 옵션 (선택적)
//
// ============================================================================
// 9. 탄젠트 핸들 렌더링 및 조작
// ============================================================================
// TODO: 탄젠트 핸들 렌더링
//   - 선택된 키에서만 표시
//   - ArriveTangent (입력), LeaveTangent (출력)
//   - 핸들 라인 + 끝점 원
//
// TODO: 탄젠트 핸들 드래그
//   - 핸들 드래그로 탄젠트 조절
//   - Break 모드: 각 핸들 독립
//   - Auto/User 모드: 양쪽 동시
//
// ============================================================================

SParticleCurveEditorPanel::SParticleCurveEditorPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
	LoadIcons();
}

void SParticleCurveEditorPanel::AddCurve(const FString& PropertyName, const FLinearColor& Color, UParticleModule* OwnerModule)
{
	// 이미 존재하는 커브인지 확인
	for (const FCurveData& Curve : Curves)
	{
		if (Curve.PropertyName == PropertyName)
		{
			return;  // 중복 추가 방지
		}
	}

	// 새 커브 추가
	FCurveData NewCurve(PropertyName, Color);
	NewCurve.OwnerModule = OwnerModule;  // 모듈 인스턴스 저장

	// 기본 키프레임 추가 (0.0, 0.5), (1.0, 0.5) - 수평선
	NewCurve.Keys.Add(FCurveKey(0.0f, 0.5f));
	NewCurve.Keys.Add(FCurveKey(1.0f, 0.5f));

	Curves.Add(NewCurve);
}

void SParticleCurveEditorPanel::RemoveCurve(const FString& PropertyName)
{
	for (int32 i = 0; i < Curves.size(); ++i)
	{
		if (Curves[i].PropertyName == PropertyName)
		{
			Curves.erase(Curves.begin() + i);
			if (Selection.CurveIndex == i)
			{
				Selection.ClearSelection();
			}
			break;
		}
	}
}

void SParticleCurveEditorPanel::RemoveAllCurves()
{
	Curves.clear();
	Selection.ClearSelection();
}

void SParticleCurveEditorPanel::RemoveAllCurvesForModule(const FString& ModuleName)
{
	// ModuleName으로 시작하는 모든 커브 제거
	for (int32 i = static_cast<int32>(Curves.size()) - 1; i >= 0; --i)
	{
		if (Curves[i].PropertyName.find(ModuleName) == 0)
		{
			// 선택된 커브면 선택 해제
			if (Selection.CurveIndex == i)
			{
				Selection.ClearSelection();
			}
			else if (Selection.CurveIndex > i)
			{
				// 인덱스 조정
				--Selection.CurveIndex;
			}

			Curves.erase(Curves.begin() + i);
		}
	}
}

bool SParticleCurveEditorPanel::HasCurve(const FString& PropertyName) const
{
	for (const FCurveData& Curve : Curves)
	{
		if (Curve.PropertyName == PropertyName)
		{
			return true;
		}
	}
	return false;
}

// ============================================================================
// Helper Functions
// ============================================================================

void SParticleCurveEditorPanel::LoadIcons()
{
	FString IconPath = "C:/Users/Jungle/Documents/My_UE_Icons_DDS/";

	// 뷰 컨트롤 아이콘
	IconHorizontal = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Horizontal_40x.dds", false);
	IconVertical = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Vertical_40x.dds", false);
	IconFit = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_ZoomToFit_40x.dds", false);
	IconPan = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Pan_40x.dds", false);
	IconZoom = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Zoom_40x.dds", false);

	// 탄젠트/보간 모드 아이콘
	IconAuto = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Auto_40x.dds", false);
	IconUser = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_User_40x.dds", false);
	IconBreak = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Break_40x.dds", false);
	IconLinear = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Linear_40x.dds", false);
	IconConstant = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Constant_40x.dds", false);

	// 키 작업 아이콘
	IconFlatten = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Flatten_40x.dds", false);
	IconStraighten = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Straighten_40x.dds", false);
	IconShowAll = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_ShowAll_40x.dds", false);
	IconCreate = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_Create_40x.dds", false);
	IconDelete = UResourceManager::GetInstance().Load<UTexture>(IconPath + "icon_CurveEditor_DeleteTab_40x.dds", false);
}

ImVec2 SParticleCurveEditorPanel::CurveToScreen(float Time, float Value, const ImVec2& CanvasPos, const ImVec2& CanvasSize) const
{
	float NormalizedX = (Time - ViewMinX) / (ViewMaxX - ViewMinX);
	float NormalizedY = (ViewMaxY - Value) / (ViewMaxY - ViewMinY);  // Y축 반전

	return ImVec2(
		CanvasPos.x + NormalizedX * CanvasSize.x,
		CanvasPos.y + NormalizedY * CanvasSize.y
	);
}

void SParticleCurveEditorPanel::ScreenToCurve(const ImVec2& ScreenPos, const ImVec2& CanvasPos, const ImVec2& CanvasSize, float& OutTime, float& OutValue) const
{
	float NormalizedX = (ScreenPos.x - CanvasPos.x) / CanvasSize.x;
	float NormalizedY = (ScreenPos.y - CanvasPos.y) / CanvasSize.y;

	OutTime = ViewMinX + NormalizedX * (ViewMaxX - ViewMinX);
	OutValue = ViewMaxY - NormalizedY * (ViewMaxY - ViewMinY);  // Y축 반전
}

void SParticleCurveEditorPanel::FitView()
{
	// 모든 커브의 키를 포함하도록 뷰 범위 조절
	if (Curves.empty())
	{
		return;
	}

	float MinTime = FLT_MAX;
	float MaxTime = -FLT_MAX;
	float MinValue = FLT_MAX;
	float MaxValue = -FLT_MAX;

	for (const FCurveData& Curve : Curves)
	{
		if (!Curve.bVisible)
		{
			continue;
		}

		for (const FCurveKey& Key : Curve.Keys)
		{
			MinTime = std::min(MinTime, Key.Time);
			MaxTime = std::max(MaxTime, Key.Time);
			MinValue = std::min(MinValue, Key.Value);
			MaxValue = std::max(MaxValue, Key.Value);
		}
	}

	if (MinTime < FLT_MAX && MaxTime > -FLT_MAX)
	{
		float TimeMargin = (MaxTime - MinTime) * 0.1f;
		ViewMinX = MinTime - TimeMargin;
		ViewMaxX = MaxTime + TimeMargin;
	}

	if (MinValue < FLT_MAX && MaxValue > -FLT_MAX)
	{
		float ValueMargin = (MaxValue - MinValue) * 0.1f;
		ViewMinY = MinValue - ValueMargin;
		ViewMaxY = MaxValue + ValueMargin;
	}
}

void SParticleCurveEditorPanel::FitViewHorizontal()
{
	// X축(시간)만 Fit
	if (Curves.empty())
	{
		return;
	}

	float MinTime = FLT_MAX;
	float MaxTime = -FLT_MAX;

	for (const FCurveData& Curve : Curves)
	{
		if (!Curve.bVisible)
		{
			continue;
		}

		for (const FCurveKey& Key : Curve.Keys)
		{
			MinTime = std::min(MinTime, Key.Time);
			MaxTime = std::max(MaxTime, Key.Time);
		}
	}

	if (MinTime < FLT_MAX && MaxTime > -FLT_MAX)
	{
		float TimeMargin = (MaxTime - MinTime) * 0.1f;
		ViewMinX = MinTime - TimeMargin;
		ViewMaxX = MaxTime + TimeMargin;
	}
}

void SParticleCurveEditorPanel::FitViewVertical()
{
	// Y축(값)만 Fit
	if (Curves.empty())
	{
		return;
	}

	float MinValue = FLT_MAX;
	float MaxValue = -FLT_MAX;

	for (const FCurveData& Curve : Curves)
	{
		if (!Curve.bVisible)
		{
			continue;
		}

		for (const FCurveKey& Key : Curve.Keys)
		{
			MinValue = std::min(MinValue, Key.Value);
			MaxValue = std::max(MaxValue, Key.Value);
		}
	}

	if (MinValue < FLT_MAX && MaxValue > -FLT_MAX)
	{
		float ValueMargin = (MaxValue - MinValue) * 0.1f;
		ViewMinY = MinValue - ValueMargin;
		ViewMaxY = MaxValue + ValueMargin;
	}
}

void SParticleCurveEditorPanel::RenderToolbar()
{
	// 아이콘 버튼 헬퍼 (토글 상태 표시)
	auto RenderIconButton = [](const char* id, UTexture* icon, const char* tooltip, bool bActive) -> bool
	{
		if (!icon || !icon->GetShaderResourceView())
		{
			return ImGui::Button(id);
		}

		ImVec4 tintColor = bActive ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		ImVec4 bgColor = bActive ? ImVec4(0.3f, 0.5f, 0.8f, 0.5f) : ImVec4(0, 0, 0, 0);

		ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.8f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.7f, 0.5f));

		bool bResult = ImGui::ImageButton(id, icon->GetShaderResourceView(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tintColor);

		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", tooltip);
		}

		return bResult;
	};

	// 뷰 컨트롤 버튼
	if (RenderIconButton("##Horizontal", IconHorizontal, "Fit Horizontal", false))
	{
		FitViewHorizontal();
	}
	ImGui::SameLine();

	if (RenderIconButton("##Vertical", IconVertical, "Fit Vertical", false))
	{
		FitViewVertical();
	}
	ImGui::SameLine();

	if (RenderIconButton("##Fit", IconFit, "Fit All", false))
	{
		FitView();
	}
	ImGui::SameLine();

	if (RenderIconButton("##Pan", IconPan, "Pan Mode", bPanMode))
	{
		bPanMode = !bPanMode;
		if (bPanMode)
		{
			bZoomMode = false;
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Zoom", IconZoom, "Zoom Mode", bZoomMode))
	{
		bZoomMode = !bZoomMode;
		if (bZoomMode)
		{
			bPanMode = false;
		}
	}

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 탄젠트/보간 모드 버튼 - 선택된 키의 상태에 따라 활성화 표시
	bool bHasAutoTangent = false;
	bool bHasUserTangent = false;
	bool bHasBreakTangent = false;
	bool bHasLinearInterp = false;
	bool bHasConstantInterp = false;

	if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size() && !Selection.SelectedKeys.empty())
	{
		const FCurveData& Curve = Curves[Selection.CurveIndex];
		for (int32 KeyIdx : Selection.SelectedKeys)
		{
			if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
			{
				const FCurveKey& Key = Curve.Keys[KeyIdx];
				if (Key.TangentMode == ETangentMode::Auto) bHasAutoTangent = true;
				if (Key.TangentMode == ETangentMode::User) bHasUserTangent = true;
				if (Key.TangentMode == ETangentMode::Break) bHasBreakTangent = true;
				if (Key.InterpMode == EInterpMode::Linear) bHasLinearInterp = true;
				if (Key.InterpMode == EInterpMode::Constant) bHasConstantInterp = true;
			}
		}
	}

	if (RenderIconButton("##Auto", IconAuto, "Auto Tangent", bHasAutoTangent))
	{
		// 선택된 키의 탄젠트 모드를 Auto로 변경
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].TangentMode = ETangentMode::Auto;
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##User", IconUser, "User Tangent", bHasUserTangent))
	{
		// 선택된 키의 탄젠트 모드를 User로 변경
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].TangentMode = ETangentMode::User;
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Break", IconBreak, "Break Tangent", bHasBreakTangent))
	{
		// 선택된 키의 탄젠트 모드를 Break로 변경
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].TangentMode = ETangentMode::Break;
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Linear", IconLinear, "Linear", bHasLinearInterp))
	{
		// 선택된 키의 보간 모드를 Linear로 변경
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].InterpMode = EInterpMode::Linear;
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Constant", IconConstant, "Constant", bHasConstantInterp))
	{
		// 선택된 키의 보간 모드를 Constant로 변경
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].InterpMode = EInterpMode::Constant;
				}
			}
		}
	}

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 키 작업 버튼
	if (RenderIconButton("##Flatten", IconFlatten, "Flatten Tangents", false))
	{
		// 선택된 키의 탄젠트를 평탄화 (수평)
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys[KeyIdx].ArriveTangent = 0.0f;
					Curve.Keys[KeyIdx].LeaveTangent = 0.0f;
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Straighten", IconStraighten, "Straighten Tangents", false))
	{
		// 선택된 키의 탄젠트를 직선화 (이웃 키로의 기울기)
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					// 이전 키와 다음 키로의 기울기 계산
					if (KeyIdx > 0 && KeyIdx < Curve.Keys.size() - 1)
					{
						float Slope = (Curve.Keys[KeyIdx + 1].Value - Curve.Keys[KeyIdx - 1].Value) /
						              (Curve.Keys[KeyIdx + 1].Time - Curve.Keys[KeyIdx - 1].Time);
						Curve.Keys[KeyIdx].ArriveTangent = Slope;
						Curve.Keys[KeyIdx].LeaveTangent = Slope;
					}
				}
			}
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##ShowAll", IconShowAll, "Show All Curves", bShowAll))
	{
		bShowAll = !bShowAll;
		for (FCurveData& Curve : Curves)
		{
			Curve.bVisible = bShowAll;
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Create", IconCreate, "Create Key at Center", false))
	{
		// 선택된 커브에 중앙 시간(0.5)에 새 키 생성
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];
			FCurveKey NewKey(0.5f, 0.5f);  // 중앙 위치에 생성
			Curve.Keys.Add(NewKey);

			// 시간 순으로 정렬
			std::sort(Curve.Keys.begin(), Curve.Keys.end(), [](const FCurveKey& A, const FCurveKey& B) {
				return A.Time < B.Time;
			});
		}
	}
	ImGui::SameLine();

	if (RenderIconButton("##Delete", IconDelete, "Delete Selected Keys", false))
	{
		// 선택된 키 삭제
		if (Selection.CurveIndex >= 0 && Selection.CurveIndex < Curves.size())
		{
			FCurveData& Curve = Curves[Selection.CurveIndex];

			// 역순으로 삭제 (인덱스 유지)
			std::sort(Selection.SelectedKeys.begin(), Selection.SelectedKeys.end(), std::greater<int32>());
			for (int32 KeyIdx : Selection.SelectedKeys)
			{
				if (KeyIdx >= 0 && KeyIdx < Curve.Keys.size())
				{
					Curve.Keys.erase(Curve.Keys.begin() + KeyIdx);
				}
			}

			Selection.SelectedKeys.clear();
		}
	}

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// Current Tab 드롭다운 (우측 정렬)
	float AvailWidth = ImGui::GetContentRegionAvail().x;
	float ComboWidth = 150.0f;
	if (AvailWidth > ComboWidth)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + AvailWidth - ComboWidth);
	}

	ImGui::SetNextItemWidth(ComboWidth);
	if (ImGui::BeginCombo("##CurrentTab", "Current Tab"))
	{
		// TODO: 탭 목록 표시
		ImGui::Selectable("Tab 1", false);
		ImGui::Selectable("Tab 2", false);
		ImGui::EndCombo();
	}
}

void SParticleCurveEditorPanel::RenderCurveList()
{
	// 좌측 커브 리스트 (280px 너비)
	ImGui::BeginChild("CurveList", ImVec2(280, 0), true);

	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Curves");
	ImGui::Separator();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 커브를 그룹으로 묶기 위한 맵 (모듈명 -> 커브 인덱스들)
	std::map<FString, std::vector<int32>> GroupedCurves;
	for (int32 i = 0; i < Curves.size(); ++i)
	{
		FString PropertyName = Curves[i].PropertyName;
		size_t LastDotPos = PropertyName.find_last_of('.');

		FString GroupName;
		if (LastDotPos != FString::npos)
		{
			// "ModuleName.Property.Component" -> "ModuleName.Property"
			GroupName = PropertyName.substr(0, LastDotPos);
		}
		else
		{
			// 그룹 없음 (단일 커브)
			GroupName = PropertyName;
		}

		GroupedCurves[GroupName].push_back(i);
	}

	// 그룹별로 렌더링
	float CurrentY = ImGui::GetCursorPosY();
	for (auto& Pair : GroupedCurves)
	{
		const FString& GroupName = Pair.first;
		const std::vector<int32>& CurveIndices = Pair.second;

		ImGui::PushID(GroupName.c_str());

		ImVec2 GroupStartPos = ImGui::GetCursorScreenPos();
		float GroupHeight = 36.0f; // 헤더 18px + 서브커브 라인 18px

		// 그룹 배경 (선택 시)
		bool bIsGroupSelected = false;
		for (int32 idx : CurveIndices)
		{
			if (Selection.CurveIndex == idx)
			{
				bIsGroupSelected = true;
				break;
			}
		}

		if (bIsGroupSelected)
		{
			DrawList->AddRectFilled(
				GroupStartPos,
				ImVec2(GroupStartPos.x + 270, GroupStartPos.y + GroupHeight),
				IM_COL32(60, 80, 100, 100)
			);
		}

		// 그룹 헤더 (첫 번째 커브 이름에서 마지막 점까지만 표시)
		FString DisplayName = GroupName;
		size_t ModuleDotPos = DisplayName.find('.');
		if (ModuleDotPos != FString::npos)
		{
			// "UParticleModuleVelocity.StartVelocity" -> "StartVelocity"
			DisplayName = DisplayName.substr(ModuleDotPos + 1);
		}

		ImVec2 HeaderTextPos = ImVec2(GroupStartPos.x + 10, GroupStartPos.y + 2);
		DrawList->AddText(HeaderTextPos, IM_COL32(255, 255, 255, 255), DisplayName.c_str());

		// 서브커브들 (컬러박스, 가로 배치) - 헤더 아래 라인
		float SubCurveX = GroupStartPos.x + 12;
		float SubCurveY = GroupStartPos.y + 18; // 헤더 아래
		// 그룹 전체 클릭 감지 영역
		ImVec2 MousePos = ImGui::GetMousePos();
		bool bMouseInGroup = (MousePos.x >= GroupStartPos.x && MousePos.x <= GroupStartPos.x + 270 &&
		                       MousePos.y >= GroupStartPos.y && MousePos.y <= GroupStartPos.y + GroupHeight);

		int32 ClickedCurveIdx = -1;
		for (int32 idx : CurveIndices)
		{
			FCurveData& Curve = Curves[idx];
			ImU32 CurveColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Curve.Color.R, Curve.Color.G, Curve.Color.B, Curve.Color.A));

			// 컬러박스 (14x14)
			ImVec2 BoxMin = ImVec2(SubCurveX, SubCurveY + 3);
			ImVec2 BoxMax = ImVec2(BoxMin.x + 14, BoxMin.y + 14);
			DrawList->AddRectFilled(BoxMin, BoxMax, CurveColor);
			DrawList->AddRect(BoxMin, BoxMax, IM_COL32(200, 200, 200, 255), 0.0f, 0, 1.5f);

			// 클릭 감지: 컬러박스 영역에서 해당 커브 선택
			if (MousePos.x >= BoxMin.x && MousePos.x <= BoxMax.x &&
			    MousePos.y >= BoxMin.y && MousePos.y <= BoxMax.y)
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					ClickedCurveIdx = idx;
				}
			}

			SubCurveX += 18; // 다음 박스 위치 (가로로 이동)
		}

		// 체크박스 (컬러박스와 동일 라인 우측, 14x14로 컬러박스와 동일 크기)
		bool bAllVisible = true;
		for (int32 idx : CurveIndices)
		{
			if (!Curves[idx].bVisible)
			{
				bAllVisible = false;
				break;
			}
		}

		ImVec2 CheckboxPos = ImVec2(GroupStartPos.x + 248, GroupStartPos.y + 18 + 2); // 헤더 아래 라인, 2px 왼쪽으로
		ImGui::SetCursorScreenPos(CheckboxPos);
		ImGui::PushID("checkbox");
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		if (ImGui::Checkbox("##vis", &bAllVisible))
		{
			for (int32 idx : CurveIndices)
			{
				Curves[idx].bVisible = bAllVisible;
			}
		}
		ImGui::PopStyleVar(2);
		ImGui::PopID();

		// 전체 그룹 클릭 (컬러박스/체크박스 외부): 첫 번째 커브 선택
		if (bMouseInGroup && ClickedCurveIdx == -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			ClickedCurveIdx = CurveIndices[0];
		}

		// 커브 선택 처리
		if (ClickedCurveIdx >= 0)
		{
			Selection.CurveIndex = ClickedCurveIdx;
			Selection.SelectedKeys.clear();
			Curves[ClickedCurveIdx].bSelected = true;

			// 다른 커브 선택 해제
			for (int32 j = 0; j < Curves.size(); ++j)
			{
				if (j != ClickedCurveIdx)
				{
					Curves[j].bSelected = false;
				}
			}

			// OwnerModule을 사용하여 디테일 패널 업데이트
			ParticleViewerState* State = Owner->GetActiveState();
			if (State && Curves[ClickedCurveIdx].OwnerModule)
			{
				State->SelectedModule = Curves[ClickedCurveIdx].OwnerModule;

				// 해당 모듈이 속한 이미터 찾기
				for (int32 EmitterIdx = 0; EmitterIdx < State->CurrentSystem->GetNumEmitters(); ++EmitterIdx)
				{
					UParticleEmitter* Emitter = State->CurrentSystem->GetEmitter(EmitterIdx);
					if (!Emitter) continue;

					UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
					if (!LOD) continue;

					bool bFound = false;

					// Required
					if (LOD->RequiredModule == State->SelectedModule)
					{
						State->SelectedEmitter = Emitter;
						State->SelectedEmitterIndex = EmitterIdx;
						State->SelectedModuleIndex = -1;
						bFound = true;
					}
					// Spawn
					else if (LOD->SpawnModule == State->SelectedModule)
					{
						State->SelectedEmitter = Emitter;
						State->SelectedEmitterIndex = EmitterIdx;
						State->SelectedModuleIndex = -2;
						bFound = true;
					}
					// TypeData
					else if (LOD->TypeDataModule == State->SelectedModule)
					{
						State->SelectedEmitter = Emitter;
						State->SelectedEmitterIndex = EmitterIdx;
						State->SelectedModuleIndex = -3;
						bFound = true;
					}
					// 일반 모듈
					else
					{
						for (int32 ModuleIdx = 0; ModuleIdx < LOD->Modules.size(); ++ModuleIdx)
						{
							if (LOD->Modules[ModuleIdx] == State->SelectedModule)
							{
								State->SelectedEmitter = Emitter;
								State->SelectedEmitterIndex = EmitterIdx;
								State->SelectedModuleIndex = ModuleIdx;
								bFound = true;
								break;
							}
						}
					}

					if (bFound) break;
				}
			}
		}

		// 좌측 컬러바 (그룹 전체 높이) - 모듈 인스턴스 기반 색상 사용
		ImVec2 BarMin = GroupStartPos;
		ImVec2 BarMax = ImVec2(GroupStartPos.x + 4, GroupStartPos.y + GroupHeight);

		// 첫 번째 커브의 OwnerModule로 모듈 인스턴스 색상 생성
		UParticleModule* OwnerModule = Curves[CurveIndices[0]].OwnerModule;
		FLinearColor ModuleColor = GetColorFromModuleInstance(OwnerModule);
		ImU32 BarColor = ImGui::ColorConvertFloat4ToU32(ImVec4(ModuleColor.R, ModuleColor.G, ModuleColor.B, ModuleColor.A));
		DrawList->AddRectFilled(BarMin, BarMax, BarColor);

		// 다음 그룹 준비 (Dummy로 공간 확보) - GroupHeight 이미 커서가 이동했으므로 추가 공간 불필요
		ImGui::Dummy(ImVec2(0, 2)); // 최소 간격만

		ImGui::PopID();
	}

	// 빈 영역 우클릭 - 전체 제거 메뉴
	if (ImGui::BeginPopupContextWindow("CurveListContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Remove All Curves"))
		{
			Curves.clear();
			Selection.ClearSelection();
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

void SParticleCurveEditorPanel::RenderCurveGrid()
{
	// 우측 커브 그리드 영역
	ImGui::BeginChild("CurveGrid", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
	ImVec2 CanvasSize = ImGui::GetContentRegionAvail();

	// 최소 크기 보장
	if (CanvasSize.x < 50.0f || CanvasSize.y < 50.0f)
	{
		ImGui::EndChild();
		return;
	}

	// 캔버스 배경
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), IM_COL32(30, 30, 30, 255));

	// 그리드 라인 렌더링
	{
		// 수평 그리드 (Y축 값)
		int32 NumHorizontalLines = 10;
		for (int32 i = 0; i <= NumHorizontalLines; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(NumHorizontalLines);
			float Value = ViewMaxY - t * (ViewMaxY - ViewMinY);
			ImVec2 Pos = CurveToScreen(ViewMinX, Value, CanvasPos, CanvasSize);

			// 라인
			DrawList->AddLine(
				ImVec2(CanvasPos.x, Pos.y),
				ImVec2(CanvasPos.x + CanvasSize.x, Pos.y),
				IM_COL32(60, 60, 60, 255),
				1.0f
			);

			// Y축 라벨 (좌측)
			char Label[32];
			snprintf(Label, sizeof(Label), "%.2f", Value);
			DrawList->AddText(ImVec2(CanvasPos.x + 5, Pos.y - 8), IM_COL32(180, 180, 180, 255), Label);
		}

		// 수직 그리드 (X축 시간)
		int32 NumVerticalLines = 10;
		for (int32 i = 0; i <= NumVerticalLines; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(NumVerticalLines);
			float Time = ViewMinX + t * (ViewMaxX - ViewMinX);
			ImVec2 Pos = CurveToScreen(Time, ViewMinY, CanvasPos, CanvasSize);

			// 라인
			DrawList->AddLine(
				ImVec2(Pos.x, CanvasPos.y),
				ImVec2(Pos.x, CanvasPos.y + CanvasSize.y),
				IM_COL32(60, 60, 60, 255),
				1.0f
			);

			// X축 라벨 (하단)
			char Label[32];
			snprintf(Label, sizeof(Label), "%.2f", Time);
			DrawList->AddText(ImVec2(Pos.x - 15, CanvasPos.y + CanvasSize.y - 20), IM_COL32(180, 180, 180, 255), Label);
		}

		// 중심 축 (X=0, Y=0) 강조
		{
			// Y=0 라인 (빨간색)
			if (ViewMinY <= 0.0f && ViewMaxY >= 0.0f)
			{
				ImVec2 Pos = CurveToScreen(ViewMinX, 0.0f, CanvasPos, CanvasSize);
				DrawList->AddLine(
					ImVec2(CanvasPos.x, Pos.y),
					ImVec2(CanvasPos.x + CanvasSize.x, Pos.y),
					IM_COL32(200, 50, 50, 255),
					2.0f
				);
			}

			// X=0 라인 (초록색)
			if (ViewMinX <= 0.0f && ViewMaxX >= 0.0f)
			{
				ImVec2 Pos = CurveToScreen(0.0f, ViewMinY, CanvasPos, CanvasSize);
				DrawList->AddLine(
					ImVec2(Pos.x, CanvasPos.y),
					ImVec2(Pos.x, CanvasPos.y + CanvasSize.y),
					IM_COL32(50, 200, 50, 255),
					2.0f
				);
			}
		}
	}

	// 커브 렌더링
	for (int32 CurveIdx = 0; CurveIdx < Curves.size(); ++CurveIdx)
	{
		const FCurveData& Curve = Curves[CurveIdx];
		if (!Curve.bVisible || Curve.Keys.empty())
		{
			continue;
		}

		RenderCurve(Curve, CanvasPos, CanvasSize, DrawList);
		RenderKeys(Curve, CurveIdx, CanvasPos, CanvasSize, DrawList);
	}

	// 마우스 입력 처리
	HandleMouseInput(CanvasPos, CanvasSize);

	ImGui::EndChild();
}

void SParticleCurveEditorPanel::HandleMouseInput(const ImVec2& CanvasPos, const ImVec2& CanvasSize)
{
	ImGuiIO& IO = ImGui::GetIO();
	ImVec2 MousePos = ImGui::GetMousePos();

	// 캔버스 내부인지 확인
	bool bMouseInCanvas = (MousePos.x >= CanvasPos.x && MousePos.x <= CanvasPos.x + CanvasSize.x &&
	                       MousePos.y >= CanvasPos.y && MousePos.y <= CanvasPos.y + CanvasSize.y);

	if (!bMouseInCanvas)
	{
		return;
	}

	// 키 드래그 중인지 확인 (패닝보다 우선)
	bool bIsDraggingKey = (Selection.DragType == FCurveEditorSelection::EDragType::Key);

	// 패닝 모드 (기본 ON, 단 키 드래그 중이 아닐 때만)
	if (bPanMode && !bIsDraggingKey && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (!bIsPanning)
		{
			bIsPanning = true;
			PanStartMousePos = MousePos;
			PanStartViewMinX = ViewMinX;
			PanStartViewMaxX = ViewMaxX;
			PanStartViewMinY = ViewMinY;
			PanStartViewMaxY = ViewMaxY;
		}

		ImVec2 Delta = ImVec2(MousePos.x - PanStartMousePos.x, MousePos.y - PanStartMousePos.y);

		// 스크린 델타를 커브 좌표 델타로 변환
		float DeltaTime = -Delta.x / CanvasSize.x * (PanStartViewMaxX - PanStartViewMinX);
		float DeltaValue = Delta.y / CanvasSize.y * (PanStartViewMaxY - PanStartViewMinY);

		ViewMinX = PanStartViewMinX + DeltaTime;
		ViewMaxX = PanStartViewMaxX + DeltaTime;
		ViewMinY = PanStartViewMinY + DeltaValue;
		ViewMaxY = PanStartViewMaxY + DeltaValue;
	}
	else if (!bIsDraggingKey)
	{
		bIsPanning = false;
	}

	// 줌 모드 (드래그로 Y축 스케일 조절) - 키 드래그 중이 아닐 때만
	if (bZoomMode && !bIsDraggingKey && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		if (!bIsPanning)
		{
			bIsPanning = true;
			PanStartMousePos = MousePos;
			PanStartViewMinY = ViewMinY;
			PanStartViewMaxY = ViewMaxY;
		}

		ImVec2 Delta = ImVec2(MousePos.x - PanStartMousePos.x, MousePos.y - PanStartMousePos.y);

		// Y축 드래그로 스케일 조절 (아래로 드래그 = 확대, 위로 드래그 = 축소)
		float ScaleFactor = 1.0f + (Delta.y / CanvasSize.y);
		ScaleFactor = std::max(0.1f, std::min(10.0f, ScaleFactor));

		float Center = (PanStartViewMinY + PanStartViewMaxY) * 0.5f;
		float HalfRange = (PanStartViewMaxY - PanStartViewMinY) * 0.5f * ScaleFactor;

		ViewMinY = Center - HalfRange;
		ViewMaxY = Center + HalfRange;
	}
	else if (!bPanMode && !bIsDraggingKey)
	{
		bIsPanning = false;
	}

	// 마우스 휠 줌 (항상 활성)
	if (bMouseInCanvas && IO.MouseWheel != 0.0f)
	{
		float ZoomFactor = 1.0f - IO.MouseWheel * 0.1f;
		ZoomFactor = std::max(0.5f, std::min(2.0f, ZoomFactor));

		// 마우스 위치를 중심으로 줌
		float CursorTime, CursorValue;
		ScreenToCurve(MousePos, CanvasPos, CanvasSize, CursorTime, CursorValue);

		float TimeRange = (ViewMaxX - ViewMinX) * ZoomFactor;
		float ValueRange = (ViewMaxY - ViewMinY) * ZoomFactor;

		// 마우스 커서 위치 비율 유지
		float TimeRatio = (CursorTime - ViewMinX) / (ViewMaxX - ViewMinX);
		float ValueRatio = (CursorValue - ViewMinY) / (ViewMaxY - ViewMinY);

		ViewMinX = CursorTime - TimeRange * TimeRatio;
		ViewMaxX = CursorTime + TimeRange * (1.0f - TimeRatio);
		ViewMinY = CursorValue - ValueRange * ValueRatio;
		ViewMaxY = CursorValue + ValueRange * (1.0f - ValueRatio);
	}
}

void SParticleCurveEditorPanel::RenderCurve(const FCurveData& Curve, const ImVec2& CanvasPos, const ImVec2& CanvasSize, ImDrawList* DrawList)
{
	if (Curve.Keys.size() < 2)
	{
		return;
	}

	ImU32 CurveColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Curve.Color.R, Curve.Color.G, Curve.Color.B, Curve.Color.A));

	// 키프레임 간 보간하여 커브 라인 그리기
	for (size_t i = 0; i < Curve.Keys.size() - 1; ++i)
	{
		const FCurveKey& Key0 = Curve.Keys[i];
		const FCurveKey& Key1 = Curve.Keys[i + 1];

		ImVec2 P0 = CurveToScreen(Key0.Time, Key0.Value, CanvasPos, CanvasSize);
		ImVec2 P1 = CurveToScreen(Key1.Time, Key1.Value, CanvasPos, CanvasSize);

		// 보간 모드에 따라 그리기
		if (Key0.InterpMode == EInterpMode::Constant)
		{
			// 계단식: 수평선 + 수직선
			DrawList->AddLine(P0, ImVec2(P1.x, P0.y), CurveColor, 2.0f);
			DrawList->AddLine(ImVec2(P1.x, P0.y), P1, CurveColor, 2.0f);
		}
		else if (Key0.InterpMode == EInterpMode::Linear)
		{
			// 선형: 직선
			DrawList->AddLine(P0, P1, CurveColor, 2.0f);
		}
		else if (Key0.InterpMode == EInterpMode::Cubic)
		{
			// 3차 베지어: 다수의 선분으로 근사
			int32 NumSegments = 20;
			ImVec2 PrevPos = P0;

			for (int32 j = 1; j <= NumSegments; ++j)
			{
				float t = static_cast<float>(j) / static_cast<float>(NumSegments);

				// 간단한 Hermite 보간 (탄젠트 사용)
				float TimeDelta = Key1.Time - Key0.Time;
				float ValueInterp = Key0.Value * (2 * t * t * t - 3 * t * t + 1) +
				                    Key1.Value * (-2 * t * t * t + 3 * t * t) +
				                    Key0.LeaveTangent * TimeDelta * (t * t * t - 2 * t * t + t) +
				                    Key1.ArriveTangent * TimeDelta * (t * t * t - t * t);

				float TimeInterp = Key0.Time + t * TimeDelta;

				ImVec2 CurrentPos = CurveToScreen(TimeInterp, ValueInterp, CanvasPos, CanvasSize);
				DrawList->AddLine(PrevPos, CurrentPos, CurveColor, 2.0f);
				PrevPos = CurrentPos;
			}
		}
	}
}

void SParticleCurveEditorPanel::RenderKeys(const FCurveData& Curve, int32 CurveIndex, const ImVec2& CanvasPos, const ImVec2& CanvasSize, ImDrawList* DrawList)
{
	ImU32 KeyColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Curve.Color.R, Curve.Color.G, Curve.Color.B, Curve.Color.A));
	ImU32 KeyColorSelected = IM_COL32(255, 200, 0, 255);
	ImU32 KeyColorHovered = IM_COL32(255, 255, 100, 255);
	ImU32 KeyColorBorder = IM_COL32(0, 0, 0, 255);

	ImVec2 MousePos = ImGui::GetMousePos();
	float KeyHitRadius = 12.0f; // 피킹 영역 확대 (8 -> 12)

	for (int32 i = 0; i < Curve.Keys.size(); ++i)
	{
		FCurveKey& Key = const_cast<FCurveKey&>(Curve.Keys[i]);
		ImVec2 KeyPos = CurveToScreen(Key.Time, Key.Value, CanvasPos, CanvasSize);

		// 마우스 호버 체크
		float DistSq = (MousePos.x - KeyPos.x) * (MousePos.x - KeyPos.x) +
		               (MousePos.y - KeyPos.y) * (MousePos.y - KeyPos.y);
		bool bIsHovered = (DistSq <= KeyHitRadius * KeyHitRadius);

		// 선택 여부 확인
		bool bIsSelected = (Selection.CurveIndex == CurveIndex && Selection.IsKeySelected(i));

		// 키프레임 노드 (다이아몬드 형태)
		float KeySize = bIsHovered ? 6.0f : 5.0f;
		ImVec2 KeyPoints[4] = {
			ImVec2(KeyPos.x, KeyPos.y - KeySize),
			ImVec2(KeyPos.x + KeySize, KeyPos.y),
			ImVec2(KeyPos.x, KeyPos.y + KeySize),
			ImVec2(KeyPos.x - KeySize, KeyPos.y)
		};

		ImU32 FillColor = bIsSelected ? KeyColorSelected : (bIsHovered ? KeyColorHovered : KeyColor);
		DrawList->AddConvexPolyFilled(KeyPoints, 4, FillColor);
		DrawList->AddPolyline(KeyPoints, 4, KeyColorBorder, ImDrawFlags_Closed, bIsHovered ? 2.0f : 1.0f);

		// 호버 시 좌표 툴팁 표시
		if (bIsHovered)
		{
			ImGui::BeginTooltip();
			ImGui::Text("Time: %.3f", Key.Time);
			ImGui::Text("Value: %.3f", Key.Value);
			ImGui::EndTooltip();

			// 클릭 시 선택
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				Selection.CurveIndex = CurveIndex;
				Selection.SelectedKeys.clear();
				Selection.SelectedKeys.push_back(i);
				Key.bSelected = true;

				// 다른 키 선택 해제
				for (int32 j = 0; j < Curve.Keys.size(); ++j)
				{
					if (j != i)
					{
						const_cast<FCurveKey&>(Curve.Keys[j]).bSelected = false;
					}
				}

				// 해당 커브의 모듈을 디테일 패널에 표시
				ParticleViewerState* State = Owner->GetActiveState();
				if (State && Curve.OwnerModule)
				{
					State->SelectedModule = Curve.OwnerModule;

					// 해당 모듈이 속한 이미터 찾기
					for (int32 EmitterIdx = 0; EmitterIdx < State->CurrentSystem->GetNumEmitters(); ++EmitterIdx)
					{
						UParticleEmitter* Emitter = State->CurrentSystem->GetEmitter(EmitterIdx);
						if (!Emitter) continue;

						UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
						if (!LOD) continue;

						bool bFound = false;

						// Required
						if (LOD->RequiredModule == State->SelectedModule)
						{
							State->SelectedEmitter = Emitter;
							State->SelectedEmitterIndex = EmitterIdx;
							State->SelectedModuleIndex = -1;
							bFound = true;
						}
						// Spawn
						else if (LOD->SpawnModule == State->SelectedModule)
						{
							State->SelectedEmitter = Emitter;
							State->SelectedEmitterIndex = EmitterIdx;
							State->SelectedModuleIndex = -2;
							bFound = true;
						}
						// TypeData
						else if (LOD->TypeDataModule == State->SelectedModule)
						{
							State->SelectedEmitter = Emitter;
							State->SelectedEmitterIndex = EmitterIdx;
							State->SelectedModuleIndex = -3;
							bFound = true;
						}
						// 일반 모듈
						else
						{
							for (int32 ModuleIdx = 0; ModuleIdx < LOD->Modules.size(); ++ModuleIdx)
							{
								if (LOD->Modules[ModuleIdx] == State->SelectedModule)
								{
									State->SelectedEmitter = Emitter;
									State->SelectedEmitterIndex = EmitterIdx;
									State->SelectedModuleIndex = ModuleIdx;
									bFound = true;
									break;
								}
							}
						}

						if (bFound) break;
					}
				}
			}

			// 드래그 시작 (마우스 다운 상태에서 움직일 때)
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && Selection.DragType == FCurveEditorSelection::EDragType::None)
			{
				Selection.DragType = FCurveEditorSelection::EDragType::Key;
				Selection.DragKeyIndex = i;
				Selection.DragStartPos = MousePos;
			}
		}

		// 드래그 중인 키 업데이트 (마우스가 노드에서 벗어나도 계속 드래그)
		if (Selection.DragType == FCurveEditorSelection::EDragType::Key &&
		    Selection.DragKeyIndex == i &&
		    Selection.CurveIndex == CurveIndex &&
		    ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			float NewTime, NewValue;
			ScreenToCurve(MousePos, CanvasPos, CanvasSize, NewTime, NewValue);

			// 시간은 0.0 ~ 1.0 범위로 클램프
			Key.Time = std::max(0.0f, std::min(1.0f, NewTime));

			// Constant 값이면 모든 키의 값을 동일하게 변경 (수평선 유지)
			if (Key.InterpMode == EInterpMode::Constant)
			{
				float ValueDelta = NewValue - Key.Value;
				// 모든 키에 동일한 값 적용
				for (int32 j = 0; j < Curve.Keys.size(); ++j)
				{
					const_cast<FCurveKey&>(Curve.Keys[j]).Value += ValueDelta;
				}
			}
			else
			{
				// Linear는 개별 키만 변경
				Key.Value = NewValue;
			}

			// 드래그 중에도 실시간으로 모듈 값 업데이트
			UpdateModuleFromCurve(CurveIndex);
		}
	}

	// 드래그 종료 (마우스 릴리즈 시점)
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (Selection.DragType == FCurveEditorSelection::EDragType::Key)
		{
			// 키 드래그가 끝났으면 모듈 값 업데이트
			UpdateModuleFromCurve(CurveIndex);
		}

		Selection.DragType = FCurveEditorSelection::EDragType::None;
		Selection.DragKeyIndex = -1;
	}
}

// 커브 값 변경 시 해당 모듈의 Distribution 값을 업데이트
void SParticleCurveEditorPanel::UpdateModuleFromCurve(int32 CurveIndex)
{
	if (CurveIndex < 0 || CurveIndex >= Curves.size())
		return;

	ParticleViewerState* State = Owner->GetActiveState();
	if (!State || !State->SelectedModule)
		return;

	const FCurveData& Curve = Curves[CurveIndex];
	if (Curve.Keys.empty())
		return;

	// 커브 이름 파싱: "UParticleModuleVelocity.StartVelocity.X" 또는 "UParticleModuleLifetime.Lifetime"
	FString PropertyName = Curve.PropertyName;

	// 첫 번째 점 찾기 (모듈 이름과 속성 구분)
	size_t FirstDot = PropertyName.find('.');
	if (FirstDot == FString::npos) return;

	// 두 번째 점 찾기 (속성과 컴포넌트 구분)
	size_t SecondDot = PropertyName.find('.', FirstDot + 1);

	FString PropName;
	FString Component;

	if (SecondDot == FString::npos)
	{
		// 단일 값 (예: "Lifetime", "StartRotation")
		PropName = PropertyName.substr(FirstDot + 1);
		Component = "";
	}
	else
	{
		// 벡터/컴포넌트 값 (예: "StartVelocity.X")
		PropName = PropertyName.substr(FirstDot + 1, SecondDot - FirstDot - 1);
		Component = PropertyName.substr(SecondDot + 1);
	}

	// 커브의 최소/최대 값 계산
	float MinValue = Curve.Keys[0].Value;
	float MaxValue = Curve.Keys[0].Value;
	for (const FCurveKey& Key : Curve.Keys)
	{
		MinValue = std::min(MinValue, Key.Value);
		MaxValue = std::max(MaxValue, Key.Value);
	}

	// 모듈 타입에 따라 Distribution 업데이트
	const char* ClassName = State->SelectedModule->GetClass()->Name;

	if (strstr(ClassName, "Velocity"))
	{
		UParticleModuleVelocity* VelModule = static_cast<UParticleModuleVelocity*>(State->SelectedModule);

		if (PropName == "StartVelocity")
		{
			if (Component == "X")
			{
				VelModule->StartVelocity.Min.X = MinValue;
				VelModule->StartVelocity.Max.X = MaxValue;
			}
			else if (Component == "Y")
			{
				VelModule->StartVelocity.Min.Y = MinValue;
				VelModule->StartVelocity.Max.Y = MaxValue;
			}
			else if (Component == "Z")
			{
				VelModule->StartVelocity.Min.Z = MinValue;
				VelModule->StartVelocity.Max.Z = MaxValue;
			}
			VelModule->StartVelocity.bIsUniform = (VelModule->StartVelocity.Min == VelModule->StartVelocity.Max);
		}
		else if (PropName == "StartVelocityRadial")
		{
			VelModule->StartVelocityRadial.Min = MinValue;
			VelModule->StartVelocityRadial.Max = MaxValue;
			VelModule->StartVelocityRadial.bIsUniform = (MinValue == MaxValue);
		}
	}
	else if (strstr(ClassName, "Size"))
	{
		UParticleModuleSize* SizeModule = static_cast<UParticleModuleSize*>(State->SelectedModule);

		if (PropName == "StartSize")
		{
			if (Component == "X")
			{
				SizeModule->StartSize.Min.X = MinValue;
				SizeModule->StartSize.Max.X = MaxValue;
			}
			else if (Component == "Y")
			{
				SizeModule->StartSize.Min.Y = MinValue;
				SizeModule->StartSize.Max.Y = MaxValue;
			}
			else if (Component == "Z")
			{
				SizeModule->StartSize.Min.Z = MinValue;
				SizeModule->StartSize.Max.Z = MaxValue;
			}
			SizeModule->StartSize.bIsUniform = (SizeModule->StartSize.Min == SizeModule->StartSize.Max);
		}
	}
	else if (strstr(ClassName, "Lifetime"))
	{
		UParticleModuleLifetime* LifetimeModule = static_cast<UParticleModuleLifetime*>(State->SelectedModule);

		if (PropName == "Lifetime")
		{
			LifetimeModule->Lifetime.Min = MinValue;
			LifetimeModule->Lifetime.Max = MaxValue;
			LifetimeModule->Lifetime.bIsUniform = (MinValue == MaxValue);
		}
	}
	else if (strstr(ClassName, "Color"))
	{
		UParticleModuleColor* ColorModule = static_cast<UParticleModuleColor*>(State->SelectedModule);

		if (PropName == "StartColor")
		{
			if (Component == "R")
			{
				ColorModule->StartColor.Min.R = MinValue;
				ColorModule->StartColor.Max.R = MaxValue;
			}
			else if (Component == "G")
			{
				ColorModule->StartColor.Min.G = MinValue;
				ColorModule->StartColor.Max.G = MaxValue;
			}
			else if (Component == "B")
			{
				ColorModule->StartColor.Min.B = MinValue;
				ColorModule->StartColor.Max.B = MaxValue;
			}
			ColorModule->StartColor.bIsUniform = (ColorModule->StartColor.Min == ColorModule->StartColor.Max);
		}
		else if (PropName == "StartAlpha")
		{
			ColorModule->StartAlpha.Min = MinValue;
			ColorModule->StartAlpha.Max = MaxValue;
			ColorModule->StartAlpha.bIsUniform = (MinValue == MaxValue);
		}
	}
	else if (strstr(ClassName, "RotationRate"))
	{
		UParticleModuleRotationRate* RotationRateModule = static_cast<UParticleModuleRotationRate*>(State->SelectedModule);

		if (PropName == "RotationRate" || PropName == "StartRotationRate")
		{
			RotationRateModule->StartRotationRate.Min = MinValue;
			RotationRateModule->StartRotationRate.Max = MaxValue;
			RotationRateModule->StartRotationRate.bIsUniform = (MinValue == MaxValue);
		}
	}
	else if (strstr(ClassName, "Rotation"))
	{
		UParticleModuleRotation* RotationModule = static_cast<UParticleModuleRotation*>(State->SelectedModule);

		if (PropName == "Rotation" || PropName == "StartRotation")
		{
			RotationModule->StartRotation.Min = MinValue;
			RotationModule->StartRotation.Max = MaxValue;
			RotationModule->StartRotation.bIsUniform = (MinValue == MaxValue);
		}
	}
}

FLinearColor SParticleCurveEditorPanel::GetColorFromModuleInstance(UParticleModule* Module)
{
	if (!Module)
	{
		return FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);  // 기본 회색
	}

	// 모듈 포인터 주소를 해시로 사용하여 일관된 색상 생성
	uintptr_t hash = reinterpret_cast<uintptr_t>(Module);

	// 해시를 기반으로 HSV 색상 생성 (Hue만 변경, Saturation/Value는 고정)
	// Hue를 0~360도 범위로 변환 (골든 앵글 사용으로 색상 분산)
	float hue = fmodf(static_cast<float>(hash) * 0.618033988749895f, 1.0f) * 360.0f;
	float saturation = 0.7f;  // 적당한 채도
	float value = 0.9f;       // 밝기

	// HSV to RGB 변환
	float c = value * saturation;
	float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
	float m = value - c;

	float r, g, b;
	if (hue < 60.0f)
	{
		r = c; g = x; b = 0;
	}
	else if (hue < 120.0f)
	{
		r = x; g = c; b = 0;
	}
	else if (hue < 180.0f)
	{
		r = 0; g = c; b = x;
	}
	else if (hue < 240.0f)
	{
		r = 0; g = x; b = c;
	}
	else if (hue < 300.0f)
	{
		r = x; g = 0; b = c;
	}
	else
	{
		r = c; g = 0; b = x;
	}

	return FLinearColor(r + m, g + m, b + m, 1.0f);
}

void SParticleCurveEditorPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Curve Editor##ParticleCurve", nullptr, flags))
	{
		// 상단 툴바
		RenderToolbar();

		ImGui::Separator();

		// 좌측 커브 리스트와 우측 커브 그리드 (수평 분할)
		RenderCurveList();
		ImGui::SameLine();
		RenderCurveGrid();
	}
	ImGui::End();
}

// ============================================================================
// Render Target Management (전용 렌더 타겟 관리)
// ============================================================================

/**
 * @brief 전용 렌더 타겟 생성
 */
void SParticleEditorWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
	// 기존 렌더 타겟 해제
	ReleaseRenderTarget();

	if (Width == 0 || Height == 0)
	{
		return;
	}

	if (!Device)
	{
		return;
	}

	// 렌더 타겟 텍스처 생성 (DXGI_FORMAT_B8G8R8A8_UNORM - D2D 호환)
	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	HRESULT hr = Device->CreateTexture2D(&TextureDesc, nullptr, &PreviewRenderTargetTexture);
	if (FAILED(hr))
	{
		UE_LOG("ParticleEditorWindow: 렌더 타겟 텍스처 생성 실패");
		return;
	}

	// 렌더 타겟 뷰 생성
	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateRenderTargetView(PreviewRenderTargetTexture, &RTVDesc, &PreviewRenderTargetView);
	if (FAILED(hr))
	{
		UE_LOG("ParticleEditorWindow: 렌더 타겟 뷰 생성 실패");
		ReleaseRenderTarget();
		return;
	}

	// 셰이더 리소스 뷰 생성 (ImGui::Image용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	hr = Device->CreateShaderResourceView(PreviewRenderTargetTexture, &SRVDesc, &PreviewShaderResourceView);
	if (FAILED(hr))
	{
		UE_LOG("ParticleEditorWindow: 셰이더 리소스 뷰 생성 실패");
		ReleaseRenderTarget();
		return;
	}

	// 깊이 스텐실 텍스처 생성
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.SampleDesc.Quality = 0;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthDesc.CPUAccessFlags = 0;
	DepthDesc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&DepthDesc, nullptr, &PreviewDepthStencilTexture);
	if (FAILED(hr))
	{
		UE_LOG("ParticleEditorWindow: 깊이 스텐실 텍스처 생성 실패");
		ReleaseRenderTarget();
		return;
	}

	// 깊이 스텐실 뷰 생성
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateDepthStencilView(PreviewDepthStencilTexture, &DSVDesc, &PreviewDepthStencilView);
	if (FAILED(hr))
	{
		UE_LOG("ParticleEditorWindow: 깊이 스텐실 뷰 생성 실패");
		ReleaseRenderTarget();
		return;
	}

	PreviewRenderTargetWidth = Width;
	PreviewRenderTargetHeight = Height;
}

/**
 * @brief 전용 렌더 타겟 해제
 */
void SParticleEditorWindow::ReleaseRenderTarget()
{
	if (PreviewDepthStencilView)
	{
		PreviewDepthStencilView->Release();
		PreviewDepthStencilView = nullptr;
	}

	if (PreviewDepthStencilTexture)
	{
		PreviewDepthStencilTexture->Release();
		PreviewDepthStencilTexture = nullptr;
	}

	if (PreviewShaderResourceView)
	{
		PreviewShaderResourceView->Release();
		PreviewShaderResourceView = nullptr;
	}

	if (PreviewRenderTargetView)
	{
		PreviewRenderTargetView->Release();
		PreviewRenderTargetView = nullptr;
	}

	if (PreviewRenderTargetTexture)
	{
		PreviewRenderTargetTexture->Release();
		PreviewRenderTargetTexture = nullptr;
	}

	PreviewRenderTargetWidth = 0;
	PreviewRenderTargetHeight = 0;
}

/**
 * @brief 렌더 타겟 크기 업데이트 (리사이즈 처리)
 */
void SParticleEditorWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
	// 크기가 변경되지 않았으면 스킵
	if (PreviewRenderTargetWidth == NewWidth && PreviewRenderTargetHeight == NewHeight)
	{
		return;
	}

	// 새 크기로 렌더 타겟 재생성
	CreateRenderTarget(NewWidth, NewHeight);
}

/**
 * @brief 파티클 프리뷰를 전용 렌더 타겟에 렌더링
 */
void SParticleEditorWindow::RenderToPreviewRenderTarget()
{
	if (!PreviewRenderTargetView || !PreviewDepthStencilView || !ActiveState)
	{
		return;
	}

	if (!ActiveState->Viewport || !ActiveState->Client)
	{
		return;
	}

	D3D11RHI* RHI = GEngine.GetRHIDevice();
	if (!RHI)
	{
		return;
	}

	ID3D11DeviceContext* Context = RHI->GetDeviceContext();

	// 현재 렌더 타겟 백업 (렌더링 후 복원용)
	ID3D11RenderTargetView* OldRTV = nullptr;
	ID3D11DepthStencilView* OldDSV = nullptr;
	Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

	// D3D 뷰포트 백업
	UINT NumViewports = 1;
	D3D11_VIEWPORT OldViewport;
	Context->RSGetViewports(&NumViewports, &OldViewport);

	// 렌더 타겟 클리어 (ActiveState의 BackgroundColor 사용)
	const float ClearColor[4] = { ActiveState->BackgroundColor[0], ActiveState->BackgroundColor[1], ActiveState->BackgroundColor[2], 1.0f };
	Context->ClearRenderTargetView(PreviewRenderTargetView, ClearColor);
	Context->ClearDepthStencilView(PreviewDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 프리뷰 전용 렌더 타겟 설정
	Context->OMSetRenderTargets(1, &PreviewRenderTargetView, PreviewDepthStencilView);

	// 프리뷰 전용 뷰포트 설정
	D3D11_VIEWPORT D3DViewport = {};
	D3DViewport.TopLeftX = 0.0f;
	D3DViewport.TopLeftY = 0.0f;
	D3DViewport.Width = static_cast<float>(PreviewRenderTargetWidth);
	D3DViewport.Height = static_cast<float>(PreviewRenderTargetHeight);
	D3DViewport.MinDepth = 0.0f;
	D3DViewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &D3DViewport);

	// Viewport 크기 업데이트 (렌더 타겟 크기만 전달, 스크린 위치 아님)
	ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

	// 파티클 씬을 프리뷰 렌더 타겟에 렌더링
	if (ActiveState->Client)
	{
		ActiveState->Client->Draw(ActiveState->Viewport);
	}

	// 원래 렌더 타겟 복원 (메인 백버퍼로 돌아가기)
	Context->OMSetRenderTargets(1, &OldRTV, OldDSV);
	Context->RSSetViewports(1, &OldViewport);

	// 백업한 렌더 타겟 Release
	if (OldRTV) OldRTV->Release();
	if (OldDSV) OldDSV->Release();
}
