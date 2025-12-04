#include "pch.h"
#include "AnimationWindow.h"
#include "SplitterH.h"
#include "SplitterV.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Collision/AABB.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

// ============================================================================
// 패널 래퍼 클래스 (SSplitter에 연결하기 위한 SWindow 래퍼)
// ============================================================================

class SAnimPanelWrapper : public SWindow
{
public:
	using RenderFunc = std::function<void()>;
	RenderFunc RenderCallback;

	void SetRenderCallback(RenderFunc Callback) { RenderCallback = Callback; }

	virtual void OnRender() override
	{
		if (RenderCallback)
		{
			RenderCallback();
		}
	}

	virtual void OnUpdate(float DeltaSeconds) override {}
	virtual void OnMouseMove(FVector2D MousePos) override {}
	virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override {}
	virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override {}
};

// ============================================================================
// SAnimationWindow 구현
// ============================================================================

SAnimationWindow::SAnimationWindow()
{
	ViewportRect = FRect(0, 0, 0, 0);
	PropertiesRect = FRect(0, 0, 0, 0);
	AnimListRect = FRect(0, 0, 0, 0);
	TimelineRect = FRect(0, 0, 0, 0);
}

SAnimationWindow::~SAnimationWindow()
{
	ReleaseRenderTarget();

	// SSplitter 해제 (역순)
	// LeftSplitter: Viewport / Timeline
	if (LeftSplitter)
	{
		delete LeftSplitter->GetLeftOrTop();       // ViewportPanel
		delete LeftSplitter->GetRightOrBottom();   // TimelinePanel
		delete LeftSplitter;
		LeftSplitter = nullptr;
	}

	// RightSplitter: Properties / AnimList
	if (RightSplitter)
	{
		delete RightSplitter->GetLeftOrTop();      // PropertiesPanel
		delete RightSplitter->GetRightOrBottom();  // AnimListPanel
		delete RightSplitter;
		RightSplitter = nullptr;
	}

	// MainSplitter: Left / Right (자식 Splitter는 이미 삭제됨)
	if (MainSplitter)
	{
		delete MainSplitter;
		MainSplitter = nullptr;
	}

	// 탭 정리
	for (FAnimationTabState* State : Tabs)
	{
		DestroyTabState(State);
	}
	Tabs.clear();
	ActiveState = nullptr;
}

bool SAnimationWindow::Initialize(float StartX, float StartY, float Width, float Height,
                                   UWorld* InWorld, ID3D11Device* InDevice)
{
	World = InWorld;
	Device = InDevice;

	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 타임라인 아이콘 로드
	IconGoToFront = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_Front_24x.png");
	IconGoToFrontOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_Front_24x_OFF.png");
	IconStepBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Backwards_24x.png");
	IconStepBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Backwards_24x_OFF.png");
	IconBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Backwards_24x.png");
	IconBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Backwards_24x_OFF.png");
	IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Record_24x.png");
	IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Pause_24x.png");
	IconPauseOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Pause_24x_OFF.png");
	IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Play_24x.png");
	IconPlayOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Play_24x_OFF.png");
	IconStepForward = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Forward_24x.png");
	IconStepForwardOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Step_Forward_24x_OFF.png");
	IconGoToEnd = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_End_24x.png");
	IconGoToEndOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Go_To_End_24x_OFF.png");
	IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Loop_24x.png");
	IconLoopOff = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Loop_24x_OFF.png");

	ScanNotifyLibrary();

	// === SSplitter 레이아웃 생성 (UE 스타일) ===
	// 구조:
	// MainSplitter (H): Left(LeftSplitter) / Right(RightSplitter)
	//   LeftSplitter (V): Top(ViewportPanel) / Bottom(TimelinePanel)
	//   RightSplitter (V): Top(PropertiesPanel) / Bottom(AnimListPanel)

	// 패널 래퍼 생성
	SAnimPanelWrapper* ViewportPanel = new SAnimPanelWrapper();
	SAnimPanelWrapper* PropertiesPanel = new SAnimPanelWrapper();
	SAnimPanelWrapper* AnimListPanel = new SAnimPanelWrapper();
	SAnimPanelWrapper* TimelinePanel = new SAnimPanelWrapper();

	// 콜백 설정
	ViewportPanel->SetRenderCallback([this]() { RenderViewportPanel(); });
	PropertiesPanel->SetRenderCallback([this]() { RenderPropertiesPanel(); });
	AnimListPanel->SetRenderCallback([this]() { RenderAnimationListPanel(); });
	TimelinePanel->SetRenderCallback([this]() { RenderTimelinePanel(); });

	// LeftSplitter: Viewport / Timeline (상하 분할)
	LeftSplitter = new SSplitterV();
	LeftSplitter->SetLeftOrTop(ViewportPanel);
	LeftSplitter->SetRightOrBottom(TimelinePanel);
	LeftSplitter->SetSplitRatio(0.7f);  // Viewport 70%, Timeline 30%
	LeftSplitter->LoadFromConfig("AnimWindow_Left");

	// RightSplitter: Properties / AnimList (상하 분할)
	RightSplitter = new SSplitterV();
	RightSplitter->SetLeftOrTop(PropertiesPanel);
	RightSplitter->SetRightOrBottom(AnimListPanel);
	RightSplitter->SetSplitRatio(0.5f);  // Properties 50%, AnimList 50%
	RightSplitter->LoadFromConfig("AnimWindow_Right");

	// MainSplitter: Left / Right (좌우 분할)
	MainSplitter = new SSplitterH();
	MainSplitter->SetLeftOrTop(LeftSplitter);
	MainSplitter->SetRightOrBottom(RightSplitter);
	MainSplitter->SetSplitRatio(0.75f);  // Left 75%, Right 25%
	MainSplitter->LoadFromConfig("AnimWindow_Main");

	bRequestFocus = true;
	return true;
}

