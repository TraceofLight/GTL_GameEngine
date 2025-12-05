#include "pch.h"
#include "SkeletalEditorWindow.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Slate/Widgets/ViewportToolbarWidget.h"
#include "SelectionManager.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "USlateManager.h"

// ============================================================================
// SSkeletalEditorWindow
// ============================================================================

SSkeletalEditorWindow::SSkeletalEditorWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
}

SSkeletalEditorWindow::~SSkeletalEditorWindow()
{
	// 렌더 타겟 해제
	ReleaseRenderTarget();

	// 뷰포트 툴바 해제
	if (ViewportToolbar)
	{
		delete ViewportToolbar;
		ViewportToolbar = nullptr;
	}

	// 스플리터/패널 정리
	if (MainSplitter)
	{
		delete MainSplitter;
		MainSplitter = nullptr;
	}

	// 모든 탭 정리
	for (ViewerState* State : Tabs)
	{
		SkeletalViewerBootstrap::DestroyViewerState(State);
	}
	Tabs.clear();
	ActiveState = nullptr;
	ActiveTabIndex = -1;
}

bool SSkeletalEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice, bool bEmbedded)
{
	Device = InDevice;
	World = InWorld;
	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 기본 탭 생성 (Embedded 모드에서는 DynamicEditorWindow에서 탭 관리하므로 생성하지 않음)
	if (!bEmbedded)
	{
		ViewerState* State = SkeletalViewerBootstrap::CreateViewerState("Skeletal 1", InWorld, InDevice);
		if (State)
		{
			State->TabId = NextTabId++;
			Tabs.Add(State);
			ActiveState = State;
			ActiveTabIndex = 0;
		}
	}

	// 패널 생성
	ToolbarPanel = new SSkeletalToolbarPanel(this);
	AssetPanel = new SSkeletalAssetPanel(this);
	ViewportPanel = new SSkeletalViewportPanel(this);
	BoneTreePanel = new SSkeletalBoneTreePanel(this);
	DetailPanel = new SSkeletalDetailPanel(this);

	// 스플리터 계층 구조 생성
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

	// 메인: Toolbar(상) | Content(하) - 툴바는 고정 높이이므로 비율 사용 안 함
	// 대신 OnRender에서 직접 처리
	MainSplitter = nullptr;  // 툴바는 고정 높이로 처리

	// 스플리터 초기 Rect 설정
	ContentSplitter->SetRect(StartX, StartY + 40, StartX + Width, StartY + Height);

	// 뷰포트 툴바 위젯 생성 및 초기화
	ViewportToolbar = new SViewportToolbarWidget();
	ViewportToolbar->Initialize(InDevice);

	bIsOpen = true;
	bRequestFocus = true;

	return ActiveState != nullptr;
}

