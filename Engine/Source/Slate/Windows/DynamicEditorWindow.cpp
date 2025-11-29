#include "pch.h"
#include "DynamicEditorWindow.h"
#include "DynamicEditorPanels.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/RHI/D3D11RHI.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Editor/PlatformProcess.h"
#include "SelectionManager.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include <filesystem>

#ifdef _EDITOR
#include "Source/Runtime/Engine/GameFramework/EditorEngine.h"
extern UEditorEngine GEngine;
#else
#include "Source/Runtime/Engine/GameFramework/GameEngine.h"
extern UGameEngine GEngine;
#endif

// ============================================================================
// SDynamicEditorWindow
// ============================================================================

SDynamicEditorWindow::SDynamicEditorWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
}

SDynamicEditorWindow::~SDynamicEditorWindow()
{
	// 렌더 타겟 해제
	ReleaseRenderTarget();

	// 스플리터 정리
	if (MainSplitter)
	{
		delete MainSplitter;
		MainSplitter = nullptr;
	}
	if (ContentSplitter)
	{
		delete ContentSplitter;
		ContentSplitter = nullptr;
	}
	if (CenterRightSplitter)
	{
		delete CenterRightSplitter;
		CenterRightSplitter = nullptr;
	}
	if (RightSplitter)
	{
		delete RightSplitter;
		RightSplitter = nullptr;
	}

	// 패널 정리
	for (SEditorPanel* Panel : Panels)
	{
		delete Panel;
	}
	Panels.clear();

	// 모든 탭 정리
	for (FEditorTabState* State : Tabs)
	{
		DestroyTab(State);
	}
	Tabs.clear();
	ActiveState = nullptr;
	ActiveTabIndex = -1;
}

bool SDynamicEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
	Device = InDevice;
	World = InWorld;
	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 기본 탭 생성 (Skeletal 모드)
	FEditorTabState* State = CreateNewTab("Editor 1", EEditorMode::Skeletal);
	if (State)
	{
		Tabs.Add(State);
		ActiveState = State;
		ActiveTabIndex = 0;
	}

	// 기본 레이아웃 설정 (Skeletal 모드)
	SetupLayoutForMode(EEditorMode::Skeletal);

	// 스플리터 초기 Rect 설정
	if (ContentSplitter)
	{
		ContentSplitter->SetRect(StartX, StartY + 40, StartX + Width, StartY + Height);
	}

	bIsOpen = true;
	bRequestFocus = true;

	return ActiveState != nullptr;
}

FEditorTabState* SDynamicEditorWindow::CreateNewTab(const char* Name, EEditorMode Mode)
{
	if (!Device)
	{
		return nullptr;
	}

	FEditorTabState* State = new FEditorTabState();
	State->Name = Name ? Name : "Editor";
	State->TabId = NextTabId++;
	State->Mode = Mode;

	// Preview world 생성
	State->World = NewObject<UWorld>();
	State->World->SetWorldType(EWorldType::PreviewMinimal);
	State->World->Initialize();
	State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

	// 기즈모 초기화
	AGizmoActor* Gizmo = State->World->GetGizmoActor();
	Gizmo->SetSpace(EGizmoSpace::Local);
	Gizmo->SetMode(EGizmoMode::Translate);
	Gizmo->SetbRender(false);

	// Viewport + ViewportClient 생성
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0, 0, 1, 1, Device);

	auto* Client = new FSkeletalViewerViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);
	Client->SetPickingEnabled(false);
	Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());
	Gizmo->SetEditorCameraActor(Client->GetCamera());

	// Preview Actor 생성 (Skeletal/Animation 모드용)
	if (Mode == EEditorMode::Skeletal || Mode == EEditorMode::Animation || Mode == EEditorMode::BlendSpace2D)
	{
		ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
		if (Preview)
		{
			Preview->SetTickInEditor(true);
			Preview->RegisterAnimNotifyDelegate();
		}
		State->PreviewActor = Preview;
	}

	return State;
}

void SDynamicEditorWindow::DestroyTab(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}

	if (State->Viewport)
	{
		delete State->Viewport;
		State->Viewport = nullptr;
	}
	if (State->Client)
	{
		delete State->Client;
		State->Client = nullptr;
	}
	if (State->World)
	{
		ObjectFactory::DeleteObject(State->World);
		State->World = nullptr;
	}

	delete State;
}

