#include "pch.h"
#include "BlendSpace2DWindow.h"
#include "SplitterH.h"
#include "SplitterV.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Slate/Widgets/PlaybackControls.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Collision/AABB.h"
#include "Source/Runtime/Engine/Animation/BlendSpace2D.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimNode_BlendSpace2D.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Editor/PlatformProcess.h"

// ============================================================================
// Helper: 패널 래퍼 클래스 (SSplitter에 콜백 연결)
// ============================================================================
class SBS2DPanelWrapper : public SWindow
{
public:
	using RenderCallbackType = std::function<void()>;

	void SetRenderCallback(RenderCallbackType Callback) { RenderCallback = std::move(Callback); }

	void OnRender() override
	{
		if (RenderCallback)
		{
			RenderCallback();
		}
	}

	void OnUpdate(float DeltaSeconds) override {}
	void OnMouseMove(FVector2D MousePos) override {}
	void OnMouseDown(FVector2D MousePos, uint32 Button) override {}
	void OnMouseUp(FVector2D MousePos, uint32 Button) override {}

private:
	RenderCallbackType RenderCallback;
};

// ============================================================================
// Helper: 애니메이션 이름 추출
// ============================================================================
static FString GetDisplayNameForAnimation(UAnimSequence* InAnimation)
{
	if (!InAnimation)
	{
		return "None";
	}

	FString FilePath = InAnimation->GetFilePath();
	FString DisplayName;

	size_t HashPos = FilePath.find('#');
	if (HashPos != FString::npos)
	{
		FString FullPath = FilePath.substr(0, HashPos);
		FString AnimStackName = FilePath.substr(HashPos + 1);

		size_t LastSlash = FullPath.find_last_of("/\\");
		FString FileName = (LastSlash != FString::npos) ? FullPath.substr(LastSlash + 1) : FullPath;

		size_t DotPos = FileName.find_last_of('.');
		if (DotPos != FString::npos)
		{
			FileName = FileName.substr(0, DotPos);
		}

		DisplayName = FileName + "#" + AnimStackName;
	}
	else
	{
		size_t LastSlash = FilePath.find_last_of("/\\");
		FString FileName = (LastSlash != FString::npos) ? FilePath.substr(LastSlash + 1) : FilePath;

		size_t DotPos = FileName.find_last_of('.');
		if (DotPos != FString::npos)
		{
			FileName = FileName.substr(0, DotPos);
		}

		DisplayName = FileName;
	}

	return DisplayName;
}

// ============================================================================
// 생성자/소멸자
// ============================================================================

SBlendSpace2DWindow::SBlendSpace2DWindow()
{
}

SBlendSpace2DWindow::~SBlendSpace2DWindow()
{
	ReleaseRenderTarget();

	// SSplitter 해제 (역순 - 리프 노드부터)
	// CenterSplitter (V): Viewport / Grid
	if (CenterSplitter)
	{
		delete CenterSplitter->GetLeftOrTop();       // ViewportPanel
		delete CenterSplitter->GetRightOrBottom();   // GridPanel
		delete CenterSplitter;
		CenterSplitter = nullptr;
	}

	// RightPanelSplitter (V): RightDetail / AssetBrowser
	if (RightPanelSplitter)
	{
		delete RightPanelSplitter->GetLeftOrTop();       // RightDetailPanel
		delete RightPanelSplitter->GetRightOrBottom();   // AssetBrowserPanel
		delete RightPanelSplitter;
		RightPanelSplitter = nullptr;
	}

	// CenterRightSplitter (H): CenterSplitter / RightPanelSplitter (자식은 이미 삭제됨)
	if (CenterRightSplitter)
	{
		delete CenterRightSplitter;
		CenterRightSplitter = nullptr;
	}

	// MainSplitter (H): Details / CenterRightSplitter (자식 Splitter는 이미 삭제됨)
	if (MainSplitter)
	{
		delete MainSplitter->GetLeftOrTop();        // DetailsPanel
		delete MainSplitter;
		MainSplitter = nullptr;
	}

	// 모든 탭 정리
	for (FBlendSpace2DTabState* State : Tabs)
	{
		DestroyTabState(State);
	}
	Tabs.clear();
	ActiveState = nullptr;
	ActiveTabIndex = -1;
}

bool SBlendSpace2DWindow::Initialize(float StartX, float StartY, float Width, float Height,
                                     UWorld* InWorld, ID3D11Device* InDevice)
{
	World = InWorld;
	Device = InDevice;

	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 타임라인 아이콘 로드
	auto& RM = UResourceManager::GetInstance();
	IconGoToFront = RM.Load<UTexture>("Data/Default/Icon/Go_To_Front_24x.png");
	IconGoToFrontOff = RM.Load<UTexture>("Data/Default/Icon/Go_To_Front_24x_OFF.png");
	IconStepBackwards = RM.Load<UTexture>("Data/Default/Icon/Step_Backwards_24x.png");
	IconStepBackwardsOff = RM.Load<UTexture>("Data/Default/Icon/Step_Backwards_24x_OFF.png");
	IconPause = RM.Load<UTexture>("Data/Default/Icon/Pause_24x.png");
	IconPauseOff = RM.Load<UTexture>("Data/Default/Icon/Pause_24x_OFF.png");
	IconPlay = RM.Load<UTexture>("Data/Default/Icon/Play_24x.png");
	IconPlayOff = RM.Load<UTexture>("Data/Default/Icon/Play_24x_OFF.png");
	IconStepForward = RM.Load<UTexture>("Data/Default/Icon/Step_Forward_24x.png");
	IconStepForwardOff = RM.Load<UTexture>("Data/Default/Icon/Step_Forward_24x_OFF.png");
	IconGoToEnd = RM.Load<UTexture>("Data/Default/Icon/Go_To_End_24x.png");
	IconGoToEndOff = RM.Load<UTexture>("Data/Default/Icon/Go_To_End_24x_OFF.png");
	IconLoop = RM.Load<UTexture>("Data/Default/Icon/Loop_24x.png");
	IconLoopOff = RM.Load<UTexture>("Data/Default/Icon/Loop_24x_OFF.png");

	// === SSplitter 레이아웃 생성 (UE5 스타일) ===
	// 구조:
	// MainSplitter (H): Left(Details) | Right(CenterRightSplitter)
	//   CenterRightSplitter (H): Left(CenterSplitter) | Right(RightPanelSplitter)
	//     CenterSplitter (V): Top(Viewport) | Bottom(Grid)
	//     RightPanelSplitter (V): Top(RightDetail) | Bottom(AssetBrowser)

	// 패널 래퍼 생성
	SBS2DPanelWrapper* DetailsPanel = new SBS2DPanelWrapper();
	SBS2DPanelWrapper* ViewportPanel = new SBS2DPanelWrapper();
	SBS2DPanelWrapper* GridPanel = new SBS2DPanelWrapper();
	SBS2DPanelWrapper* RightDetailPanel = new SBS2DPanelWrapper();
	SBS2DPanelWrapper* AssetBrowserPanel = new SBS2DPanelWrapper();

	// 콜백 설정
	DetailsPanel->SetRenderCallback([this]() { RenderDetailsPanel(); });
	ViewportPanel->SetRenderCallback([this]() { RenderViewportPanel(); });
	GridPanel->SetRenderCallback([this]() { RenderGridPanel(); });
	RightDetailPanel->SetRenderCallback([this]() { RenderRightDetailPanel(); });
	AssetBrowserPanel->SetRenderCallback([this]() { RenderAssetBrowserPanel(); });

	// CenterSplitter (V): Viewport / Grid (상하 분할)
	CenterSplitter = new SSplitterV();
	CenterSplitter->SetLeftOrTop(ViewportPanel);
	CenterSplitter->SetRightOrBottom(GridPanel);
	CenterSplitter->SetSplitRatio(0.55f);  // Viewport 55%, Grid 45%
	CenterSplitter->LoadFromConfig("BS2DWindow_Center");

	// RightPanelSplitter (V): RightDetail / AssetBrowser (상하 분할)
	RightPanelSplitter = new SSplitterV();
	RightPanelSplitter->SetLeftOrTop(RightDetailPanel);
	RightPanelSplitter->SetRightOrBottom(AssetBrowserPanel);
	RightPanelSplitter->SetSplitRatio(0.5f);  // Detail 50%, AssetBrowser 50%
	RightPanelSplitter->LoadFromConfig("BS2DWindow_RightPanel");

	// CenterRightSplitter (H): CenterSplitter / RightPanelSplitter (좌우 분할)
	CenterRightSplitter = new SSplitterH();
	CenterRightSplitter->SetLeftOrTop(CenterSplitter);
	CenterRightSplitter->SetRightOrBottom(RightPanelSplitter);
	CenterRightSplitter->SetSplitRatio(0.75f);  // Center 75%, RightPanel 25%
	CenterRightSplitter->LoadFromConfig("BS2DWindow_CenterRight");

	// MainSplitter (H): Details / CenterRightSplitter (좌우 분할)
	MainSplitter = new SSplitterH();
	MainSplitter->SetLeftOrTop(DetailsPanel);
	MainSplitter->SetRightOrBottom(CenterRightSplitter);
	MainSplitter->SetSplitRatio(0.2f);  // Details 20%, CenterRight 80%
	MainSplitter->LoadFromConfig("BS2DWindow_Main");

	bRequestFocus = true;
	bIsOpen = true;
	return true;
}

// ============================================================================
// 탭 생성/삭제
// ============================================================================

