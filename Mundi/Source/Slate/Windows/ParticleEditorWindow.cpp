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
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"
#include "Source/Runtime/Engine/GameFramework/ParticleSystemActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Runtime/RHI/D3D11RHI.h"
#include "FViewport.h"
#include "FViewportClient.h"
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
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotationRate.h"
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
	LeftSplitter->SetSplitRatio(0.6f);
	LeftSplitter->SideLT = ViewportPanel;
	LeftSplitter->SideRB = DetailPanel;

	// 우측: Emitters(상) | CurveEditor(하)
	RightSplitter = new SSplitterV();
	RightSplitter->SetSplitRatio(0.5f);
	RightSplitter->SideLT = EmittersPanel;
	RightSplitter->SideRB = CurveEditorPanel;

	// 메인: Left(좌) | Right(우)
	MainSplitter = new SSplitterH();
	MainSplitter->SetSplitRatio(0.5f);
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
	}
	ImGui::End();
	ImGui::PopStyleVar(2);  // WindowRounding, WindowBorderSize

	// ParticleEditor가 포커스되어 있으면 모든 패널을 최상위로 올림 (항상)
	if (bIsFocused)
	{
		ImGui::SetWindowFocus("##ParticleViewport");
		ImGui::SetWindowFocus("Details##ParticleDetail");
		ImGui::SetWindowFocus("Emitters##ParticleEmitters");
		ImGui::SetWindowFocus("Curve Editor##ParticleCurve");
	}

	// 컬러피커 윈도우 렌더링 (패널 포커스 이후에 렌더링하여 최상위에 표시)
	if (bShowColorPicker)
	{
		ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (ImGui::Begin("Color Picker##BgColorPicker", &bShowColorPicker, popupFlags))
		{
			ImGui::Text("Background Color");
			ImGui::Separator();
			ImGui::ColorPicker3("##BgColorPickerWidget", ActiveState->BackgroundColor,
				ImGuiColorEditFlags_PickerHueWheel);
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120, 0)))
			{
				bShowColorPicker = false;
			}
		}
		ImGui::End();

		// 컬러피커를 최상위로 (패널 포커스 후에 호출)
		ImGui::SetWindowFocus("Color Picker##BgColorPicker");
	}

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
	if (RenderIconButton("Save", IconSave, "Save", "Save Particle System"))
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
	if (RenderIconButton("Load", IconLoad, "Load", "Load Particle System"))
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
	if (RenderIconButton("RestartSim", IconRestartSim, "Restart Sim", "Restart Particle Simulation"))
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
	if (RenderIconButton("RestartLevel", IconRestartLevel, "Restart Level", "Reset Level and Simulation"))
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
	if (RenderIconButton("Undo", IconUndo, "Undo", "Undo (Ctrl+Z)"))
	{
		// TODO: Undo 구현
	}

	ImGui::SameLine();
	if (RenderIconButton("Redo", IconRedo, "Redo", "Redo (Ctrl+Y)"))
	{
		// TODO: Redo 구현
	}

	DrawVerticalSeparator();

	// ================================================================
	// Thumbnail - 선택된 이미터의 썸네일 캡처
	// ================================================================
	if (RenderIconButton("Thumbnail", IconThumbnail, "Thumbnail", "Capture Thumbnail for Selected Emitter"))
	{
		// 선택된 이미터가 있으면 썸네일 캡처
		if (ActiveState->SelectedEmitter && ActiveState->CurrentSystem)
		{
			D3D11RHI* RHI = GEngine.GetRHIDevice();
			if (RHI)
			{
				ID3D11Texture2D* SceneColorTexture = RHI->GetCurrentSceneColorTexture();
				if (SceneColorTexture)
				{
					// 썸네일 저장 경로 생성
					FString ThumbnailDir = "Data/ParticleThumbnails/";
					FString SystemName = ActiveState->CurrentSystem->GetName();
					FString ThumbnailPath = ThumbnailDir + SystemName + "_" + std::to_string(ActiveState->SelectedEmitterIndex) + ".dds";

					if (RHI->CaptureRenderTargetToDDS(SceneColorTexture, ThumbnailPath, 64, 64))
					{
						// 캡처 성공 - 경로 저장 및 텍스처 로드
						ActiveState->SelectedEmitter->ThumbnailTexturePath = ThumbnailPath;
						// 이미 로드된 텍스처가 있으면 Reload, 없으면 Load
						if (UResourceManager::GetInstance().Get<UTexture>(ThumbnailPath))
						{
							UResourceManager::GetInstance().Reload<UTexture>(ThumbnailPath);
						}
						else
						{
							UResourceManager::GetInstance().Load<UTexture>(ThumbnailPath);
						}
					}
				}
			}
		}
	}

	DrawVerticalSeparator();

	// ================================================================
	// 뷰 옵션: Bounds (토글)
	// ================================================================
	if (RenderIconButton("Bounds", IconBounds, "Bounds", "Toggle Bounds Display", ActiveState->bShowBounds))
	{
		ActiveState->bShowBounds = !ActiveState->bShowBounds;
	}

	DrawVerticalSeparator();

	// ================================================================
	// Origin Axis (토글)
	// ================================================================
	if (RenderIconButton("OriginAxis", IconOriginAxis, "Origin Axis", "Toggle Origin Axis Display", ActiveState->bShowOriginAxis))
	{
		ActiveState->bShowOriginAxis = !ActiveState->bShowOriginAxis;
	}

	DrawVerticalSeparator();

	// ================================================================
	// Background Color (클릭 시 컬러피커 팝업 - OnRender에서 렌더링)
	// ================================================================
	if (RenderIconButton("BgColor", IconBackgroundColor, "BG Color", "Change Background Color", bShowColorPicker))
	{
		bShowColorPicker = !bShowColorPicker;
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 관련: Regen LOD 버튼들
	// ================================================================
	if (RenderIconButton("RegenLODDup", IconRegenLOD, "Regen LOD", "Regenerate Lowest LOD (Duplicate Highest)"))
	{
		// TODO: LOD 재생성 (최하위, 최상위 복제)
	}

	ImGui::SameLine();
	if (RenderIconButton("RegenLOD", IconRegenLOD, "Regen LOD", "Regenerate Lowest LOD"))
	{
		// TODO: LOD 재생성 (최하위)
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 네비게이션: Lowest, Lower
	// ================================================================
	if (RenderIconButton("LowestLOD", IconLowestLOD, "Lowest LOD", "Go to Lowest LOD"))
	{
		CurrentLODIndex = 0;
	}

	ImGui::SameLine();
	if (RenderIconButton("LowerLOD", IconLowerLOD, "Lower LOD", "Go to Lower LOD"))
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
	if (RenderIconButton("AddLODBefore", IconAddLOD, "Add LOD", "Add LOD Before Current"))
	{
		// TODO: LOD 추가 (현재 앞에)
	}

	ImGui::SameLine();

	// LOD 입력창
	ImGui::BeginGroup();
	{
		ImGui::Text("LOD");
		ImGui::SetNextItemWidth(40);
		ImGui::InputInt("##LODIndex", &CurrentLODIndex, 0, 0);
		if (CurrentLODIndex < 0)
		{
			CurrentLODIndex = 0;
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();
	if (RenderIconButton("AddLODAfter", IconAddLOD, "Add LOD", "Add LOD After Current"))
	{
		// TODO: LOD 추가 (현재 뒤에)
	}

	DrawVerticalSeparator();

	// ================================================================
	// LOD 네비게이션: Higher
	// ================================================================
	if (RenderIconButton("HigherLOD", IconHigherLOD, "Higher LOD", "Go to Higher LOD"))
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

		// PreviewWindow와 동일한 방식: BeginChild로 뷰포트 영역 생성
		ImGui::BeginChild("ParticleViewportRenderArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
		{
			// ImGui child 윈도우의 실제 위치/크기 가져오기
			ImVec2 childPos = ImGui::GetWindowPos();
			ImVec2 childSize = ImGui::GetWindowSize();

			// ContentRect 업데이트 (실제 3D 렌더링 영역)
			ContentRect.Left = childPos.x;
			ContentRect.Top = childPos.y;
			ContentRect.Right = childPos.x + childSize.x;
			ContentRect.Bottom = childPos.y + childSize.y;
			ContentRect.UpdateMinMax();
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

	// 썸네일 텍스처가 있으면 표시, 없으면 기본 아이콘
	UTexture* ThumbTex = nullptr;
	if (!Emitter->ThumbnailTexturePath.empty())
	{
		// 이미 로드된 텍스처가 있는지 확인, 없으면 로드 시도
		ThumbTex = UResourceManager::GetInstance().Get<UTexture>(Emitter->ThumbnailTexturePath);
		if (!ThumbTex)
		{
			// 파일이 존재하면 로드
			if (std::filesystem::exists(Emitter->ThumbnailTexturePath))
			{
				ThumbTex = UResourceManager::GetInstance().Load<UTexture>(Emitter->ThumbnailTexturePath);
			}
		}
	}

	if (ThumbTex && ThumbTex->GetShaderResourceView())
	{
		// 썸네일 텍스처 표시
		DrawList->AddImage(
			(ImTextureID)ThumbTex->GetShaderResourceView(),
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
			ImGui::SetTooltip("%s (Click to change)", RenderModeTooltip);
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
		if (!Emitter->bIsSoloing)
		{
			Emitter->bWasEnabledBeforeSolo = Emitter->bIsEnabled;
			Emitter->bIsSoloing = true;
		}
		else
		{
			Emitter->bIsSoloing = false;
			Emitter->bIsEnabled = Emitter->bWasEnabledBeforeSolo;
		}
		// LODLevel->bEnabled 업데이트
		ParticleViewerState* State = Owner->GetActiveState();
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

void SParticleEmittersPanel::RenderModuleItem(UParticleModule* Module, int32 ModuleIndex, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	bool bSelected = (State->SelectedEmitterIndex == EmitterIndex && State->SelectedModuleIndex == ModuleIndex);

	// 모듈 타입에 따른 색상
	ImVec4 moduleColor;
	const char* className = Module->GetClass() ? Module->GetClass()->Name : "Unknown";

	if (strstr(className, "Required"))
	{
		moduleColor = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); // 노란색
	}
	else if (strstr(className, "Spawn"))
	{
		moduleColor = ImVec4(0.7f, 0.3f, 0.3f, 1.0f); // 빨간색
	}
	else if (strstr(className, "Lifetime"))
	{
		moduleColor = ImVec4(0.4f, 0.6f, 0.4f, 1.0f); // 녹색
	}
	else if (strstr(className, "Size"))
	{
		moduleColor = ImVec4(0.5f, 0.5f, 0.7f, 1.0f); // 파란색
	}
	else if (strstr(className, "Velocity") || strstr(className, "Acceleration"))
	{
		moduleColor = ImVec4(0.6f, 0.4f, 0.6f, 1.0f); // 보라색
	}
	else if (strstr(className, "Color"))
	{
		moduleColor = ImVec4(0.7f, 0.5f, 0.3f, 1.0f); // 주황색
	}
	else if (strstr(className, "TypeData"))
	{
		moduleColor = ImVec4(0.3f, 0.7f, 0.7f, 1.0f); // 청록색 (TypeData)
	}
	else
	{
		moduleColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // 기본 회색
	}

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

	// 모듈 이름 (Selectable) - 우측에 아이콘 공간 확보
	if (ImGui::Selectable(ModuleName.c_str(), bSelected, 0, ImVec2(AvailWidth - IconAreaWidth, 20)))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = ModuleIndex;
		State->SelectedModule = Module;

		if (EmitterIndex >= 0 && State->CurrentSystem && EmitterIndex < State->CurrentSystem->GetNumEmitters())
		{
			State->SelectedEmitter = State->CurrentSystem->GetEmitter(EmitterIndex);
		}
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

	ImGui::SameLine(AvailWidth - IconAreaWidth + 16);  // 아이콘 시작 위치

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
				Module->SetCurvesInEditor(!bCurvesInEditor);
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
		ImGui::MenuItem("Beam", nullptr, false, false); // TODO
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
		ImGui::MenuItem("SubImage Index", nullptr, false, false); // TODO
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

SParticleCurveEditorPanel::SParticleCurveEditorPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
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
		// 툴바
		if (ImGui::Button("Fit Horizontal"))
		{
		}
		ImGui::SameLine();
		if (ImGui::Button("Fit Vertical"))
		{
		}
		ImGui::SameLine();
		if (ImGui::Button("Fit All"))
		{
		}

		ImGui::Separator();

		// 커브 목록 (좌측)
		ImGui::BeginChild("CurveList", ImVec2(150, 0), true);
		{
			ParticleViewerState* State = Owner->GetActiveState();
			if (State && State->SelectedModule)
			{
				// TODO: 선택된 모듈의 커브 프로퍼티 나열
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Module curves:");
				ImGui::Selectable("Alpha Over Life");
				ImGui::Selectable("Size Over Life");
				ImGui::Selectable("Color Over Life");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a module");
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// 커브 에디터 그리드 (우측)
		ImGui::BeginChild("CurveGrid", ImVec2(0, 0), true);
		{
			ImVec2 canvasSize = ImGui::GetContentRegionAvail();
			ImVec2 canvasPos = ImGui::GetCursorScreenPos();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			// 배경
			drawList->AddRectFilled(canvasPos,
				ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
				IM_COL32(40, 40, 40, 255));

			// 그리드 라인
			const int gridLines = 10;
			for (int i = 0; i <= gridLines; ++i)
			{
				float x = canvasPos.x + (canvasSize.x * i / gridLines);
				float y = canvasPos.y + (canvasSize.y * i / gridLines);

				// 수직선
				drawList->AddLine(
					ImVec2(x, canvasPos.y),
					ImVec2(x, canvasPos.y + canvasSize.y),
					IM_COL32(60, 60, 60, 255));

				// 수평선
				drawList->AddLine(
					ImVec2(canvasPos.x, y),
					ImVec2(canvasPos.x + canvasSize.x, y),
					IM_COL32(60, 60, 60, 255));
			}

			// 중앙선 강조
			float centerY = canvasPos.y + canvasSize.y * 0.5f;
			drawList->AddLine(
				ImVec2(canvasPos.x, centerY),
				ImVec2(canvasPos.x + canvasSize.x, centerY),
				IM_COL32(80, 80, 80, 255), 2.0f);

			// TODO: 실제 커브 렌더링
		}
		ImGui::EndChild();
	}
	ImGui::End();
}