void SDynamicEditorWindow::CloseTab(int32 Index)
{
	if (Index < 0 || Index >= Tabs.Num())
	{
		return;
	}

	DestroyTab(Tabs[Index]);
	Tabs.erase(Tabs.begin() + Index);

	// 활성 탭 조정
	if (Tabs.Num() == 0)
	{
		ActiveState = nullptr;
		ActiveTabIndex = -1;
		bIsOpen = false;
	}
	else
	{
		if (ActiveTabIndex >= Tabs.Num())
		{
			ActiveTabIndex = Tabs.Num() - 1;
		}
		ActiveState = Tabs[ActiveTabIndex];
	}
}

void SDynamicEditorWindow::SetupLayoutForMode(EEditorMode Mode)
{
	// 기존 패널 정리
	for (SEditorPanel* Panel : Panels)
	{
		delete Panel;
	}
	Panels.clear();

	// 기존 스플리터 정리
	if (MainSplitter)
	{
		delete MainSplitter;
		MainSplitter = nullptr;
	}
	if (RightSplitter)
	{
		delete RightSplitter;
		RightSplitter = nullptr;
	}
	if (CenterRightSplitter)
	{
		delete CenterRightSplitter;
		CenterRightSplitter = nullptr;
	}
	if (ContentSplitter)
	{
		delete ContentSplitter;
		ContentSplitter = nullptr;
	}

	// 모드별 레이아웃 구성
	switch (Mode)
	{
	case EEditorMode::Skeletal:
		{
			// Skeletal 레이아웃: Asset | Viewport | (BoneTree / Detail)
			// 패널 생성
			SAssetPanel* AssetPanel = new SAssetPanel(this);
			SViewportPreviewPanel* ViewportPanel = new SViewportPreviewPanel(this);
			SBoneTreePanel* BoneTreePanel = new SBoneTreePanel(this);
			SBoneDetailPanel* DetailPanel = new SBoneDetailPanel(this);

			Panels.Add(AssetPanel);
			Panels.Add(ViewportPanel);
			Panels.Add(BoneTreePanel);
			Panels.Add(DetailPanel);

			// 우측: BoneTree(상) | Detail(하)
			RightSplitter = new SSplitterV();
			RightSplitter->SetSplitRatio(0.55f);
			RightSplitter->SideLT = BoneTreePanel;
			RightSplitter->SideRB = DetailPanel;

			// 중앙+우측: Viewport(좌) | Right(우)
			CenterRightSplitter = new SSplitterH();
			CenterRightSplitter->SetSplitRatio(0.70f);
			CenterRightSplitter->SideLT = ViewportPanel;
			CenterRightSplitter->SideRB = RightSplitter;

			// 컨텐츠: Asset(좌) | CenterRight(우)
			ContentSplitter = new SSplitterH();
			ContentSplitter->SetSplitRatio(0.20f);
			ContentSplitter->SideLT = AssetPanel;
			ContentSplitter->SideRB = CenterRightSplitter;

			// ViewportPanel 멤버 참조 저장 (마우스 입력용)
			this->ViewportPanel = ViewportPanel;
		}
		break;

	case EEditorMode::Animation:
		{
			// Animation 레이아웃:
			// [AnimList] | [Viewport] | [BoneTree/Detail]
			//            [   Timeline   ]
			// MainSplitter(V): Content(상) | Timeline(하)
			// ContentSplitter(H): AnimList(좌) | CenterRight(우)
			// CenterRightSplitter(H): Viewport(좌) | RightSplitter(우)
			// RightSplitter(V): BoneTree(상) | Detail(하)

			SAnimationListPanel* AnimListPanel = new SAnimationListPanel(this);
			SViewportPreviewPanel* ViewportPanel = new SViewportPreviewPanel(this);
			SBoneTreePanel* BoneTreePanel = new SBoneTreePanel(this);
			SBoneDetailPanel* DetailPanel = new SBoneDetailPanel(this);
			STimelinePanel* TimelinePanel = new STimelinePanel(this);

			Panels.Add(AnimListPanel);
			Panels.Add(ViewportPanel);
			Panels.Add(BoneTreePanel);
			Panels.Add(DetailPanel);
			Panels.Add(TimelinePanel);

			// 우측: BoneTree(상) | Detail(하)
			RightSplitter = new SSplitterV();
			RightSplitter->SetSplitRatio(0.55f);
			RightSplitter->SideLT = BoneTreePanel;
			RightSplitter->SideRB = DetailPanel;

			// 중앙+우측: Viewport(좌) | Right(우)
			CenterRightSplitter = new SSplitterH();
			CenterRightSplitter->SetSplitRatio(0.70f);
			CenterRightSplitter->SideLT = ViewportPanel;
			CenterRightSplitter->SideRB = RightSplitter;

			// 컨텐츠 상단: AnimList(좌) | CenterRight(우)
			ContentSplitter = new SSplitterH();
			ContentSplitter->SetSplitRatio(0.18f);
			ContentSplitter->SideLT = AnimListPanel;
			ContentSplitter->SideRB = CenterRightSplitter;

			// 메인: Content(상) | Timeline(하)
			MainSplitter = new SSplitterV();
			MainSplitter->SetSplitRatio(0.70f);
			MainSplitter->SideLT = ContentSplitter;
			MainSplitter->SideRB = TimelinePanel;

			// ViewportPanel 멤버 참조 저장
			this->ViewportPanel = ViewportPanel;
		}
		break;

	case EEditorMode::AnimGraph:
		{
			// AnimGraph 레이아웃:
			// [StateList] | [Node Editor] | [Details]
			// ContentSplitter(H): StateList(좌) | CenterRight(우)
			// CenterRightSplitter(H): NodeEditor(좌) | Details(우)

			SAnimGraphStateListPanel* StateListPanel = new SAnimGraphStateListPanel(this);
			SAnimGraphNodePanel* NodePanel = new SAnimGraphNodePanel(this);
			SAnimGraphDetailsPanel* DetailsPanel = new SAnimGraphDetailsPanel(this);

			Panels.Add(StateListPanel);
			Panels.Add(NodePanel);
			Panels.Add(DetailsPanel);

			// 중앙+우측: NodeEditor(좌) | Details(우)
			CenterRightSplitter = new SSplitterH();
			CenterRightSplitter->SetSplitRatio(0.75f);
			CenterRightSplitter->SideLT = NodePanel;
			CenterRightSplitter->SideRB = DetailsPanel;

			// 전체: StateList(좌) | CenterRight(우)
			ContentSplitter = new SSplitterH();
			ContentSplitter->SetSplitRatio(0.18f);
			ContentSplitter->SideLT = StateListPanel;
			ContentSplitter->SideRB = CenterRightSplitter;
		}
		break;

	case EEditorMode::BlendSpace2D:
		{
			// BlendSpace 레이아웃:
			// [Preview] | [Grid]
			//           | [SampleList]
			// ContentSplitter(H): Preview(좌) | RightSplitter(우)
			// RightSplitter(V): Grid(상) | SampleList(하)

			SViewportPreviewPanel* ViewportPanel = new SViewportPreviewPanel(this);
			SBlendSpaceGridPanel* GridPanel = new SBlendSpaceGridPanel(this);
			SBlendSpaceSampleListPanel* SampleListPanel = new SBlendSpaceSampleListPanel(this);

			Panels.Add(ViewportPanel);
			Panels.Add(GridPanel);
			Panels.Add(SampleListPanel);

			// 우측: Grid(상) | SampleList(하)
			RightSplitter = new SSplitterV();
			RightSplitter->SetSplitRatio(0.60f);
			RightSplitter->SideLT = GridPanel;
			RightSplitter->SideRB = SampleListPanel;

			// 전체: Preview(좌) | Right(우)
			ContentSplitter = new SSplitterH();
			ContentSplitter->SetSplitRatio(0.45f);
			ContentSplitter->SideLT = ViewportPanel;
			ContentSplitter->SideRB = RightSplitter;

			// ViewportPanel 멤버 참조 저장
			this->ViewportPanel = ViewportPanel;
		}
		break;
	}
}