void SSkeletalEditorWindow::OnRender()
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	// 리사이즈 가능하도록 size constraints 설정
	ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(10000, 10000));

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
	FString WindowTitle = "Skeletal Mesh Editor";
	if (ActiveState->CurrentMesh)
	{
		const FString& Path = ActiveState->CurrentMesh->GetFilePath();
		if (!Path.empty())
		{
			std::filesystem::path fsPath(Path);
			WindowTitle += " - " + fsPath.filename().string();
		}
	}
	WindowTitle += "###SkeletalEditor";

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	bIsFocused = false;

	if (ImGui::Begin(WindowTitle.c_str(), &bIsOpen, flags))
	{
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 윈도우 위치/크기 추적
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		Rect = FRect(windowPos.x, windowPos.y, windowPos.x + windowSize.x, windowPos.y + windowSize.y);

		// ================================================================
		// 탭바 렌더링 (멀티 탭 지원)
		// ================================================================
		int32 PreviousTabIndex = ActiveTabIndex;
		bool bTabClosed = false;
		int32 TabToClose = -1;

		if (ImGui::BeginTabBar("SkeletalEditorTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		{
			for (int32 i = 0; i < Tabs.Num(); ++i)
			{
				ViewerState* State = Tabs[i];
				bool open = true;

				// 탭 ID를 포함한 고유 라벨 생성
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
				sprintf_s(label, "Skeletal %d", NextTabId);
				ViewerState* NewState = SkeletalViewerBootstrap::CreateViewerState(label, World, Device);
				if (NewState)
				{
					NewState->TabId = NextTabId++;
					Tabs.Add(NewState);
					ActiveTabIndex = (int32)Tabs.Num() - 1;
					ActiveState = NewState;
				}
			}
			ImGui::EndTabBar();
		}

		// 탭 닫기 처리 (루프 밖에서 안전하게)
		if (TabToClose >= 0 && TabToClose < Tabs.Num())
		{
			SkeletalViewerBootstrap::DestroyViewerState(Tabs[TabToClose]);
			Tabs.erase(Tabs.begin() + TabToClose);

			if (Tabs.IsEmpty())
			{
				ActiveState = nullptr;
				ActiveTabIndex = -1;
				bIsOpen = false;
			}
			else
			{
				ActiveTabIndex = std::min(ActiveTabIndex, (int32)Tabs.Num() - 1);
				if (ActiveTabIndex < 0)
				{
					ActiveTabIndex = 0;
				}
				ActiveState = Tabs[ActiveTabIndex];
			}
			bTabClosed = true;
		}

		// 탭이 닫혔으면 즉시 종료 (댕글링 포인터 방지)
		if (bTabClosed)
		{
			ImGui::End();
			ImGui::PopStyleVar(2);
			return;
		}

		// ================================================================
		// 상단 툴바 (모드 전환 버튼) - UE5 스타일
		// ================================================================
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));

			// Skeleton 버튼 (현재 활성화됨)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
			ImGui::Button("Skeleton");
			ImGui::PopStyleColor();

			ImGui::SameLine();

			// Animation 버튼 (비활성화 - 다른 윈도우로 전환)
			if (ImGui::Button("Animation"))
			{
				// TODO: Animation Editor로 전환
			}

			ImGui::PopStyleVar(2);
		}

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

		// 스플리터 영역 업데이트
		if (ContentSplitter)
		{
			ContentSplitter->SetRect(contentRect.Left, contentRect.Top, contentRect.Right, contentRect.Bottom);
			ContentSplitter->OnRender();
		}

		// 패널들을 항상 앞으로 가져오기 (메인 윈도우 클릭 시에도 패널 유지)
		// Content Browser가 열려있으면 z-order 유지를 위해 front로 가져오지 않음
		ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##SkeletalViewport");
		ImGuiWindow* DisplayWin = ImGui::FindWindowByName("Display##SkeletalDisplay");
		ImGuiWindow* BoneTreeWin = ImGui::FindWindowByName("Skeleton##SkeletalBoneTree");
		ImGuiWindow* DetailWin = ImGui::FindWindowByName("Details##SkeletalDetail");

		if (!SLATE.IsContentBrowserVisible())
		{
			if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
			if (DisplayWin) ImGui::BringWindowToDisplayFront(DisplayWin);
			if (BoneTreeWin) ImGui::BringWindowToDisplayFront(BoneTreeWin);
			if (DetailWin) ImGui::BringWindowToDisplayFront(DetailWin);
		}

		// 하위 패널들의 포커스도 체크 (별도의 ImGui 윈도우이므로)
		// NavWindow는 현재 키보드/게임패드 네비게이션이 활성화된 윈도우
		ImGuiContext* g = ImGui::GetCurrentContext();
		if (!bIsFocused && g)
		{
			ImGuiWindow* FocusedWin = g->NavWindow;
			if (FocusedWin == ViewportWin ||
				FocusedWin == DisplayWin ||
				FocusedWin == BoneTreeWin ||
				FocusedWin == DetailWin)
			{
				bIsFocused = true;
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
}

// ============================================================================
// RenderEmbedded - DynamicEditorWindow 내장 모드용 렌더링
// ============================================================================

void SSkeletalEditorWindow::RenderEmbedded(const FRect& ContentRect)
{
	if (Tabs.Num() == 0)
	{
		return;
	}

	if (!ActiveState)
	{
		if (Tabs.Num() > 0)
		{
			ActiveState = Tabs[0];
			ActiveTabIndex = 0;
		}
		else
		{
			return;
		}
	}

	// SSplitter에 영역 설정 및 렌더링 (탭 바는 DynamicEditorWindow에서 관리)
	if (ContentSplitter)
	{
		ContentSplitter->SetRect(ContentRect.Left, ContentRect.Top, ContentRect.Right, ContentRect.Bottom);
		ContentSplitter->OnRender();

		// ViewportRect 캐시 (입력 처리용)
		if (CenterRightSplitter && CenterRightSplitter->GetLeftOrTop())
		{
			ViewportRect = CenterRightSplitter->GetLeftOrTop()->GetRect();
		}
	}

	// 패널 윈도우들을 앞으로 가져오기 (DynamicEditorWindow 뒤에 가려지는 문제 해결)
	// Content Browser가 열려있으면 z-order 유지를 위해 front로 가져오지 않음
	if (!SLATE.IsContentBrowserVisible())
	{
		ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##SkeletalViewport");
		ImGuiWindow* DisplayWin = ImGui::FindWindowByName("Display##SkeletalDisplay");
		ImGuiWindow* BoneTreeWin = ImGui::FindWindowByName("Skeleton##SkeletalBoneTree");
		ImGuiWindow* DetailWin = ImGui::FindWindowByName("Details##SkeletalDetail");

		if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
		if (DisplayWin) ImGui::BringWindowToDisplayFront(DisplayWin);
		if (BoneTreeWin) ImGui::BringWindowToDisplayFront(BoneTreeWin);
		if (DetailWin) ImGui::BringWindowToDisplayFront(DetailWin);
	}

	// 팝업들도 패널 위로 가져오기
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
}

void SSkeletalEditorWindow::OnUpdate(float DeltaSeconds)
{
	if (!bIsOpen || !ActiveState)
	{
		return;
	}

	// ViewportClient Tick (카메라 입력 처리)
	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}

	// World Tick (기즈모 등 액터 업데이트)
	if (ActiveState->World)
	{
		ActiveState->World->Tick(DeltaSeconds);
	}

	// 기즈모 모드 전환은 USlateManager::ProcessInput()에서 처리됨

	// 스플리터 업데이트
	if (ContentSplitter)
	{
		ContentSplitter->OnUpdate(DeltaSeconds);
	}

	// 뷰포트 업데이트
	if (ViewportPanel)
	{
		ViewportPanel->OnUpdate(DeltaSeconds);
	}
}

