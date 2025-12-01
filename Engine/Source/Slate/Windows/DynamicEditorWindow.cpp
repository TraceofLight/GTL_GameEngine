#include "pch.h"
#include "DynamicEditorWindow.h"
#include "SkeletalEditorWindow.h"
#include "AnimationWindow.h"
#include "BlendSpace2DWindow.h"
#include "AnimStateMachineWindow.h"
#include "SPhysicsAssetEditorWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/StaticMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/StaticMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "ViewerState.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/BlendSpace2D.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Runtime/Core/Misc/Archive.h"
#include "Source/Editor/Gizmo/GizmoActor.h"

// ============================================================================
// SDynamicEditorWindow 구현
// SPreviewWindow의 레이아웃 로직을 그대로 사용
// ============================================================================

SDynamicEditorWindow::SDynamicEditorWindow()
{
	CenterRect = FRect(0, 0, 0, 0);
}

SDynamicEditorWindow::~SDynamicEditorWindow()
{
	// 렌더 타겟 해제
	ReleaseRenderTarget();

	// EmbeddedAnimationEditor 해제
	if (EmbeddedAnimationEditor)
	{
		delete EmbeddedAnimationEditor;
		EmbeddedAnimationEditor = nullptr;
	}

	// EmbeddedBlendSpace2DEditor 해제
	if (EmbeddedBlendSpace2DEditor)
	{
		delete EmbeddedBlendSpace2DEditor;
		EmbeddedBlendSpace2DEditor = nullptr;
	}

	// EmbeddedStateMachineEditor 해제
	if (EmbeddedStateMachineEditor)
	{
		delete EmbeddedStateMachineEditor;
		EmbeddedStateMachineEditor = nullptr;
	}

	// EmbeddedSkeletalEditor 해제
	if (EmbeddedSkeletalEditor)
	{
		delete EmbeddedSkeletalEditor;
		EmbeddedSkeletalEditor = nullptr;
	}

	// EmbeddedPhysicsAssetEditor 해제
	if (EmbeddedPhysicsAssetEditor)
	{
		EmbeddedPhysicsAssetEditor->PrepareForDelete();
		delete EmbeddedPhysicsAssetEditor;
		EmbeddedPhysicsAssetEditor = nullptr;
	}

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
	World = InWorld;
	Device = InDevice;

	SetRect(StartX, StartY, StartX + Width, StartY + Height);

	// 타임라인 아이콘 로드 (SPreviewWindow와 동일)
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

	// Mode 아이콘 로드
	IconModeSkeletal = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/SkeletalMeshActor_64.dds");
	IconModeAnimation = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/AnimSequence_64.dds");
	IconModeAnimGraph = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/StateMachine_512.png");
	IconModeBlendSpace = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/BlendSpace_64.dds");
	IconModePhysicsAsset = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/PhysicsAsset_64.dds");

	// New/Save/Load 아이콘 로드
	IconNew = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/IconFileNew_40x.dds");
	IconSave = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Toolbar_Save.dds");
	IconLoad = UResourceManager::GetInstance().Load<UTexture>("Data/Default/Icon/Toolbar_Load.dds");

	ScanNotifyLibrary();

	// 기본 탭 없이 시작 - 컨텐츠 브라우저에서 파일 열 때 탭 생성
	bRequestFocus = true;
	return true;
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

		// 바닥판 액터 생성
		State->FloorActor = SkeletalViewerBootstrap::CreateFloorActor(State->World);
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

	// 닫히는 탭의 모드 저장
	EEditorMode ClosingMode = Tabs[Index]->Mode;

	DestroyTab(Tabs[Index]);
	Tabs.erase(Tabs.begin() + Index);

	// 해당 모드의 남은 탭 수 확인
	int32 RemainingTabsOfMode = 0;
	for (const auto& Tab : Tabs)
	{
		if (Tab->Mode == ClosingMode)
		{
			++RemainingTabsOfMode;
		}
	}

	// 해당 모드의 탭이 모두 닫히면 EmbeddedEditor 리셋
	if (RemainingTabsOfMode == 0)
	{
		switch (ClosingMode)
		{
		case EEditorMode::AnimGraph:
			if (EmbeddedStateMachineEditor)
			{
				delete EmbeddedStateMachineEditor;
				EmbeddedStateMachineEditor = nullptr;
			}
			break;
		case EEditorMode::BlendSpace2D:
			if (EmbeddedBlendSpace2DEditor)
			{
				delete EmbeddedBlendSpace2DEditor;
				EmbeddedBlendSpace2DEditor = nullptr;
			}
			break;
		case EEditorMode::Skeletal:
			if (EmbeddedSkeletalEditor)
			{
				delete EmbeddedSkeletalEditor;
				EmbeddedSkeletalEditor = nullptr;
			}
			break;
		case EEditorMode::PhysicsAsset:
			if (EmbeddedPhysicsAssetEditor)
			{
				EmbeddedPhysicsAssetEditor->PrepareForDelete();
				delete EmbeddedPhysicsAssetEditor;
				EmbeddedPhysicsAssetEditor = nullptr;
			}
			break;
		default:
			break;
		}
	}

	// 활성 탭 조정
	if (Tabs.Num() == 0)
	{
		ActiveState = nullptr;
		ActiveTabIndex = -1;
		bIsOpen = false;

		// 모든 탭이 닫히면 모든 EmbeddedEditor 정리
		if (EmbeddedStateMachineEditor)
		{
			delete EmbeddedStateMachineEditor;
			EmbeddedStateMachineEditor = nullptr;
		}
		if (EmbeddedBlendSpace2DEditor)
		{
			delete EmbeddedBlendSpace2DEditor;
			EmbeddedBlendSpace2DEditor = nullptr;
		}
		if (EmbeddedSkeletalEditor)
		{
			delete EmbeddedSkeletalEditor;
			EmbeddedSkeletalEditor = nullptr;
		}
		if (EmbeddedAnimationEditor)
		{
			delete EmbeddedAnimationEditor;
			EmbeddedAnimationEditor = nullptr;
		}
		if (EmbeddedPhysicsAssetEditor)
		{
			EmbeddedPhysicsAssetEditor->PrepareForDelete();
			delete EmbeddedPhysicsAssetEditor;
			EmbeddedPhysicsAssetEditor = nullptr;
		}
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

void SDynamicEditorWindow::SetEditorMode(EEditorMode NewMode)
{
	if (ActiveState && ActiveState->Mode != NewMode)
	{
		ActiveState->Mode = NewMode;
	}
}

EEditorMode SDynamicEditorWindow::GetEditorMode() const
{
	return ActiveState ? ActiveState->Mode : EEditorMode::Skeletal;
}

// ============================================================================
// OnRender - SPreviewWindow의 레이아웃 구조를 그대로 사용
// ============================================================================

void SDynamicEditorWindow::OnRender()
{
	if (!bIsOpen)
	{
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

	if (!bInitialPlacementDone)
	{
		// 전체화면 대비 95% 크기로 설정
		ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
		float WindowWidth = DisplaySize.x * 0.95f;
		float WindowHeight = DisplaySize.y * 0.95f;
		float PosX = (DisplaySize.x - WindowWidth) * 0.5f;
		float PosY = (DisplaySize.y - WindowHeight) * 0.5f;

		ImGui::SetNextWindowPos(ImVec2(PosX, PosY));
		ImGui::SetNextWindowSize(ImVec2(WindowWidth, WindowHeight));
		bInitialPlacementDone = true;
	}

	if (bRequestFocus)
	{
		ImGui::SetNextWindowFocus();
	}

	bool bViewerVisible = false;
	if (ImGui::Begin("Dynamic Editor", &bIsOpen, flags))
	{
		bViewerVisible = true;
		bIsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 탭 바 렌더링
		int32 PreviousTabIndex = ActiveTabIndex;
		bool bTabClosed = false;

		if (ImGui::BeginTabBar("DynamicEditorTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
		{
			for (int i = 0; i < Tabs.Num(); ++i)
			{
				FEditorTabState* State = Tabs[i];
				bool open = true;

				// Mode 버튼으로 탭 전환 요청 시 SetSelected 플래그 사용
				ImGuiTabItemFlags tabFlags = 0;
				if (bRequestTabSwitch && RequestedTabIndex == i)
				{
					tabFlags |= ImGuiTabItemFlags_SetSelected;
				}

				// ImGui는 라벨로 탭을 식별하므로, ###를 사용하여 고정 ID 부여
				// 이렇게 하면 탭 이름이 바뀌어도 ImGui가 같은 탭으로 인식
				char tabLabel[256];
				sprintf_s(tabLabel, "%s###DynEdTab_%d", State->Name.ToString().c_str(), State->TabId);

				bool bTabSelected = ImGui::BeginTabItem(tabLabel, &open, tabFlags);
				if (bTabSelected)
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
			// 탭 전환 요청 처리 완료
			bRequestTabSwitch = false;
			RequestedTabIndex = -1;
			ImGui::EndTabBar();
		}

		// 탭이 닫혔으면 즉시 종료
		if (bTabClosed)
		{
			// 모든 탭이 닫히면 윈도우도 닫기
			if (Tabs.Num() == 0)
			{
				bIsOpen = false;
			}
			ImGui::End();
			return;
		}

		if (!ActiveState || Tabs.Num() == 0)
		{
			ImGui::End();
			return;
		}

		// 탭 전환 시 EmbeddedEditor에 해당 탭으로 전환
		if (PreviousTabIndex != ActiveTabIndex && ActiveState)
		{
			switch (ActiveState->Mode)
			{
			case EEditorMode::Skeletal:
				if (EmbeddedSkeletalEditor && ActiveState->EmbeddedSkeletalTabIndex >= 0)
				{
					EmbeddedSkeletalEditor->SetActiveTab(ActiveState->EmbeddedSkeletalTabIndex);
				}
				break;
			case EEditorMode::BlendSpace2D:
				if (EmbeddedBlendSpace2DEditor && !ActiveState->BlendSpaceFilePath.empty())
				{
					EmbeddedBlendSpace2DEditor->LoadBlendSpaceFile(WideToUTF8(ActiveState->BlendSpaceFilePath).c_str());
				}
				break;
			case EEditorMode::AnimGraph:
				if (EmbeddedStateMachineEditor && !ActiveState->StateMachineFilePath.empty())
				{
					EmbeddedStateMachineEditor->LoadStateMachineFile(WideToUTF8(ActiveState->StateMachineFilePath).c_str());
				}
				break;
			default:
				break;
			}

			// 애니메이션 자동 재생
			if (ActiveState->CurrentAnimation && ActiveState->PreviewActor)
			{
				USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
				if (SkelComp)
				{
					UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
					if (AnimInst && !ActiveState->bIsPlaying)
					{
						AnimInst->PlayAnimation(ActiveState->CurrentAnimation, ActiveState->PlaybackSpeed);
						ActiveState->bIsPlaying = true;
					}
				}
			}
		}

		ImVec2 pos = ImGui::GetWindowPos();
		ImVec2 size = ImGui::GetWindowSize();
		Rect.Left = pos.x;
		Rect.Top = pos.y;
		Rect.Right = pos.x + size.x;
		Rect.Bottom = pos.y + size.y;
		Rect.UpdateMinMax();

		ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		float totalWidth = contentAvail.x;
		float totalHeight = contentAvail.y;

		// Toolbar: 좌측 New/Save/Load + 우측 Mode 아이콘
		{
			// 공통 설정 - 모든 버튼 동일한 패딩
			float IconSize = 24.0f;
			float UniformPadding = 4.0f;  // 모든 방향 동일 패딩
			float ButtonSpacing = 10.0f;

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ButtonSpacing, 4));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(UniformPadding, UniformPadding));

			// === 좌측: New (StateMachine/BlendSpace만) / Save / Load 버튼 ===
			bool bShowNewButton = ActiveState &&
				(ActiveState->Mode == EEditorMode::AnimGraph || ActiveState->Mode == EEditorMode::BlendSpace2D);

			if (bShowNewButton)
			{
				if (IconNew && IconNew->GetShaderResourceView())
				{
					if (ImGui::ImageButton("##NewBtn", (ImTextureID)IconNew->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
					{
						OnNewClicked();
					}
				}
				else
				{
					if (ImGui::Button("New"))
					{
						OnNewClicked();
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Create new asset");
				}
				ImGui::SameLine();
			}

			if (IconSave && IconSave->GetShaderResourceView())
			{
				if (ImGui::ImageButton("##SaveBtn", (ImTextureID)IconSave->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
				{
					OnSaveClicked();
				}
			}
			else
			{
				if (ImGui::Button("Save"))
				{
					OnSaveClicked();
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Save current asset");
			}

			ImGui::SameLine();

			if (IconLoad && IconLoad->GetShaderResourceView())
			{
				if (ImGui::ImageButton("##LoadBtn", (ImTextureID)IconLoad->GetShaderResourceView(), ImVec2(IconSize, IconSize)))
				{
					OnLoadClicked();
				}
			}
			else
			{
				if (ImGui::Button("Load"))
				{
					OnLoadClicked();
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Open Content Browser to load asset");
			}

			ImGui::SameLine();

			// === 우측: Mode 아이콘 버튼 (가로 넓게) ===
			const char* ModeNames[] = { "Skeletal", "Animation", "AnimGraph", "BlendSpace", "PhysicsAsset" };
			const char* ModeTooltips[] = { "Skeletal Mesh Viewer", "Animation Editor", "Animation State Machine", "BlendSpace 2D Editor", "Physics Asset Editor" };
			EEditorMode Modes[] = { EEditorMode::Skeletal, EEditorMode::Animation, EEditorMode::AnimGraph, EEditorMode::BlendSpace2D, EEditorMode::PhysicsAsset };
			UTexture* ModeIcons[] = { IconModeSkeletal, IconModeAnimation, IconModeAnimGraph, IconModeBlendSpace, IconModePhysicsAsset };

			float ModePaddingX = 12.0f;  // 좌우 패딩 확장
			float ModeButtonWidth = IconSize + ModePaddingX * 2;
			float TotalButtonsWidth = ModeButtonWidth * 5 + ButtonSpacing * 4;

			// 우측 정렬
			float AvailWidth = ImGui::GetContentRegionAvail().x;
			float ModeStartX = ImGui::GetCursorPosX() + AvailWidth - TotalButtonsWidth;
			ImGui::SetCursorPosX(ModeStartX);

			// Mode 버튼용 패딩 (가로만 확장)
			ImGui::PopStyleVar();  // FramePadding 해제
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ModePaddingX, UniformPadding));

			for (int32 i = 0; i < 5; ++i)
			{
				if (i > 0)
				{
					ImGui::SameLine();
				}

				// 현재 탭의 모드와 비교
				bool bIsCurrentMode = (ActiveState && ActiveState->Mode == Modes[i]);

				if (bIsCurrentMode)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
				}

				// 아이콘 버튼
				bool bClicked = false;
				UTexture* Icon = ModeIcons[i];

				ImGui::PushID(i);
				if (Icon && Icon->GetShaderResourceView())
				{
					bClicked = ImGui::ImageButton("##ModeBtn", (ImTextureID)Icon->GetShaderResourceView(), ImVec2(IconSize, IconSize));
				}
				else
				{
					bClicked = ImGui::Button(ModeNames[i], ImVec2(ModeButtonWidth, IconSize + UniformPadding * 2));
				}
				ImGui::PopID();

				// 툴팁 표시
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", ModeTooltips[i]);
				}

				if (bClicked)
				{
					// 1탭 1기능: 같은 탭에서 모드 전환 금지
					// 해당 모드 + 같은 Mesh를 사용하는 탭이 있으면 이동, 없으면 새 탭 생성
					EEditorMode TargetMode = Modes[i];

					// 현재 Mesh 정보 가져오기
					USkeletalMesh* CurrentMesh = ActiveState ? ActiveState->CurrentMesh : nullptr;
					FString CurrentMeshPath = ActiveState ? ActiveState->LoadedMeshPath : "";

					// 해당 모드 + 같은 Mesh를 사용하는 탭 검색
					int32 ExistingTabIndex = -1;
					for (int32 j = 0; j < Tabs.Num(); ++j)
					{
						if (Tabs[j]->Mode == TargetMode)
						{
							// 같은 Mesh를 사용하는지 확인 (nullptr끼리는 같은 것으로 취급하지 않음)
							bool bSameMesh = (CurrentMesh != nullptr && Tabs[j]->CurrentMesh == CurrentMesh);
							bool bSamePath = (!CurrentMeshPath.empty() && Tabs[j]->LoadedMeshPath == CurrentMeshPath);

							if (bSameMesh || bSamePath)
							{
								ExistingTabIndex = j;
								break;
							}
						}
					}

					if (ExistingTabIndex >= 0)
					{
						// 기존 탭으로 이동 (ImGui 탭 전환 요청)
						bRequestTabSwitch = true;
						RequestedTabIndex = ExistingTabIndex;
						ActiveTabIndex = ExistingTabIndex;
						ActiveState = Tabs[ExistingTabIndex];
					}
					else
					{
						// 새 탭 생성
						FString TabName = ModeNames[i];
						if (!CurrentMeshPath.empty())
						{
							size_t LastSlash = CurrentMeshPath.find_last_of("/\\");
							TabName = (LastSlash != FString::npos) ? CurrentMeshPath.substr(LastSlash + 1) : CurrentMeshPath;
						}

						FEditorTabState* NewState = CreateNewTab(TabName.c_str(), TargetMode);
						if (NewState)
						{
							// 현재 Mesh 정보 복사
							NewState->CurrentMesh = CurrentMesh;
							NewState->LoadedMeshPath = CurrentMeshPath;

							Tabs.Add(NewState);
							ActiveTabIndex = (int32)Tabs.Num() - 1;
							ActiveState = NewState;
						}
					}
				}

				if (bIsCurrentMode)
				{
					ImGui::PopStyleColor(3);
				}
			}

			ImGui::PopStyleVar(2);  // FramePadding, ItemSpacing
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Toolbar 영역 만큼 높이 빼기
			contentAvail = ImGui::GetContentRegionAvail();
			totalWidth = contentAvail.x;
			totalHeight = contentAvail.y;
		}

		// ================================================================
		// Animation 모드: EmbeddedAnimationEditor 사용 (SSplitter 4패널 레이아웃)
		// ================================================================
		bool bIsAnimationMode = (ActiveState && ActiveState->Mode == EEditorMode::Animation);

		if (bIsAnimationMode)
		{
			// EmbeddedAnimationEditor 생성 (아직 없으면)
			if (!EmbeddedAnimationEditor)
			{
				EmbeddedAnimationEditor = new SAnimationWindow();
			}

			// 콘텐츠 영역 계산
			ImVec2 contentMin = ImGui::GetCursorScreenPos();

			FRect ContentRect;
			ContentRect.Left = contentMin.x;
			ContentRect.Top = contentMin.y;
			ContentRect.Right = contentMin.x + totalWidth;
			ContentRect.Bottom = contentMin.y + totalHeight;
			ContentRect.UpdateMinMax();

			// EmbeddedAnimationEditor 초기화 (한 번만)
			if (EmbeddedAnimationEditor->GetTabCount() == 0)
			{
				EmbeddedAnimationEditor->Initialize(ContentRect.Left, ContentRect.Top,
					ContentRect.GetWidth(), ContentRect.GetHeight(), World, Device);
				EmbeddedAnimationEditor->SetEmbeddedMode(true);

				// 현재 Skeletal Mesh가 있으면 함께 전달, 없으면 빈 탭
				if (ActiveState && ActiveState->CurrentMesh && !ActiveState->LoadedMeshPath.empty())
				{
					EmbeddedAnimationEditor->OpenNewTabWithMesh(ActiveState->CurrentMesh, ActiveState->LoadedMeshPath);
				}
				else
				{
					EmbeddedAnimationEditor->OpenNewTab("");
				}
			}

			// SSplitter 기반 레이아웃 렌더링
			EmbeddedAnimationEditor->RenderEmbedded(ContentRect);

			// CenterRect는 EmbeddedAnimationEditor의 ViewportRect 사용
			CenterRect = EmbeddedAnimationEditor->GetViewportRect();

			// Animation 탭 이름을 현재 Animation 이름으로 동기화
			FAnimationTabState* AnimState = EmbeddedAnimationEditor->GetActiveState();
			if (AnimState && AnimState->CurrentAnimation && ActiveState)
			{
				FString AnimName = AnimState->CurrentAnimation->GetName();
				if (!AnimName.empty())
				{
					ActiveState->Name = FName(AnimName.c_str());
				}
			}
		}
		else
		{
			// Animation 모드가 아닐 때도 EmbeddedAnimationEditor 유지 (상태 보존)
			// 소멸자에서만 해제

			// ================================================================
			// BlendSpace2D 모드: EmbeddedBlendSpace2DEditor 사용 (SSplitter 4패널 레이아웃)
			// ================================================================
			bool bIsBlendSpace2DMode = (ActiveState && ActiveState->Mode == EEditorMode::BlendSpace2D);

			if (bIsBlendSpace2DMode)
			{
				// EmbeddedBlendSpace2DEditor 생성 (아직 없으면)
				if (!EmbeddedBlendSpace2DEditor)
				{
					EmbeddedBlendSpace2DEditor = new SBlendSpace2DWindow();
				}

				// 콘텐츠 영역 계산
				ImVec2 contentMin = ImGui::GetCursorScreenPos();

				FRect ContentRect;
				ContentRect.Left = contentMin.x;
				ContentRect.Top = contentMin.y;
				ContentRect.Right = contentMin.x + totalWidth;
				ContentRect.Bottom = contentMin.y + totalHeight;
				ContentRect.UpdateMinMax();

				// EmbeddedBlendSpace2DEditor 초기화 (한 번만)
				if (EmbeddedBlendSpace2DEditor->GetTabCount() == 0)
				{
					EmbeddedBlendSpace2DEditor->Initialize(ContentRect.Left, ContentRect.Top,
						ContentRect.GetWidth(), ContentRect.GetHeight(), World, Device);
					EmbeddedBlendSpace2DEditor->SetEmbeddedMode(true);

					// BlendSpace 파일이 있으면 로드, 없으면 빈 탭 (메쉬 없이 시작)
					if (ActiveState && ActiveState->BlendSpace)
					{
						EmbeddedBlendSpace2DEditor->OpenNewTabWithBlendSpace(ActiveState->BlendSpace, "");
					}
					else if (ActiveState && !ActiveState->BlendSpaceFilePath.empty())
					{
						// 파일 경로가 있으면 먼저 탭 생성 후 파일 로드
						EmbeddedBlendSpace2DEditor->OpenNewTab("");
						EmbeddedBlendSpace2DEditor->LoadBlendSpaceFile(WideToUTF8(ActiveState->BlendSpaceFilePath).c_str());
					}
					else
					{
						// 새 BlendSpace는 메쉬 없이 빈 상태로 시작 (UE 동작과 동일)
						EmbeddedBlendSpace2DEditor->OpenNewTab("");
					}
				}

				// SSplitter 기반 레이아웃 렌더링
				EmbeddedBlendSpace2DEditor->RenderEmbedded(ContentRect);

				// CenterRect는 EmbeddedBlendSpace2DEditor의 ViewportRect 사용
				CenterRect = EmbeddedBlendSpace2DEditor->GetViewportRect();

				// BlendSpace 탭 이름 동기화
				FBlendSpace2DTabState* BS2DState = EmbeddedBlendSpace2DEditor->GetActiveState();
				if (BS2DState && ActiveState)
				{
					// 1순위: BlendSpace 파일 경로가 있으면 파일명 사용
					if (!BS2DState->FilePath.empty())
					{
						FString FilePath = BS2DState->FilePath;
						size_t LastSlash = FilePath.find_last_of("/\\");
						FString FileName = (LastSlash != FString::npos) ? FilePath.substr(LastSlash + 1) : FilePath;
						size_t DotPos = FileName.find_last_of('.');
						if (DotPos != FString::npos)
						{
							FileName = FileName.substr(0, DotPos);
						}
						ActiveState->Name = FName(FileName.c_str());
					}
					// 2순위: EmbeddedEditor의 TabName 사용 (New BlendSpace 등)
					else
					{
						FString TabNameStr = BS2DState->TabName.ToString();
						if (!TabNameStr.empty() && TabNameStr != "Untitled")
						{
							ActiveState->Name = BS2DState->TabName;
						}
					}
				}
			}
			else
			{
				// BlendSpace2D 모드가 아닐 때도 EmbeddedBlendSpace2DEditor 유지 (상태 보존)
				// 소멸자에서만 해제

				// ================================================================
				// AnimGraph 모드: EmbeddedStateMachineEditor 사용 (SSplitter 3패널 레이아웃)
				// ================================================================
				bool bIsAnimGraphMode = (ActiveState && ActiveState->Mode == EEditorMode::AnimGraph);

				if (bIsAnimGraphMode)
				{
					// EmbeddedStateMachineEditor 생성 (아직 없으면)
					if (!EmbeddedStateMachineEditor)
					{
						EmbeddedStateMachineEditor = new SAnimStateMachineWindow();
					}

					// 콘텐츠 영역 계산
					ImVec2 contentMin = ImGui::GetCursorScreenPos();

					FRect ContentRect;
					ContentRect.Left = contentMin.x;
					ContentRect.Top = contentMin.y;
					ContentRect.Right = contentMin.x + totalWidth;
					ContentRect.Bottom = contentMin.y + totalHeight;
					ContentRect.UpdateMinMax();

					// EmbeddedStateMachineEditor 초기화 (한 번만)
					if (EmbeddedStateMachineEditor->GetTabCount() == 0)
					{
						EmbeddedStateMachineEditor->Initialize(ContentRect.Left, ContentRect.Top,
							ContentRect.GetWidth(), ContentRect.GetHeight());
						EmbeddedStateMachineEditor->SetEmbeddedMode(true);

						// StateMachine 파일 경로가 있으면 해당 파일 로드 (독립 모드로 탭 생성)
						// 파일 경로가 없으면 빈 탭 생성
						if (ActiveState && !ActiveState->StateMachineFilePath.empty())
						{
							// 일시적으로 Embedded 모드 해제하여 새 탭으로 로드
							EmbeddedStateMachineEditor->SetEmbeddedMode(false);
							EmbeddedStateMachineEditor->LoadStateMachineFile(
								WideToUTF8(ActiveState->StateMachineFilePath).c_str());
							EmbeddedStateMachineEditor->SetEmbeddedMode(true);
						}
						else
						{
							EmbeddedStateMachineEditor->CreateNewEmptyTab();
						}
					}

					// SSplitter 기반 레이아웃 렌더링
					EmbeddedStateMachineEditor->RenderEmbedded(ContentRect);

					// CenterRect는 Graph 영역 (StateMachine은 뷰포트가 없으므로 ContentRect 사용)
					CenterRect = ContentRect;

					// StateMachine 탭 이름 동기화
					FGraphState* SMState = EmbeddedStateMachineEditor->GetActiveState();
					if (SMState && ActiveState)
					{
						if (!SMState->Name.empty())
						{
							ActiveState->Name = FName(SMState->Name.c_str());
						}
					}
				}
				else
				{
					// AnimGraph 모드가 아닐 때도 EmbeddedStateMachineEditor 유지 (상태 보존)
					// 소멸자에서만 해제

					// ================================================================
					// Skeletal 모드: EmbeddedSkeletalEditor 사용 (SSplitter 기반 레이아웃)
					// ================================================================
					bool bIsSkeletalMode = (ActiveState && ActiveState->Mode == EEditorMode::Skeletal);

					if (bIsSkeletalMode)
					{
						// EmbeddedSkeletalEditor 생성 (아직 없으면)
						if (!EmbeddedSkeletalEditor)
						{
							EmbeddedSkeletalEditor = new SSkeletalEditorWindow();
						}

						// 콘텐츠 영역 계산
						ImVec2 contentMin = ImGui::GetCursorScreenPos();

						FRect ContentRect;
						ContentRect.Left = contentMin.x;
						ContentRect.Top = contentMin.y;
						ContentRect.Right = contentMin.x + totalWidth;
						ContentRect.Bottom = contentMin.y + totalHeight;
						ContentRect.UpdateMinMax();

						// EmbeddedSkeletalEditor 초기화 (한 번만)
						if (EmbeddedSkeletalEditor->GetTabCount() == 0)
						{
							EmbeddedSkeletalEditor->Initialize(ContentRect.Left, ContentRect.Top,
								ContentRect.GetWidth(), ContentRect.GetHeight(), World, Device, true);
							EmbeddedSkeletalEditor->SetEmbeddedMode(true);

							// 현재 Skeletal Mesh가 있으면 함께 전달
							if (ActiveState && ActiveState->CurrentMesh && !ActiveState->LoadedMeshPath.empty())
							{
								EmbeddedSkeletalEditor->OpenNewTabWithMesh(ActiveState->CurrentMesh, ActiveState->LoadedMeshPath);
								ActiveState->EmbeddedSkeletalTabIndex = EmbeddedSkeletalEditor->GetActiveTabIndex();
							}
							else
							{
								EmbeddedSkeletalEditor->OpenNewTab("");
								if (ActiveState)
								{
									ActiveState->EmbeddedSkeletalTabIndex = EmbeddedSkeletalEditor->GetActiveTabIndex();
								}
							}
						}

						// SSplitter 기반 레이아웃 렌더링
						EmbeddedSkeletalEditor->RenderEmbedded(ContentRect);

						// CenterRect는 EmbeddedSkeletalEditor의 ViewportRect 사용
						CenterRect = EmbeddedSkeletalEditor->GetViewportRect();

						// Skeletal 탭 이름 동기화
						ViewerState* SkelState = EmbeddedSkeletalEditor->GetActiveState();
						if (SkelState && ActiveState)
						{
							// 파일 경로에서 파일 이름 추출하여 탭 이름으로 사용 (확장자 제거)
							if (!ActiveState->LoadedMeshPath.empty())
							{
								FString MeshPath = ActiveState->LoadedMeshPath;
								size_t LastSlash = MeshPath.find_last_of("/\\");
								FString FileName = (LastSlash != FString::npos) ? MeshPath.substr(LastSlash + 1) : MeshPath;
								size_t DotPos = FileName.find_last_of('.');
								if (DotPos != FString::npos)
								{
									FileName = FileName.substr(0, DotPos);
								}
								ActiveState->Name = FName(FileName.c_str());
							}
							else
							{
								// 메쉬가 없으면 EmbeddedEditor의 Name 사용
								FString NameStr = SkelState->Name.ToString();
								if (!NameStr.empty() && NameStr != "Untitled")
								{
									ActiveState->Name = SkelState->Name;
								}
							}
						}
					}
					else
					{
						// ================================================================
						// PhysicsAsset 모드: EmbeddedPhysicsAssetEditor 사용 (SSplitter 기반 레이아웃)
						// ================================================================
						bool bIsPhysicsAssetMode = (ActiveState && ActiveState->Mode == EEditorMode::PhysicsAsset);

						if (bIsPhysicsAssetMode)
						{
							// EmbeddedPhysicsAssetEditor 생성 (아직 없으면)
							if (!EmbeddedPhysicsAssetEditor)
							{
								EmbeddedPhysicsAssetEditor = new SPhysicsAssetEditorWindow();
							}

							// 콘텐츠 영역 계산
							ImVec2 contentMin = ImGui::GetCursorScreenPos();

							FRect ContentRect;
							ContentRect.Left = contentMin.x;
							ContentRect.Top = contentMin.y;
							ContentRect.Right = contentMin.x + totalWidth;
							ContentRect.Bottom = contentMin.y + totalHeight;
							ContentRect.UpdateMinMax();

							// EmbeddedPhysicsAssetEditor 초기화 (한 번만)
							if (!EmbeddedPhysicsAssetEditor->GetActiveState())
							{
								EmbeddedPhysicsAssetEditor->Initialize(ContentRect.Left, ContentRect.Top,
									ContentRect.GetWidth(), ContentRect.GetHeight(), World, Device, true);
								EmbeddedPhysicsAssetEditor->SetEmbeddedMode(true);
							}

							// SSplitter 기반 레이아웃 렌더링
							EmbeddedPhysicsAssetEditor->RenderEmbedded(ContentRect);

							// CenterRect는 EmbeddedPhysicsAssetEditor의 ViewportRect 사용
							CenterRect = EmbeddedPhysicsAssetEditor->GetViewportRect();

							// PhysicsAsset 탭 이름 설정
							if (ActiveState && ActiveState->Name.ToString().empty())
							{
								ActiveState->Name = FName("Physics Asset");
							}
						}
					}  // else (non-Skeletal mode = PhysicsAsset mode)
				}  // else (non-AnimGraph mode)
			}  // else (non-BlendSpace2D mode)
		}  // else (non-Animation mode)
	}
	ImGui::End();

	// If collapsed or not visible
	if (!bViewerVisible)
	{
		CenterRect = FRect(0, 0, 0, 0);
		CenterRect.UpdateMinMax();
	}

	// 윈도우 닫기는 USlateManager::Render()에서 IsOpen() 체크 후 처리됨
	// OnRender 내부에서 self-delete 하면 크래시 발생

	bRequestFocus = false;
}

// ============================================================================
// OnUpdate - SPreviewWindow에서 복사
// ============================================================================

void SDynamicEditorWindow::OnUpdate(float DeltaSeconds)
{
	// 공통 처리: World Tick 및 기즈모 모드 전환
	if (ActiveState && ActiveState->World)
	{
		ActiveState->World->Tick(DeltaSeconds);
		if (bIsFocused && ActiveState->World->GetGizmoActor())
		{
			ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
		}
	}

	// 현재 모드에 따라 적절한 에디터 업데이트
	if (ActiveState)
	{
		if (ActiveState->Mode == EEditorMode::Animation && EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->OnUpdate(DeltaSeconds);
			return;
		}
		if (ActiveState->Mode == EEditorMode::BlendSpace2D && EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->OnUpdate(DeltaSeconds);
			return;
		}
		if (ActiveState->Mode == EEditorMode::AnimGraph && EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->OnUpdate(DeltaSeconds);
			return;
		}
		if (ActiveState->Mode == EEditorMode::Skeletal && EmbeddedSkeletalEditor)
		{
			EmbeddedSkeletalEditor->OnUpdate(DeltaSeconds);
			return;
		}
		if (ActiveState->Mode == EEditorMode::PhysicsAsset && EmbeddedPhysicsAssetEditor)
		{
			EmbeddedPhysicsAssetEditor->OnUpdate(DeltaSeconds);
			return;
		}
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ActiveState->Client)
	{
		ActiveState->Client->Tick(DeltaSeconds);
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

		bool bShouldTick = (ActiveState->Mode == EEditorMode::Animation) && ActiveState->bIsPlaying;
		SkelComp->SetTickEnabled(bShouldTick);
	}
}

// ============================================================================
// OnMouseMove/OnMouseDown/OnMouseUp - SPreviewWindow에서 복사
// ============================================================================

void SDynamicEditorWindow::OnMouseMove(FVector2D MousePos)
{
	// 현재 모드에 따라 적절한 에디터로 라우팅
	if (ActiveState)
	{
		if (ActiveState->Mode == EEditorMode::Animation && EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->OnMouseMove(MousePos);
			return;
		}
		if (ActiveState->Mode == EEditorMode::BlendSpace2D && EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->OnMouseMove(MousePos);
			return;
		}
		if (ActiveState->Mode == EEditorMode::AnimGraph && EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->OnMouseMove(MousePos);
			return;
		}
		if (ActiveState->Mode == EEditorMode::Skeletal && EmbeddedSkeletalEditor)
		{
			EmbeddedSkeletalEditor->OnMouseMove(MousePos);
			return;
		}
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	AGizmoActor* Gizmo = ActiveState->World ? ActiveState->World->GetGizmoActor() : nullptr;
	bool bGizmoDragging = (Gizmo && Gizmo->GetbIsDragging());

	if (bGizmoDragging || CenterRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
		ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
	}
}

void SDynamicEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	// 현재 모드에 따라 적절한 에디터로 라우팅
	if (ActiveState)
	{
		if (ActiveState->Mode == EEditorMode::Animation && EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->OnMouseDown(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::BlendSpace2D && EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->OnMouseDown(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::AnimGraph && EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->OnMouseDown(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::Skeletal && EmbeddedSkeletalEditor)
		{
			EmbeddedSkeletalEditor->OnMouseDown(MousePos, Button);
			return;
		}
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	if (CenterRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
		ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

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

					FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
					FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

					FRay Ray = MakeRayFromViewport(
						Camera->GetViewMatrix(),
						Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), ActiveState->Viewport),
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

void SDynamicEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	// 현재 모드에 따라 적절한 에디터로 라우팅
	if (ActiveState)
	{
		if (ActiveState->Mode == EEditorMode::Animation && EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->OnMouseUp(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::BlendSpace2D && EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->OnMouseUp(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::AnimGraph && EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->OnMouseUp(MousePos, Button);
			return;
		}
		if (ActiveState->Mode == EEditorMode::Skeletal && EmbeddedSkeletalEditor)
		{
			EmbeddedSkeletalEditor->OnMouseUp(MousePos, Button);
			return;
		}
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	if (CenterRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
		ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

void SDynamicEditorWindow::OnRenderViewport()
{
	// Phase 2에서 사용
}

// ============================================================================
// Asset Loading
// ============================================================================

void SDynamicEditorWindow::LoadSkeletalMesh(const FString& Path)
{
	if (Path.empty())
	{
		return;
	}

	// 경로 정규화 (슬래시 통일, 소문자 변환)
	auto NormalizePath = [](const FString& InPath) -> FString {
		FString Result = InPath;
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(Result.begin(), Result.end(), Result.begin(), ::tolower);
		return Result;
	};

	FString NormalizedPath = NormalizePath(Path);

	// 같은 파일이 열린 탭 찾기, 없으면 새로 생성
	FEditorTabState* TargetState = nullptr;

	// 같은 파일이 열린 Skeletal 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		if (Tabs[i]->Mode == EEditorMode::Skeletal && NormalizePath(Tabs[i]->LoadedMeshPath) == NormalizedPath)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			TargetState = Tabs[i];
			bRequestFocus = true;  // 기존 탭으로 전환 시 포커스 요청

			// EmbeddedSkeletalEditor 탭도 전환
			if (EmbeddedSkeletalEditor && Tabs[i]->EmbeddedSkeletalTabIndex >= 0)
			{
				EmbeddedSkeletalEditor->SetActiveTab(Tabs[i]->EmbeddedSkeletalTabIndex);
			}
			return;  // 이미 열린 파일이므로 추가 로드 불필요
		}
	}

	// 같은 파일이 없으면 새 탭 생성
	bool bNewTabCreated = false;
	if (!TargetState)
	{
		FString TabName = Path;
		size_t LastSlash = Path.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = Path.substr(LastSlash + 1);
		}

		TargetState = CreateNewTab(TabName.c_str(), EEditorMode::Skeletal);
		if (TargetState)
		{
			Tabs.Add(TargetState);
			ActiveTabIndex = Tabs.Num() - 1;
			ActiveState = TargetState;
			bNewTabCreated = true;
		}
	}

	if (!TargetState || !TargetState->PreviewActor)
	{
		return;
	}

	USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
	if (Mesh)
	{
		TargetState->PreviewActor->SetSkeletalMesh(Path);
		TargetState->CurrentMesh = Mesh;
		TargetState->LoadedMeshPath = Path;

		// 탭 이름을 로드된 에셋 이름으로 업데이트
		FString TabName = Path;
		size_t LastSlash = Path.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = Path.substr(LastSlash + 1);
		}
		TargetState->Name = FName(TabName.c_str());

		if (auto* Skeletal = TargetState->PreviewActor->GetSkeletalMeshComponent())
		{
			Skeletal->SetVisibility(TargetState->bShowMesh);
		}
		TargetState->bBoneLinesDirty = true;
		if (auto* LineComp = TargetState->PreviewActor->GetBoneLineComponent())
		{
			LineComp->ClearLines();
			LineComp->SetLineVisible(TargetState->bShowBones);
		}

		// 이미 로드된 모든 AnimSequence를 이 메쉬에 추가
		TArray<UAnimSequence*> AllAnimSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		for (UAnimSequence* AnimSeq : AllAnimSequences)
		{
			if (AnimSeq && !AnimSeq->GetName().empty())
			{
				Mesh->AddAnimation(AnimSeq);
			}
		}

		// EmbeddedSkeletalEditor가 이미 초기화되어 있으면 새 탭 생성
		if (bNewTabCreated && EmbeddedSkeletalEditor && EmbeddedSkeletalEditor->GetTabCount() > 0)
		{
			EmbeddedSkeletalEditor->OpenNewTabWithMesh(Mesh, Path);
			TargetState->EmbeddedSkeletalTabIndex = EmbeddedSkeletalEditor->GetActiveTabIndex();
		}

		// 바닥판과 카메라 위치 설정 (AABB 기반)
		SetupFloorAndCamera(TargetState);
	}
}

void SDynamicEditorWindow::LoadAnimation(const FString& Path)
{
	if (Path.empty())
	{
		return;
	}

	// Animation 모드로 전환
	SetEditorMode(EEditorMode::Animation);

	// EmbeddedAnimationEditor에서 애니메이션 파일 열기
	if (EmbeddedAnimationEditor)
	{
		EmbeddedAnimationEditor->OpenNewTab(Path);
	}
}

void SDynamicEditorWindow::LoadAnimGraph(const FString& Path)
{
	// 경로 정규화 (슬래시 통일, 소문자 변환)
	auto NormalizePath = [](const FString& InPath) -> FString {
		FString Result = InPath;
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(Result.begin(), Result.end(), Result.begin(), ::tolower);
		return Result;
	};
	auto NormalizeWidePath = [](const FWideString& InPath) -> FString {
		FString Result;
		Result.reserve(InPath.size());
		for (wchar_t c : InPath)
		{
			Result.push_back(static_cast<char>(c));
		}
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(Result.begin(), Result.end(), Result.begin(), ::tolower);
		return Result;
	};

	FString NormalizedPath = NormalizePath(Path);

	// 파일명 추출 (확장자 제거)
	FString TabName = Path;
	size_t LastSlash = Path.find_last_of("/\\");
	if (LastSlash != FString::npos)
	{
		TabName = Path.substr(LastSlash + 1);
	}
	size_t DotPos = TabName.find_last_of('.');
	if (DotPos != FString::npos)
	{
		TabName = TabName.substr(0, DotPos);
	}

	// 같은 파일이 열린 탭 찾기
	FEditorTabState* TargetState = nullptr;

	// 같은 파일이 열린 AnimGraph 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		if (Tabs[i]->Mode == EEditorMode::AnimGraph && NormalizeWidePath(Tabs[i]->StateMachineFilePath) == NormalizedPath)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			TargetState = Tabs[i];
			bRequestFocus = true;
			return;  // 이미 열린 파일이므로 추가 로드 불필요
		}
	}

	// 현재 탭이 AnimGraph 모드이고 수정되지 않은 New 상태면 재사용
	if (!TargetState && ActiveState && ActiveState->Mode == EEditorMode::AnimGraph && ActiveState->StateMachineFilePath.empty())
	{
		TargetState = ActiveState;
		TargetState->Name = FName(TabName.c_str());
	}

	// 재사용 가능한 탭이 없으면 새 탭 생성
	if (!TargetState)
	{
		TargetState = CreateNewTab(TabName.c_str(), EEditorMode::AnimGraph);
		if (TargetState)
		{
			if (ActiveState)
			{
				TargetState->CurrentMesh = ActiveState->CurrentMesh;
				TargetState->LoadedMeshPath = ActiveState->LoadedMeshPath;
			}
			Tabs.Add(TargetState);
			ActiveTabIndex = (int32)Tabs.Num() - 1;
			ActiveState = TargetState;
		}
	}

	if (TargetState)
	{
		TargetState->StateMachineFilePath = FWideString(Path.begin(), Path.end());

		// EmbeddedStateMachineEditor가 이미 초기화되어 있으면 파일 로드
		// GetTabCount() == 0이면 아직 초기화 안 됐으므로 OnRender에서 처리
		if (EmbeddedStateMachineEditor && EmbeddedStateMachineEditor->GetTabCount() > 0)
		{
			EmbeddedStateMachineEditor->LoadStateMachineFile(Path.c_str());
		}
	}
}

void SDynamicEditorWindow::LoadBlendSpace(const FString& Path)
{
	// 경로 정규화 (슬래시 통일, 소문자 변환)
	auto NormalizePath = [](const FString& InPath) -> FString {
		FString Result = InPath;
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(Result.begin(), Result.end(), Result.begin(), ::tolower);
		return Result;
	};
	auto NormalizeWidePath = [](const FWideString& InPath) -> FString {
		FString Result;
		Result.reserve(InPath.size());
		for (wchar_t c : InPath)
		{
			Result.push_back(static_cast<char>(c));
		}
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(Result.begin(), Result.end(), Result.begin(), ::tolower);
		return Result;
	};

	FString NormalizedPath = NormalizePath(Path);

	// 파일명 추출 (확장자 제거)
	FString TabName = Path;
	size_t LastSlash = Path.find_last_of("/\\");
	if (LastSlash != FString::npos)
	{
		TabName = Path.substr(LastSlash + 1);
	}
	size_t DotPos = TabName.find_last_of('.');
	if (DotPos != FString::npos)
	{
		TabName = TabName.substr(0, DotPos);
	}

	// 같은 파일이 열린 탭 찾기
	FEditorTabState* TargetState = nullptr;

	// 같은 파일이 열린 BlendSpace2D 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		if (Tabs[i]->Mode == EEditorMode::BlendSpace2D && NormalizeWidePath(Tabs[i]->BlendSpaceFilePath) == NormalizedPath)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			TargetState = Tabs[i];
			bRequestFocus = true;
			return;  // 이미 열린 파일이므로 추가 로드 불필요
		}
	}

	// 현재 탭이 BlendSpace2D 모드이고 수정되지 않은 New 상태면 재사용
	if (!TargetState && ActiveState && ActiveState->Mode == EEditorMode::BlendSpace2D && ActiveState->BlendSpaceFilePath.empty())
	{
		TargetState = ActiveState;
		TargetState->Name = FName(TabName.c_str());
	}

	// 재사용 가능한 탭이 없으면 새 탭 생성
	if (!TargetState)
	{
		TargetState = CreateNewTab(TabName.c_str(), EEditorMode::BlendSpace2D);
		if (TargetState)
		{
			if (ActiveState)
			{
				TargetState->CurrentMesh = ActiveState->CurrentMesh;
				TargetState->LoadedMeshPath = ActiveState->LoadedMeshPath;
			}
			Tabs.Add(TargetState);
			ActiveTabIndex = (int32)Tabs.Num() - 1;
			ActiveState = TargetState;
		}
	}

	if (TargetState)
	{
		TargetState->BlendSpaceFilePath = FWideString(Path.begin(), Path.end());

		// EmbeddedBlendSpace2DEditor가 이미 존재하면 파일 로드
		if (EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->LoadBlendSpaceFile(Path.c_str());
		}
	}
}

void SDynamicEditorWindow::SetBlendSpace(UBlendSpace2D* InBlendSpace)
{
	// 1탭 1기능: BlendSpace2D 모드 탭 찾거나 새로 생성
	FEditorTabState* TargetState = nullptr;

	// 기존 BlendSpace2D 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		if (Tabs[i]->Mode == EEditorMode::BlendSpace2D)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			TargetState = Tabs[i];
			break;
		}
	}

	// 없으면 새 탭 생성
	if (!TargetState)
	{
		// 파일 경로가 없으므로 "New BlendSpace" 사용
		TargetState = CreateNewTab("New BlendSpace", EEditorMode::BlendSpace2D);
		if (TargetState)
		{
			if (ActiveState)
			{
				TargetState->CurrentMesh = ActiveState->CurrentMesh;
				TargetState->LoadedMeshPath = ActiveState->LoadedMeshPath;
			}
			Tabs.Add(TargetState);
			ActiveTabIndex = (int32)Tabs.Num() - 1;
			ActiveState = TargetState;
		}
	}

	if (TargetState)
	{
		TargetState->BlendSpace = InBlendSpace;

		// EmbeddedBlendSpace2DEditor가 이미 존재하면 BlendSpace 전달
		if (EmbeddedBlendSpace2DEditor && InBlendSpace)
		{
			EmbeddedBlendSpace2DEditor->OpenNewTabWithBlendSpace(InBlendSpace, "");
		}
	}
}

void SDynamicEditorWindow::SetAnimStateMachine(UAnimStateMachine* InStateMachine)
{
	// 1탭 1기능: AnimGraph 모드 탭 찾거나 새로 생성
	FEditorTabState* TargetState = nullptr;

	// 기존 AnimGraph 탭 검색
	for (int32 i = 0; i < Tabs.Num(); ++i)
	{
		if (Tabs[i]->Mode == EEditorMode::AnimGraph)
		{
			ActiveTabIndex = i;
			ActiveState = Tabs[i];
			TargetState = Tabs[i];
			break;
		}
	}

	// 없으면 새 탭 생성
	if (!TargetState)
	{
		FString TabName = InStateMachine ? InStateMachine->GetName() : "AnimGraph";

		TargetState = CreateNewTab(TabName.c_str(), EEditorMode::AnimGraph);
		if (TargetState)
		{
			if (ActiveState)
			{
				TargetState->CurrentMesh = ActiveState->CurrentMesh;
				TargetState->LoadedMeshPath = ActiveState->LoadedMeshPath;
			}
			Tabs.Add(TargetState);
			ActiveTabIndex = (int32)Tabs.Num() - 1;
			ActiveState = TargetState;
		}
	}

	if (TargetState)
	{
		TargetState->StateMachine = InStateMachine;
	}
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

// ============================================================================
// Bone Transform Helpers
// ============================================================================

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
	Delta.Rotation = AnimTransform.Rotation.IsIdentity() ? NewTransform.Rotation : (AnimTransform.Rotation.Inverse() * NewTransform.Rotation).GetNormalized();
	Delta.Scale3D = FVector(
		AnimTransform.Scale3D.X != 0.0f ? NewTransform.Scale3D.X / AnimTransform.Scale3D.X : 1.0f,
		AnimTransform.Scale3D.Y != 0.0f ? NewTransform.Scale3D.Y / AnimTransform.Scale3D.Y : 1.0f,
		AnimTransform.Scale3D.Z != 0.0f ? NewTransform.Scale3D.Z / AnimTransform.Scale3D.Z : 1.0f
	);

	SkelComp->SetBoneDelta(State->SelectedBoneIndex, Delta);
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
// Timeline Controls - 기본 구현 (PreviewWindow_Timeline.cpp 참조)
// ============================================================================

void SDynamicEditorWindow::RenderTimelineControls(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation)
	{
		ImGui::TextDisabled("No animation selected");
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		ImGui::TextDisabled("No animation data");
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();
	float FrameRate = DataModel->GetFrameRate().AsDecimal();

	// 재생 컨트롤 버튼
	ImGui::BeginGroup();

	float buttonSize = 24.0f;
	float spacing = 2.0f;

	// ToFront 버튼
	UTexture* ToFrontIcon = (State->CurrentAnimationTime <= 0.001f) ? IconGoToFrontOff : IconGoToFront;
	if (ToFrontIcon && ToFrontIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToFront", (ImTextureID)ToFrontIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			TimelineToFront(State);
		}
	}

	ImGui::SameLine(0, spacing);

	// Step Backwards 버튼
	UTexture* StepBackIcon = (State->CurrentAnimationTime <= 0.001f) ? IconStepBackwardsOff : IconStepBackwards;
	if (StepBackIcon && StepBackIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepBack", (ImTextureID)StepBackIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			TimelineToPrevious(State);
		}
	}

	ImGui::SameLine(0, spacing);

	// Play/Pause 버튼
	UTexture* PlayPauseIcon = State->bIsPlaying ? IconPause : IconPlay;
	if (PlayPauseIcon && PlayPauseIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##PlayPause", (ImTextureID)PlayPauseIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			TimelinePlay(State);
		}
	}

	ImGui::SameLine(0, spacing);

	// Step Forward 버튼
	UTexture* StepForwardIcon = (State->CurrentAnimationTime >= MaxTime - 0.001f) ? IconStepForwardOff : IconStepForward;
	if (StepForwardIcon && StepForwardIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepForward", (ImTextureID)StepForwardIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			TimelineToNext(State);
		}
	}

	ImGui::SameLine(0, spacing);

	// ToEnd 버튼
	UTexture* ToEndIcon = (State->CurrentAnimationTime >= MaxTime - 0.001f) ? IconGoToEndOff : IconGoToEnd;
	if (ToEndIcon && ToEndIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToEnd", (ImTextureID)ToEndIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			TimelineToEnd(State);
		}
	}

	ImGui::SameLine(0, spacing);

	// Loop 버튼
	UTexture* LoopIcon = State->bLoopAnimation ? IconLoop : IconLoopOff;
	if (LoopIcon && LoopIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Loop", (ImTextureID)LoopIcon->GetShaderResourceView(), ImVec2(buttonSize, buttonSize)))
		{
			State->bLoopAnimation = !State->bLoopAnimation;
		}
	}

	ImGui::EndGroup();

	ImGui::SameLine();

	// 시간/프레임 표시
	int32 CurrentFrame = (FrameRate > 0.0f) ? static_cast<int32>(State->CurrentAnimationTime * FrameRate) : 0;
	ImGui::Text("Frame: %d / %d  Time: %.2f / %.2f", CurrentFrame, TotalFrames, State->CurrentAnimationTime, MaxTime);

	// 타임라인 슬라이더
	ImGui::Spacing();
	ImGui::PushItemWidth(-1);
	float SliderTime = State->CurrentAnimationTime;
	if (ImGui::SliderFloat("##TimeSlider", &SliderTime, 0.0f, MaxTime, "%.2f"))
	{
		State->CurrentAnimationTime = SliderTime;

		// AnimInstance 시간 동기화
		if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
		{
			UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
			if (AnimInst)
			{
				AnimInst->SetPosition(SliderTime);
			}
		}
	}
	ImGui::PopItemWidth();

	// 재생 속도
	ImGui::Spacing();
	ImGui::Text("Speed:");
	ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::SliderFloat("##PlaybackSpeed", &State->PlaybackSpeed, 0.0f, 2.0f, "%.2fx");
	ImGui::PopItemWidth();
}

void SDynamicEditorWindow::RenderTimeline(FEditorTabState* State)
{
	// 상세 타임라인 렌더링 (필요시 구현)
}

void SDynamicEditorWindow::TimelineToFront(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}

	State->CurrentAnimationTime = 0.0f;
	RefreshAnimationFrame(State);
}

