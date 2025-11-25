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

SParticleEditorWindow::SParticleEditorWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
}

SParticleEditorWindow::~SParticleEditorWindow()
{
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

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

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

	if (ImGui::Begin(WindowTitle.c_str(), &bIsOpen, flags))
	{
		// 메뉴바
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Time", nullptr, &ActiveState->bIsSimulating);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// 툴바
		RenderToolbar();

		ImGui::Separator();

		// 메인 컨텐츠 영역
		ImVec2 contentSize = ImGui::GetContentRegionAvail();

		// 커브 에디터 높이 계산
		float curveEditorHeight = contentSize.y * CurveEditorHeightRatio;
		float upperHeight = contentSize.y - curveEditorHeight - 5.0f;

		// 상단 영역 (Viewport + Emitters + Details)
		ImGui::BeginChild("UpperArea", ImVec2(0, upperHeight), false);
		{
			ImVec2 upperSize = ImGui::GetContentRegionAvail();

			// 뷰포트 너비 계산
			float viewportWidth = upperSize.x * ViewportWidthRatio;
			float emittersPanelWidth = upperSize.x * EmittersPanelWidthRatio;
			float detailsPanelWidth = upperSize.x * DetailsPanelWidthRatio;

			// 뷰포트 패널
			ImGui::BeginChild("ViewportPanel", ImVec2(viewportWidth, 0), true);
			RenderViewportPanel();
			ImGui::EndChild();

			ImGui::SameLine();

			// 이미터 패널
			ImGui::BeginChild("EmittersPanel", ImVec2(emittersPanelWidth, 0), true);
			RenderEmittersPanel();
			ImGui::EndChild();

			ImGui::SameLine();

			// 디테일 패널
			ImGui::BeginChild("DetailsPanel", ImVec2(detailsPanelWidth, 0), true);
			RenderDetailsPanel();
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::Separator();

		// 커브 에디터
		ImGui::BeginChild("CurveEditor", ImVec2(0, curveEditorHeight), true);
		RenderCurveEditor();
		ImGui::EndChild();

		// 윈도우 Rect 업데이트
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x;
		Rect.Top = pos.y;
		Rect.Right = pos.x + size.x;
		Rect.Bottom = pos.y + size.y;
	}
	ImGui::End();
}

void SParticleEditorWindow::OnRenderViewport()
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 뷰포트 렌더링
	ActiveState->Viewport->Render();
}

void SParticleEditorWindow::RenderToolbar()
{
	// Restart Sim
	if (ImGui::Button("Restart Sim"))
	{
		RestartSimulation();
	}
	ImGui::SameLine();

	// Restart Level (동일 기능)
	if (ImGui::Button("Restart Level"))
	{
		RestartSimulation();
	}
	ImGui::SameLine();

	ImGui::Separator();
	ImGui::SameLine();

	// Undo/Redo (TODO)
	ImGui::BeginDisabled(true);
	ImGui::Button("Undo");
	ImGui::SameLine();
	ImGui::Button("Redo");
	ImGui::EndDisabled();
	ImGui::SameLine();

	ImGui::Separator();
	ImGui::SameLine();

	// Thumbnail (TODO)
	if (ImGui::Button("Thumbnail"))
	{
		// TODO: 썸네일 캡처
	}
	ImGui::SameLine();

	// Bounds
	if (ImGui::Checkbox("Bounds", &ActiveState->bShowBounds))
	{
		// TODO: 바운드 표시 토글
	}
	ImGui::SameLine();

	// Origin Axis
	if (ImGui::Checkbox("Origin Axis", &ActiveState->bShowOriginAxis))
	{
		// TODO: 원점 축 표시 토글
	}
	ImGui::SameLine();

	// Background Color
	ImGui::ColorEdit3("##BgColor", BackgroundColor, ImGuiColorEditFlags_NoInputs);
}