void SSkeletalEditorWindow::OnMouseMove(FVector2D MousePos)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 팝업/모달이 열려있으면 뷰포트 입력 무시
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();

	// 기즈모 드래그 중인지 확인
	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

	// 기즈모 드래그 중이거나 뷰포트 영역 안에 있을 때 마우스 이벤트 전달
	if (bGizmoDragging || VPRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);
		ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);

		// 기즈모 호버링/드래그 인터랙션 처리
		if (Gizmo && ActiveState->Client)
		{
			ACameraActor* Camera = ActiveState->Client->GetCamera();
			if (Camera)
			{
				Gizmo->ProcessGizmoInteraction(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
			}
		}
	}

	// 스플리터에도 전달 (리사이즈용)
	if (ContentSplitter)
	{
		ContentSplitter->OnMouseMove(MousePos);
	}
}

void SSkeletalEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 팝업/모달이 열려있으면 뷰포트 입력 무시
	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();
	bool bInViewport = VPRect.Contains(MousePos);

	if (bInViewport)
	{
		FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);

		// 뷰포트에 마우스 다운 전달
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

		// 좌클릭: 본 피킹 시도
		if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
		{
			// 기즈모가 클릭되었는지 확인
			AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
			bool bGizmoClicked = (Gizmo && Gizmo->GetbIsHovering());

			// 기즈모 위에서 클릭했고 본 타겟 모드이면 드래그 시작
			if (bGizmoClicked && Gizmo && Gizmo->IsBoneTarget())
			{
				ACameraActor* Camera = ActiveState->Client->GetCamera();
				if (Camera)
				{
					Gizmo->StartDrag(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
				}
				return;
			}

			// 기즈모가 클릭되지 않았으면 본 피킹 시도
			if (!bGizmoClicked)
			{
				ACameraActor* Camera = ActiveState->Client->GetCamera();
				if (Camera)
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

						// 기즈모를 본에 연결
						AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
						USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
						if (GizmoActor && SkelComp)
						{
							GizmoActor->SetBoneTarget(SkelComp, PickedBoneIndex);
							GizmoActor->SetbRender(true);
						}
						ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
					}
					else
					{
						// 본이 피킹되지 않음 - 선택 해제
						ActiveState->SelectedBoneIndex = -1;
						ActiveState->bBoneLinesDirty = true;

						AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
						if (GizmoActor)
						{
							GizmoActor->ClearBoneTarget();
							GizmoActor->SetbRender(false);
						}
						ActiveState->World->GetSelectionManager()->ClearSelection();
					}
				}
			}
		}
		// 뷰포트 내 클릭은 스플리터에 전달하지 않음
		return;
	}

	// 뷰포트 밖: 스플리터에만 전달 (리사이즈용)
	if (ContentSplitter)
	{
		ContentSplitter->OnMouseDown(MousePos, Button);
	}
}

void SSkeletalEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 뷰포트 영역 가져오기
	FRect VPRect = ViewportPanel ? ViewportPanel->ContentRect : FRect();

	// 기즈모 드래그 중이면 항상 처리
	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

	if (bGizmoDragging || VPRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(VPRect.Left, VPRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}

	// 스플리터에도 전달
	if (ContentSplitter)
	{
		ContentSplitter->OnMouseUp(MousePos, Button);
	}
}

void SSkeletalEditorWindow::OnRenderViewport()
{
	RenderToPreviewRenderTarget();
}

void SSkeletalEditorWindow::LoadSkeletalMesh(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	// 스켈레탈 메시 로드
	USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
	if (Mesh && ActiveState->PreviewActor)
	{
		// PreviewActor에 메시 설정
		ActiveState->PreviewActor->SetSkeletalMesh(Path);
		ActiveState->CurrentMesh = Mesh;
		ActiveState->LoadedMeshPath = Path;

		// UI 경로 버퍼 업데이트
		strncpy_s(ActiveState->MeshPathBuffer, Path.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);

		// 메쉬 가시성 동기화
		if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
		{
			SkelComp->SetVisibility(ActiveState->bShowMesh);
		}

		// 본 라인 재구성
		ActiveState->bBoneLinesDirty = true;
		ActiveState->SelectedBoneIndex = -1;

		// 본 라인 가시성 동기화
		if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
		{
			LineComp->ClearLines();
			LineComp->SetLineVisible(ActiveState->bShowBones);
		}

		// 바닥판과 카메라 위치 설정 (AABB 기반)
		SetupFloorAndCamera(ActiveState);

		UE_LOG("SkeletalEditor: Load: %s", Path.c_str());
	}
	else
	{
		UE_LOG("SkeletalEditor: Load: Failed %s", Path.c_str());
	}
}

void SSkeletalEditorWindow::OpenNewTab(const FString& FilePath)
{
	// 파일명 추출
	FString TabName = "Skeletal";
	if (!FilePath.empty())
	{
		size_t LastSlash = FilePath.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = FilePath.substr(LastSlash + 1);
		}
		else
		{
			TabName = FilePath;
		}
	}
	else
	{
		char label[32];
		sprintf_s(label, "Skeletal %d", NextTabId);
		TabName = label;
	}

	// 새 탭 생성
	ViewerState* NewState = SkeletalViewerBootstrap::CreateViewerState(TabName.c_str(), World, Device);
	if (NewState)
	{
		NewState->TabId = NextTabId++;
		Tabs.Add(NewState);
		ActiveTabIndex = (int32)Tabs.Num() - 1;
		ActiveState = NewState;

		// 파일이 있으면 로드
		if (!FilePath.empty())
		{
			LoadSkeletalMesh(FilePath);
		}
	}
}