void SDynamicEditorWindow::SetEditorMode(EEditorMode NewMode)
{
	if (ActiveState && ActiveState->Mode != NewMode)
	{
		ActiveState->Mode = NewMode;
		SetupLayoutForMode(NewMode);
	}
}

EEditorMode SDynamicEditorWindow::GetEditorMode() const
{
	return ActiveState ? ActiveState->Mode : EEditorMode::Skeletal;
}

void SDynamicEditorWindow::OnRender()
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(10000, 10000));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	// 윈도우 타이틀
	FString WindowTitle = "Dynamic Editor";
	if (ActiveState->CurrentMesh)
	{
		const FString& Path = ActiveState->CurrentMesh->GetFilePath();
		if (!Path.empty())
		{
			std::filesystem::path fsPath(Path);
			WindowTitle += " - " + fsPath.filename().string();
		}
	}
	WindowTitle += "###DynamicEditor";

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	bIsFocused = false;

	if (ImGui::Begin(WindowTitle.c_str(), &bIsOpen, flags))
	{
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		Rect = FRect(windowPos.x, windowPos.y, windowPos.x + windowSize.x, windowPos.y + windowSize.y);

		// ================================================================
		// 탭바 렌더링 (멀티 탭 지원)
		// ================================================================
		int32 TabToClose = -1;

		if (ImGui::BeginTabBar("DynamicEditorTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		{
			for (int32 i = 0; i < Tabs.Num(); ++i)
			{
				FEditorTabState* State = Tabs[i];
				bool open = true;

				char tabLabel[64];
				sprintf_s(tabLabel, "%s###Tab%d", State->Name.ToString().c_str(), State->TabId);

				if (ImGui::BeginTabItem(tabLabel, &open))
				{
					ActiveTabIndex = i;
					ActiveState = State;
					ImGui::EndTabItem();
				}
				if (!open)
				{
					TabToClose = i;
				}
			}

			// 새 탭 버튼
			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
			{
				char label[32];
				sprintf_s(label, "Editor %d", NextTabId);
				FEditorTabState* NewState = CreateNewTab(label, EEditorMode::Skeletal);
				if (NewState)
				{
					Tabs.Add(NewState);
					ActiveTabIndex = (int32)Tabs.Num() - 1;
					ActiveState = NewState;
				}
			}
			ImGui::EndTabBar();
		}

		// 탭 닫기 처리
		if (TabToClose >= 0)
		{
			CloseTab(TabToClose);
		}

		if (!ActiveState)
		{
			ImGui::End();
			ImGui::PopStyleVar(2);
			return;
		}

		// ================================================================
		// 모드 선택 버튼 (Toolbar 영역)
		// ================================================================
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

		const char* ModeNames[] = { "Skeleton", "Animation", "AnimGraph", "BlendSpace" };
		EEditorMode Modes[] = { EEditorMode::Skeletal, EEditorMode::Animation, EEditorMode::AnimGraph, EEditorMode::BlendSpace2D };

		for (int32 i = 0; i < 4; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}

			bool bIsCurrentMode = (ActiveState->Mode == Modes[i]);
			if (bIsCurrentMode)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
			}

			if (ImGui::Button(ModeNames[i]))
			{
				SetEditorMode(Modes[i]);
			}

			if (bIsCurrentMode)
			{
				ImGui::PopStyleColor();
			}
		}

		ImGui::PopStyleVar(2);
		ImGui::Separator();

		// ================================================================
		// 컨텐츠 영역 (스플리터 기반)
		// ================================================================
		const float ResizeHandlePadding = 16.0f;
		ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
		ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
		FRect contentRect(
			windowPos.x + contentMin.x,
			windowPos.y + ImGui::GetCursorPosY(),
			windowPos.x + contentMax.x - ResizeHandlePadding,
			windowPos.y + contentMax.y - ResizeHandlePadding
		);

		// 스플리터 영역 업데이트 및 렌더링
		// Animation 모드에서는 MainSplitter가 최상위 스플리터
		if (MainSplitter)
		{
			MainSplitter->SetRect(contentRect.Left, contentRect.Top, contentRect.Right, contentRect.Bottom);
			MainSplitter->OnRender();
		}
		else if (ContentSplitter)
		{
			ContentSplitter->SetRect(contentRect.Left, contentRect.Top, contentRect.Right, contentRect.Bottom);
			ContentSplitter->OnRender();
		}

		// 패널들을 항상 앞으로 가져오기 (메인 윈도우 뒤에 가려지지 않도록)
		// 모드별 패널 윈도우 이름
		const char* PanelNames[] = {
			"##DynamicViewport",
			"Asset##DynamicAsset",
			"Skeleton Tree##DynamicBoneTree",
			"Details##DynamicDetail",
			"Timeline##DynamicTimeline",
			"Animations##DynamicAnimList",
			"##AnimGraphNodePanel",
			"States##AnimGraphStates",
			"Details##AnimGraphDetails",
			"##BlendSpaceGrid",
			"Samples##BlendSpaceSamples"
		};

		for (const char* PanelName : PanelNames)
		{
			ImGuiWindow* PanelWin = ImGui::FindWindowByName(PanelName);
			if (PanelWin)
			{
				ImGui::BringWindowToDisplayFront(PanelWin);
			}
		}

		// 하위 패널들의 포커스도 체크
		ImGuiContext* g = ImGui::GetCurrentContext();
		if (!bIsFocused && g)
		{
			ImGuiWindow* FocusedWin = g->NavWindow;
			for (const char* PanelName : PanelNames)
			{
				ImGuiWindow* PanelWin = ImGui::FindWindowByName(PanelName);
				if (FocusedWin == PanelWin)
				{
					bIsFocused = true;
					break;
				}
			}
		}
	}
	ImGui::End();

	ImGui::PopStyleVar(2);
}