void SParticleEditorWindow::RenderViewportPanel()
{
	ImGui::Text("Viewport");
	ImGui::Separator();

	// View/Time 탭
	if (ImGui::BeginTabBar("ViewportTabs"))
	{
		if (ImGui::BeginTabItem("View"))
		{
			// 뷰포트 영역
			ImVec2 availSize = ImGui::GetContentRegionAvail();
			if (availSize.x > 10 && availSize.y > 10 && ActiveState && ActiveState->Viewport)
			{
				// 뷰포트 크기 업데이트
				uint32 newWidth = static_cast<uint32>(availSize.x);
				uint32 newHeight = static_cast<uint32>(availSize.y);

				ImVec2 cursorPos = ImGui::GetCursorScreenPos();
				ActiveState->Viewport->Resize(
					static_cast<uint32>(cursorPos.x),
					static_cast<uint32>(cursorPos.y),
					newWidth, newHeight);

				// 뷰포트 영역 캐시
				ViewportRect.Left = cursorPos.x;
				ViewportRect.Top = cursorPos.y;
				ViewportRect.Right = cursorPos.x + availSize.x;
				ViewportRect.Bottom = cursorPos.y + availSize.y;

				// 프레임 카운터 표시
				ImGui::Text("Frame: %d / %d",
					ActiveState->PreviewActor ? 0 : 0, 0);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Time"))
		{
			// 시간 설정
			ImGui::SliderFloat("Speed", &ActiveState->SimulationSpeed, 0.0f, 2.0f);

			if (ImGui::Checkbox("Simulating", &ActiveState->bIsSimulating))
			{
			}

			ImGui::Text("Time: %.2f", ActiveState->AccumulatedTime);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void SParticleEditorWindow::RenderEmittersPanel()
{
	ImGui::Text("Emitters");
	ImGui::Separator();

	if (!ActiveState || !ActiveState->CurrentSystem)
	{
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No particle system loaded");
		return;
	}

	UParticleSystem* System = ActiveState->CurrentSystem;

	// 이미터 목록을 가로로 나열
	ImGui::BeginChild("EmitterList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (int32 i = 0; i < System->GetNumEmitters(); ++i)
	{
		UParticleEmitter* Emitter = System->GetEmitter(i);
		if (!Emitter)
		{
			continue;
		}

		ImGui::PushID(i);

		// 이미터 컬럼 (고정 너비)
		ImGui::BeginChild("EmitterColumn", ImVec2(180, 0), true);

		// 이미터 헤더
		RenderEmitterHeader(Emitter, i);

		// 모듈 스택
		RenderModuleStack(Emitter, i);

		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::PopID();
	}

	ImGui::EndChild();
}

void SParticleEditorWindow::RenderEmitterHeader(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	// 이미터 이름 (편집 가능)
	bool bSelected = (ActiveState->SelectedEmitterIndex == EmitterIndex &&
		ActiveState->SelectedModuleIndex == -1);

	ImVec4 headerColor = bSelected ?
		ImVec4(0.8f, 0.5f, 0.2f, 1.0f) :
		ImVec4(0.6f, 0.4f, 0.2f, 1.0f);

	ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(headerColor.x + 0.1f, headerColor.y + 0.1f, headerColor.z + 0.1f, 1.0f));

	FString HeaderLabel = Emitter->EmitterName.empty() ? "Emitter" : Emitter->EmitterName;

	if (ImGui::Selectable(HeaderLabel.c_str(), bSelected))
	{
		ActiveState->SelectedEmitterIndex = EmitterIndex;
		ActiveState->SelectedModuleIndex = -1;
		ActiveState->SelectedEmitter = Emitter;
		ActiveState->SelectedModule = nullptr;
	}

	ImGui::PopStyleColor(2);

	// 솔로 모드 체크박스
	ImGui::SameLine(ImGui::GetWindowWidth() - 30);
	bool bSoloing = Emitter->bIsSoloing;
	if (ImGui::Checkbox("##Solo", &bSoloing))
	{
		Emitter->bIsSoloing = bSoloing;
	}
}

void SParticleEditorWindow::RenderModuleStack(UParticleEmitter* Emitter, int32 EmitterIndex)
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

void SParticleEditorWindow::RenderModuleItem(UParticleModule* Module, int32 ModuleIndex, int32 EmitterIndex)
{
	bool bSelected = (ActiveState->SelectedEmitterIndex == EmitterIndex &&
		ActiveState->SelectedModuleIndex == ModuleIndex);

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
		moduleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // 회색
	}
	else if (strstr(className, "Size"))
	{
		moduleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // 회색
	}
	else if (strstr(className, "Velocity") || strstr(className, "Acceleration"))
	{
		moduleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // 회색
	}
	else if (strstr(className, "Color"))
	{
		moduleColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // 회색
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

	// 모듈 이름 추출 (UParticleModule 접두사 제거)
	FString ModuleName = className;
	if (ModuleName.find("UParticleModule") == 0)
	{
		ModuleName = ModuleName.substr(15);
	}

	ImGui::PushID(ModuleIndex);
	if (ImGui::Selectable(ModuleName.c_str(), bSelected))
	{
		ActiveState->SelectedEmitterIndex = EmitterIndex;
		ActiveState->SelectedModuleIndex = ModuleIndex;
		ActiveState->SelectedModule = Module;

		if (EmitterIndex >= 0 && EmitterIndex < ActiveState->CurrentSystem->GetNumEmitters())
		{
			ActiveState->SelectedEmitter = ActiveState->CurrentSystem->GetEmitter(EmitterIndex);
		}
	}
	ImGui::PopID();

	ImGui::PopStyleColor();

	// 3D 드로우 모드 표시 (읽기 전용)
	ImGui::SameLine(ImGui::GetWindowWidth() - 50);
	if (Module->IsSupported3DDrawMode())
	{
		ImGui::TextColored(
			Module->Is3DDrawMode() ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
			"3D");
	}
}

void SParticleEditorWindow::RenderDetailsPanel()
{
	ImGui::Text("Details");
	ImGui::Separator();

	if (!ActiveState->SelectedModule)
	{
		if (ActiveState->SelectedEmitter)
		{
			ImGui::Text("Emitter: %s", ActiveState->SelectedEmitter->EmitterName.c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a module to edit");
		}
		return;
	}

	RenderModuleProperties(ActiveState->SelectedModule);
}

void SParticleEditorWindow::RenderModuleProperties(UParticleModule* Module)
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

	// TODO: 모듈 타입별 프로퍼티 렌더링 (리플렉션 시스템 연동)
}

void SParticleEditorWindow::RenderCurveEditor()
{
	ImGui::Text("Curve Editor");
	ImGui::Separator();

	// 툴바
	if (ImGui::Button("Horizontal"))
	{
	}
	ImGui::SameLine();
	if (ImGui::Button("Vertical"))
	{
	}
	ImGui::SameLine();
	if (ImGui::Button("Fit"))
	{
	}
	ImGui::SameLine();
	if (ImGui::Button("Pan"))
	{
	}
	ImGui::SameLine();
	if (ImGui::Button("Zoom"))
	{
	}

	ImGui::Separator();

	// 커브 목록 (왼쪽)과 커브 에디터 (오른쪽)
	ImVec2 availSize = ImGui::GetContentRegionAvail();
	float curveListWidth = 150.0f;

	// 커브 목록
	ImGui::BeginChild("CurveList", ImVec2(curveListWidth, availSize.y), true);
	{
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Curves");
		ImGui::Separator();

		// TODO: 선택된 모듈의 커브 목록 표시
		ImGui::Text("(No curves)");
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// 커브 에디터 그래프
	ImGui::BeginChild("CurveGraph", ImVec2(0, availSize.y), true);
	{
		ImVec2 graphSize = ImGui::GetContentRegionAvail();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();

		// 배경
		drawList->AddRectFilled(
			canvasPos,
			ImVec2(canvasPos.x + graphSize.x, canvasPos.y + graphSize.y),
			IM_COL32(30, 30, 30, 255));

		// 그리드
		float gridSpacing = 50.0f;
		for (float x = 0; x < graphSize.x; x += gridSpacing)
		{
			drawList->AddLine(
				ImVec2(canvasPos.x + x, canvasPos.y),
				ImVec2(canvasPos.x + x, canvasPos.y + graphSize.y),
				IM_COL32(50, 50, 50, 255));
		}
		for (float y = 0; y < graphSize.y; y += gridSpacing)
		{
			drawList->AddLine(
				ImVec2(canvasPos.x, canvasPos.y + y),
				ImVec2(canvasPos.x + graphSize.x, canvasPos.y + y),
				IM_COL32(50, 50, 50, 255));
		}

		// 중앙선 (0 기준선)
		float centerY = canvasPos.y + graphSize.y * 0.5f;
		drawList->AddLine(
			ImVec2(canvasPos.x, centerY),
			ImVec2(canvasPos.x + graphSize.x, centerY),
			IM_COL32(80, 80, 80, 255), 2.0f);

		// 축 레이블
		drawList->AddText(ImVec2(canvasPos.x + 5, canvasPos.y + 5), IM_COL32(150, 150, 150, 255), "1.0");
		drawList->AddText(ImVec2(canvasPos.x + 5, centerY - 10), IM_COL32(150, 150, 150, 255), "0.0");
		drawList->AddText(ImVec2(canvasPos.x + 5, canvasPos.y + graphSize.y - 20), IM_COL32(150, 150, 150, 255), "-1.0");
	}
	ImGui::EndChild();
}

void SParticleEditorWindow::OnUpdate(float DeltaSeconds)
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	// 시뮬레이션 업데이트
	if (ActiveState->bIsSimulating && ActiveState->World)
	{
		float ScaledDelta = DeltaSeconds * ActiveState->SimulationSpeed;
		ActiveState->AccumulatedTime += ScaledDelta;
		ActiveState->World->Tick(ScaledDelta);
	}

	// 뷰포트 클라이언트 틱
	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}
}

void SParticleEditorWindow::OnMouseMove(FVector2D MousePos)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 뷰포트 영역 내에서만 처리
	if (MousePos.X >= ViewportRect.Left && MousePos.X <= ViewportRect.Right &&
		MousePos.Y >= ViewportRect.Top && MousePos.Y <= ViewportRect.Bottom)
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseMove(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y));
	}
}

void SParticleEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (MousePos.X >= ViewportRect.Left && MousePos.X <= ViewportRect.Right &&
		MousePos.Y >= ViewportRect.Top && MousePos.Y <= ViewportRect.Bottom)
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y), Button);
	}
}

void SParticleEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (MousePos.X >= ViewportRect.Left && MousePos.X <= ViewportRect.Right &&
		MousePos.Y >= ViewportRect.Top && MousePos.Y <= ViewportRect.Bottom)
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp(static_cast<int32>(LocalPos.X), static_cast<int32>(LocalPos.Y), Button);
	}
}

void SParticleEditorWindow::LoadParticleSystem(const FString& Path)
{
	if (Path.empty())
	{
		return;
	}

	UParticleSystem* System = UResourceManager::GetInstance().Load<UParticleSystem>(Path);
	if (System)
	{
		SetParticleSystem(System);
	}
}

void SParticleEditorWindow::SetParticleSystem(UParticleSystem* InSystem)
{
	if (!ActiveState || !InSystem)
	{
		return;
	}

	ActiveState->CurrentSystem = InSystem;
	ActiveState->LoadedSystemPath = InSystem->GetFilePath();

	// 프리뷰 액터에 설정
	if (ActiveState->PreviewActor)
	{
		ActiveState->PreviewActor->SetParticleSystem(InSystem);
	}

	// 선택 초기화
	ActiveState->SelectedEmitterIndex = -1;
	ActiveState->SelectedModuleIndex = -1;
	ActiveState->SelectedEmitter = nullptr;
	ActiveState->SelectedModule = nullptr;

	UE_LOG("ParticleEditorWindow: Loaded particle system: %s", ActiveState->LoadedSystemPath.c_str());
}

void SParticleEditorWindow::RestartSimulation()
{
	if (!ActiveState)
	{
		return;
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

void SParticleEditorWindow::ToggleSimulation()
{
	if (!ActiveState)
	{
		return;
	}

	ActiveState->bIsSimulating = !ActiveState->bIsSimulating;
}

FViewport* SParticleEditorWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SParticleEditorWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}