void SSkeletalEditorWindow::OpenNewTabWithMesh(USkeletalMesh* Mesh, const FString& MeshPath)
{
	if (!Mesh)
	{
		OpenNewTab("");
		return;
	}

	// 메시 이름으로 탭 이름 설정
	FString TabName = Mesh->GetName();
	if (TabName.empty())
	{
		size_t LastSlash = MeshPath.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = MeshPath.substr(LastSlash + 1);
		}
		else
		{
			TabName = MeshPath.empty() ? "Skeletal" : MeshPath;
		}
	}

	// 새 탭 생성
	ViewerState* NewState = SkeletalViewerBootstrap::CreateViewerState(TabName.c_str(), World, Device);
	if (NewState)
	{
		NewState->TabId = NextTabId++;
		Tabs.Add(NewState);
		ActiveTabIndex = (int32)Tabs.Num() - 1;
		ActiveState = NewState;

		// 메시 설정
		if (NewState->PreviewActor)
		{
			NewState->PreviewActor->SetSkeletalMesh(MeshPath);
			NewState->CurrentMesh = Mesh;
			NewState->LoadedMeshPath = MeshPath;
			strncpy_s(NewState->MeshPathBuffer, MeshPath.c_str(), sizeof(NewState->MeshPathBuffer) - 1);

			if (USkeletalMeshComponent* SkelComp = NewState->PreviewActor->GetSkeletalMeshComponent())
			{
				SkelComp->SetVisibility(NewState->bShowMesh);
			}

			NewState->bBoneLinesDirty = true;
			if (auto* LineComp = NewState->PreviewActor->GetBoneLineComponent())
			{
				LineComp->ClearLines();
				LineComp->SetLineVisible(NewState->bShowBones);
			}

			// 바닥판과 카메라 위치 설정 (AABB 기반)
			SetupFloorAndCamera(NewState);
		}
	}
}

FViewport* SSkeletalEditorWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SSkeletalEditorWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}

AGizmoActor* SSkeletalEditorWindow::GetGizmoActor() const
{
	if (ActiveState && ActiveState->World)
	{
		return ActiveState->World->GetGizmoActor();
	}
	return nullptr;
}

void SSkeletalEditorWindow::SetActiveTab(int32 TabIndex)
{
	if (TabIndex >= 0 && TabIndex < Tabs.Num())
	{
		ActiveTabIndex = TabIndex;
		ActiveState = Tabs[TabIndex];
	}
}

// ============================================================================
// 렌더 타겟 관리
// ============================================================================

void SSkeletalEditorWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	ReleaseRenderTarget();

	D3D11RHI* RHI = GEngine.GetRHIDevice();
	if (!RHI)
	{
		return;
	}

	ID3D11Device* D3DDevice = RHI->GetDevice();

	// 렌더 타겟 텍스처 생성
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = D3DDevice->CreateTexture2D(&texDesc, nullptr, &PreviewRenderTargetTexture);
	if (FAILED(hr))
	{
		return;
	}

	// 렌더 타겟 뷰 생성
	hr = D3DDevice->CreateRenderTargetView(PreviewRenderTargetTexture, nullptr, &PreviewRenderTargetView);
	if (FAILED(hr))
	{
		return;
	}

	// 셰이더 리소스 뷰 생성
	hr = D3DDevice->CreateShaderResourceView(PreviewRenderTargetTexture, nullptr, &PreviewShaderResourceView);
	if (FAILED(hr))
	{
		return;
	}

	// 깊이 스텐실 텍스처 생성
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = Width;
	depthDesc.Height = Height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
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

void SSkeletalEditorWindow::ReleaseRenderTarget()
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

void SSkeletalEditorWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
	if (NewWidth != PreviewRenderTargetWidth || NewHeight != PreviewRenderTargetHeight)
	{
		CreateRenderTarget(NewWidth, NewHeight);
	}
}

void SSkeletalEditorWindow::RenderToPreviewRenderTarget()
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

	// 현재 렌더 타겟 백업
	ID3D11RenderTargetView* OldRTV = nullptr;
	ID3D11DepthStencilView* OldDSV = nullptr;
	Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

	// D3D 뷰포트 백업
	UINT NumViewports = 1;
	D3D11_VIEWPORT OldViewport;
	Context->RSGetViewports(&NumViewports, &OldViewport);

	// 렌더 타겟 클리어
	const float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
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

	// Viewport 크기 업데이트
	ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

	// 본 오버레이 재구축
	if (ActiveState->bShowBones && ActiveState->bBoneLinesDirty && ActiveState->PreviewActor && ActiveState->CurrentMesh)
	{
		if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
		{
			LineComp->SetLineVisible(true);
		}
		ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, false, true);
		ActiveState->bBoneLinesDirty = false;
	}

	// 스켈레탈 메쉬 씬 렌더링
	if (ActiveState->Client)
	{
		ActiveState->Client->Draw(ActiveState->Viewport);
	}

	// 렌더 타겟 복원
	Context->OMSetRenderTargets(1, &OldRTV, OldDSV);
	Context->RSSetViewports(1, &OldViewport);

	if (OldRTV)
	{
		OldRTV->Release();
	}
	if (OldDSV)
	{
		OldDSV->Release();
	}
}