// ============================================================================
// 탭 관리
// ============================================================================

FString SAnimationWindow::ExtractFileName(const FString& FilePath) const
{
	if (FilePath.empty())
	{
		return "New Animation";
	}

	// 경로에서 파일명 추출
	size_t LastSlash = FilePath.find_last_of("/\\");
	FString FileName = (LastSlash != FString::npos) ? FilePath.substr(LastSlash + 1) : FilePath;

	// 확장자 제거 (선택적)
	// size_t LastDot = FileName.find_last_of('.');
	// if (LastDot != FString::npos)
	// {
	//     FileName = FileName.substr(0, LastDot);
	// }

	return FileName;
}

FAnimationTabState* SAnimationWindow::CreateTabState(const FString& FilePath)
{
	if (!Device)
	{
		return nullptr;
	}

	FAnimationTabState* State = new FAnimationTabState();
	State->TabName = ExtractFileName(FilePath).c_str();
	State->TabId = NextTabId++;
	State->FilePath = FilePath;

	// Preview World 생성
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

	// Preview Actor 생성
	ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
	if (Preview)
	{
		Preview->SetTickInEditor(true);
		Preview->RegisterAnimNotifyDelegate();
	}
	State->PreviewActor = Preview;

	// 바닥판 액터 생성
	State->FloorActor = SkeletalViewerBootstrap::CreateFloorActor(State->World);

	return State;
}

void SAnimationWindow::DestroyTabState(FAnimationTabState* State)
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
	// World가 이미 삭제되었는지 확인 (엔진 종료 시 DeleteAll에서 먼저 삭제될 수 있음)
	if (State->World && ObjectFactory::IsValidObject(State->World))
	{
		ObjectFactory::DeleteObject(State->World);
	}
	State->World = nullptr;

	delete State;
}

void SAnimationWindow::OpenNewTab(const FString& FilePath)
{
	FAnimationTabState* State = CreateTabState(FilePath);
	if (State)
	{
		Tabs.Add(State);
		ActiveTabIndex = (int32)Tabs.Num() - 1;
		ActiveState = State;

		// FilePath가 있으면 애니메이션 로드 시도
		if (!FilePath.empty())
		{
			LoadAnimation(FilePath);
		}
	}
}

void SAnimationWindow::OpenNewTabWithAnimation(UAnimSequence* Animation, const FString& FilePath)
{
	OpenNewTab(FilePath);

	if (ActiveState && Animation)
	{
		ActiveState->CurrentAnimation = Animation;
		ActiveState->SelectedAnimationIndex = 0;
		ActiveState->CurrentAnimationTime = 0.0f;
		ActiveState->bIsPlaying = true;

		if (UAnimDataModel* DataModel = Animation->GetDataModel())
		{
			int32 TotalFrames = DataModel->GetNumberOfFrames();
			ActiveState->WorkingRangeStartFrame = 0;
			ActiveState->WorkingRangeEndFrame = TotalFrames;
			ActiveState->ViewRangeStartFrame = 0;
			ActiveState->ViewRangeEndFrame = TotalFrames;
		}

		RebuildNotifyTracks(ActiveState);
	}
}

void SAnimationWindow::OpenNewTabWithMesh(USkeletalMesh* Mesh, const FString& MeshPath)
{
	// 탭 이름은 메쉬 파일명 기반
	OpenNewTab(MeshPath);

	// Skeletal Mesh 로드
	if (ActiveState && Mesh && !MeshPath.empty())
	{
		LoadSkeletalMesh(MeshPath);

		// 적용 가능한 첫 번째 Animation 자동 선택
		TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		TArray<UAnimSequence*> Animations;
		for (UAnimSequence* Anim : AllAnimations)
		{
			if (Anim && Anim->GetFilePath().ends_with(".anim"))
			{
				Animations.push_back(Anim);
			}
		}

		if (Animations.Num() > 0 && ActiveState->PreviewActor)
		{
			UAnimSequence* FirstAnim = Animations[0];
			ActiveState->SelectedAnimationIndex = 0;
			ActiveState->CurrentAnimation = FirstAnim;
			ActiveState->CurrentAnimationTime = 0.0f;

			// Notify Track 재구성
			RebuildNotifyTracks(ActiveState);

			// 프레임 범위 설정
			if (FirstAnim->GetDataModel())
			{
				int32 TotalFrames = FirstAnim->GetDataModel()->GetNumberOfFrames();
				ActiveState->WorkingRangeStartFrame = 0;
				ActiveState->WorkingRangeEndFrame = TotalFrames;
				ActiveState->ViewRangeStartFrame = 0;
				ActiveState->ViewRangeEndFrame = FMath::Min(60, TotalFrames);
			}

			// AnimInstance 재생 시작
			USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
			if (SkelComp)
			{
				SkelComp->ResetToReferencePose();
				SkelComp->ClearAllBoneDeltas();

				UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
				if (!AnimInst)
				{
					UAnimSingleNodeInstance* SingleNodeInst = NewObject<UAnimSingleNodeInstance>();
					AnimInst = SingleNodeInst;
					if (AnimInst)
					{
						SkelComp->SetAnimInstance(AnimInst);
					}
				}

				if (AnimInst)
				{
					AnimInst->PlayAnimation(FirstAnim, ActiveState->PlaybackSpeed);
					ActiveState->bIsPlaying = true;
				}
			}

			// 탭 이름을 Animation 이름으로 설정
			FString AnimName = FirstAnim->GetName();
			if (!AnimName.empty())
			{
				ActiveState->TabName = FName(AnimName.c_str());
			}
		}
	}
}