FBlendSpace2DTabState* SBlendSpace2DWindow::CreateTabState(const FString& FilePath)
{
	if (!Device)
	{
		return nullptr;
	}

	FBlendSpace2DTabState* State = new FBlendSpace2DTabState();
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

void SBlendSpace2DWindow::DestroyTabState(FBlendSpace2DTabState* State)
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

FString SBlendSpace2DWindow::ExtractFileName(const FString& FilePath) const
{
	if (FilePath.empty())
	{
		return "Untitled";
	}

	size_t LastSlash = FilePath.find_last_of("/\\");
	FString FileName = (LastSlash != FString::npos) ? FilePath.substr(LastSlash + 1) : FilePath;

	size_t DotPos = FileName.find_last_of('.');
	if (DotPos != FString::npos)
	{
		FileName = FileName.substr(0, DotPos);
	}

	return FileName;
}

void SBlendSpace2DWindow::CloseTab(int32 Index)
{
	if (Index < 0 || Index >= Tabs.Num())
	{
		return;
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
		ActiveTabIndex = std::min(Index, (int32)Tabs.Num() - 1);
		ActiveState = Tabs[ActiveTabIndex];
	}
}

void SBlendSpace2DWindow::OpenNewTab(const FString& FilePath)
{
	FBlendSpace2DTabState* NewState = CreateTabState(FilePath);
	if (NewState)
	{
		NewState->TabName = FName(ExtractFileName(FilePath).c_str());
		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;

		if (!FilePath.empty())
		{
			LoadBlendSpace(FilePath);
		}
		else
		{
			// 기본 BlendSpace 생성
			UBlendSpace2D* DefaultBS = NewObject<UBlendSpace2D>();
			DefaultBS->ObjectName = FName("New BlendSpace");
			DefaultBS->XAxisMin = -100.0f;
			DefaultBS->XAxisMax = 100.0f;
			DefaultBS->YAxisMin = -100.0f;
			DefaultBS->YAxisMax = 100.0f;
			DefaultBS->XAxisName = "Speed";
			DefaultBS->YAxisName = "Direction";
			NewState->BlendSpace = DefaultBS;
			NewState->TabName = FName("New BlendSpace");

			// 프리뷰 파라미터 초기화 (중앙)
			NewState->PreviewParameter.X = 0.0f;
			NewState->PreviewParameter.Y = 0.0f;
		}
	}
}

void SBlendSpace2DWindow::OpenNewTabWithBlendSpace(UBlendSpace2D* InBlendSpace, const FString& FilePath)
{
	FBlendSpace2DTabState* NewState = CreateTabState(FilePath);
	if (NewState)
	{
		// 파일 경로가 있으면 파일명 사용, 없으면 "New BlendSpace"
		FString TabName = FilePath.empty() ? "New BlendSpace" : ExtractFileName(FilePath);
		NewState->TabName = FName(TabName.c_str());
		NewState->BlendSpace = InBlendSpace;

		if (InBlendSpace)
		{
			// 프리뷰 파라미터 초기화
			NewState->PreviewParameter.X = (InBlendSpace->XAxisMin + InBlendSpace->XAxisMax) * 0.5f;
			NewState->PreviewParameter.Y = (InBlendSpace->YAxisMin + InBlendSpace->YAxisMax) * 0.5f;

			// 저장된 스켈레톤 메시 로드
			if (!InBlendSpace->EditorSkeletalMeshPath.empty())
			{
				NewState->LoadedMeshPath = InBlendSpace->EditorSkeletalMeshPath;
				USkeletalMesh* SavedMesh = UResourceManager::GetInstance().Get<USkeletalMesh>(InBlendSpace->EditorSkeletalMeshPath);
				if (SavedMesh && NewState->PreviewActor)
				{
					NewState->PreviewActor->SetSkeletalMesh(SavedMesh->GetFilePath());
					NewState->CurrentMesh = SavedMesh;

					// 애니메이션 목록 초기화
					NewState->AvailableAnimations.clear();
					for (UAnimSequence* Anim : SavedMesh->GetAnimations())
					{
						if (Anim && Anim->GetFilePath().ends_with(".anim"))
						{
							NewState->AvailableAnimations.Add(Anim);
						}
					}
				}
			}
		}

		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;
	}
}

void SBlendSpace2DWindow::OpenNewTabWithMesh(USkeletalMesh* Mesh, const FString& MeshPath)
{
	FBlendSpace2DTabState* NewState = CreateTabState("");
	if (NewState)
	{
		// 새 BlendSpace이므로 "New BlendSpace" 사용 (mesh 이름 아님)
		NewState->TabName = FName("New BlendSpace");
		NewState->CurrentMesh = Mesh;
		NewState->LoadedMeshPath = MeshPath;

		// 기본 BlendSpace 생성
		UBlendSpace2D* DefaultBS = NewObject<UBlendSpace2D>();
		DefaultBS->ObjectName = FName("New BlendSpace");
		DefaultBS->XAxisMin = -100.0f;
		DefaultBS->XAxisMax = 100.0f;
		DefaultBS->YAxisMin = -100.0f;
		DefaultBS->YAxisMax = 100.0f;
		DefaultBS->XAxisName = "Speed";
		DefaultBS->YAxisName = "Direction";
		NewState->BlendSpace = DefaultBS;

		// 프리뷰 파라미터 초기화 (중앙)
		NewState->PreviewParameter.X = 0.0f;
		NewState->PreviewParameter.Y = 0.0f;

		if (Mesh && NewState->PreviewActor)
		{
			NewState->PreviewActor->SetSkeletalMesh(MeshPath);

			// 애니메이션 목록 초기화
			NewState->AvailableAnimations.clear();
			for (UAnimSequence* Anim : Mesh->GetAnimations())
			{
				if (Anim && Anim->GetFilePath().ends_with(".anim"))
				{
					NewState->AvailableAnimations.Add(Anim);
				}
			}
		}

		Tabs.Add(NewState);
		ActiveTabIndex = Tabs.Num() - 1;
		ActiveState = NewState;
	}
}

// ============================================================================
// 에셋 로드
// ============================================================================

void SBlendSpace2DWindow::LoadBlendSpace(const FString& Path)
{
	if (!ActiveState || Path.empty())
	{
		return;
	}

	UBlendSpace2D* LoadedBS = UBlendSpace2D::LoadFromFile(Path);
	if (LoadedBS)
	{
		ActiveState->BlendSpace = LoadedBS;
		ActiveState->FilePath = Path;
		ActiveState->TabName = FName(ExtractFileName(Path).c_str());

		// 프리뷰 파라미터 초기화
		ActiveState->PreviewParameter.X = (LoadedBS->XAxisMin + LoadedBS->XAxisMax) * 0.5f;
		ActiveState->PreviewParameter.Y = (LoadedBS->YAxisMin + LoadedBS->YAxisMax) * 0.5f;

		// 저장된 스켈레톤 메시 로드
		if (!LoadedBS->EditorSkeletalMeshPath.empty())
		{
			LoadSkeletalMesh(LoadedBS->EditorSkeletalMeshPath);
		}
	}
}

// ============================================================================
// 바닥판 및 카메라 설정
// ============================================================================

static void SetupFloorAndCamera(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}
	SkeletalViewerBootstrap::SetupFloorAndCamera(State->PreviewActor, State->FloorActor, State->Client);
}

void SBlendSpace2DWindow::LoadSkeletalMesh(const FString& Path)
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

		// 스켈레탈 컴포넌트 설정
		if (USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
		{
			SkelComp->SetVisibility(true);
		}

		// 애니메이션 목록 초기화
		ActiveState->AvailableAnimations.clear();
		for (UAnimSequence* Anim : Mesh->GetAnimations())
		{
			if (Anim && Anim->GetFilePath().ends_with(".anim"))
			{
				ActiveState->AvailableAnimations.Add(Anim);
			}
		}

		// BlendSpace에 경로 저장
		if (ActiveState->BlendSpace)
		{
			ActiveState->BlendSpace->EditorSkeletalMeshPath = Path;
		}

		// 바닥판 및 카메라 설정
		SetupFloorAndCamera(ActiveState);
	}
}

void SBlendSpace2DWindow::LoadBlendSpaceFile(const char* FilePath)
{
	if (!FilePath || FilePath[0] == '\0')
	{
		return;
	}

	FString PathStr(FilePath);

	// Embedded 모드에서 이미 같은 파일이 로드되어 있으면 스킵
	if (bIsEmbeddedMode && ActiveState && ActiveState->FilePath == PathStr)
	{
		return;
	}

	LoadBlendSpace(PathStr);
}

void SBlendSpace2DWindow::CreateNewEmptyTab()
{
	OpenNewTab("");  // 빈 경로로 호출하면 기본 BlendSpace 생성
}

void SBlendSpace2DWindow::SaveCurrentBlendSpace()
{
	if (!ActiveState || !ActiveState->BlendSpace)
	{
		UE_LOG("No active BlendSpace to save");
		return;
	}

	// FilePath가 없으면 Save As 다이얼로그 열기
	if (ActiveState->FilePath.empty())
	{
		std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
			L"Data/BlendSpace",
			L".blend2d",
			L"BlendSpace Files (*.blend2d)"
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

	// BlendSpace 저장
	if (ActiveState->BlendSpace->SaveToFile(ActiveState->FilePath))
	{
		UE_LOG("BlendSpace saved: %s", ActiveState->FilePath.c_str());
	}
	else
	{
		UE_LOG("BlendSpace: Save: Failed %s", ActiveState->FilePath.c_str());
	}
}

// ============================================================================
// Accessors
// ============================================================================

FViewport* SBlendSpace2DWindow::GetViewport() const
{
	return ActiveState ? ActiveState->Viewport : nullptr;
}

FViewportClient* SBlendSpace2DWindow::GetViewportClient() const
{
	return ActiveState ? ActiveState->Client : nullptr;
}

AGizmoActor* SBlendSpace2DWindow::GetGizmoActor() const
{
	if (ActiveState && ActiveState->World)
	{
		return ActiveState->World->GetGizmoActor();
	}
	return nullptr;
}

// ============================================================================
// OnRender
// ============================================================================

void SBlendSpace2DWindow::OnRender()
{
	if (!bIsOpen)
	{
		return;
	}

	// 내장 모드에서는 별도 윈도우 생성하지 않음
	if (bIsEmbeddedMode)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

	if (!bInitialPlacementDone)
	{
		ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
		float WindowWidth = DisplaySize.x * 0.85f;
		float WindowHeight = DisplaySize.y * 0.85f;
		float PosX = (DisplaySize.x - WindowWidth) * 0.5f;
		float PosY = (DisplaySize.y - WindowHeight) * 0.5f;

		ImGui::SetNextWindowPos(ImVec2(PosX, PosY));
		ImGui::SetNextWindowSize(ImVec2(WindowWidth, WindowHeight));
		bInitialPlacementDone = true;
	}

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
		bRequestFocus = false;
	}

	if (ImGui::Begin("BlendSpace 2D Editor", &bIsOpen, flags))
	{
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 탭 바 렌더링
		bool bTabClosed = false;
		if (ImGui::BeginTabBar("BlendSpace2DTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		{
			for (int32 i = 0; i < Tabs.Num(); ++i)
			{
				FBlendSpace2DTabState* State = Tabs[i];
				bool open = true;

				char tabLabel[256];
				sprintf_s(tabLabel, "%s###BS2DTab_%d", State->TabName.ToString().c_str(), State->TabId);

				if (ImGui::BeginTabItem(tabLabel, &open))
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

			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
			{
				OpenNewTab("");
			}

			ImGui::EndTabBar();
		}

		if (bTabClosed || !ActiveState)
		{
			ImGui::End();
			return;
		}

		// 콘텐츠 렌더링
		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		ImVec2 contentMin = ImGui::GetCursorScreenPos();

		FRect ContentRect;
		ContentRect.Left = contentMin.x;
		ContentRect.Top = contentMin.y;
		ContentRect.Right = contentMin.x + contentAvail.x;
		ContentRect.Bottom = contentMin.y + contentAvail.y;

		RenderEmbedded(ContentRect);
	}
	ImGui::End();
}

void SBlendSpace2DWindow::RenderEmbedded(const FRect& ContentRect)
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
		// MainSplitter: Details / CenterRightSplitter (좌우 분할)
		if (MainSplitter->GetLeftOrTop())
		{
			DetailsRect = MainSplitter->GetLeftOrTop()->GetRect();
		}

		// CenterSplitter: Viewport / Grid (상하 분할)
		if (CenterSplitter)
		{
			if (CenterSplitter->GetLeftOrTop())
			{
				ViewportRect = CenterSplitter->GetLeftOrTop()->GetRect();
			}
			if (CenterSplitter->GetRightOrBottom())
			{
				GridRect = CenterSplitter->GetRightOrBottom()->GetRect();
			}
		}

		// RightPanelSplitter: RightDetail / AssetBrowser (상하 분할)
		if (RightPanelSplitter)
		{
			if (RightPanelSplitter->GetLeftOrTop())
			{
				RightDetailRect = RightPanelSplitter->GetLeftOrTop()->GetRect();
			}
			if (RightPanelSplitter->GetRightOrBottom())
			{
				AssetBrowserRect = RightPanelSplitter->GetRightOrBottom()->GetRect();
			}
		}
	}

	// 키보드 입력 처리
	HandleKeyboardInput(ActiveState);
}

