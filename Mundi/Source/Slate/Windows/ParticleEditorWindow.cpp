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
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "ImGui/imgui.h"

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

	bool bIsFocused = false;

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

		// 뷰포트 영역 캐시 (ViewportPanel의 Rect)
		if (ViewportPanel)
		{
			ViewportRect = ViewportPanel->Rect;
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
}

void SParticleEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (MainSplitter)
	{
		MainSplitter->OnMouseDown(MousePos, Button);
	}
}

void SParticleEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (MainSplitter)
	{
		MainSplitter->OnMouseUp(MousePos, Button);
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
}

bool SParticleEditorWindow::RenderIconButton(const char* id, UTexture* icon, const char* label, const char* tooltip, bool bActive)
{
	const float IconSize = 24.0f;
	const float ButtonWidth = 70.0f;
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
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

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
		// TODO: 파티클 시스템 저장
	}

	ImGui::SameLine();
	if (RenderIconButton("Load", IconLoad, "Load", "Load Particle System"))
	{
		// TODO: 파일 다이얼로그 열기
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
	// Thumbnail
	// ================================================================
	if (RenderIconButton("Thumbnail", IconThumbnail, "Thumbnail", "Capture Thumbnail"))
	{
		// TODO: 썸네일 캡처
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

// ============================================================================
// SParticleViewportPanel
// ============================================================================

SParticleViewportPanel::SParticleViewportPanel(SParticleEditorWindow* InOwner)
	: Owner(InOwner)
{
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

		// 뷰포트 간단 툴바 (View/Time 탭)
		if (ImGui::Button("View"))
		{
			// TODO: 뷰 옵션
		}
		ImGui::SameLine();
		if (ImGui::Button("Time"))
		{
			// TODO: 타임라인 옵션
		}

		// 뷰포트 이미지 영역 (배경색으로 표시)
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();

		// 배경색 적용
		ImU32 bgColor = IM_COL32(30, 30, 30, 255);
		if (State)
		{
			bgColor = IM_COL32(
				static_cast<int>(State->BackgroundColor[0] * 255),
				static_cast<int>(State->BackgroundColor[1] * 255),
				static_cast<int>(State->BackgroundColor[2] * 255),
				255
			);
		}
		drawList->AddRectFilled(p, ImVec2(p.x + viewportSize.x, p.y + viewportSize.y), bgColor);
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
			if (State->SelectedEmitter)
			{
				ImGui::Text("Emitter: %s", State->SelectedEmitter->EmitterName.c_str());
				ImGui::Separator();

				// 이미터 기본 정보
				ImGui::Text("LOD Levels: %d", State->SelectedEmitter->GetNumLODs());
				ImGui::Text("Peak Particles: %d", State->SelectedEmitter->GetPeakActiveParticles());

				// 에디터 색상
				float color[4] = {
					State->SelectedEmitter->EmitterEditorColor.R,
					State->SelectedEmitter->EmitterEditorColor.G,
					State->SelectedEmitter->EmitterEditorColor.B,
					State->SelectedEmitter->EmitterEditorColor.A
				};
				if (ImGui::ColorEdit4("Editor Color", color))
				{
					State->SelectedEmitter->EmitterEditorColor.R = color[0];
					State->SelectedEmitter->EmitterEditorColor.G = color[1];
					State->SelectedEmitter->EmitterEditorColor.B = color[2];
					State->SelectedEmitter->EmitterEditorColor.A = color[3];
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a module to edit");
			}
		}
		else
		{
			RenderModuleProperties(State->SelectedModule);
		}
	}
	ImGui::End();
}

void SParticleDetailPanel::RenderModuleProperties(UParticleModule* Module)
{
	if (!Module)
	{
		return;
	}

	const char* className = Module->GetClass() ? Module->GetClass()->Name : "Unknown";
	ImGui::Text("Module: %s", className);
	ImGui::Separator();

	// 모듈 기본 정보 표시
	ImGui::Text("Spawn Module: %s", Module->IsSpawnModule() ? "Yes" : "No");
	ImGui::Text("Update Module: %s", Module->IsUpdateModule() ? "Yes" : "No");
	ImGui::Text("3D Draw Mode: %s", Module->Is3DDrawMode() ? "Yes" : "No");

	// TODO: 리플렉션 시스템을 통한 프로퍼티 렌더링
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
		if (!State || !State->CurrentSystem)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No particle system loaded");
			ImGui::End();
			return;
		}

		UParticleSystem* System = State->CurrentSystem;

		// 이미터들을 수평으로 배치 (Cascade 스타일)
		const float EmitterColumnWidth = 180.0f;
		const float ModuleHeight = 24.0f;

		for (int32 i = 0; i < System->GetNumEmitters(); ++i)
		{
			UParticleEmitter* Emitter = System->GetEmitter(i);
			if (!Emitter)
			{
				continue;
			}

			ImGui::BeginGroup();

			// 이미터 헤더
			RenderEmitterHeader(Emitter, i);

			// 모듈 스택
			ImGui::BeginChild(("EmitterModules" + std::to_string(i)).c_str(),
				ImVec2(EmitterColumnWidth, 0), true);
			RenderModuleStack(Emitter, i);
			ImGui::EndChild();

			ImGui::EndGroup();

			ImGui::SameLine();
		}

		// + 버튼으로 새 이미터 추가
		if (ImGui::Button("+", ImVec2(30, 30)))
		{
			// TODO: 새 이미터 추가
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Add Emitter");
		}
	}
	ImGui::End();
}

void SParticleEmittersPanel::RenderEmitterHeader(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	ParticleViewerState* State = Owner->GetActiveState();
	bool bSelected = (State->SelectedEmitterIndex == EmitterIndex && State->SelectedModuleIndex == -1);

	// 이미터 색상으로 헤더 배경
	ImVec4 headerColor(
		Emitter->EmitterEditorColor.R,
		Emitter->EmitterEditorColor.G,
		Emitter->EmitterEditorColor.B,
		1.0f
	);

	ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(headerColor.x * 1.2f, headerColor.y * 1.2f, headerColor.z * 1.2f, 1.0f));

	if (ImGui::Selectable(Emitter->EmitterName.c_str(), bSelected, 0, ImVec2(180, 24)))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = -1;
		State->SelectedEmitter = Emitter;
		State->SelectedModule = nullptr;
	}

	ImGui::PopStyleColor(2);

	// 솔로 모드 체크박스
	ImGui::SameLine(150);
	bool bSoloing = Emitter->bIsSoloing;
	if (ImGui::Checkbox(("##Solo" + std::to_string(EmitterIndex)).c_str(), &bSoloing))
	{
		Emitter->bIsSoloing = bSoloing;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Solo");
	}
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
	if (ImGui::Selectable(ModuleName.c_str(), bSelected, 0, ImVec2(0, 20)))
	{
		State->SelectedEmitterIndex = EmitterIndex;
		State->SelectedModuleIndex = ModuleIndex;
		State->SelectedModule = Module;

		if (EmitterIndex >= 0 && State->CurrentSystem && EmitterIndex < State->CurrentSystem->GetNumEmitters())
		{
			State->SelectedEmitter = State->CurrentSystem->GetEmitter(EmitterIndex);
		}
	}
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