void SAnimationWindow::CloseTab(int32 Index)
{
	if (Index < 0 || Index >= Tabs.Num())
	{
		return;
	}

	// SSplitter 설정 저장
	if (MainSplitter)
	{
		MainSplitter->SaveToConfig("AnimWindow_Main");
	}
	if (LeftSplitter)
	{
		LeftSplitter->SaveToConfig("AnimWindow_Left");
	}
	if (RightSplitter)
	{
		RightSplitter->SaveToConfig("AnimWindow_Right");
	}

	DestroyTabState(Tabs[Index]);
	Tabs.erase(Tabs.begin() + Index);

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

// ============================================================================
// OnRender
// ============================================================================

void SAnimationWindow::OnRender()
{
	// 내장 모드에서는 OnRender를 호출하지 않고 RenderEmbedded를 사용
	if (bIsEmbeddedMode)
	{
		return;
	}

	if (!bIsOpen)
	{
		return;
	}

	if (Tabs.Num() == 0)
	{
		bIsOpen = false;
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	if (!bInitialPlacementDone)
	{
		ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
		ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
		bInitialPlacementDone = true;
	}

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
	}

	bool bViewerVisible = false;
	if (ImGui::Begin("Animation Editor", &bIsOpen, flags))
	{
		bViewerVisible = true;
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 탭 바 렌더링
		int32 PreviousTabIndex = ActiveTabIndex;
		bool bTabClosed = false;

		if (ImGui::BeginTabBar("AnimationEditorTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		{
			for (int32 i = 0; i < Tabs.Num(); ++i)
			{
				FAnimationTabState* State = Tabs[i];
				bool open = true;

				// 탭 이름: 파일명 기반 + 고정 ID (이름 변경되어도 ImGui가 같은 탭으로 인식)
				FString TabLabel = State->TabName.ToString();
				if (TabLabel.empty())
				{
					TabLabel = "Animation " + std::to_string(i + 1);
				}

				char tabLabelWithId[256];
				sprintf_s(tabLabelWithId, "%s###AnimTab_%d", TabLabel.c_str(), State->TabId);

				if (ImGui::BeginTabItem(tabLabelWithId, &open))
				{
					ActiveTabIndex = i;
					ActiveState = State;
					ImGui::EndTabItem();
				}

				if (!open)
				{
					CloseTab(i);
					bTabClosed = true;
					break;
				}
			}

			// + 버튼으로 새 탭 생성
			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
			{
				OpenNewTab("");
			}

			ImGui::EndTabBar();
		}

		if (bTabClosed)
		{
			ImGui::End();
			bRequestFocus = false;
			return;
		}

		if (!ActiveState)
		{
			ImGui::End();
			bRequestFocus = false;
			return;
		}

		// 윈도우 위치/크기 업데이트
		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x;
		Rect.Top = pos.y;
		Rect.Right = pos.x + size.x;
		Rect.Bottom = pos.y + size.y;
		Rect.UpdateMinMax();

		// 콘텐츠 영역 계산
		ImVec2 contentMin = ImGui::GetCursorScreenPos();
		ImVec2 contentAvail = ImGui::GetContentRegionAvail();

		FRect ContentRect;
		ContentRect.Left = contentMin.x;
		ContentRect.Top = contentMin.y;
		ContentRect.Right = contentMin.x + contentAvail.x;
		ContentRect.Bottom = contentMin.y + contentAvail.y;
		ContentRect.UpdateMinMax();

		// SSplitter에 영역 설정 및 렌더링
		if (MainSplitter)
		{
			MainSplitter->SetRect(ContentRect.Left, ContentRect.Top, ContentRect.Right, ContentRect.Bottom);
			MainSplitter->OnRender();

			// 패널 Rect 캐시 (입력 처리용)
			// LeftSplitter: Viewport / Timeline
			if (LeftSplitter)
			{
				if (LeftSplitter->GetLeftOrTop())
				{
					ViewportRect = LeftSplitter->GetLeftOrTop()->GetRect();
				}
				if (LeftSplitter->GetRightOrBottom())
				{
					TimelineRect = LeftSplitter->GetRightOrBottom()->GetRect();
				}
			}
			// RightSplitter: Properties / AnimList
			if (RightSplitter)
			{
				if (RightSplitter->GetLeftOrTop())
				{
					PropertiesRect = RightSplitter->GetLeftOrTop()->GetRect();
				}
				if (RightSplitter->GetRightOrBottom())
				{
					AnimListRect = RightSplitter->GetRightOrBottom()->GetRect();
				}
			}
		}

		// 패널 윈도우들을 앞으로 가져오기
		// Content Browser가 열려있으면 z-order 유지를 위해 front로 가져오지 않음
		ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##AnimViewport");
		ImGuiWindow* PropertiesWin = ImGui::FindWindowByName("Properties##AnimProperties");
		ImGuiWindow* AnimListWin = ImGui::FindWindowByName("Animation List##AnimList");
		ImGuiWindow* TimelineWin = ImGui::FindWindowByName("Timeline##AnimTimeline");

		if (!SLATE.IsContentBrowserVisible())
		{
			if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
			if (PropertiesWin) ImGui::BringWindowToDisplayFront(PropertiesWin);
			if (AnimListWin) ImGui::BringWindowToDisplayFront(AnimListWin);
			if (TimelineWin) ImGui::BringWindowToDisplayFront(TimelineWin);
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
	ImGui::End();

	if (!bViewerVisible)
	{
		ViewportRect = FRect(0, 0, 0, 0);
	}

	if (!bIsOpen)
	{
		// 윈도우 닫힘 처리
	}

	bRequestFocus = false;
}

// ============================================================================
// RenderEmbedded - DynamicEditorWindow 내장 모드용 렌더링
// ============================================================================

void SAnimationWindow::RenderEmbedded(const FRect& ContentRect)
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
	if (MainSplitter)
	{
		MainSplitter->SetRect(ContentRect.Left, ContentRect.Top, ContentRect.Right, ContentRect.Bottom);
		MainSplitter->OnRender();

		// 패널 Rect 캐시 (입력 처리용)
		// LeftSplitter: Viewport / Timeline
		if (LeftSplitter)
		{
			if (LeftSplitter->GetLeftOrTop())
			{
				ViewportRect = LeftSplitter->GetLeftOrTop()->GetRect();
			}
			if (LeftSplitter->GetRightOrBottom())
			{
				TimelineRect = LeftSplitter->GetRightOrBottom()->GetRect();
			}
		}
		// RightSplitter: Properties / AnimList
		if (RightSplitter)
		{
			if (RightSplitter->GetLeftOrTop())
			{
				PropertiesRect = RightSplitter->GetLeftOrTop()->GetRect();
			}
			if (RightSplitter->GetRightOrBottom())
			{
				AnimListRect = RightSplitter->GetRightOrBottom()->GetRect();
			}
		}
	}

	// 패널 윈도우들을 앞으로 가져오기 (DynamicEditorWindow 뒤에 가려지는 문제 해결)
	// Content Browser가 열려있으면 z-order 유지를 위해 front로 가져오지 않음
	if (!SLATE.IsContentBrowserVisible())
	{
		ImGuiWindow* ViewportWin = ImGui::FindWindowByName("##AnimViewport");
		ImGuiWindow* PropertiesWin = ImGui::FindWindowByName("Properties##AnimProperties");
		ImGuiWindow* AnimListWin = ImGui::FindWindowByName("Animation List##AnimList");
		ImGuiWindow* TimelineWin = ImGui::FindWindowByName("Timeline##AnimTimeline");

		if (ViewportWin) ImGui::BringWindowToDisplayFront(ViewportWin);
		if (PropertiesWin) ImGui::BringWindowToDisplayFront(PropertiesWin);
		if (AnimListWin) ImGui::BringWindowToDisplayFront(AnimListWin);
		if (TimelineWin) ImGui::BringWindowToDisplayFront(TimelineWin);
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

// ============================================================================
// OnUpdate
// ============================================================================

void SAnimationWindow::OnUpdate(float DeltaSeconds)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// 기즈모 드래그 상태 감지
	UInputManager& InputMgr = UInputManager::GetInstance();
	bool bIsGizmoDragging = InputMgr.GetIsGizmoDragging();

	// 기즈모 드래그 시작 감지 - DragStartBoneIndex 설정
	if (!ActiveState->bWasGizmoDragging && bIsGizmoDragging)
	{
		AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
		if (Gizmo && Gizmo->IsBoneTarget() && ActiveState->SelectedBoneIndex >= 0 && ActiveState->PreviewActor)
		{
			ActiveState->DragStartBoneIndex = ActiveState->SelectedBoneIndex;
		}
	}

	// 기즈모 드래그 종료 감지 - 새로운 총 델타 계산 (누적 아님, 교체)
	if (ActiveState->bWasGizmoDragging && !bIsGizmoDragging)
	{
		AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
		if (Gizmo && Gizmo->IsBoneTarget() && ActiveState->DragStartBoneIndex >= 0 && ActiveState->PreviewActor)
		{
			USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
			if (SkeletalComp)
			{
				int32 BoneIndex = ActiveState->DragStartBoneIndex;

				// 현재 본의 로컬 트랜스폼 (드래그 후 최종 위치)
				FTransform CurrentLocalTransform = SkeletalComp->GetBoneLocalTransform(BoneIndex);

				// 현재 애니메이션 시간의 순수 애니메이션 값 가져오기
				FTransform PureAnimTransform = CurrentLocalTransform;

				UAnimInstance* AnimInst = SkeletalComp->GetAnimInstance();
				if (AnimInst && AnimInst->GetCurrentAnimation())
				{
					UAnimSequence* AnimSeq = Cast<UAnimSequence>(AnimInst->GetCurrentAnimation());
					if (AnimSeq && AnimSeq->GetDataModel())
					{
						UAnimDataModel* DataModel = AnimSeq->GetDataModel();
						const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByIndex(BoneIndex);
						if (Track)
						{
							// 시간을 프레임으로 변환하여 보간
							float FrameRate = DataModel->GetFrameRate().AsDecimal();
							float CurrentTime = AnimInst->GetCurrentTime();
							float FrameFloat = CurrentTime * FrameRate;
							int32 Frame0 = static_cast<int32>(FrameFloat);
							int32 Frame1 = Frame0 + 1;
							float Alpha = FrameFloat - static_cast<float>(Frame0);

							int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
							Frame0 = FMath::Clamp(Frame0, 0, MaxFrame);
							Frame1 = FMath::Clamp(Frame1, 0, MaxFrame);

							// 키프레임 보간
							const auto& PosKeys = Track->InternalTrack.PosKeys;
							const auto& RotKeys = Track->InternalTrack.RotKeys;
							const auto& ScaleKeys = Track->InternalTrack.ScaleKeys;

							if (PosKeys.Num() > 0 && RotKeys.Num() > 0 && ScaleKeys.Num() > 0)
							{
								int32 PosIdx0 = FMath::Clamp(Frame0, 0, PosKeys.Num() - 1);
								int32 PosIdx1 = FMath::Clamp(Frame1, 0, PosKeys.Num() - 1);
								int32 RotIdx0 = FMath::Clamp(Frame0, 0, RotKeys.Num() - 1);
								int32 RotIdx1 = FMath::Clamp(Frame1, 0, RotKeys.Num() - 1);
								int32 ScaleIdx0 = FMath::Clamp(Frame0, 0, ScaleKeys.Num() - 1);
								int32 ScaleIdx1 = FMath::Clamp(Frame1, 0, ScaleKeys.Num() - 1);

								FVector Pos = FVector::Lerp(PosKeys[PosIdx0], PosKeys[PosIdx1], Alpha);
								FQuat Rot = FQuat::Slerp(RotKeys[RotIdx0], RotKeys[RotIdx1], Alpha);
								FVector Scl = FVector::Lerp(ScaleKeys[ScaleIdx0], ScaleKeys[ScaleIdx1], Alpha);

								PureAnimTransform = FTransform(Pos, Rot, Scl);
							}
						}
					}
				}

				// 새로운 총 델타 계산: 현재 위치 - 순수 애니메이션 값
				FTransform NewTotalDelta;
				NewTotalDelta.Translation = CurrentLocalTransform.Translation - PureAnimTransform.Translation;
				NewTotalDelta.Rotation = (PureAnimTransform.Rotation.Inverse() * CurrentLocalTransform.Rotation).GetNormalized();
				NewTotalDelta.Scale3D = FVector(
					PureAnimTransform.Scale3D.X != 0.0f ? CurrentLocalTransform.Scale3D.X / PureAnimTransform.Scale3D.X : 1.0f,
					PureAnimTransform.Scale3D.Y != 0.0f ? CurrentLocalTransform.Scale3D.Y / PureAnimTransform.Scale3D.Y : 1.0f,
					PureAnimTransform.Scale3D.Z != 0.0f ? CurrentLocalTransform.Scale3D.Z / PureAnimTransform.Scale3D.Z : 1.0f
				);

				// 총 델타 저장 (교체, 누적 아님)
				SkeletalComp->SetBoneDelta(BoneIndex, NewTotalDelta);

				// UI 업데이트
				ActiveState->EditBoneLocation = CurrentLocalTransform.Translation;
				ActiveState->EditBoneRotation = CurrentLocalTransform.Rotation.ToEulerZYXDeg();
				ActiveState->EditBoneScale = CurrentLocalTransform.Scale3D;

				// 드래그 상태 초기화
				ActiveState->DragStartBoneIndex = -1;
			}
		}
	}
	ActiveState->bWasGizmoDragging = bIsGizmoDragging;

	// Preview World 틱
	if (ActiveState->World)
	{
		ActiveState->World->Tick(DeltaSeconds);
		if (ActiveState->World->GetGizmoActor())
		{
			ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
		}
	}

	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}

	// SSplitter 업데이트
	if (MainSplitter)
	{
		MainSplitter->OnUpdate(DeltaSeconds);
	}

	// AnimInstance를 통한 애니메이션 재생 컨트롤
	if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
	{
		USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
		UAnimInstance* AnimInst = SkelComp->GetAnimInstance();

		if (AnimInst)
		{
			ActiveState->bIsPlaying = AnimInst->IsPlaying();
			ActiveState->CurrentAnimationTime = AnimInst->GetCurrentTime();

			if (FMath::Abs(AnimInst->GetPlayRate() - ActiveState->PlaybackSpeed) > 0.001f)
			{
				AnimInst->SetPlayRate(ActiveState->PlaybackSpeed);
			}
		}

		SkelComp->SetTickEnabled(ActiveState->bIsPlaying);
	}

	// Bone Line 업데이트 (애니메이션 재생 중 매 프레임)
	bool bNeedsRebuild = false;
	bool bForceHighlightRefresh = false;
	bool bIsAnimationPlaying = ActiveState->bIsPlaying;

	if (ActiveState->bShowBones)
	{
		// 애니메이션 재생 중이거나 Dirty 플래그가 설정되어 있으면 재구축
		if (bIsAnimationPlaying || ActiveState->bBoneLinesDirty)
		{
			bNeedsRebuild = true;
			// Dirty 플래그가 설정되어 있으면 색상 강제 갱신
			if (ActiveState->bBoneLinesDirty)
			{
				bForceHighlightRefresh = true;
			}
			ActiveState->bBoneLinesDirty = false;
		}
	}

	if (bNeedsRebuild && ActiveState->PreviewActor && ActiveState->CurrentMesh)
	{
		if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
		{
			LineComp->SetLineVisible(true);
		}
		// 애니메이션 재생 중이면 모든 본 업데이트, Dirty 플래그가 있었으면 색상 강제 갱신
		ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, bIsAnimationPlaying, bForceHighlightRefresh);
	}

	// 애니메이션 재생 중이고 본이 선택되어 있으면 기즈모 위치도 실시간 업데이트
	if (bIsAnimationPlaying && ActiveState->SelectedBoneIndex >= 0 && ActiveState->PreviewActor && ActiveState->World)
	{
		AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
		USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
		if (Gizmo && SkelComp && Gizmo->IsBoneTarget())
		{
			FTransform BoneWorldTransform = SkelComp->GetBoneWorldTransform(ActiveState->SelectedBoneIndex);
			Gizmo->SetActorLocation(BoneWorldTransform.Translation);
		}
	}
}

// ============================================================================
// 마우스 입력
// ============================================================================

void SAnimationWindow::OnMouseMove(FVector2D MousePos)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// SSplitter 마우스 이동 처리
	if (MainSplitter)
	{
		MainSplitter->OnMouseMove(MousePos);
	}

	// 뷰포트 영역 마우스 처리
	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

	if (bGizmoDragging || ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
	}
}

void SAnimationWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// SSplitter 마우스 다운 처리
	if (MainSplitter)
	{
		MainSplitter->OnMouseDown(MousePos, Button);
	}

	// 뷰포트 영역 클릭 처리
	if (ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

		// Bone 피킹 (좌클릭)
		if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
		{
			AGizmoActor* Gizmo = ActiveState->World->GetGizmoActor();
			bool bGizmoClicked = (Gizmo && Gizmo->GetbIsHovering());

			if (bGizmoClicked && Gizmo && Gizmo->IsBoneTarget())
			{
				ACameraActor* Camera = ActiveState->Client->GetCamera();
				if (Camera)
				{
					Gizmo->StartDrag(Camera, ActiveState->Viewport, (float)LocalPos.X, (float)LocalPos.Y);
				}
				return;
			}

			if (!bGizmoClicked)
			{
				ACameraActor* Camera = ActiveState->Client->GetCamera();
				if (Camera)
				{
					FVector CameraPos = Camera->GetActorLocation();
					FVector CameraRight = Camera->GetRight();
					FVector CameraUp = Camera->GetUp();
					FVector CameraForward = Camera->GetForward();

					FVector2D ViewportMousePos(MousePos.X - ViewportRect.Left, MousePos.Y - ViewportRect.Top);
					FVector2D ViewportSize(ViewportRect.GetWidth(), ViewportRect.GetHeight());

					FRay Ray = MakeRayFromViewport(
						Camera->GetViewMatrix(),
						Camera->GetProjectionMatrix(ViewportRect.GetWidth() / ViewportRect.GetHeight(), ActiveState->Viewport),
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
	}
}

void SAnimationWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	// SSplitter 마우스 업 처리
	if (MainSplitter)
	{
		MainSplitter->OnMouseUp(MousePos, Button);
	}

	if (ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

// ============================================================================
// Accessors
// ============================================================================

FViewport* SAnimationWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SAnimationWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}

AGizmoActor* SAnimationWindow::GetGizmoActor() const
{
	if (ActiveState && ActiveState->World)
	{
		return ActiveState->World->GetGizmoActor();
	}
	return nullptr;
}

// ============================================================================
// 바닥판 및 카메라 설정
// ============================================================================

static void SetupFloorAndCamera(FAnimationTabState* State)
{
	if (!State)
	{
		return;
	}
	SkeletalViewerBootstrap::SetupFloorAndCamera(State->PreviewActor, State->FloorActor, State->Client);
}

// ============================================================================
// 에셋 로드
// ============================================================================

void SAnimationWindow::LoadSkeletalMesh(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
	if (Mesh && ActiveState->PreviewActor)
	{
		ActiveState->PreviewActor->SetSkeletalMesh(Path);
		ActiveState->CurrentMesh = Mesh;
		ActiveState->LoadedMeshPath = Path;

		if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
		{
			Skeletal->SetVisibility(ActiveState->bShowMesh);
		}

		ActiveState->bBoneLinesDirty = true;

		if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
		{
			LineComp->ClearLines();
			LineComp->SetLineVisible(ActiveState->bShowBones);
		}

		// 이미 로드된 AnimSequence를 메쉬에 추가
		TArray<UAnimSequence*> AllAnimSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		for (UAnimSequence* AnimSeq : AllAnimSequences)
		{
			if (AnimSeq && !AnimSeq->GetName().empty())
			{
				Mesh->AddAnimation(AnimSeq);
			}
		}

		// 바닥판 및 카메라 설정
		SetupFloorAndCamera(ActiveState);
	}
}

void SAnimationWindow::LoadAnimation(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	UAnimSequence* Anim = nullptr;

	// FBX 파일인 경우 직접 로드
	if (Path.ends_with(".fbx") || Path.ends_with(".FBX"))
	{
		// FBX에서 스켈레톤 추출
		FSkeleton* FbxSkeleton = UFbxLoader::GetInstance().ExtractSkeletonFromFbx(Path);
		if (FbxSkeleton)
		{
			// 모든 AnimStack 로드
			TArray<UAnimSequence*> AllAnims = UFbxLoader::GetInstance().LoadAllFbxAnimations(Path, *FbxSkeleton);
			if (AllAnims.Num() > 0)
			{
				Anim = AllAnims[0];  // 첫 번째 애니메이션 사용

				// PreviewActor에 스켈레탈 메시 설정 (애니메이션과 호환되는 메시 찾기)
				if (ActiveState->PreviewActor)
				{
					USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
					if (MeshComp && !MeshComp->GetSkeletalMesh())
					{
						// 호환되는 스켈레탈 메시 찾기
						TArray<USkeletalMesh*> AllMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();
						for (USkeletalMesh* Mesh : AllMeshes)
						{
							if (Mesh && Mesh->GetSkeleton())
							{
								// 스켈레톤이 호환되는지 간단히 본 개수로 체크
								if (Mesh->GetSkeleton()->Bones.Num() == FbxSkeleton->Bones.Num())
								{
									MeshComp->SetSkeletalMesh(Mesh->GetPathFileName());
									SetupFloorAndCamera(ActiveState);
									break;
								}
							}
						}
					}
				}
			}
			delete FbxSkeleton;
		}
	}
	else
	{
		// .anim 파일 또는 다른 형식
		Anim = UResourceManager::GetInstance().Load<UAnimSequence>(Path);

		// .anim 파일에서도 호환되는 스켈레탈 메시 찾기
		if (Anim && ActiveState->PreviewActor)
		{
			USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
			if (MeshComp && !MeshComp->GetSkeletalMesh())
			{
				const FSkeleton* AnimSkeleton = Anim->GetSkeleton();
				if (AnimSkeleton)
				{
					// 호환되는 스켈레탈 메시 찾기
					TArray<USkeletalMesh*> AllMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();
					for (USkeletalMesh* Mesh : AllMeshes)
					{
						if (Mesh && Mesh->GetSkeleton())
						{
							// 스켈레톤이 호환되는지 본 개수로 체크
							if (Mesh->GetSkeleton()->Bones.Num() == AnimSkeleton->Bones.Num())
							{
								MeshComp->SetSkeletalMesh(Mesh->GetPathFileName());
								SetupFloorAndCamera(ActiveState);
								break;
							}
						}
					}
				}
			}
		}
	}

	if (Anim)
	{
		ActiveState->CurrentAnimation = Anim;
		ActiveState->CurrentAnimationTime = 0.0f;
		ActiveState->bIsPlaying = false;

		// 탭 이름 업데이트
		ActiveState->TabName = ExtractFileName(Path).c_str();
		ActiveState->FilePath = Path;

		if (UAnimDataModel* DataModel = Anim->GetDataModel())
		{
			int32 TotalFrames = DataModel->GetNumberOfFrames();
			ActiveState->WorkingRangeStartFrame = 0;
			ActiveState->WorkingRangeEndFrame = TotalFrames;
			ActiveState->ViewRangeStartFrame = 0;
			ActiveState->ViewRangeEndFrame = TotalFrames;
		}

		RebuildNotifyTracks(ActiveState);
	}
}

void SAnimationWindow::LoadAnimationFile(const char* FilePath)
{
	if (!FilePath || FilePath[0] == '\0')
	{
		return;
	}
	LoadAnimation(FString(FilePath));
}

void SAnimationWindow::SaveCurrentAnimation()
{
	if (!ActiveState || !ActiveState->CurrentAnimation)
	{
		UE_LOG("No active animation to save");
		return;
	}

	// FilePath가 없으면 Save As 다이얼로그 열기
	if (ActiveState->FilePath.empty())
	{
		std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
			L"Data/Animation",
			L".anim",
			L"Animation Files (*.anim)"
		);

		if (!SavePath.empty())
		{
			ActiveState->FilePath = SavePath.string();
		}
		else
		{
			return;  // 취소됨
		}
	}

	// 애니메이션 저장
	if (ActiveState->CurrentAnimation->SaveToFile(ActiveState->FilePath))
	{
		UE_LOG("Animation saved: %s", ActiveState->FilePath.c_str());
	}
	else
	{
		UE_LOG("[Error] Failed to save Animation: %s", ActiveState->FilePath.c_str());
	}
}

// ============================================================================
// Bone Transform 헬퍼
// ============================================================================

void SAnimationWindow::UpdateBoneTransformFromSkeleton(FAnimationTabState* State)
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

	const FTransform& BoneTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);
	State->EditBoneLocation = BoneTransform.Translation;
	State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
	State->EditBoneScale = BoneTransform.Scale3D;
}

void SAnimationWindow::ApplyBoneTransform(FAnimationTabState* State)
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

	FTransform NewTransform(State->EditBoneLocation,
	                        FQuat::MakeFromEulerZYX(State->EditBoneRotation),
	                        State->EditBoneScale);

	FTransform AnimTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);

	const FTransform* ExistingDelta = SkelComp->GetBoneDelta(State->SelectedBoneIndex);
	if (ExistingDelta)
	{
		AnimTransform.Translation = AnimTransform.Translation - ExistingDelta->Translation;
		AnimTransform.Rotation = (AnimTransform.Rotation * ExistingDelta->Rotation.Inverse()).GetNormalized();
		AnimTransform.Scale3D = FVector(
			ExistingDelta->Scale3D.X != 0.0f ? AnimTransform.Scale3D.X / ExistingDelta->Scale3D.X : AnimTransform.Scale3D.X,
			ExistingDelta->Scale3D.Y != 0.0f ? AnimTransform.Scale3D.Y / ExistingDelta->Scale3D.Y : AnimTransform.Scale3D.Y,
			ExistingDelta->Scale3D.Z != 0.0f ? AnimTransform.Scale3D.Z / ExistingDelta->Scale3D.Z : AnimTransform.Scale3D.Z
		);
	}

	FTransform Delta;
	Delta.Translation = NewTransform.Translation - AnimTransform.Translation;
	Delta.Rotation = AnimTransform.Rotation.IsIdentity()
		? NewTransform.Rotation
		: (AnimTransform.Rotation.Inverse() * NewTransform.Rotation).GetNormalized();
	Delta.Scale3D = FVector(
		AnimTransform.Scale3D.X != 0.0f ? NewTransform.Scale3D.X / AnimTransform.Scale3D.X : 1.0f,
		AnimTransform.Scale3D.Y != 0.0f ? NewTransform.Scale3D.Y / AnimTransform.Scale3D.Y : 1.0f,
		AnimTransform.Scale3D.Z != 0.0f ? NewTransform.Scale3D.Z / AnimTransform.Scale3D.Z : 1.0f
	);

	SkelComp->SetBoneDelta(State->SelectedBoneIndex, Delta);
	SkelComp->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SAnimationWindow::ExpandToSelectedBone(FAnimationTabState* State, int32 BoneIndex)
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
// Notify 관리
// ============================================================================