// ============================================================================
// OnUpdate
// ============================================================================

void SBlendSpace2DWindow::OnUpdate(float DeltaSeconds)
{
	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// Client 틱 (카메라 입력 처리)
	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
	}

	// BlendSpace2D 애니메이션 블렌딩 및 재생
	if (ActiveState->BlendSpace && ActiveState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp && ActiveState->BlendSpace->GetNumSamples() > 0)
		{
			// AnimInstance 생성 (없으면)
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (!AnimInst)
			{
				AnimInst = NewObject<UAnimInstance>();
				if (AnimInst)
				{
					SkelComp->SetAnimInstance(AnimInst);
				}
			}

			if (AnimInst)
			{
				// BlendSpace2D 노드 설정
				FAnimNode_BlendSpace2D* BlendNode = AnimInst->GetBlendSpace2DNode();
				if (BlendNode)
				{
					// BlendSpace 설정 (최초 1회)
					if (BlendNode->GetBlendSpace() != ActiveState->BlendSpace)
					{
						BlendNode->SetBlendSpace(ActiveState->BlendSpace);
						BlendNode->SetAutoCalculateParameter(false);  // 수동으로 파라미터 설정

						// Initialize 호출하여 AnimInstance와 MeshComp 설정 (Notify 트리거링용)
						BlendNode->Initialize(nullptr, AnimInst, SkelComp);
					}

					// 현재 PreviewParameter 설정
					BlendNode->SetBlendParameter(ActiveState->PreviewParameter);

					// 재생/일시정지/루프 상태 동기화
					BlendNode->SetPaused(!ActiveState->bIsPlaying);
					BlendNode->SetLoop(ActiveState->bLoopAnimation);
				}
			}
		}
	}

	// PlaybackSpeed를 World TimeDilation으로 적용 (레거시와 동일)
	// 일시정지는 AnimInstance에서 제어하고, TimeDilation은 항상 PlaybackSpeed 유지
	if (ActiveState->World)
	{
		ActiveState->World->SetTimeDilation(ActiveState->PlaybackSpeed);
	}

	// World 틱 (마지막에 호출 - BlendSpace 설정 후 Actor 업데이트)
	if (ActiveState->World)
	{
		ActiveState->World->Tick(DeltaSeconds);
		if (ActiveState->World->GetGizmoActor())
		{
			ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
		}
	}
}

// ============================================================================
// 마우스 입력
// ============================================================================