void SDynamicEditorWindow::TimelineToPrevious(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation || !State->CurrentAnimation->GetDataModel())
	{
		return;
	}

	float FrameRate = State->CurrentAnimation->GetDataModel()->GetFrameRate().AsDecimal();
	if (FrameRate > 0.0f)
	{
		State->CurrentAnimationTime = std::max(0.0f, State->CurrentAnimationTime - (1.0f / FrameRate));
		RefreshAnimationFrame(State);
	}
}

void SDynamicEditorWindow::TimelineReverse(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}

	State->PlaybackSpeed = -FMath::Abs(State->PlaybackSpeed);
}

void SDynamicEditorWindow::TimelineRecord(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}

	State->bShowRecordDialog = true;
}

void SDynamicEditorWindow::TimelinePlay(FEditorTabState* State)
{
	if (!State || !State->PreviewActor)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

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
		if (AnimInst->IsPlaying())
		{
			AnimInst->StopAnimation();
			State->bIsPlaying = false;
		}
		else
		{
			if (State->CurrentAnimation)
			{
				AnimInst->PlayAnimation(State->CurrentAnimation, State->PlaybackSpeed);
			}
			State->bIsPlaying = true;
		}
	}
}

void SDynamicEditorWindow::TimelineToNext(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation || !State->CurrentAnimation->GetDataModel())
	{
		return;
	}

	float FrameRate = State->CurrentAnimation->GetDataModel()->GetFrameRate().AsDecimal();
	float MaxTime = State->CurrentAnimation->GetDataModel()->GetPlayLength();
	if (FrameRate > 0.0f)
	{
		State->CurrentAnimationTime = std::min(MaxTime, State->CurrentAnimationTime + (1.0f / FrameRate));
		RefreshAnimationFrame(State);
	}
}