void SAnimationWindow::RebuildNotifyTracks(FAnimationTabState* State)
{
	if (!State || !State->CurrentAnimation)
	{
		return;
	}

	State->NotifyTrackNames.clear();
	State->UsedTrackNumbers.clear();

	State->NotifyTrackNames.push_back("Track 1");
	State->UsedTrackNumbers.insert(1);
}

void SAnimationWindow::ScanNotifyLibrary()
{
	AvailableNotifyClasses.clear();
	AvailableNotifyStateClasses.clear();

	std::filesystem::path NotifyDir = "Data/Scripts/Notify";
	if (std::filesystem::exists(NotifyDir))
	{
		for (const auto& Entry : std::filesystem::directory_iterator(NotifyDir))
		{
			if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
			{
				FString ClassName = Entry.path().stem().string();
				AvailableNotifyClasses.push_back(ClassName);
			}
		}
	}

	std::filesystem::path NotifyStateDir = "Data/Scripts/NotifyState";
	if (std::filesystem::exists(NotifyStateDir))
	{
		for (const auto& Entry : std::filesystem::directory_iterator(NotifyStateDir))
		{
			if (Entry.is_regular_file() && Entry.path().extension() == ".lua")
			{
				FString ClassName = Entry.path().stem().string();
				AvailableNotifyStateClasses.push_back(ClassName);
			}
		}
	}
}