void SBlendSpace2DWindow::OnMouseMove(FVector2D MousePos)
{
	// SSplitter 마우스 이벤트 전달
	if (MainSplitter)
	{
		MainSplitter->OnMouseMove(MousePos);
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

	// 뷰포트 영역 내에서만 처리
	if (bGizmoDragging || ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
	}
}

void SBlendSpace2DWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	// SSplitter 마우스 이벤트 전달
	if (MainSplitter)
	{
		MainSplitter->OnMouseDown(MousePos, Button);
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

void SBlendSpace2DWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	// SSplitter 마우스 이벤트 전달
	if (MainSplitter)
	{
		MainSplitter->OnMouseUp(MousePos, Button);
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

// ============================================================================
// 렌더 타겟 관리
// ============================================================================

void SBlendSpace2DWindow::CreateRenderTarget(uint32 Width, uint32 Height)
{
	if (!Device || Width == 0 || Height == 0)
	{
		return;
	}

	ReleaseRenderTarget();

	// 렌더 타겟 텍스처 생성
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = Width;
	texDesc.Height = Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&texDesc, nullptr, &PreviewRenderTargetTexture);
	if (FAILED(hr))
	{
		return;
	}

	// 렌더 타겟 뷰 생성
	hr = Device->CreateRenderTargetView(PreviewRenderTargetTexture, nullptr, &PreviewRenderTargetView);
	if (FAILED(hr))
	{
		ReleaseRenderTarget();
		return;
	}

	// 셰이더 리소스 뷰 생성
	hr = Device->CreateShaderResourceView(PreviewRenderTargetTexture, nullptr, &PreviewShaderResourceView);
	if (FAILED(hr))
	{
		ReleaseRenderTarget();
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
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = Device->CreateTexture2D(&depthDesc, nullptr, &PreviewDepthStencilTexture);
	if (FAILED(hr))
	{
		ReleaseRenderTarget();
		return;
	}

	hr = Device->CreateDepthStencilView(PreviewDepthStencilTexture, nullptr, &PreviewDepthStencilView);
	if (FAILED(hr))
	{
		ReleaseRenderTarget();
		return;
	}

	PreviewRenderTargetWidth = Width;
	PreviewRenderTargetHeight = Height;
}

void SBlendSpace2DWindow::ReleaseRenderTarget()
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

void SBlendSpace2DWindow::UpdateViewportRenderTarget(uint32 NewWidth, uint32 NewHeight)
{
	if (NewWidth == PreviewRenderTargetWidth && NewHeight == PreviewRenderTargetHeight)
	{
		return;
	}

	CreateRenderTarget(NewWidth, NewHeight);
}

void SBlendSpace2DWindow::RenderToPreviewRenderTarget()
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

	// 레거시와 동일하게 본 라인 렌더링 없음
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

// ============================================================================
// 패널 렌더링
// ============================================================================

void SBlendSpace2DWindow::RenderViewportPanel()
{
	if (!ActiveState)
	{
		return;
	}

	ImGui::SetCursorScreenPos(ImVec2(ViewportRect.Left, ViewportRect.Top));
	ImGui::BeginChild("ViewportPanel", ImVec2(ViewportRect.GetWidth(), ViewportRect.GetHeight()), true, ImGuiWindowFlags_NoScrollbar);
	{
		// 뷰포트 렌더링 영역
		ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
		uint32 NewWidth = static_cast<uint32>(ViewportSize.x);
		uint32 NewHeight = static_cast<uint32>(ViewportSize.y);

		if (NewWidth > 0 && NewHeight > 0)
		{
			UpdateViewportRenderTarget(NewWidth, NewHeight);
			RenderToPreviewRenderTarget();

			ID3D11ShaderResourceView* PreviewSRV = GetPreviewShaderResourceView();
			if (PreviewSRV)
			{
				ImTextureID TextureID = reinterpret_cast<ImTextureID>(PreviewSRV);
				ImGui::Image(TextureID, ViewportSize);
			}
		}

		// 드래그 앤 드롭 타겟 (Content Browser에서 파일 드롭)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				const char* filePath = static_cast<const char*>(payload->Data);
				FString path(filePath);
				FString lowerPath = path;
				std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

				if (lowerPath.ends_with(".blend2d"))
				{
					LoadBlendSpaceFile(filePath);
				}
				else if (lowerPath.ends_with(".fbx"))
				{
					LoadSkeletalMesh(path);
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::EndChild();
}

void SBlendSpace2DWindow::RenderDetailsPanel()
{
	if (!ActiveState)
	{
		return;
	}

	ImGui::SetCursorScreenPos(ImVec2(DetailsRect.Left, DetailsRect.Top));
	ImGui::BeginChild("DetailsPanel", ImVec2(DetailsRect.GetWidth(), DetailsRect.GetHeight()), true);
	{
		UBlendSpace2D* BlendSpace = ActiveState->BlendSpace;

		if (ActiveState->SelectedSampleIndex >= 0 && BlendSpace &&
			ActiveState->SelectedSampleIndex < BlendSpace->GetNumSamples())
		{
			// 샘플 속성 표시
			ImGui::Text("Sample Properties [%d]", ActiveState->SelectedSampleIndex);
			ImGui::Separator();

			FBlendSample& Sample = BlendSpace->Samples[ActiveState->SelectedSampleIndex];

			// 위치
			ImGui::Text("Position");
			ImGui::InputFloat("X##SamplePosX", &Sample.Position.X);
			ImGui::InputFloat("Y##SamplePosY", &Sample.Position.Y);

			ImGui::Separator();

			// RateScale
			ImGui::Text("Playback");
			ImGui::SliderFloat("Rate Scale", &Sample.RateScale, 0.1f, 3.0f, "%.2f");
			ImGui::SameLine();
			if (ImGui::Button("Reset##RateScale"))
			{
				Sample.RateScale = 1.0f;
			}

			ImGui::Separator();

			// 애니메이션 정보
			ImGui::Text("Animation");
			if (Sample.Animation)
			{
				ImGui::Text("Name: %s", Sample.Animation->GetName().c_str());
				ImGui::Text("Duration: %.2fs", Sample.Animation->GetPlayLength());
				ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
					"Effective: %.2fs (with RateScale)",
					Sample.Animation->GetPlayLength() / Sample.RateScale);

				// Sync Markers 섹션
				ImGui::Separator();
				const TArray<FAnimSyncMarker>& Markers = Sample.Animation->GetSyncMarkers();
				ImGui::Text("Sync Markers (%d)", Markers.Num());

				// Sync Marker 목록
				for (int32 m = 0; m < Markers.Num(); ++m)
				{
					const FAnimSyncMarker& Marker = Markers[m];
					ImGui::Text("  [%d] %s @ %.3fs", m, Marker.MarkerName.c_str(), Marker.Time);

					ImGui::SameLine();
					char DeleteLabel[64];
					sprintf_s(DeleteLabel, "X##Marker%d", m);
					if (ImGui::SmallButton(DeleteLabel))
					{
						Sample.Animation->RemoveSyncMarker(m);
					}
				}

				// Sync Marker 추가
				static char MarkerNameBuffer[128] = "LeftFoot";
				static float MarkerTime = 0.0f;

				ImGui::Separator();
				ImGui::Text("Add Sync Marker");
				ImGui::InputText("Marker Name##AddMarker", MarkerNameBuffer, 128);
				ImGui::InputFloat("Time (sec)##AddMarker", &MarkerTime, 0.1f, 1.0f, "%.3f");

				if (ImGui::Button("Add Marker"))
				{
					Sample.Animation->AddSyncMarker(MarkerNameBuffer, MarkerTime);
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Animation");
			}
		}
		else if (BlendSpace)
		{
			// BlendSpace 전체 속성 표시
			ImGui::Text("Blend Space Properties");
			ImGui::Separator();

			// 축 이름 편집
			static char XAxisNameBuffer[128];
			static char YAxisNameBuffer[128];

			if (BlendSpace->XAxisName.length() < 128)
			{
				strcpy_s(XAxisNameBuffer, BlendSpace->XAxisName.c_str());
			}
			if (BlendSpace->YAxisName.length() < 128)
			{
				strcpy_s(YAxisNameBuffer, BlendSpace->YAxisName.c_str());
			}

			if (ImGui::InputText("X Axis", XAxisNameBuffer, 128))
			{
				BlendSpace->XAxisName = XAxisNameBuffer;
			}
			if (ImGui::InputText("Y Axis", YAxisNameBuffer, 128))
			{
				BlendSpace->YAxisName = YAxisNameBuffer;
			}

			ImGui::Separator();

			// 축 범위
			ImGui::Text("Axis Ranges");
			ImGui::InputFloat("X Min", &BlendSpace->XAxisMin);
			ImGui::InputFloat("X Max", &BlendSpace->XAxisMax);
			ImGui::InputFloat("Y Min", &BlendSpace->YAxisMin);
			ImGui::InputFloat("Y Max", &BlendSpace->YAxisMax);

			ImGui::Separator();

			// 축별 블렌드 가중치
			ImGui::Text("Axis Blend Weights");
			ImGui::SliderFloat("X Weight", &BlendSpace->XAxisBlendWeight, 0.1f, 3.0f, "%.2f");
			ImGui::SliderFloat("Y Weight", &BlendSpace->YAxisBlendWeight, 0.1f, 3.0f, "%.2f");
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
				"Higher = more important");

			ImGui::Separator();

			// Sync Group 설정
			ImGui::Text("Sync Group");
			ImGui::Checkbox("Use Sync Markers", &BlendSpace->bUseSyncMarkers);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
				"Sync animations using markers");

			// Sync Group 이름
			static char SyncGroupBuffer[128] = "Default";
			if (BlendSpace->SyncGroupName.length() < 128)
			{
				strcpy_s(SyncGroupBuffer, BlendSpace->SyncGroupName.c_str());
			}

			if (ImGui::InputText("Group Name", SyncGroupBuffer, 128))
			{
				BlendSpace->SyncGroupName = SyncGroupBuffer;
			}
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
				"Animations in same group sync together");

			ImGui::Separator();

			// 삼각분할 정보
			ImGui::Text("Triangulation");
			ImGui::Text("Triangles: %d", BlendSpace->Triangles.Num());
			if (ImGui::Button("Regenerate"))
			{
				BlendSpace->GenerateTriangulation();
			}

			ImGui::Separator();

			// 통계
			ImGui::Text("Statistics");
			ImGui::Text("Samples: %d", BlendSpace->GetNumSamples());
		}
		else
		{
			ImGui::Text("No Blend Space loaded");
		}
	}
	ImGui::EndChild();
}

void SBlendSpace2DWindow::RenderGridPanel()
{
	if (!ActiveState)
	{
		return;
	}

	ImGui::SetCursorScreenPos(ImVec2(GridRect.Left, GridRect.Top));
	ImGui::BeginChild("GridPanel", ImVec2(GridRect.GetWidth(), GridRect.GetHeight()), true, ImGuiWindowFlags_NoScrollbar);
	{
		UBlendSpace2D* BlendSpace = ActiveState->BlendSpace;

		// Validation Warnings만 표시 (축 정보는 툴팁으로 이동)
		if (BlendSpace)
		{
			TArray<FString> Warnings;

			// 1. 샘플이 너무 가까운지 체크
			for (int32 i = 0; i < BlendSpace->GetNumSamples(); ++i)
			{
				for (int32 j = i + 1; j < BlendSpace->GetNumSamples(); ++j)
				{
					FVector2D Pos1 = BlendSpace->Samples[i].Position;
					FVector2D Pos2 = BlendSpace->Samples[j].Position;
					float Distance = sqrtf((Pos2.X - Pos1.X) * (Pos2.X - Pos1.X) + (Pos2.Y - Pos1.Y) * (Pos2.Y - Pos1.Y));

					if (Distance < 5.0f)
					{
						char WarningMsg[256];
						sprintf_s(WarningMsg, "Samples %d and %d are too close (%.1f)", i, j, Distance);
						Warnings.Add(WarningMsg);
					}
				}
			}

			// 2. 삼각분할 실패 체크
			if (BlendSpace->GetNumSamples() >= 3 && BlendSpace->Triangles.Num() == 0)
			{
				Warnings.Add("Triangulation failed! Check sample positions.");
			}

			// 경고 표시
			if (Warnings.Num() > 0)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.0f, 1.0f));
				for (const FString& Warning : Warnings)
				{
					ImGui::TextWrapped("%s", Warning.c_str());
				}
				ImGui::PopStyleColor();
				ImGui::Separator();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "No BlendSpace - use New/Load");
			ImGui::Separator();
		}

		// 캔버스 영역 계산 (BlendSpace 유무와 관계없이 항상 표시)
		ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
		const float LeftMargin = 50.0f;
		const float BottomMargin = 40.0f;
		const float RightMargin = 10.0f;
		const float TopMargin = 10.0f;
		const float TimelineHeight = 50.0f;

		ActiveState->CanvasPos = ImGui::GetCursorScreenPos();
		ActiveState->CanvasPos.x += LeftMargin;
		ActiveState->CanvasPos.y += TopMargin;
		ActiveState->CanvasSize = ImVec2(
			AvailableSize.x - LeftMargin - RightMargin,
			AvailableSize.y - TopMargin - BottomMargin - TimelineHeight
		);

		// 블렌딩 정보 가져오기
		TArray<int32> CurrentSampleIndices;
		TArray<float> CurrentWeights;
		int32 ActiveTriangle = -1;

		if (BlendSpace && BlendSpace->GetNumSamples() >= 3)
		{
			BlendSpace->GetBlendWeights(ActiveState->PreviewParameter, CurrentSampleIndices, CurrentWeights);

			// 활성 삼각형 찾기
			if (CurrentSampleIndices.Num() == 3)
			{
				for (int32 i = 0; i < BlendSpace->Triangles.Num(); ++i)
				{
					const FBlendTriangle& Tri = BlendSpace->Triangles[i];
					bool bHasV0 = CurrentSampleIndices.Contains(Tri.Index0);
					bool bHasV1 = CurrentSampleIndices.Contains(Tri.Index1);
					bool bHasV2 = CurrentSampleIndices.Contains(Tri.Index2);

					if (bHasV0 && bHasV1 && bHasV2)
					{
						ActiveTriangle = i;
						break;
					}
				}
			}
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		// 캔버스 클리핑 영역 설정
		ImVec2 CanvasMin = ActiveState->CanvasPos;
		ImVec2 CanvasMax = ImVec2(ActiveState->CanvasPos.x + ActiveState->CanvasSize.x,
		                          ActiveState->CanvasPos.y + ActiveState->CanvasSize.y);

		// 캔버스 배경
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(30, 30, 30, 255));

		// 클리핑 시작 (캔버스 영역 외부는 렌더링하지 않음)
		DrawList->PushClipRect(CanvasMin, CanvasMax, true);

		// 그리드 렌더링 (항상 표시)
		RenderGrid(ActiveState);

		// BlendSpace가 있을 때만 삼각분할, 샘플포인트, 입력 처리
		if (BlendSpace)
		{
			// 삼각분할 렌더링
			RenderTriangulation_Enhanced(ActiveState, ActiveTriangle);

			// 샘플 포인트 렌더링
			RenderSamplePoints_Enhanced(ActiveState, CurrentSampleIndices, CurrentWeights);

			// 입력 처리
			HandleGridMouseInput(ActiveState);
		}

		// 프리뷰 마커 렌더링 (항상 표시)
		RenderPreviewMarker(ActiveState);

		// 클리핑 종료
		DrawList->PopClipRect();

		// 축 라벨 렌더링 (클리핑 외부 - 축 라벨은 캔버스 밖에 표시)
		RenderAxisLabels(ActiveState);

		// 캔버스 영역 확보 및 드롭 타겟 설정
		ImGui::SetCursorScreenPos(CanvasMin);
		ImGui::InvisibleButton("GridCanvas", ActiveState->CanvasSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

		// 드롭 타겟 - 애니메이션을 그리드에 드롭하면 샘플 추가
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("ANIM_SEQUENCE_PTR"))
			{
				UAnimSequence* DroppedAnim = *(UAnimSequence**)Payload->Data;
				if (DroppedAnim && BlendSpace)
				{
					// 드롭된 위치를 파라미터 좌표로 변환
					ImVec2 MousePos = ImGui::GetMousePos();
					FVector2D DropPos = ScreenToParam(ActiveState, MousePos);

					// 축 범위 내로 클램프
					DropPos.X = FMath::Clamp(DropPos.X, BlendSpace->XAxisMin, BlendSpace->XAxisMax);
					DropPos.Y = FMath::Clamp(DropPos.Y, BlendSpace->YAxisMin, BlendSpace->YAxisMax);

					// 그리드 스냅 적용
					if (ActiveState->bEnableGridSnapping)
					{
						DropPos.X = roundf(DropPos.X / ActiveState->GridSnapSize) * ActiveState->GridSnapSize;
						DropPos.Y = roundf(DropPos.Y / ActiveState->GridSnapSize) * ActiveState->GridSnapSize;
					}

					// 새 샘플 추가
					FBlendSample NewSample;
					NewSample.Position = DropPos;
					NewSample.Animation = DroppedAnim;
					NewSample.RateScale = 1.0f;
					BlendSpace->Samples.Add(NewSample);

					// 삼각분할 갱신
					BlendSpace->GenerateTriangulation();

					// 새로 추가된 샘플 선택
					ActiveState->SelectedSampleIndex = BlendSpace->Samples.Num() - 1;
				}
			}
			ImGui::EndDragDropTarget();
		}

		// 나머지 캔버스 영역 (타임라인 위 공간)
		ImGui::SetCursorScreenPos(ImVec2(CanvasMin.x, CanvasMax.y));
		ImGui::Dummy(ImVec2(ActiveState->CanvasSize.x, AvailableSize.y - ActiveState->CanvasSize.y - TimelineHeight - TopMargin));

		// 타임라인 컨트롤
		ImGui::SetCursorScreenPos(ImVec2(GridRect.Left + 5.0f, GridRect.Bottom - TimelineHeight - 5.0f));
		ImGui::BeginChild("TimelineControls", ImVec2(GridRect.GetWidth() - 10.0f, TimelineHeight), true);
		{
			RenderTimelineControls(ActiveState);
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
}