void SDynamicEditorWindow::TimelineToEnd(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation || !State->CurrentAnimation->GetDataModel())
	{
		return;
	}

	State->CurrentAnimationTime = State->CurrentAnimation->GetDataModel()->GetPlayLength();
	RefreshAnimationFrame(State);
}

void SDynamicEditorWindow::RefreshAnimationFrame(FEditorTabState* State)
{
	if (!State || !State->PreviewActor)
	{
		return;
	}

	USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
	if (!SkelComp)
	{
		return;
	}

	UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
	if (AnimInst)
	{
		AnimInst->SetPosition(State->CurrentAnimationTime);
	}
}

// Timeline Drawing Helpers (Stub)
void SDynamicEditorWindow::DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, FEditorTabState* State)
{
	// 상세 구현은 PreviewWindow_Timeline.cpp 참조
}

void SDynamicEditorWindow::DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State)
{
	// 상세 구현은 PreviewWindow_Timeline.cpp 참조
}

void SDynamicEditorWindow::DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime)
{
	// 상세 구현은 PreviewWindow_Timeline.cpp 참조
}

void SDynamicEditorWindow::DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State)
{
	// 상세 구현은 PreviewWindow_Timeline.cpp 참조
}

void SDynamicEditorWindow::DrawNotifyTracksPanel(FEditorTabState* State, float StartTime, float EndTime)
{
	// 상세 구현은 PreviewWindow_Timeline.cpp 참조
}