void SAnimationWindow::CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState)
{
	// 구현은 PreviewWindow.cpp 참조
}

void SAnimationWindow::OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState)
{
	FString Dir = bIsNotifyState ? "Data/Scripts/NotifyState/" : "Data/Scripts/Notify/";
	FString FilePath = Dir + NotifyClassName + ".lua";

	ShellExecuteA(nullptr, "open", FilePath.c_str(), nullptr, nullptr, SW_SHOW);
}

// ============================================================================
// 렌더 타겟 관리
// ============================================================================

void SAnimationWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
	if (!Device || Width == 0 || Height == 0)
	{
		return;
	}

	ReleaseRenderTarget();

	ID3D11Device* D3DDevice = static_cast<ID3D11Device*>(Device);

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

void SAnimationWindow::ReleaseRenderTarget()
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

void SAnimationWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
	if (NewWidth == PreviewRenderTargetWidth && NewHeight == PreviewRenderTargetHeight)
	{
		return;
	}

	CreateRenderTarget(NewWidth, NewHeight);
}

void SAnimationWindow::RenderToPreviewRenderTarget()
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

	ID3D11RenderTargetView* OldRTV = nullptr;
	ID3D11DepthStencilView* OldDSV = nullptr;
	Context->OMGetRenderTargets(1, &OldRTV, &OldDSV);

	UINT NumViewports = 1;
	D3D11_VIEWPORT OldViewport;
	Context->RSGetViewports(&NumViewports, &OldViewport);

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

	ActiveState->Viewport->Resize(0, 0, PreviewRenderTargetWidth, PreviewRenderTargetHeight);

	if (ActiveState->PreviewActor && ActiveState->bBoneLinesDirty)
	{
		ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex, false, true);
		ActiveState->bBoneLinesDirty = false;
	}

	ActiveState->Client->Draw(ActiveState->Viewport);

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
