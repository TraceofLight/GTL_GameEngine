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

	return ActiveState != nullptr;
}

void SParticleEditorWindow::OnRender()
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

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

		// 콘텐츠 영역 계산
		ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
		ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
		FRect contentRect(
			windowPos.x + contentMin.x,
			windowPos.y + contentMin.y,
			windowPos.x + contentMax.x,
			windowPos.y + contentMax.y
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

	// ParticleEditor가 포커스되어 있으면 모든 패널을 최상위로 올림
	if (bIsFocused)
	{
		ImGui::SetWindowFocus("##ParticleViewport");
		ImGui::SetWindowFocus("Details##ParticleDetail");
		ImGui::SetWindowFocus("Emitters##ParticleEmitters");
		ImGui::SetWindowFocus("Curve Editor##ParticleCurve");
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
		// 툴바
		{
			ParticleViewerState* State = Owner->GetActiveState();

			if (ImGui::Button(State && State->bIsSimulating ? "Pause" : "Play"))
			{
				if (State)
				{
					State->bIsSimulating = !State->bIsSimulating;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Restart"))
			{
				if (State && State->PreviewActor)
				{
					UParticleSystemComponent* PSC = State->PreviewActor->GetParticleSystemComponent();
					if (PSC)
					{
						PSC->ActivateSystem(true);
					}
				}
			}
			ImGui::SameLine();
			if (State)
			{
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Speed", &State->SimulationSpeed, 0.1f, 2.0f, "%.1fx");
			}
		}

		// 뷰포트 이미지 영역 (배경색으로 표시)
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 p = ImGui::GetCursorScreenPos();
		drawList->AddRectFilled(p, ImVec2(p.x + viewportSize.x, p.y + viewportSize.y), IM_COL32(30, 30, 30, 255));
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