void SDynamicEditorWindow::RebuildNotifyTracks(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation)
	{
		return;
	}

	State->NotifyTrackNames.clear();
	State->UsedTrackNumbers.clear();

	// 기본 Track 생성
	State->NotifyTrackNames.push_back("Track 1");
	State->UsedTrackNumbers.insert(1);
}

void SDynamicEditorWindow::ScanNotifyLibrary()
{
	AvailableNotifyClasses.clear();
	AvailableNotifyStateClasses.clear();

	// Data/Scripts/Notify 폴더 스캔
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

	// Data/Scripts/NotifyState 폴더 스캔
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

void SDynamicEditorWindow::CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState)
{
	// 상세 구현은 PreviewWindow.cpp 참조
}

void SDynamicEditorWindow::OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState)
{
	FString Dir = bIsNotifyState ? "Data/Scripts/NotifyState/" : "Data/Scripts/Notify/";
	FString FilePath = Dir + NotifyClassName + ".lua";

	ShellExecuteA(nullptr, "open", FilePath.c_str(), nullptr, nullptr, SW_SHOW);
}

// ============================================================================
// New/Save/Load 핸들러
// ============================================================================

void SDynamicEditorWindow::OnNewClicked()
{
	if (!ActiveState)
	{
		return;
	}

	// AnimGraph 또는 BlendSpace2D 모드에서만 New 동작
	switch (ActiveState->Mode)
	{
	case EEditorMode::AnimGraph:
		{
			// 기존 탭 중 "New State Machine"의 최대 번호 찾기
			int32 MaxNumber = 0;
			for (const auto& Tab : Tabs)
			{
				if (Tab->Mode == EEditorMode::AnimGraph)
				{
					FString TabName = Tab->Name.ToString();
					if (TabName.find("New State Machine") != FString::npos)
					{
						// "New State Machine" 뒤에 숫자가 있으면 추출
						size_t SpacePos = TabName.rfind(' ');
						if (SpacePos != FString::npos && SpacePos > 0)
						{
							FString NumStr = TabName.substr(SpacePos + 1);
							try
							{
								int32 Num = std::stoi(NumStr);
								MaxNumber = std::max(MaxNumber, Num);
							}
							catch (...) {}
						}
						// "New State Machine" 자체는 1번으로 취급
						if (TabName == "New State Machine")
						{
							MaxNumber = std::max(MaxNumber, 1);
						}
					}
				}
			}

			// 새 탭 이름 생성
			FString NewTabName = (MaxNumber == 0) ? "New State Machine" : ("New State Machine " + std::to_string(MaxNumber + 1));

			// DynamicEditorWindow에 새 탭 생성
			FEditorTabState* NewState = CreateNewTab(NewTabName.c_str(), EEditorMode::AnimGraph);
			if (NewState)
			{
				Tabs.Add(NewState);
				ActiveTabIndex = Tabs.Num() - 1;
				ActiveState = NewState;

				// EmbeddedStateMachineEditor에도 새 탭 생성
				if (EmbeddedStateMachineEditor)
				{
					EmbeddedStateMachineEditor->CreateNewEmptyTab();
				}
			}
		}
		break;

	case EEditorMode::BlendSpace2D:
		{
			// 기존 탭 중 "New BlendSpace"의 최대 번호 찾기
			int32 MaxNumber = 0;
			for (const auto& Tab : Tabs)
			{
				if (Tab->Mode == EEditorMode::BlendSpace2D)
				{
					FString TabName = Tab->Name.ToString();
					if (TabName.find("New BlendSpace") != FString::npos)
					{
						size_t SpacePos = TabName.rfind(' ');
						if (SpacePos != FString::npos && SpacePos > 0)
						{
							FString NumStr = TabName.substr(SpacePos + 1);
							try
							{
								int32 Num = std::stoi(NumStr);
								MaxNumber = std::max(MaxNumber, Num);
							}
							catch (...) {}
						}
						if (TabName == "New BlendSpace")
						{
							MaxNumber = std::max(MaxNumber, 1);
						}
					}
				}
			}

			// 새 탭 이름 생성
			FString NewTabName = (MaxNumber == 0) ? "New BlendSpace" : ("New BlendSpace " + std::to_string(MaxNumber + 1));

			// DynamicEditorWindow에 새 탭 생성
			FEditorTabState* NewState = CreateNewTab(NewTabName.c_str(), EEditorMode::BlendSpace2D);
			if (NewState)
			{
				Tabs.Add(NewState);
				ActiveTabIndex = Tabs.Num() - 1;
				ActiveState = NewState;

				// EmbeddedBlendSpace2DEditor에도 새 탭 생성
				if (EmbeddedBlendSpace2DEditor)
				{
					EmbeddedBlendSpace2DEditor->CreateNewEmptyTab();
				}
			}
		}
		break;

	default:
		break;
	}
}