// ============================================================================
// 우측 패널 렌더링
// ============================================================================

void SBlendSpace2DWindow::RenderRightDetailPanel()
{
	if (!ActiveState)
	{
		return;
	}

	ImGui::SetCursorScreenPos(ImVec2(RightDetailRect.Left, RightDetailRect.Top));
	ImGui::BeginChild("RightDetailPanel", ImVec2(RightDetailRect.GetWidth(), RightDetailRect.GetHeight()), true);
	{
		ImGui::Text("Details");
		ImGui::Separator();

		// Asset Browser에서 선택된 애니메이션 정보 표시
		TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		TArray<UAnimSequence*> Animations;
		for (UAnimSequence* Anim : AllAnimations)
		{
			if (Anim && Anim->GetFilePath().ends_with(".anim"))
			{
				Animations.Add(Anim);
			}
		}

		if (ActiveState->SelectedAnimationIndex >= 0 && ActiveState->SelectedAnimationIndex < Animations.Num())
		{
			UAnimSequence* SelectedAnim = Animations[ActiveState->SelectedAnimationIndex];
			if (SelectedAnim)
			{
				ImGui::Text("Selected Animation");
				ImGui::Separator();

				// 기본 정보
				ImGui::Text("Name: %s", SelectedAnim->GetName().c_str());
				ImGui::Text("Duration: %.2f sec", SelectedAnim->GetPlayLength());
				ImGui::Text("Frames: %d", SelectedAnim->GetDataModel() ? SelectedAnim->GetDataModel()->GetNumberOfFrames() : 0);

				ImGui::Separator();

				// 파일 경로
				ImGui::Text("Path:");
				ImGui::TextWrapped("%s", SelectedAnim->GetFilePath().c_str());

				ImGui::Separator();

				// Sync Markers
				const TArray<FAnimSyncMarker>& Markers = SelectedAnim->GetSyncMarkers();
				if (ImGui::CollapsingHeader("Sync Markers", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (Markers.Num() > 0)
					{
						for (int32 i = 0; i < Markers.Num(); ++i)
						{
							const FAnimSyncMarker& Marker = Markers[i];
							ImGui::Text("[%d] %s @ %.3fs", i, Marker.MarkerName.c_str(), Marker.Time);
						}
					}
					else
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No sync markers");
					}
				}

				ImGui::Separator();

				// 힌트
				ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Drag to Grid to add sample");
			}
		}
		else
		{
			// Preview 정보
			ImGui::Text("Preview Info");
			ImGui::Separator();

			if (ActiveState->CurrentMesh)
			{
				ImGui::Text("Mesh: %s", ActiveState->CurrentMesh->GetName().c_str());
				ImGui::Text("Bones: %d", ActiveState->CurrentMesh->GetBoneCount());
			}
			else
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No mesh loaded");
			}

			UBlendSpace2D* BlendSpace = ActiveState->BlendSpace;
			if (BlendSpace)
			{
				ImGui::Separator();
				ImGui::Text("Samples: %d", BlendSpace->GetNumSamples());
				ImGui::Text("Triangles: %d", BlendSpace->Triangles.Num());
			}

			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select animation in Asset Browser");
		}
	}
	ImGui::EndChild();
}

void SBlendSpace2DWindow::RenderAssetBrowserPanel()
{
	if (!ActiveState)
	{
		return;
	}

	ImGui::SetCursorScreenPos(ImVec2(AssetBrowserRect.Left, AssetBrowserRect.Top));
	ImGui::BeginChild("AssetBrowserPanel", ImVec2(AssetBrowserRect.GetWidth(), AssetBrowserRect.GetHeight()), true);
	{
		ImGui::Text("Asset Browser");
		ImGui::Separator();

		// 애니메이션 목록 표시
		TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		TArray<UAnimSequence*> Animations;
		for (UAnimSequence* Anim : AllAnimations)
		{
			if (Anim && Anim->GetFilePath().ends_with(".anim"))
			{
				Animations.Add(Anim);
			}
		}

		if (Animations.Num() > 0)
		{
			ImGui::Text("Animations (%d)", Animations.Num());
			ImGui::Separator();

			ImGui::BeginChild("AnimAssetList", ImVec2(0, 0), false);
			{
				for (int32 i = 0; i < Animations.Num(); ++i)
				{
					UAnimSequence* Anim = Animations[i];
					if (!Anim)
					{
						continue;
					}

					FString DisplayName = Anim->GetName();
					if (DisplayName.empty())
					{
						DisplayName = "Anim " + std::to_string(i);
					}

					char LabelBuffer[256];
					sprintf_s(LabelBuffer, "%s (%.1fs)##AnimAsset_%d",
						DisplayName.c_str(), Anim->GetPlayLength(), i);

					// 선택 가능한 항목
					bool bIsSelected = (ActiveState->SelectedAnimationIndex == i);
					if (ImGui::Selectable(LabelBuffer, bIsSelected))
					{
						ActiveState->SelectedAnimationIndex = i;
					}

					// 드래그 소스 (그리드에 드롭하여 샘플 추가)
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
					{
						// UAnimSequence 포인터를 전달
						ImGui::SetDragDropPayload("ANIM_SEQUENCE_PTR", &Anim, sizeof(UAnimSequence*));
						ImGui::Text("Drop on grid: %s", DisplayName.c_str());
						ImGui::EndDragDropSource();
					}

					// 툴팁
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Name: %s", DisplayName.c_str());
						ImGui::Text("Duration: %.2f seconds", Anim->GetPlayLength());
						ImGui::Text("Path: %s", Anim->GetFilePath().c_str());
						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f),
							"Drag to grid to add sample");
						ImGui::EndTooltip();
					}
				}
			}
			ImGui::EndChild();
		}
		else
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No animations loaded");
			ImGui::Spacing();
			ImGui::TextWrapped("Load .anim files or FBX with animations");
		}
	}
	ImGui::EndChild();
}

// ============================================================================
// 그리드 렌더링 헬퍼
// ============================================================================

void SBlendSpace2DWindow::RenderGrid(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	UBlendSpace2D* BS = State->BlendSpace;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 파라미터 범위 계산
	float XRange = BS->XAxisMax - BS->XAxisMin;
	float YRange = BS->YAxisMax - BS->YAxisMin;

	// 현재 뷰에서 보이는 파라미터 범위 계산 (패닝/줌 반영)
	// X축: 왼쪽에서 오른쪽으로 증가
	float VisibleXMin = BS->XAxisMin - State->PanOffset.X * XRange;
	float VisibleXMax = BS->XAxisMax - State->PanOffset.X * XRange + XRange * (1.0f / State->ZoomLevel - 1.0f);
	// Y축: 화면상 위쪽이 YMax, 아래쪽이 YMin (화면 좌표계 반전)
	float VisibleYMax = BS->YAxisMax - State->PanOffset.Y * YRange;
	float VisibleYMin = BS->YAxisMin - State->PanOffset.Y * YRange - YRange * (1.0f / State->ZoomLevel - 1.0f);

	// 캔버스 크기에 따라 그리드 간격 동적 계산
	int NumGridLinesX = std::max(4, std::min(12, static_cast<int>(State->CanvasSize.x / 40.0f)));
	int NumGridLinesY = std::max(4, std::min(12, static_cast<int>(State->CanvasSize.y / 40.0f)));

	// 깔끔한 간격 계산
	auto CalcNiceStep = [](float range, int numLines) -> float {
		float roughStep = range / numLines;
		float magnitude = powf(10.0f, floorf(log10f(roughStep)));
		float normalized = roughStep / magnitude;

		float niceStep;
		if (normalized < 1.5f) niceStep = 1.0f;
		else if (normalized < 3.0f) niceStep = 2.0f;
		else if (normalized < 7.0f) niceStep = 5.0f;
		else niceStep = 10.0f;

		return niceStep * magnitude;
	};

	float XStep = CalcNiceStep(XRange, NumGridLinesX);
	float YStep = CalcNiceStep(YRange, NumGridLinesY);

	// X축 그리드 라인 시작점 (보이는 범위 바깥에서 시작)
	float XGridStart = floorf(VisibleXMin / XStep) * XStep;
	float YGridStart = floorf(VisibleYMin / YStep) * YStep;

	// X축 그리드 라인 그리기
	for (float ParamX = XGridStart; ParamX <= VisibleXMax + XStep; ParamX += XStep)
	{
		ImVec2 ScreenPos = ParamToScreen(State, FVector2D(ParamX, BS->YAxisMin));

		// 캔버스 범위 내에만 그리기
		if (ScreenPos.x >= State->CanvasPos.x && ScreenPos.x <= State->CanvasPos.x + State->CanvasSize.x)
		{
			ImVec2 p1(ScreenPos.x, State->CanvasPos.y);
			ImVec2 p2(ScreenPos.x, State->CanvasPos.y + State->CanvasSize.y);
			DrawList->AddLine(p1, p2, GridColor, 1.0f);
		}
	}

	// Y축 그리드 라인 그리기
	for (float ParamY = YGridStart; ParamY <= VisibleYMax + YStep; ParamY += YStep)
	{
		ImVec2 ScreenPos = ParamToScreen(State, FVector2D(BS->XAxisMin, ParamY));

		// 캔버스 범위 내에만 그리기
		if (ScreenPos.y >= State->CanvasPos.y && ScreenPos.y <= State->CanvasPos.y + State->CanvasSize.y)
		{
			ImVec2 p1(State->CanvasPos.x, ScreenPos.y);
			ImVec2 p2(State->CanvasPos.x + State->CanvasSize.x, ScreenPos.y);
			DrawList->AddLine(p1, p2, GridColor, 1.0f);
		}
	}

	// 원점 축 그리기 (0,0이 보이는 경우에만)
	ImVec2 OriginScreen = ParamToScreen(State, FVector2D(0.0f, 0.0f));

	// X=0 수직선 (중앙축)
	if (OriginScreen.x >= State->CanvasPos.x && OriginScreen.x <= State->CanvasPos.x + State->CanvasSize.x)
	{
		ImVec2 p1(OriginScreen.x, State->CanvasPos.y);
		ImVec2 p2(OriginScreen.x, State->CanvasPos.y + State->CanvasSize.y);
		DrawList->AddLine(p1, p2, AxisColor, 2.0f);
	}

	// Y=0 수평선 (중앙축)
	if (OriginScreen.y >= State->CanvasPos.y && OriginScreen.y <= State->CanvasPos.y + State->CanvasSize.y)
	{
		ImVec2 p1(State->CanvasPos.x, OriginScreen.y);
		ImVec2 p2(State->CanvasPos.x + State->CanvasSize.x, OriginScreen.y);
		DrawList->AddLine(p1, p2, AxisColor, 2.0f);
	}
}