void SDynamicEditorWindow::OnUpdate(float DeltaSeconds)
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	// World Tick
	if (ActiveState->World)
	{
		ActiveState->World->Tick(DeltaSeconds);
	}

	// ViewportClient Tick (카메라 입력 처리)
	if (ActiveState->Client && bIsFocused)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}

	// 패널 업데이트
	for (SEditorPanel* Panel : Panels)
	{
		if (Panel && Panel->IsActiveForMode(ActiveState->Mode))
		{
			Panel->OnUpdate(DeltaSeconds);
		}
	}
}

void SDynamicEditorWindow::OnMouseMove(FVector2D MousePos)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// ViewportPanel에서 실제 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();

	// 뷰포트 내 마우스 처리
	if (VPRect.Contains(MousePos))
	{
		FVector2D LocalPos(MousePos.X - VPRect.Left, MousePos.Y - VPRect.Top);
		ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);

		// 기즈모 상호작용
		AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
		if (Gizmo && Gizmo->GetbIsDragging())
		{
			ACameraActor* Camera = static_cast<FSkeletalViewerViewportClient*>(ActiveState->Client)->GetCamera();
			if (Camera)
			{
				Gizmo->ProcessGizmoInteraction(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
			}
		}
	}
}

void SDynamicEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// ViewportPanel에서 실제 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();

	if (VPRect.Contains(MousePos))
	{
		FVector2D LocalPos(MousePos.X - VPRect.Left, MousePos.Y - VPRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

		// 좌클릭: 기즈모 드래그 시작 또는 본 피킹
		if (Button == 0)
		{
			AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
			ACameraActor* Camera = static_cast<FSkeletalViewerViewportClient*>(ActiveState->Client)->GetCamera();

			if (Gizmo && Camera)
			{
				// 기즈모 드래그 시도
				if (Gizmo->GetbRender())
				{
					Gizmo->StartDrag(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
					if (Gizmo->GetbIsDragging())
					{
						ActiveState->DragStartBoneIndex = ActiveState->SelectedBoneIndex;
						return;
					}
				}

				// 본 피킹
				if (ActiveState->PreviewActor && ActiveState->CurrentMesh)
				{
					FVector CameraPos = Camera->GetActorLocation();
					FVector CameraRight = Camera->GetRight();
					FVector CameraUp = Camera->GetUp();
					FVector CameraForward = Camera->GetForward();

					FVector2D ViewportMousePos(MousePos.X - VPRect.Left, MousePos.Y - VPRect.Top);
					FVector2D ViewportSize(VPRect.GetWidth(), VPRect.GetHeight());

					FRay Ray = MakeRayFromViewport(
						Camera->GetViewMatrix(),
						Camera->GetProjectionMatrix(VPRect.GetWidth() / VPRect.GetHeight(), ActiveState->Viewport),
						CameraPos, CameraRight, CameraUp, CameraForward,
						ViewportMousePos, ViewportSize
					);

					float HitDistance;
					int32 PickedBoneIndex = ActiveState->PreviewActor->PickBone(Ray, HitDistance);

					if (PickedBoneIndex >= 0)
					{
						ActiveState->SelectedBoneIndex = PickedBoneIndex;
						ActiveState->bBoneLinesDirty = true;
						ExpandToSelectedBone(ActiveState, PickedBoneIndex);

						USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
						if (Gizmo && SkelComp)
						{
							Gizmo->SetBoneTarget(SkelComp, PickedBoneIndex);
							Gizmo->SetbRender(true);
						}
						ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
					}
					else
					{
						ActiveState->SelectedBoneIndex = -1;
						ActiveState->bBoneLinesDirty = true;

						if (Gizmo)
						{
							Gizmo->ClearBoneTarget();
							Gizmo->SetbRender(false);
						}
						ActiveState->World->GetSelectionManager()->ClearSelection();
					}
				}
			}
		}
	}
}

void SDynamicEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// ViewportPanel에서 실제 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();

	FVector2D LocalPos(MousePos.X - VPRect.Left, MousePos.Y - VPRect.Top);
	ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

	// 기즈모 드래그 종료
	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	if (Gizmo && Gizmo->GetbIsDragging())
	{
		Gizmo->EndDrag();
	}
}