// ============================================================================
// Panel Implementations
// ============================================================================

// SSkeletalToolbarPanel
SSkeletalToolbarPanel::SSkeletalToolbarPanel(SSkeletalEditorWindow* InOwner) : Owner(InOwner) {}

void SSkeletalToolbarPanel::OnRender()
{
	// 툴바는 메인 윈도우에서 직접 렌더링
}

// SSkeletalAssetPanel
SSkeletalAssetPanel::SSkeletalAssetPanel(SSkeletalEditorWindow* InOwner) : Owner(InOwner) {}

void SSkeletalAssetPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Display##SkeletalDisplay", nullptr, flags))
	{
		ViewerState* State = Owner->GetActiveState();
		if (State)
		{
			// 표시 옵션
			ImGui::Text("Display Options");
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Checkbox("Show Mesh", &State->bShowMesh);
			ImGui::Checkbox("Show Bones", &State->bShowBones);

			if (State->PreviewActor)
			{
				if (USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent())
				{
					SkelComp->SetVisibility(State->bShowMesh);
				}
				if (State->PreviewActor->GetBoneLineComponent())
				{
					State->PreviewActor->GetBoneLineComponent()->SetLineVisible(State->bShowBones);
				}
				if (State->bShowBones)
				{
					State->bBoneLinesDirty = true;
				}
			}
		}
	}
	ImGui::End();
}

// SSkeletalViewportPanel
SSkeletalViewportPanel::SSkeletalViewportPanel(SSkeletalEditorWindow* InOwner) : Owner(InOwner) {}

void SSkeletalViewportPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("##SkeletalViewport", nullptr, flags))
	{
		ViewerState* State = Owner->GetActiveState();
		if (State)
		{
			// ================================================================
			// 공용 뷰포트 툴바 렌더링
			// ================================================================
			AGizmoActor* GizmoActor = State->World ? State->World->GetGizmoActor() : nullptr;
			if (Owner->ViewportToolbar)
			{
				Owner->ViewportToolbar->Render(State->Client, GizmoActor, false);
			}

			// ================================================================
			// 뷰포트 영역
			// ================================================================
			ImVec2 vpMin = ImGui::GetCursorScreenPos();
			ImVec2 vpSize = ImGui::GetContentRegionAvail();

			// 렌더 타겟 크기 업데이트
			uint32 vpWidth = (uint32)std::max(1.0f, vpSize.x);
			uint32 vpHeight = (uint32)std::max(1.0f, vpSize.y);

			if (vpWidth > 0 && vpHeight > 0)
			{
				Owner->UpdateViewportRenderTarget(vpWidth, vpHeight);

				// 스켈레탈 메쉬 씬을 전용 렌더 타겟에 렌더링
				Owner->RenderToPreviewRenderTarget();

				// 렌더 타겟 표시
				ID3D11ShaderResourceView* srv = Owner->GetPreviewShaderResourceView();
				if (srv)
				{
					ImGui::Image((ImTextureID)srv, vpSize);
				}
				else
				{
					ImGui::Dummy(vpSize);
				}

				// 뷰포트 영역에서 마우스 휠 처리 (카메라 속도 조절)
				if (ImGui::IsItemHovered() && State->Client)
				{
					ImGuiIO& IO = ImGui::GetIO();
					if (IO.MouseWheel != 0.0f)
					{
						ACameraActor* Camera = State->Client->GetCamera();
						if (Camera)
						{
							// 우클릭 드래그 중일 때만 카메라 속도 조절
							bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
							if (bRightMouseDown)
							{
								float ScalarMultiplier = (IO.MouseWheel > 0) ? 1.15f : (1.0f / 1.15f);
								float NewScalar = Camera->GetSpeedScalar() * ScalarMultiplier;
								NewScalar = std::max(0.25f, std::min(128.0f, NewScalar));
								Camera->SetSpeedScalar(NewScalar);
							}
						}
					}
				}

				// 뷰포트 영역 저장 (마우스 입력 처리용)
				ContentRect = FRect(vpMin.x, vpMin.y, vpMin.x + vpSize.x, vpMin.y + vpSize.y);
			}
		}
	}
	ImGui::End();
}

void SSkeletalViewportPanel::OnUpdate(float DeltaSeconds)
{
	// 뷰포트 업데이트 로직
}

// SSkeletalBoneTreePanel
SSkeletalBoneTreePanel::SSkeletalBoneTreePanel(SSkeletalEditorWindow* InOwner) : Owner(InOwner) {}

void SSkeletalBoneTreePanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Skeleton##SkeletalBoneTree", nullptr, flags))
	{
		ViewerState* State = Owner->GetActiveState();
		if (State)
		{
			RenderBoneTree(State);
		}
	}
	ImGui::End();
}