void SBlendSpace2DWindow::RenderSamplePoints(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (int32 i = 0; i < State->BlendSpace->GetNumSamples(); ++i)
	{
		const FBlendSample& Sample = State->BlendSpace->Samples[i];
		ImVec2 ScreenPos = ParamToScreen(State, Sample.Position);

		ImU32 Color = (i == State->SelectedSampleIndex) ? SelectedSampleColor : SampleColor;

		DrawList->AddCircleFilled(ScreenPos, SamplePointRadius, Color);
		DrawList->AddCircle(ScreenPos, SamplePointRadius, IM_COL32(255, 255, 255, 255), 0, 2.0f);

		FString DisplayName = GetDisplayNameForAnimation(Sample.Animation);
		ImVec2 TextPos(ScreenPos.x + SamplePointRadius + 5, ScreenPos.y - 8);
		DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), DisplayName.c_str());
	}
}

void SBlendSpace2DWindow::RenderSamplePoints_Enhanced(FBlendSpace2DTabState* State,
                                                       const TArray<int32>& InSampleIndices,
                                                       const TArray<float>& InWeights)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 가중치 정보를 맵으로 변환
	TMap<int32, float> WeightMap;
	for (int32 i = 0; i < InSampleIndices.Num(); ++i)
	{
		WeightMap.Add(InSampleIndices[i], InWeights[i]);
	}

	for (int32 i = 0; i < State->BlendSpace->GetNumSamples(); ++i)
	{
		const FBlendSample& Sample = State->BlendSpace->Samples[i];
		ImVec2 ScreenPos = ParamToScreen(State, Sample.Position);

		bool bIsActive = WeightMap.Contains(i) && WeightMap[i] > 0.001f;
		float Weight = bIsActive ? WeightMap[i] : 0.0f;

		ImU32 Color;
		float Radius = SamplePointRadius;

		if (i == State->SelectedSampleIndex)
		{
			Color = SelectedSampleColor;
			Radius = SamplePointRadius * 1.2f;
		}
		else if (bIsActive)
		{
			float Intensity = FMath::Lerp(0.5f, 1.0f, Weight);
			Color = IM_COL32(
				static_cast<uint8_t>(100 * Intensity),
				static_cast<uint8_t>(255 * Intensity),
				static_cast<uint8_t>(100 * Intensity),
				255
			);
			Radius = SamplePointRadius * (1.0f + Weight * 0.5f);
		}
		else
		{
			Color = SampleColor;
		}

		ImU32 OutlineColor = IM_COL32(255, 255, 255, 255);
		if (Sample.RateScale != 1.0f)
		{
			OutlineColor = IM_COL32(255, 150, 0, 255);
		}

		DrawList->AddCircleFilled(ScreenPos, Radius, Color);
		DrawList->AddCircle(ScreenPos, Radius, OutlineColor, 0, 2.0f);

		FString DisplayName = GetDisplayNameForAnimation(Sample.Animation);
		ImVec2 TextPos(ScreenPos.x + Radius + 5, ScreenPos.y - 8);

		ImU32 TextColor = bIsActive ? IM_COL32(150, 255, 150, 255) : IM_COL32(255, 255, 255, 200);
		DrawList->AddText(TextPos, TextColor, DisplayName.c_str());

		if (bIsActive)
		{
			char WeightText[32];
			sprintf_s(WeightText, "%.1f%%", Weight * 100.0f);

			ImVec2 WeightTextSize = ImGui::CalcTextSize(WeightText);
			ImVec2 WeightTextPos(ScreenPos.x - WeightTextSize.x * 0.5f, ScreenPos.y - Radius - WeightTextSize.y - 2);

			ImVec2 BgMin(WeightTextPos.x - 2, WeightTextPos.y - 2);
			ImVec2 BgMax(WeightTextPos.x + WeightTextSize.x + 2, WeightTextPos.y + WeightTextSize.y + 2);
			DrawList->AddRectFilled(BgMin, BgMax, IM_COL32(0, 0, 0, 180));

			DrawList->AddText(WeightTextPos, IM_COL32(230, 255, 230, 255), WeightText);
		}
	}
}

void SBlendSpace2DWindow::RenderPreviewMarker(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 PreviewPos = ParamToScreen(State, State->PreviewParameter);

	DrawList->AddCircle(PreviewPos, PreviewMarkerRadius, PreviewColor, 0, 3.0f);

	DrawList->AddLine(
		ImVec2(PreviewPos.x - PreviewMarkerRadius, PreviewPos.y),
		ImVec2(PreviewPos.x + PreviewMarkerRadius, PreviewPos.y),
		PreviewColor, 2.0f);

	DrawList->AddLine(
		ImVec2(PreviewPos.x, PreviewPos.y - PreviewMarkerRadius),
		ImVec2(PreviewPos.x, PreviewPos.y + PreviewMarkerRadius),
		PreviewColor, 2.0f);
}

void SBlendSpace2DWindow::RenderAxisLabels(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	UBlendSpace2D* BS = State->BlendSpace;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImU32 LabelColor = IM_COL32(200, 200, 200, 255);

	// 파라미터 범위 계산
	float XRange = BS->XAxisMax - BS->XAxisMin;
	float YRange = BS->YAxisMax - BS->YAxisMin;

	// 현재 뷰에서 보이는 파라미터 범위 계산 (패닝/줌 반영)
	// X축: 왼쪽에서 오른쪽으로 증가
	float VisibleXMin = BS->XAxisMin - State->PanOffset.X * XRange;
	float VisibleXMax = BS->XAxisMax - State->PanOffset.X * XRange + XRange * (1.0f / State->ZoomLevel - 1.0f);
	// Y축: 화면상 위쪽이 YMax, 아래쪽이 YMin (화면 좌표계 반전)
	float VisibleYMax = BS->YAxisMax - State->PanOffset.Y * YRange;
	float VisibleYMin = BS->YAxisMin - State->PanOffset.Y * YRange - YRange * (1.0f / State->ZoomLevel - 1.0f);

	// 캔버스 크기에 따라 눈금 수 동적 계산 (최소 4개, 최대 10개)
	int NumXTicks = std::max(4, std::min(10, static_cast<int>(State->CanvasSize.x / 50.0f)));
	int NumYTicks = std::max(4, std::min(10, static_cast<int>(State->CanvasSize.y / 50.0f)));

	// 깔끔한 간격 계산 (10, 20, 25, 50, 100 등)
	auto CalcNiceStep = [](float range, int numTicks) -> float {
		float roughStep = range / numTicks;
		float magnitude = powf(10.0f, floorf(log10f(roughStep)));
		float normalized = roughStep / magnitude;

		float niceStep;
		if (normalized < 1.5f) niceStep = 1.0f;
		else if (normalized < 3.0f) niceStep = 2.0f;
		else if (normalized < 7.0f) niceStep = 5.0f;
		else niceStep = 10.0f;

		return niceStep * magnitude;
	};

	float XStep = CalcNiceStep(XRange, NumXTicks);
	float YStep = CalcNiceStep(YRange, NumYTicks);

	// X축 시작값 (보이는 범위 바깥에서 시작, 깔끔하게 정렬)
	float XStart = floorf(VisibleXMin / XStep) * XStep;
	float YStart = floorf(VisibleYMin / YStep) * YStep;

	// X축 눈금 (패닝된 범위)
	for (float ParamValue = XStart; ParamValue <= VisibleXMax + XStep; ParamValue += XStep)
	{
		ImVec2 ScreenPos = ParamToScreen(State, FVector2D(ParamValue, BS->YAxisMin));

		// 라벨 텍스트 준비
		char Label[32];
		float DisplayValue = ParamValue;
		if (fabsf(DisplayValue) < 0.001f) DisplayValue = 0.0f;  // -0 방지
		sprintf_s(Label, "%.0f", DisplayValue);
		ImVec2 TextSize = ImGui::CalcTextSize(Label);

		// 캔버스 범위 내에만 눈금선 그리기
		if (ScreenPos.x >= State->CanvasPos.x && ScreenPos.x <= State->CanvasPos.x + State->CanvasSize.x)
		{
			DrawList->AddLine(
				ImVec2(ScreenPos.x, State->CanvasPos.y + State->CanvasSize.y),
				ImVec2(ScreenPos.x, State->CanvasPos.y + State->CanvasSize.y + 5.0f),
				LabelColor, 1.0f
			);

			// 라벨 (캔버스 범위 내에만)
			ImVec2 TextPos(ScreenPos.x - TextSize.x * 0.5f, State->CanvasPos.y + State->CanvasSize.y + 7.0f);
			DrawList->AddText(TextPos, LabelColor, Label);
		}
	}

	// Y축 눈금 (패닝된 범위)
	for (float ParamValue = YStart; ParamValue <= VisibleYMax + YStep; ParamValue += YStep)
	{
		ImVec2 ScreenPos = ParamToScreen(State, FVector2D(BS->XAxisMin, ParamValue));

		// 라벨 텍스트 준비
		char Label[32];
		float DisplayValue = ParamValue;
		if (fabsf(DisplayValue) < 0.001f) DisplayValue = 0.0f;  // -0 방지
		sprintf_s(Label, "%.0f", DisplayValue);
		ImVec2 TextSize = ImGui::CalcTextSize(Label);

		// 캔버스 범위 내에만 눈금선 그리기
		if (ScreenPos.y >= State->CanvasPos.y && ScreenPos.y <= State->CanvasPos.y + State->CanvasSize.y)
		{
			DrawList->AddLine(
				ImVec2(State->CanvasPos.x, ScreenPos.y),
				ImVec2(State->CanvasPos.x - 5.0f, ScreenPos.y),
				LabelColor, 1.0f
			);

			// 라벨 (캔버스 범위 내에만)
			ImVec2 TextPos(State->CanvasPos.x - 7.0f - TextSize.x, ScreenPos.y - TextSize.y * 0.5f);
			DrawList->AddText(TextPos, LabelColor, Label);
		}
	}

	// X축 호버 툴팁
	{
		ImVec2 XAxisMin(State->CanvasPos.x, State->CanvasPos.y + State->CanvasSize.y);
		ImVec2 XAxisMax(State->CanvasPos.x + State->CanvasSize.x, State->CanvasPos.y + State->CanvasSize.y + 25.0f);
		if (ImGui::IsMouseHoveringRect(XAxisMin, XAxisMax))
		{
			ImGui::SetTooltip("X: %s (%.0f ~ %.0f)", BS->XAxisName.c_str(), BS->XAxisMin, BS->XAxisMax);
		}
	}

	// Y축 호버 툴팁
	{
		ImVec2 YAxisMin(State->CanvasPos.x - 45.0f, State->CanvasPos.y);
		ImVec2 YAxisMax(State->CanvasPos.x, State->CanvasPos.y + State->CanvasSize.y);
		if (ImGui::IsMouseHoveringRect(YAxisMin, YAxisMax))
		{
			ImGui::SetTooltip("Y: %s (%.0f ~ %.0f)", BS->YAxisName.c_str(), BS->YAxisMin, BS->YAxisMax);
		}
	}
}

