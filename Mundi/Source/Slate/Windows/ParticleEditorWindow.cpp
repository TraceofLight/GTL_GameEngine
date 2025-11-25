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

// 파티클 모듈 헤더
#include "Source/Runtime/Engine/Particle/Color/ParticleModuleColor.h"
#include "Source/Runtime/Engine/Particle/Lifetime/ParticleModuleLifetime.h"
#include "Source/Runtime/Engine/Particle/Location/ParticleModuleLocation.h"
#include "Source/Runtime/Engine/Particle/Size/ParticleModuleSize.h"
#include "Source/Runtime/Engine/Particle/Velocity/ParticleModuleVelocity.h"
#include "Source/Runtime/Engine/Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Source/Slate/Widgets/ParticleModuleDetailRenderer.h"

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
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 2));

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

	// 드래그 앤 드롭 - 일반 모듈만 (Required/Spawn 제외)
	// ModuleIndex: -1 = Required, -2 = Spawn, 0+ = 일반 모듈
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
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODULE_REORDER"))
			{
				struct ModuleDragPayload
				{
					int32 EmitterIndex;
					int32 ModuleIndex;
				};
				ModuleDragPayload* data = (ModuleDragPayload*)payload->Data;

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
					}
				}
			}
			ImGui::EndDragDropTarget();
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

	// 선택 상태 업데이트
	ParticleViewerState* State = Owner->GetActiveState();
	if (State)
	{
		State->SelectedEmitterIndex = System->GetNumEmitters() - 1;
		State->SelectedModuleIndex = -1;
		State->SelectedEmitter = NewEmitter;
		State->SelectedModule = nullptr;
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
		ImGui::MenuItem("GPU Sprites", nullptr, false, false); // TODO
		ImGui::MenuItem("Mesh", nullptr, false, false); // TODO
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
			LODLevel->Modules.Add(Module);
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
			LODLevel->Modules.Add(Module);
		}
		ImGui::EndMenu();
	}

	// Location 서브메뉴
	if (ImGui::BeginMenu("Location"))
	{
		if (ImGui::MenuItem("Initial Location"))
		{
			UParticleModuleLocation* Module = new UParticleModuleLocation();
			LODLevel->Modules.Add(Module);
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
		ImGui::MenuItem("Initial Rotation", nullptr, false, false); // TODO
		ImGui::MenuItem("Rotation Over Life", nullptr, false, false); // TODO
		ImGui::EndMenu();
	}

	// Rotation Rate 서브메뉴
	if (ImGui::BeginMenu("Rotation Rate"))
	{
		ImGui::MenuItem("Initial Rotation Rate", nullptr, false, false); // TODO
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
			LODLevel->Modules.Add(Module);
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
			LODLevel->Modules.Add(Module);
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