void SDynamicEditorWindow::OnRenderViewport()
{
	// Phase 2에서 ViewportPanel에서 호출
}

void SDynamicEditorWindow::LoadSkeletalMesh(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	// 모드를 Skeletal로 전환
	if (ActiveState->Mode != EEditorMode::Skeletal && ActiveState->Mode != EEditorMode::Animation)
	{
		SetEditorMode(EEditorMode::Skeletal);
	}

	// 새 메시 로드
	USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
	if (Mesh && ActiveState->PreviewActor)
	{
		ActiveState->CurrentMesh = Mesh;
		ActiveState->LoadedMeshPath = Path;
		ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Path);
		ActiveState->SelectedBoneIndex = -1;
		ActiveState->bBoneLinesDirty = true;
	}
}

void SDynamicEditorWindow::LoadAnimation(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	// Animation 모드로 전환
	if (ActiveState->Mode != EEditorMode::Animation)
	{
		SetEditorMode(EEditorMode::Animation);
	}

	// 애니메이션 파일 로드
	UAnimSequence* Anim = UResourceManager::GetInstance().Load<UAnimSequence>(Path);
	if (Anim)
	{
		ActiveState->CurrentAnimation = Anim;
		ActiveState->CurrentAnimationTime = 0.0f;
		ActiveState->bIsPlaying = false;

		// Working/View range 리셋
		if (UAnimDataModel* DataModel = Anim->GetDataModel())
		{
			int32 TotalFrames = DataModel->GetNumberOfFrames();
			ActiveState->WorkingRangeStartFrame = 0;
			ActiveState->WorkingRangeEndFrame = TotalFrames;
			ActiveState->ViewRangeStartFrame = 0;
			ActiveState->ViewRangeEndFrame = TotalFrames;
		}
	}
}