void SBlendSpace2DWindow::RenderTriangulation(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (const FBlendTriangle& Tri : State->BlendSpace->Triangles)
	{
		if (!Tri.IsValid())
		{
			continue;
		}

		if (Tri.Index0 < 0 || Tri.Index0 >= State->BlendSpace->GetNumSamples() ||
			Tri.Index1 < 0 || Tri.Index1 >= State->BlendSpace->GetNumSamples() ||
			Tri.Index2 < 0 || Tri.Index2 >= State->BlendSpace->GetNumSamples())
		{
			continue;
		}

		ImVec2 P0 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index0].Position);
		ImVec2 P1 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index1].Position);
		ImVec2 P2 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index2].Position);

		ImU32 TriColor = IM_COL32(255, 255, 0, 100);
		DrawList->AddLine(P0, P1, TriColor, 1.5f);
		DrawList->AddLine(P1, P2, TriColor, 1.5f);
		DrawList->AddLine(P2, P0, TriColor, 1.5f);
	}
}

void SBlendSpace2DWindow::RenderTriangulation_Enhanced(FBlendSpace2DTabState* State, int32 InActiveTriangle)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (int32 i = 0; i < State->BlendSpace->Triangles.Num(); ++i)
	{
		const FBlendTriangle& Tri = State->BlendSpace->Triangles[i];

		if (!Tri.IsValid())
		{
			continue;
		}

		if (Tri.Index0 < 0 || Tri.Index0 >= State->BlendSpace->GetNumSamples() ||
			Tri.Index1 < 0 || Tri.Index1 >= State->BlendSpace->GetNumSamples() ||
			Tri.Index2 < 0 || Tri.Index2 >= State->BlendSpace->GetNumSamples())
		{
			continue;
		}

		ImVec2 P0 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index0].Position);
		ImVec2 P1 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index1].Position);
		ImVec2 P2 = ParamToScreen(State, State->BlendSpace->Samples[Tri.Index2].Position);

		if (i == InActiveTriangle)
		{
			DrawList->AddTriangleFilled(P0, P1, P2, IM_COL32(0, 255, 100, 40));
			DrawList->AddLine(P0, P1, IM_COL32(150, 255, 150, 200), 2.0f);
			DrawList->AddLine(P1, P2, IM_COL32(150, 255, 150, 200), 2.0f);
			DrawList->AddLine(P2, P0, IM_COL32(150, 255, 150, 200), 2.0f);
		}
		else
		{
			ImU32 TriColor = IM_COL32(255, 255, 0, 80);
			DrawList->AddLine(P0, P1, TriColor, 1.0f);
			DrawList->AddLine(P1, P2, TriColor, 1.0f);
			DrawList->AddLine(P2, P0, TriColor, 1.0f);
		}
	}
}

// ============================================================================
// 좌표 변환
// ============================================================================

ImVec2 SBlendSpace2DWindow::ParamToScreen(FBlendSpace2DTabState* State, FVector2D Param) const
{
	if (!State || !State->BlendSpace)
	{
		return State ? State->CanvasPos : ImVec2(0, 0);
	}

	UBlendSpace2D* BS = State->BlendSpace;

	float NormX = (Param.X - BS->XAxisMin) / (BS->XAxisMax - BS->XAxisMin);
	float NormY = (Param.Y - BS->YAxisMin) / (BS->YAxisMax - BS->YAxisMin);

	NormX += State->PanOffset.X;
	NormY += State->PanOffset.Y;

	float ScreenX = State->CanvasPos.x + NormX * State->CanvasSize.x * State->ZoomLevel;
	float ScreenY = State->CanvasPos.y + (1.0f - NormY) * State->CanvasSize.y * State->ZoomLevel;

	return ImVec2(ScreenX, ScreenY);
}

FVector2D SBlendSpace2DWindow::ScreenToParam(FBlendSpace2DTabState* State, ImVec2 ScreenPos) const
{
	if (!State || !State->BlendSpace)
	{
		return FVector2D::Zero();
	}

	UBlendSpace2D* BS = State->BlendSpace;

	float NormX = (ScreenPos.x - State->CanvasPos.x) / (State->CanvasSize.x * State->ZoomLevel);
	float NormY = (State->CanvasPos.y + State->CanvasSize.y * State->ZoomLevel - ScreenPos.y) / (State->CanvasSize.y * State->ZoomLevel);

	NormX -= State->PanOffset.X;
	NormY -= State->PanOffset.Y;

	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);

	float ParamX = BS->XAxisMin + NormX * (BS->XAxisMax - BS->XAxisMin);
	float ParamY = BS->YAxisMin + NormY * (BS->YAxisMax - BS->YAxisMin);

	return FVector2D(ParamX, ParamY);
}

// ============================================================================
// 입력 처리
// ============================================================================

void SBlendSpace2DWindow::HandleGridMouseInput(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	UBlendSpace2D* BS = State->BlendSpace;
	ImVec2 MousePos = ImGui::GetMousePos();
	bool bMouseInCanvas = ImGui::IsMouseHoveringRect(
		State->CanvasPos,
		ImVec2(State->CanvasPos.x + State->CanvasSize.x, State->CanvasPos.y + State->CanvasSize.y)
	);

	if (!bMouseInCanvas)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	// 줌 (마우스 휠)
	if (io.MouseWheel != 0.0f)
	{
		float ZoomDelta = io.MouseWheel * 0.1f;
		State->ZoomLevel = FMath::Clamp(State->ZoomLevel + ZoomDelta, 0.5f, 3.0f);
	}

	// 팬 (마우스 중간 버튼 드래그)
	if (ImGui::IsMouseClicked(2))
	{
		State->bPanning = true;
		State->PanStartMousePos = MousePos;
	}

	if (State->bPanning && ImGui::IsMouseDragging(2))
	{
		ImVec2 Delta = ImVec2(MousePos.x - State->PanStartMousePos.x, MousePos.y - State->PanStartMousePos.y);
		State->PanOffset.X += Delta.x / (State->CanvasSize.x * State->ZoomLevel);
		State->PanOffset.Y -= Delta.y / (State->CanvasSize.y * State->ZoomLevel);
		State->PanStartMousePos = MousePos;
	}

	if (ImGui::IsMouseReleased(2))
	{
		State->bPanning = false;
	}

	// 왼쪽 클릭
	if (ImGui::IsMouseClicked(0))
	{
		// 프리뷰 마커 클릭 체크
		ImVec2 PreviewScreenPos = ParamToScreen(State, State->PreviewParameter);
		float DistToPreview = sqrtf(
			(MousePos.x - PreviewScreenPos.x) * (MousePos.x - PreviewScreenPos.x) +
			(MousePos.y - PreviewScreenPos.y) * (MousePos.y - PreviewScreenPos.y)
		);

		if (DistToPreview <= PreviewMarkerRadius + 5.0f)
		{
			State->bDraggingPreviewMarker = true;
			State->SelectedSampleIndex = -1;
		}
		else
		{
			// 샘플 클릭 체크
			bool bFoundSample = false;
			for (int32 i = 0; i < BS->GetNumSamples(); ++i)
			{
				ImVec2 SampleScreenPos = ParamToScreen(State, BS->Samples[i].Position);
				float Dist = sqrtf(
					(MousePos.x - SampleScreenPos.x) * (MousePos.x - SampleScreenPos.x) +
					(MousePos.y - SampleScreenPos.y) * (MousePos.y - SampleScreenPos.y)
				);

				if (Dist <= SamplePointRadius + 5.0f)
				{
					SelectSample(State, i);

					// Ctrl 키를 누른 상태면 복제 모드
					if (io.KeyCtrl)
					{
						// 샘플 복제
						FBlendSample OriginalSample = BS->Samples[i];
						BS->AddSample(OriginalSample.Position, OriginalSample.Animation);

						// RateScale 복사
						if (BS->GetNumSamples() > 0)
						{
							BS->Samples[BS->GetNumSamples() - 1].RateScale = OriginalSample.RateScale;
						}

						State->SelectedSampleIndex = BS->GetNumSamples() - 1;
					}

					State->bDraggingSample = true;
					bFoundSample = true;
					break;
				}
			}

			if (!bFoundSample)
			{
				State->SelectedSampleIndex = -1;
			}
		}
	}

	// 우클릭: 컨텍스트 메뉴
	if (ImGui::IsMouseClicked(1))
	{
		State->ContextMenuPos = MousePos;
		State->ContextMenuSampleIndex = -1;

		for (int32 i = 0; i < BS->GetNumSamples(); ++i)
		{
			ImVec2 SampleScreenPos = ParamToScreen(State, BS->Samples[i].Position);
			float Dist = sqrtf(
				(MousePos.x - SampleScreenPos.x) * (MousePos.x - SampleScreenPos.x) +
				(MousePos.y - SampleScreenPos.y) * (MousePos.y - SampleScreenPos.y)
			);

			if (Dist <= SamplePointRadius + 5.0f)
			{
				State->ContextMenuSampleIndex = i;
				break;
			}
		}

		State->bShowContextMenu = true;
	}

	// 프리뷰 마커 드래그
	if (State->bDraggingPreviewMarker && ImGui::IsMouseDragging(0))
	{
		FVector2D NewPos = ScreenToParam(State, MousePos);
		NewPos.X = FMath::Clamp(NewPos.X, BS->XAxisMin, BS->XAxisMax);
		NewPos.Y = FMath::Clamp(NewPos.Y, BS->YAxisMin, BS->YAxisMax);
		State->PreviewParameter = NewPos;
	}

	// 샘플 드래그
	if (State->bDraggingSample && ImGui::IsMouseDragging(0))
	{
		if (State->SelectedSampleIndex >= 0 && State->SelectedSampleIndex < BS->GetNumSamples())
		{
			FVector2D NewPos = ScreenToParam(State, MousePos);

			if (State->bEnableGridSnapping)
			{
				NewPos.X = floorf(NewPos.X / State->GridSnapSize + 0.5f) * State->GridSnapSize;
				NewPos.Y = floorf(NewPos.Y / State->GridSnapSize + 0.5f) * State->GridSnapSize;
			}

			BS->Samples[State->SelectedSampleIndex].Position = NewPos;
		}
	}

	// 드래그 종료
	if (ImGui::IsMouseReleased(0))
	{
		if (State->bDraggingSample && BS)
		{
			BS->GenerateTriangulation();
		}

		State->bDraggingSample = false;
		State->bDraggingPreviewMarker = false;
	}

	// 더블 클릭: 새 샘플 추가
	if (ImGui::IsMouseDoubleClicked(0))
	{
		FVector2D ClickPos = ScreenToParam(State, MousePos);
		AddSampleAtPosition(State, ClickPos);
	}

	// 컨텍스트 메뉴 렌더링
	if (State->bShowContextMenu)
	{
		ImGui::OpenPopup("SampleContextMenu");
		State->bShowContextMenu = false;
	}

	if (ImGui::BeginPopup("SampleContextMenu"))
	{
		if (State->ContextMenuSampleIndex >= 0)
		{
			ImGui::Text("Sample %d", State->ContextMenuSampleIndex);
			ImGui::Separator();

			if (ImGui::MenuItem("Delete"))
			{
				if (State->ContextMenuSampleIndex < BS->GetNumSamples())
				{
					BS->RemoveSample(State->ContextMenuSampleIndex);
					State->SelectedSampleIndex = -1;

					if (BS->GetNumSamples() >= 3)
					{
						BS->GenerateTriangulation();
					}
				}
			}

			if (ImGui::MenuItem("Duplicate"))
			{
				if (State->ContextMenuSampleIndex < BS->GetNumSamples())
				{
					FBlendSample OriginalSample = BS->Samples[State->ContextMenuSampleIndex];
					FVector2D NewPos = OriginalSample.Position;
					NewPos.X += 10.0f;
					NewPos.Y += 10.0f;
					BS->AddSample(NewPos, OriginalSample.Animation);

					if (BS->GetNumSamples() > 0)
					{
						BS->Samples[BS->GetNumSamples() - 1].RateScale = OriginalSample.RateScale;
					}

					State->SelectedSampleIndex = BS->GetNumSamples() - 1;

					if (BS->GetNumSamples() >= 3)
					{
						BS->GenerateTriangulation();
					}
				}
			}
		}
		else
		{
			if (ImGui::MenuItem("Add Sample Here"))
			{
				FVector2D ClickPos = ScreenToParam(State, State->ContextMenuPos);
				AddSampleAtPosition(State, ClickPos);
			}
		}

		ImGui::EndPopup();
	}
}