void SDynamicEditorWindow::OnSaveClicked()
{
	if (!ActiveState)
	{
		return;
	}

	// 현재 모드에 따라 적절한 에디터의 Save 호출
	switch (ActiveState->Mode)
	{
	case EEditorMode::Skeletal:
		// Skeletal은 뷰어 모드이므로 저장할 내용 없음
		UE_LOG("Skeletal mode: No asset to save (viewer only)");
		break;

	case EEditorMode::Animation:
		if (EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->SaveCurrentAnimation();
		}
		break;

	case EEditorMode::AnimGraph:
		if (EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->SaveCurrentStateMachine();
		}
		break;

	case EEditorMode::BlendSpace2D:
		if (EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->SaveCurrentBlendSpace();
		}
		break;
	}
}

void SDynamicEditorWindow::OnLoadClicked()
{
	// Content Browser 열기 - 현재 모드에 맞는 폴더로 이동
	FString InitialPath = "";

	if (ActiveState)
	{
		switch (ActiveState->Mode)
		{
		case EEditorMode::Skeletal:
			InitialPath = "FBX";  // 스켈레탈 메시 (FBX 파일)
			break;
		case EEditorMode::Animation:
			InitialPath = "Animation";  // 애니메이션 파일
			break;
		case EEditorMode::BlendSpace2D:
			InitialPath = "Blend";  // 블렌드 스페이스 파일
			break;
		case EEditorMode::AnimGraph:
			InitialPath = "StateMachine";  // 스테이트 머신 파일
			break;
		}
	}

	SLATE.OpenContentBrowser(InitialPath);
}