void SDynamicEditorWindow::LoadAnimGraph(const FString& Path)
{
	if (!ActiveState)
	{
		return;
	}

	// AnimGraph 모드로 전환
	if (ActiveState->Mode != EEditorMode::AnimGraph)
	{
		SetEditorMode(EEditorMode::AnimGraph);
	}

	// 경로 저장
	ActiveState->StateMachineFilePath = FWideString(Path.begin(), Path.end());

	// TODO: 파일에서 UAnimStateMachine 로드 로직 추가
}

void SDynamicEditorWindow::LoadBlendSpace(const FString& Path)
{
	if (!ActiveState)
	{
		return;
	}

	// BlendSpace2D 모드로 전환
	if (ActiveState->Mode != EEditorMode::BlendSpace2D)
	{
		SetEditorMode(EEditorMode::BlendSpace2D);
	}

	// 경로 저장
	ActiveState->BlendSpaceFilePath = FWideString(Path.begin(), Path.end());

	// TODO: 파일에서 UBlendSpace2D 로드 로직 추가
}

void SDynamicEditorWindow::SetBlendSpace(UBlendSpace2D* InBlendSpace)
{
	if (!ActiveState)
	{
		return;
	}

	// BlendSpace2D 모드로 전환
	if (ActiveState->Mode != EEditorMode::BlendSpace2D)
	{
		SetEditorMode(EEditorMode::BlendSpace2D);
	}

	ActiveState->BlendSpace = InBlendSpace;
}