void SSkeletalBoneTreePanel::RenderBoneTree(ViewerState* State)
{
	if (!State->CurrentMesh)
	{
		ImGui::TextDisabled("No skeletal mesh loaded.");
		return;
	}

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || Skeleton->Bones.IsEmpty())
	{
		ImGui::TextDisabled("This mesh has no skeleton data.");
		return;
	}

	// 본 계층 구조 빌드
	const TArray<FBone>& Bones = Skeleton->Bones;
	TArray<TArray<int32>> Children;
	Children.resize(Bones.size());

	for (int32 i = 0; i < Bones.size(); ++i)
	{
		int32 Parent = Bones[i].ParentIndex;
		if (Parent >= 0 && Parent < Bones.size())
		{
			Children[Parent].Add(i);
		}
	}

	// 재귀적 트리 렌더링
	std::function<void(int32)> DrawNode = [&](int32 Index)
	{
		const bool bLeaf = Children[Index].IsEmpty();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

		if (bLeaf)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		if (State->ExpandedBoneIndices.count(Index) > 0)
		{
			ImGui::SetNextItemOpen(true);
		}

		if (State->SelectedBoneIndex == Index)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(Index);
		const char* Label = Bones[Index].Name.c_str();

		bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			State->SelectedBoneIndex = Index;
			State->bBoneLinesDirty = true;

			// 기즈모를 선택된 본에 연결
			AGizmoActor* GizmoActor = State->World ? State->World->GetGizmoActor() : nullptr;
			USkeletalMeshComponent* SkelComp = State->PreviewActor ? State->PreviewActor->GetSkeletalMeshComponent() : nullptr;
			if (GizmoActor && SkelComp)
			{
				GizmoActor->SetBoneTarget(SkelComp, Index);
				GizmoActor->SetbRender(true);
			}
			if (State->World && State->PreviewActor)
			{
				State->World->GetSelectionManager()->SelectActor(State->PreviewActor);
			}
		}

		if (open)
		{
			State->ExpandedBoneIndices.insert(Index);
		}
		else
		{
			State->ExpandedBoneIndices.erase(Index);
		}

		if (!bLeaf && open)
		{
			for (int32 ChildIndex : Children[Index])
			{
				DrawNode(ChildIndex);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	// 루트 본들 렌더링
	ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), false);
	for (int32 i = 0; i < Bones.size(); ++i)
	{
		if (Bones[i].ParentIndex < 0)
		{
			DrawNode(i);
		}
	}
	ImGui::EndChild();
}

// SSkeletalDetailPanel
SSkeletalDetailPanel::SSkeletalDetailPanel(SSkeletalEditorWindow* InOwner) : Owner(InOwner) {}

void SSkeletalDetailPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoTitleBar;

	if (ImGui::Begin("Details##SkeletalDetail", nullptr, flags))
	{
		ViewerState* State = Owner->GetActiveState();
		if (State)
		{
			// UE5 스타일 탭바
			if (ImGui::BeginTabBar("DetailPanelTabs"))
			{
				if (ImGui::BeginTabItem("Asset Details"))
				{
					State->DetailPanelTabIndex = 0;
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Details"))
				{
					State->DetailPanelTabIndex = 1;
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			ImGui::Separator();

			// 선택된 탭에 따라 렌더링
			if (State->DetailPanelTabIndex == 0)
			{
				RenderAssetDetailsTab(State);
			}
			else
			{
				RenderDetailsTab(State);
			}
		}
	}
	ImGui::End();
}

// ============================================================================
// Asset Details 탭
// ============================================================================

void SSkeletalDetailPanel::RenderAssetDetailsTab(ViewerState* State)
{
	if (!State->CurrentMesh)
	{
		ImGui::TextDisabled("No skeletal mesh loaded.");
		return;
	}

	RenderMaterialSlotsSection(State);
	RenderLODPickerSection(State);
}

void SSkeletalDetailPanel::RenderMaterialSlotsSection(ViewerState* State)
{
	if (ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen))
	{
		USkeletalMeshComponent* SkelComp = State->PreviewActor ? State->PreviewActor->GetSkeletalMeshComponent() : nullptr;
		if (!SkelComp || !State->CurrentMesh)
		{
			ImGui::TextDisabled("No mesh component.");
			return;
		}

		const TArray<FGroupInfo>& GroupInfos = State->CurrentMesh->GetMeshGroupInfo();
		int32 MaterialCount = (int32)GroupInfos.size();

		ImGui::Text("Material Slots");
		ImGui::SameLine();
		ImGui::TextDisabled("%d Material Slots", MaterialCount);

		for (int32 i = 0; i < MaterialCount; ++i)
		{
			ImGui::PushID(i);

			if (ImGui::TreeNode("Element", "Element %d", i))
			{
				const FGroupInfo& Group = GroupInfos[i];

				// 머티리얼 이름
				ImGui::Text("Material");
				ImGui::SameLine(100);
				ImGui::SetNextItemWidth(-1);
				if (Group.InitialMaterialName.empty())
				{
					ImGui::TextDisabled("(None)");
				}
				else
				{
					ImGui::TextDisabled("%s", Group.InitialMaterialName.c_str());
				}

				// 인덱스 정보
				ImGui::Text("Start Index: %u", Group.StartIndex);
				ImGui::Text("Index Count: %u", Group.IndexCount);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
}

void SSkeletalDetailPanel::RenderLODPickerSection(ViewerState* State)
{
	if (ImGui::CollapsingHeader("LOD Picker", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// LOD 정보 (현재는 기본값 표시)
		static int32 CurrentLOD = 0;
		const char* LODOptions[] = { "Auto (LOD 0)", "LOD 0", "LOD 1", "LOD 2" };

		ImGui::Text("LOD");
		ImGui::SameLine(100);
		ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##LODCombo", &CurrentLOD, LODOptions, IM_ARRAYSIZE(LODOptions));
	}
}

// ============================================================================
// Details 탭 (Bone Transform)
// ============================================================================

void SSkeletalDetailPanel::RenderDetailsTab(ViewerState* State)
{
	if (State->SelectedBoneIndex < 0)
	{
		ImGui::TextDisabled("No bone selected.");
		return;
	}

	if (!State->CurrentMesh || !State->PreviewActor)
	{
		return;
	}

	// 뷰포트에서 트랜스폼 동기화 (UI 편집 중이 아닐 때만)
	bool bIsEditingUI = ImGui::IsAnyItemActive();
	AGizmoActor* Gizmo = State->World ? State->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = Gizmo && Gizmo->GetbIsDragging();

	if (!bIsEditingUI || bGizmoDragging)
	{
		SyncBoneTransformFromViewport(State);
	}

	RenderBoneSection(State);
	RenderTransformsSection(State);
}

void SSkeletalDetailPanel::RenderBoneSection(ViewerState* State)
{
	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || State->SelectedBoneIndex >= Skeleton->Bones.Num())
	{
		return;
	}

	const FBone& Bone = Skeleton->Bones[State->SelectedBoneIndex];

	if (ImGui::CollapsingHeader("Bone", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Bone Name");
		ImGui::SameLine(120);
		ImGui::SetNextItemWidth(-1);
		ImGui::TextDisabled("%s", Bone.Name.c_str());
	}
}

void SSkeletalDetailPanel::RenderTransformsSection(ViewerState* State)
{
	// 상단에 Bone / Reference / Mesh Relative 토글 버튼 배치
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

	float buttonWidth = (ImGui::GetContentRegionAvail().x - 4) / 3.0f;

	// 현재 선택 상태 (비트마스크)
	bool bBoneSelected = (State->BoneTransformModeFlags & 0x1) != 0;
	bool bRefSelected = (State->BoneTransformModeFlags & 0x2) != 0;
	bool bMeshSelected = (State->BoneTransformModeFlags & 0x4) != 0;

	// Shift 키 상태
	bool bShiftDown = ImGui::GetIO().KeyShift;

	// Bone 버튼
	if (bBoneSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
	}
	if (ImGui::Button("Bone##TransformMode", ImVec2(buttonWidth, 0)))
	{
		if (bShiftDown)
		{
			// Shift+클릭: 토글
			State->BoneTransformModeFlags ^= 0x1;
			// 최소 하나는 선택되어야 함
			if (State->BoneTransformModeFlags == 0)
			{
				State->BoneTransformModeFlags = 0x1;
			}
		}
		else
		{
			// 일반 클릭: 단독 선택
			State->BoneTransformModeFlags = 0x1;
		}
	}
	if (bBoneSelected)
	{
		ImGui::PopStyleColor();
	}

	ImGui::SameLine();

	// Reference 버튼
	if (bRefSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
	}
	if (ImGui::Button("Reference##TransformMode", ImVec2(buttonWidth, 0)))
	{
		if (bShiftDown)
		{
			State->BoneTransformModeFlags ^= 0x2;
			if (State->BoneTransformModeFlags == 0)
			{
				State->BoneTransformModeFlags = 0x2;
			}
		}
		else
		{
			State->BoneTransformModeFlags = 0x2;
		}
	}
	if (bRefSelected)
	{
		ImGui::PopStyleColor();
	}

	ImGui::SameLine();

	// Mesh Relative 버튼
	if (bMeshSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
	}
	if (ImGui::Button("Mesh Relative##TransformMode", ImVec2(buttonWidth, 0)))
	{
		if (bShiftDown)
		{
			State->BoneTransformModeFlags ^= 0x4;
			if (State->BoneTransformModeFlags == 0)
			{
				State->BoneTransformModeFlags = 0x4;
			}
		}
		else
		{
			State->BoneTransformModeFlags = 0x4;
		}
	}
	if (bMeshSelected)
	{
		ImGui::PopStyleColor();
	}

	ImGui::PopStyleVar();

	ImGui::Spacing();

	// 선택된 모드들의 Transform 섹션 표시
	if (State->BoneTransformModeFlags & 0x1)
	{
		if (ImGui::CollapsingHeader("Bone##BoneTransform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Bone (Local) Transform - 편집 가능
			if (RenderTransformEditor("BoneEdit", State->EditBoneLocation, State->EditBoneRotation, State->EditBoneScale, false))
			{
				ApplyBoneTransformToViewport(State);
			}
		}
	}

	if (State->BoneTransformModeFlags & 0x2)
	{
		if (ImGui::CollapsingHeader("Reference##RefTransform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Reference Pose - 읽기 전용
			RenderTransformEditor("RefEdit", State->ReferenceBoneLocation, State->ReferenceBoneRotation, State->ReferenceBoneScale, true);
		}
	}

	if (State->BoneTransformModeFlags & 0x4)
	{
		if (ImGui::CollapsingHeader("Mesh Relative##MeshTransform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Mesh Relative (Component Space) - 읽기 전용
			RenderTransformEditor("MeshEdit", State->MeshRelativeBoneLocation, State->MeshRelativeBoneRotation, State->MeshRelativeBoneScale, true);
		}
	}
}

bool SSkeletalDetailPanel::RenderTransformEditor(const char* Label, FVector& Location, FVector& Rotation, FVector& Scale, bool bReadOnly)
{
	bool bChanged = false;

	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	ImGui::PushID(Label);

	// RGB 색상 정의 (X=Red, Y=Green, Z=Blue)
	const ImVec4 ColorX(0.7f, 0.2f, 0.2f, 0.5f);
	const ImVec4 ColorY(0.2f, 0.7f, 0.2f, 0.5f);
	const ImVec4 ColorZ(0.2f, 0.2f, 0.7f, 0.5f);

	float fieldWidth = (ImGui::GetContentRegionAvail().x - 80) / 3.0f - 2;

	// Location
	ImGui::Text("Location");
	ImGui::SameLine(80);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorX);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##LocX", &Location.X, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorY);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##LocY", &Location.Y, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorZ);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##LocZ", &Location.Z, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();

	// Rotation
	ImGui::Text("Rotation");
	ImGui::SameLine(80);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorX);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##RotX", &Rotation.X, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorY);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##RotY", &Rotation.Y, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorZ);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##RotZ", &Rotation.Z, 0.1f)) { bChanged = true; }
	ImGui::PopStyleColor();

	// Scale
	ImGui::Text("Scale");
	ImGui::SameLine(80);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorX);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##ScaleX", &Scale.X, 0.01f, 0.01f, 100.0f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorY);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##ScaleY", &Scale.Y, 0.01f, 0.01f, 100.0f)) { bChanged = true; }
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorZ);
	ImGui::SetNextItemWidth(fieldWidth);
	if (ImGui::DragFloat("##ScaleZ", &Scale.Z, 0.01f, 0.01f, 100.0f)) { bChanged = true; }
	ImGui::PopStyleColor();

	ImGui::PopID();

	if (bReadOnly)
	{
		ImGui::EndDisabled();
	}

	return bChanged;
}

void SSkeletalDetailPanel::SyncBoneTransformFromViewport(ViewerState* State)
{
	if (!State->PreviewActor || State->SelectedBoneIndex < 0)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	// 현재 로컬 트랜스폼 (편집 가능한 값)
	FTransform LocalTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);
	State->EditBoneLocation = LocalTransform.Translation;
	State->EditBoneRotation = LocalTransform.Rotation.ToEulerZYXDeg();
	State->EditBoneScale = LocalTransform.Scale3D;

	// Reference Pose 트랜스폼 (FSkeleton의 RefLocalPose 캐시 사용)
	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (Skeleton && State->SelectedBoneIndex < Skeleton->RefLocalPose.Num())
	{
		const FTransform& RefPose = Skeleton->RefLocalPose[State->SelectedBoneIndex];
		State->ReferenceBoneLocation = RefPose.Translation;
		State->ReferenceBoneRotation = RefPose.Rotation.ToEulerZYXDeg();
		State->ReferenceBoneScale = RefPose.Scale3D;
	}

	// Component Space (Mesh Relative) 트랜스폼
	FTransform ComponentTransform = SkelComp->GetBoneWorldTransform(State->SelectedBoneIndex);
	// Actor 트랜스폼 역적용하여 컴포넌트 공간으로 변환
	FTransform ActorTransform = State->PreviewActor->GetActorTransform();
	FVector RelativeLocation = ComponentTransform.Translation - ActorTransform.Translation;
	RelativeLocation = ActorTransform.Rotation.Inverse().RotateVector(RelativeLocation);

	State->MeshRelativeBoneLocation = RelativeLocation;
	State->MeshRelativeBoneRotation = (ActorTransform.Rotation.Inverse() * ComponentTransform.Rotation).ToEulerZYXDeg();
	State->MeshRelativeBoneScale = ComponentTransform.Scale3D;
}

void SSkeletalDetailPanel::ApplyBoneTransformToViewport(ViewerState* State)
{
	if (!State->PreviewActor || State->SelectedBoneIndex < 0)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	// UI에서 수정된 값을 본에 적용
	FTransform NewTransform;
	NewTransform.Translation = State->EditBoneLocation;
	NewTransform.Rotation = FQuat::MakeFromEulerZYX(State->EditBoneRotation);
	NewTransform.Scale3D = State->EditBoneScale;

	SkelComp->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);

	// 본 라인 재구성
	State->bBoneLinesDirty = true;
}

// ============================================================================
// Bone Transform Helpers
// ============================================================================

void SSkeletalEditorWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
	if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
	{
		return;
	}

	// 본의 로컬 트랜스폼에서 값 추출
	const FTransform& BoneTransform = State->PreviewActor->GetSkeletalMeshComponent()->GetBoneLocalTransform(State->SelectedBoneIndex);
	State->EditBoneLocation = BoneTransform.Translation;
	State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
	State->EditBoneScale = BoneTransform.Scale3D;
}

void SSkeletalEditorWindow::ApplyBoneTransform(ViewerState* State)
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

void SSkeletalEditorWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
{
	if (!State || !State->CurrentMesh)
	{
		return;
	}

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
	{
		return;
	}

	// 선택된 본부터 루트까지 모든 부모를 펼침
	int32 CurrentIndex = BoneIndex;
	while (CurrentIndex >= 0)
	{
		State->ExpandedBoneIndices.insert(CurrentIndex);
		CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
	}
}

void SSkeletalEditorWindow::SetupFloorAndCamera(ViewerState* State)
{
	if (!State)
	{
		return;
	}
	SkeletalViewerBootstrap::SetupFloorAndCamera(State->PreviewActor, State->FloorActor, State->Client);
}