void SBlendSpace2DWindow::HandleKeyboardInput(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	// Ctrl+S: 빠른 저장 (텍스트 입력 중에도 동작)
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
	{
		if (State->BlendSpace && !State->FilePath.empty())
		{
			// 프리뷰 파라미터 저장
			State->BlendSpace->EditorPreviewParameter = State->PreviewParameter;

			// 현재 메시 경로 저장
			if (!State->LoadedMeshPath.empty())
			{
				State->BlendSpace->EditorSkeletalMeshPath = State->LoadedMeshPath;
			}

			State->BlendSpace->SaveToFile(State->FilePath);
			UE_LOG("BlendSpace: Save: %s", State->FilePath.c_str());
		}
	}

	// 텍스트 입력 중이면 나머지 키보드 단축키 무시
	if (io.WantTextInput)
	{
		return;
	}

	// Delete 키: 선택된 샘플 삭제
	if (ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		RemoveSelectedSample(State);
	}

	// Space 키: 재생/일시정지
	if (ImGui::IsKeyPressed(ImGuiKey_Space))
	{
		State->bIsPlaying = !State->bIsPlaying;
	}
}

// ============================================================================
// 샘플 관리
// ============================================================================

void SBlendSpace2DWindow::AddSampleAtPosition(FBlendSpace2DTabState* State, FVector2D Position)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	State->BlendSpace->AddSample(Position, nullptr);
	State->SelectedSampleIndex = State->BlendSpace->GetNumSamples() - 1;

	if (State->BlendSpace->GetNumSamples() >= 3)
	{
		State->BlendSpace->GenerateTriangulation();
	}
}

void SBlendSpace2DWindow::RemoveSelectedSample(FBlendSpace2DTabState* State)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	if (State->SelectedSampleIndex >= 0 && State->SelectedSampleIndex < State->BlendSpace->GetNumSamples())
	{
		State->BlendSpace->RemoveSample(State->SelectedSampleIndex);
		State->SelectedSampleIndex = -1;

		if (State->BlendSpace->GetNumSamples() >= 3)
		{
			State->BlendSpace->GenerateTriangulation();
		}
	}
}

void SBlendSpace2DWindow::SelectSample(FBlendSpace2DTabState* State, int32 Index)
{
	if (!State || !State->BlendSpace)
	{
		return;
	}

	if (Index >= 0 && Index < State->BlendSpace->GetNumSamples())
	{
		State->SelectedSampleIndex = Index;
	}
}

// ============================================================================
// 타임라인 컨트롤
// ============================================================================

// BlendNode 가져오기 헬퍼
static FAnimNode_BlendSpace2D* GetBlendNodeFromState(FBlendSpace2DTabState* State)
{
	if (!State || !State->PreviewActor)
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return nullptr;
	}

	UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
	if (!AnimInst)
	{
		return nullptr;
	}

	return AnimInst->GetBlendSpace2DNode();
}

void SBlendSpace2DWindow::RenderTimelineControls(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	// 아이콘 세트 설정
	FPlaybackIcons Icons;
	Icons.GoToFront = IconGoToFront;
	Icons.StepBackwards = IconStepBackwards;
	Icons.Play = IconPlay;
	Icons.Pause = IconPause;
	Icons.StepForward = IconStepForward;
	Icons.GoToEnd = IconGoToEnd;
	Icons.Loop = IconLoop;
	Icons.LoopOff = IconLoopOff;

	// 상태 설정
	FPlaybackState PlaybackState;
	PlaybackState.bIsPlaying = State->bIsPlaying;
	PlaybackState.bLoopAnimation = State->bLoopAnimation;
	PlaybackState.PlaybackSpeed = State->PlaybackSpeed;

	// 콜백 설정
	FPlaybackCallbacks Callbacks;
	Callbacks.OnToFront = [this, State]() { TimelineToFront(State); };
	Callbacks.OnStepBackwards = [this, State]() { TimelineToPrevious(State); };
	Callbacks.OnStepForward = [this, State]() { TimelineToNext(State); };
	Callbacks.OnToEnd = [this, State]() { TimelineToEnd(State); };

	// 렌더링
	PlaybackControls::Render(Icons, PlaybackState, Callbacks, 20.0f);

	// 상태 동기화
	State->bIsPlaying = PlaybackState.bIsPlaying;
	State->bLoopAnimation = PlaybackState.bLoopAnimation;
	State->PlaybackSpeed = PlaybackState.PlaybackSpeed;

	// === 타임라인 스크러버 (재생 컨트롤 옆에 배치) ===
	ImGui::SameLine();
	ImGui::Dummy(ImVec2(10, 0));  // 간격
	ImGui::SameLine();

	FAnimNode_BlendSpace2D* BlendNode = GetBlendNodeFromState(State);

	// 최대 애니메이션 길이 가져오기
	float MaxTime = 1.0f;
	if (BlendNode)
	{
		MaxTime = BlendNode->GetMaxAnimationLength();
	}
	else if (State->BlendSpace && State->BlendSpace->GetNumSamples() > 0)
	{
		for (int32 i = 0; i < State->BlendSpace->GetNumSamples(); ++i)
		{
			UAnimSequence* Anim = State->BlendSpace->Samples[i].Animation;
			if (Anim)
			{
				MaxTime = std::max(MaxTime, Anim->GetPlayLength());
			}
		}
	}

	if (MaxTime <= 0.0f)
	{
		MaxTime = 1.0f;
	}

	// 현재 시간 계산
	float CurrentNormTime = BlendNode ? BlendNode->GetNormalizedTime() : 0.0f;
	float CurrentTime = CurrentNormTime * MaxTime;

	// 타임라인 슬라이더 (남은 공간 전체 사용)
	ImGui::PushItemWidth(-1);
	float SliderTime = CurrentTime;
	if (ImGui::SliderFloat("##BlendTimeSlider", &SliderTime, 0.0f, MaxTime, "%.2fs"))
	{
		if (BlendNode && MaxTime > 0.0f)
		{
			float NewNormTime = SliderTime / MaxTime;
			BlendNode->SetNormalizedTime(NewNormTime);
		}
		State->CurrentAnimationTime = SliderTime;
	}
	ImGui::PopItemWidth();
}

void SBlendSpace2DWindow::TimelineToFront(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	FAnimNode_BlendSpace2D* BlendNode = GetBlendNodeFromState(State);
	if (BlendNode)
	{
		BlendNode->SetNormalizedTime(0.0f);
	}
	State->CurrentAnimationTime = 0.0f;
}

void SBlendSpace2DWindow::TimelineToPrevious(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	FAnimNode_BlendSpace2D* BlendNode = GetBlendNodeFromState(State);
	if (BlendNode)
	{
		float CurrentNormTime = BlendNode->GetNormalizedTime();
		float MaxLength = BlendNode->GetMaxAnimationLength();
		float StepTime = (MaxLength > 0.0f) ? (1.0f / 30.0f) / MaxLength : 0.0f;
		BlendNode->SetNormalizedTime(CurrentNormTime - StepTime);
	}
}

void SBlendSpace2DWindow::TimelinePlay(FBlendSpace2DTabState* State)
{
	if (State)
	{
		State->bIsPlaying = !State->bIsPlaying;
	}
}

void SBlendSpace2DWindow::TimelineToNext(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	FAnimNode_BlendSpace2D* BlendNode = GetBlendNodeFromState(State);
	if (BlendNode)
	{
		float CurrentNormTime = BlendNode->GetNormalizedTime();
		float MaxLength = BlendNode->GetMaxAnimationLength();
		float StepTime = (MaxLength > 0.0f) ? (1.0f / 30.0f) / MaxLength : 0.0f;
		BlendNode->SetNormalizedTime(CurrentNormTime + StepTime);
	}
}

void SBlendSpace2DWindow::TimelineToEnd(FBlendSpace2DTabState* State)
{
	if (!State)
	{
		return;
	}

	FAnimNode_BlendSpace2D* BlendNode = GetBlendNodeFromState(State);
	if (BlendNode)
	{
		BlendNode->SetNormalizedTime(1.0f);
	}

	// Legacy: CurrentAnimationTime도 업데이트
	float MaxLength = 0.0f;
	if (State->BlendSpace)
	{
		for (int32 i = 0; i < State->BlendSpace->GetNumSamples(); ++i)
		{
			UAnimSequence* Anim = State->BlendSpace->Samples[i].Animation;
			if (Anim)
			{
				MaxLength = std::max(MaxLength, Anim->GetPlayLength());
			}
		}
	}
	State->CurrentAnimationTime = MaxLength;
}