void SDynamicEditorWindow::SetAnimStateMachine(UAnimStateMachine* InStateMachine)
{
	if (!ActiveState)
	{
		return;
	}

	// AnimGraph 모드로 전환
	if (ActiveState->Mode != EEditorMode::AnimGraph)
	{
		SetEditorMode(EEditorMode::AnimGraph);
	}

	ActiveState->StateMachine = InStateMachine;
}

FViewport* SDynamicEditorWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SDynamicEditorWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}

AGizmoActor* SDynamicEditorWindow::GetGizmoActor() const
{
	if (ActiveState && ActiveState->World)
	{
		return ActiveState->World->GetGizmoActor();
	}
	return nullptr;
}

void SDynamicEditorWindow::UpdateBoneTransformFromSkeleton(FEditorTabState* State)
{
	if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
	{
		return;
	}

	if (!State->PreviewActor)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	// 본의 로컬 트랜스폼에서 값 추출
	const FTransform& BoneTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);
	State->EditBoneLocation = BoneTransform.Translation;
	State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
	State->EditBoneScale = BoneTransform.Scale3D;
}

void SDynamicEditorWindow::ApplyBoneTransform(FEditorTabState* State)
{
	if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0 || !State->PreviewActor)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	FTransform NewTransform(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);

	// 현재 애니메이션 원본 트랜스폼 계산 (델타 제외)
	FTransform AnimTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);

	// 기존 델타가 있으면 원본 트랜스폼 역산
	const FTransform* ExistingDelta = SkelComp->GetBoneDelta(State->SelectedBoneIndex);
	if (ExistingDelta)
	{
		// 델타 역적용하여 순수 애니메이션 값 계산
		AnimTransform.Translation = AnimTransform.Translation - ExistingDelta->Translation;
		AnimTransform.Rotation = (AnimTransform.Rotation * ExistingDelta->Rotation.Inverse()).GetNormalized();
		AnimTransform.Scale3D = FVector(
			ExistingDelta->Scale3D.X != 0.0f ? AnimTransform.Scale3D.X / ExistingDelta->Scale3D.X : AnimTransform.Scale3D.X,
			ExistingDelta->Scale3D.Y != 0.0f ? AnimTransform.Scale3D.Y / ExistingDelta->Scale3D.Y : AnimTransform.Scale3D.Y,
			ExistingDelta->Scale3D.Z != 0.0f ? AnimTransform.Scale3D.Z / ExistingDelta->Scale3D.Z : AnimTransform.Scale3D.Z
		);
	}

	// 델타 계산: 새 값 - 애니메이션 원본 값
	FTransform Delta;
	Delta.Translation = NewTransform.Translation - AnimTransform.Translation;
	Delta.Rotation = AnimTransform.Rotation.IsIdentity() ? NewTransform.Rotation : (AnimTransform.Rotation.Inverse() * NewTransform.Rotation).GetNormalized();
	Delta.Scale3D = FVector(
		AnimTransform.Scale3D.X != 0.0f ? NewTransform.Scale3D.X / AnimTransform.Scale3D.X : 1.0f,
		AnimTransform.Scale3D.Y != 0.0f ? NewTransform.Scale3D.Y / AnimTransform.Scale3D.Y : 1.0f,
		AnimTransform.Scale3D.Z != 0.0f ? NewTransform.Scale3D.Z / AnimTransform.Scale3D.Z : 1.0f
	);

	// 델타를 SkeletalMeshComponent에 저장
	SkelComp->SetBoneDelta(State->SelectedBoneIndex, Delta);

	// 즉시 적용 (다음 Tick에서 AnimInstance가 적용)
	SkelComp->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SDynamicEditorWindow::ExpandToSelectedBone(FEditorTabState* State, int32 BoneIndex)
{
	if (!State || !State->CurrentMesh || BoneIndex < 0)
	{
		return;
	}

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton)
	{
		return;
	}

	// 선택된 본의 모든 부모를 확장
	int32 CurrentIndex = BoneIndex;
	while (CurrentIndex >= 0 && CurrentIndex < Skeleton->Bones.Num())
	{
		int32 ParentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
		if (ParentIndex >= 0)
		{
			State->ExpandedBoneIndices.insert(ParentIndex);
		}
		CurrentIndex = ParentIndex;
	}
}