void SDynamicEditorWindow::OpenFileInCurrentMode(const FString& FilePath)
{
	if (!ActiveState || FilePath.empty())
	{
		return;
	}

	// 확장자에 따라 적절한 모드로 열기
	FString LowerPath = FilePath;
	std::transform(LowerPath.begin(), LowerPath.end(), LowerPath.begin(), ::tolower);

	if (LowerPath.ends_with(".fbx") || LowerPath.ends_with(".skeletalmesh"))
	{
		// Skeletal Mesh
		if (ActiveState->Mode == EEditorMode::Skeletal)
		{
			LoadSkeletalMesh(FilePath);
		}
	}
	else if (LowerPath.ends_with(".anim"))
	{
		// Animation
		if (ActiveState->Mode == EEditorMode::Animation && EmbeddedAnimationEditor)
		{
			EmbeddedAnimationEditor->LoadAnimationFile(FilePath.c_str());
		}
	}
	else if (LowerPath.ends_with(".statemachine"))
	{
		// State Machine
		if (ActiveState->Mode == EEditorMode::AnimGraph && EmbeddedStateMachineEditor)
		{
			EmbeddedStateMachineEditor->LoadStateMachineFile(FilePath.c_str());
		}
	}
	else if (LowerPath.ends_with(".blend2d"))
	{
		// BlendSpace 2D
		if (ActiveState->Mode == EEditorMode::BlendSpace2D && EmbeddedBlendSpace2DEditor)
		{
			EmbeddedBlendSpace2DEditor->LoadBlendSpaceFile(FilePath.c_str());
		}
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

void SDynamicEditorWindow::SetupFloorAndCamera(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}
	SkeletalViewerBootstrap::SetupFloorAndCamera(State->PreviewActor, State->FloorActor, State->Client);
}