// ============================================================================
// Render Target Management
// ============================================================================

void SDynamicEditorWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
	if (!Device || Width == 0 || Height == 0)
	{
		return;
	}

	ReleaseRenderTarget();

	ID3D11Device* D3DDevice = static_cast<ID3D11Device*>(Device);

	// Render Target Texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &PreviewRenderTargetTexture);
	if (FAILED(hr))
	{
		return;
	}

	hr = D3DDevice->CreateRenderTargetView(PreviewRenderTargetTexture, nullptr, &PreviewRenderTargetView);
	if (FAILED(hr))
	{
		return;
	}

	hr = D3DDevice->CreateShaderResourceView(PreviewRenderTargetTexture, nullptr, &PreviewShaderResourceView);
	if (FAILED(hr))
	{
		return;
	}

	// Depth Stencil
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = Width;
	depthDesc.Height = Height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = D3DDevice->CreateTexture2D(&depthDesc, nullptr, &PreviewDepthStencilTexture);
	if (FAILED(hr))
	{
		return;
	}

	hr = D3DDevice->CreateDepthStencilView(PreviewDepthStencilTexture, nullptr, &PreviewDepthStencilView);
	if (FAILED(hr))
	{
		return;
	}

	PreviewRenderTargetWidth = Width;
	PreviewRenderTargetHeight = Height;
}

void SDynamicEditorWindow::ReleaseRenderTarget()
{
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

	PreviewRenderTargetWidth = 0;
	PreviewRenderTargetHeight = 0;
}

void SDynamicEditorWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
	if (NewWidth == PreviewRenderTargetWidth && NewHeight == PreviewRenderTargetHeight)
	{
		return;
	}

	CreateRenderTarget(NewWidth, NewHeight);
}

void SDynamicEditorWindow::RenderToPreviewRenderTarget()
{
	if (!PreviewRenderTargetView || !PreviewDepthStencilView || !ActiveState)
	{
		return;
	}

	if (!ActiveState->Viewport || !ActiveState->Client)
	{
		return;
	}

	D3D11RHI* RHI = GEngine.GetRenderer()->GetRHIDevice();
	if (!RHI)
	{
		return;
	}

	ID3D11DeviceContext* Context = RHI->GetDeviceContext();

	// 기존 렌더 타겟 저장
	ID3D11RenderTargetView* OldRTV = nullptr;
	ID3D11DepthStencilView* OldDSV = nullptr;
	Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

	UINT NumViewports = 1;
	D3D11_VIEWPORT OldViewport;
	Context->RSGetViewports(&NumViewports, &OldViewport);

	// 프리뷰 렌더 타겟으로 전환
	float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	Context->ClearRenderTargetView(PreviewRenderTargetView, ClearColor);
	Context->ClearDepthStencilView(PreviewDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	Context->OMSetRenderTargets(1, &PreviewRenderTargetView, PreviewDepthStencilView);

	D3D11_VIEWPORT D3DViewport = {};
	D3DViewport.TopLeftX = 0.0f;
	D3DViewport.TopLeftY = 0.0f;
	D3DViewport.Width = static_cast<float>(PreviewRenderTargetWidth);
	D3DViewport.Height = static_cast<float>(PreviewRenderTargetHeight);
	D3DViewport.MinDepth = 0.0f;
	D3DViewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &D3DViewport);

	// Viewport 크기 업데이트
	ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

	// 본 라인 갱신
	if (ActiveState->PreviewActor && ActiveState->bBoneLinesDirty)
	{
		ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, false, true);
		ActiveState->bBoneLinesDirty = false;
	}

	// 렌더링
	ActiveState->Client->Draw(ActiveState->Viewport);

	// 기존 렌더 타겟 복원
	Context->OMSetRenderTargets(1, &OldRTV, OldDSV);
	Context->RSSetViewports(1, &OldViewport);

	if (OldRTV) OldRTV->Release();
	if (OldDSV) OldDSV->Release();
}
