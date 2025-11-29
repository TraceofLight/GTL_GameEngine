#include "pch.h"
#include "DynamicEditorWindow.h"
#include "AnimationWindow.h"
#include "BlendSpace2DWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
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

	ScanNotifyLibrary();

	// 기본 탭 생성
	FEditorTabState* State = CreateNewTab("Untitled", EEditorMode::Skeletal);
	if (State)
	{
		Tabs.Add(State);
		ActiveState = State;
		ActiveTabIndex = 0;

		if (State->Viewport)
		{
			State->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
		}
	}

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

	// 탭이 없으면 렌더링하지 않음
	if (Tabs.Num() == 0)
	{
		bIsOpen = false;
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
			if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
			{
				char label[32];
				sprintf_s(label, "Editor %d", Tabs.Num() + 1);
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

		// 탭이 닫혔으면 즉시 종료
		if (bTabClosed)
		{
			ImGui::End();
			return;
		}

		if (!ActiveState)
		{
			ImGui::End();
			return;
		}

		// 탭 전환 시 애니메이션 자동 재생
		if (PreviousTabIndex != ActiveTabIndex)
		{
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

		// Mode 선택 버튼 (Toolbar)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

			const char* ModeNames[] = { "Skeletal", "Animation", "AnimGraph", "BlendSpace" };
			EEditorMode Modes[] = { EEditorMode::Skeletal, EEditorMode::Animation, EEditorMode::AnimGraph, EEditorMode::BlendSpace2D };

			for (int32 i = 0; i < 4; ++i)
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

				if (ImGui::Button(ModeNames[i]))
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

			ImGui::PopStyleVar(2);
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
			// Animation 모드가 아닐 때 EmbeddedAnimationEditor 해제
			if (EmbeddedAnimationEditor)
			{
				delete EmbeddedAnimationEditor;
				EmbeddedAnimationEditor = nullptr;
			}

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

					// BlendSpace나 Mesh가 있으면 함께 전달
					if (ActiveState && ActiveState->BlendSpace)
					{
						EmbeddedBlendSpace2DEditor->OpenNewTabWithBlendSpace(ActiveState->BlendSpace, "");
					}
					else if (ActiveState && ActiveState->CurrentMesh && !ActiveState->LoadedMeshPath.empty())
					{
						EmbeddedBlendSpace2DEditor->OpenNewTabWithMesh(ActiveState->CurrentMesh, ActiveState->LoadedMeshPath);
					}
					else
					{
						EmbeddedBlendSpace2DEditor->OpenNewTab("");
					}
				}

				// SSplitter 기반 레이아웃 렌더링
				EmbeddedBlendSpace2DEditor->RenderEmbedded(ContentRect);

				// CenterRect는 EmbeddedBlendSpace2DEditor의 ViewportRect 사용
				CenterRect = EmbeddedBlendSpace2DEditor->GetViewportRect();

				// BlendSpace 탭 이름을 현재 BlendSpace 이름으로 동기화
				FBlendSpace2DTabState* BS2DState = EmbeddedBlendSpace2DEditor->GetActiveState();
				if (BS2DState && BS2DState->BlendSpace && ActiveState)
				{
					FString BSName = BS2DState->BlendSpace->GetName();
					if (!BSName.empty())
					{
						ActiveState->Name = FName(BSName.c_str());
					}
				}
			}
			else
			{
				// BlendSpace2D 모드가 아닐 때 EmbeddedBlendSpace2DEditor 해제
				if (EmbeddedBlendSpace2DEditor)
				{
					delete EmbeddedBlendSpace2DEditor;
					EmbeddedBlendSpace2DEditor = nullptr;
				}

				// Skeletal 모드인지 확인
				bool bIsSkeletalMode = (ActiveState->Mode == EEditorMode::Skeletal);

				// 패널 너비 계산 (SPreviewWindow와 동일)
				bool bShowLeftPanel = bIsSkeletalMode;  // Skeletal 모드에서만 좌측 패널 표시
				float leftWidth = bShowLeftPanel ? (totalWidth * LeftPanelRatio) : 0.0f;
				float rightWidth = totalWidth * RightPanelRatio;
				float centerWidth = totalWidth - leftWidth - rightWidth;

				// else 블록은 Animation 모드가 아닐 때만 실행됨
				float mainAreaHeight = totalHeight;

				// 패널 간 간격 제거
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

				// ================================================================
				// 좌측 패널 - Asset Browser & Bone Hierarchy (Skeletal 모드에서만)
				// ================================================================
				if (bShowLeftPanel)
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
					ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, mainAreaHeight), true, ImGuiWindowFlags_NoScrollbar);
					ImGui::PopStyleVar();

					// Asset Browser Section
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
					ImGui::Indent(8.0f);
					ImGui::Text("Asset Browser");
					ImGui::Unindent(8.0f);
					ImGui::PopStyleColor();

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					// Display Options
					ImGui::Text("Display Options:");
					ImGui::Spacing();

					if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
					{
						if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
						{
							ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(ActiveState->bShowMesh);
						}
					}

					ImGui::SameLine();
					if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
					{
						if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
						{
							ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
						}
						if (ActiveState->bShowBones)
						{
							ActiveState->bBoneLinesDirty = true;
						}
					}

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					// Bone Hierarchy Section
					ImGui::Text("Bone Hierarchy:");
					ImGui::Spacing();

					if (!ActiveState->CurrentMesh)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
						ImGui::TextWrapped("No skeletal mesh loaded.");
						ImGui::PopStyleColor();
					}
					else
					{
						const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
						if (!Skeleton || Skeleton->Bones.IsEmpty())
						{
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
							ImGui::TextWrapped("This mesh has no skeleton data.");
							ImGui::PopStyleColor();
						}
						else
						{
							// Scrollable tree view
							ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), true);
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

							std::function<void(int32)> DrawNode = [&](int32 Index)
							{
								const bool bLeaf = Children[Index].IsEmpty();
								ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

								if (bLeaf)
								{
									treeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
								}

								if (ActiveState->ExpandedBoneIndices.count(Index) > 0)
								{
									ImGui::SetNextItemOpen(true);
								}

								if (ActiveState->SelectedBoneIndex == Index)
								{
									treeFlags |= ImGuiTreeNodeFlags_Selected;
								}

								ImGui::PushID(Index);
								const char* Label = Bones[Index].Name.c_str();

								if (ActiveState->SelectedBoneIndex == Index)
								{
									ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
									ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
									ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
								}

								bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, treeFlags, "%s", Label ? Label : "<noname>");

								if (ActiveState->SelectedBoneIndex == Index)
								{
									ImGui::PopStyleColor(3);
									ImGui::SetScrollHereY(0.5f);
								}

								if (ImGui::IsItemToggledOpen())
								{
									if (open)
									{
										ActiveState->ExpandedBoneIndices.insert(Index);
									}
									else
									{
										ActiveState->ExpandedBoneIndices.erase(Index);
									}
								}

								if (ImGui::IsItemClicked())
								{
									if (ActiveState->SelectedBoneIndex != Index)
									{
										ActiveState->SelectedBoneIndex = Index;
										ActiveState->bBoneLinesDirty = true;

										ExpandToSelectedBone(ActiveState, Index);

										if (ActiveState->PreviewActor && ActiveState->World && ActiveState->CurrentMesh)
										{
											AGizmoActor* GizmoActor = ActiveState->World->GetGizmoActor();
											USkeletalMeshComponent* SkelComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
											if (GizmoActor && SkelComp)
											{
												GizmoActor->SetBoneTarget(SkelComp, Index);
												GizmoActor->SetbRender(true);
											}
											ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
										}
									}
								}

								if (!bLeaf && open)
								{
									for (int32 Child : Children[Index])
									{
										DrawNode(Child);
									}
									ImGui::TreePop();
								}
								ImGui::PopID();
							};

							for (int32 i = 0; i < Bones.size(); ++i)
							{
								if (Bones[i].ParentIndex < 0)
								{
									DrawNode(i);
								}
							}

							ImGui::EndChild();
						}
					}

					ImGui::EndChild();

					ImGui::SameLine(0, 0);
				}

				// ================================================================
				// 중앙 패널 - Viewport
				// ================================================================
				ImGui::BeginChild("ViewportArea", ImVec2(centerWidth, mainAreaHeight), true, ImGuiWindowFlags_NoScrollbar);

				ImGui::BeginChild("ViewportRenderArea", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);

				const ImVec2 ViewportSize = ImGui::GetContentRegionAvail();
				const uint32 NewWidth = static_cast<uint32>(ViewportSize.x);
				const uint32 NewHeight = static_cast<uint32>(ViewportSize.y);

				if (NewWidth > 0 && NewHeight > 0)
				{
					UpdateViewportRenderTarget(NewWidth, NewHeight);
					RenderToPreviewRenderTarget();

					ID3D11ShaderResourceView* PreviewSRV = GetPreviewShaderResourceView();
					if (PreviewSRV)
					{
						ImVec2 ImagePos = ImGui::GetCursorScreenPos();

						ImTextureID TextureID = reinterpret_cast<ImTextureID>(PreviewSRV);
						ImGui::Image(TextureID, ViewportSize);

						// CenterRect 업데이트
						CenterRect.Left = ImagePos.x;
						CenterRect.Top = ImagePos.y;
						CenterRect.Right = ImagePos.x + ViewportSize.x;
						CenterRect.Bottom = ImagePos.y + ViewportSize.y;
						CenterRect.UpdateMinMax();

						// 드래그 앤 드랍 타겟 (Content Browser에서 파일 드롭)
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
							{
								const char* filePath = static_cast<const char*>(payload->Data);
								FString path(filePath);

								if (path.ends_with(".anim"))
								{
									// Animation은 별도의 SAnimationWindow에서 처리
									USlateManager::GetInstance().OpenAnimationWindowWithFile(path.c_str());
								}
								else if (path.ends_with(".fbx") || path.ends_with(".FBX"))
								{
									// FBX에서 애니메이션 로드 시 SAnimationWindow에서 처리
									USlateManager::GetInstance().OpenAnimationWindowWithFile(path.c_str());
								}
							}
							ImGui::EndDragDropTarget();
						}
					}
				}
				else
				{
					CenterRect = FRect(0, 0, 0, 0);
					CenterRect.UpdateMinMax();
				}

				// Recording 상태 오버레이
				if (ActiveState->bIsRecording)
				{
					ImGui::SetCursorPos(ImVec2(ViewportSize.x - 180, ViewportSize.y - 40));
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.05f, 0.05f, 0.85f));
					ImGui::BeginChild("RecordingOverlay", ImVec2(170, 30), true, ImGuiWindowFlags_NoScrollbar);
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
					ImGui::Text("REC");
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
					ImGui::Text("%d frames", ActiveState->RecordedFrames.Num());
					ImGui::PopStyleColor();
					ImGui::EndChild();
					ImGui::PopStyleColor();
				}

				ImGui::EndChild();  // ViewportRenderArea
				ImGui::EndChild();  // ViewportArea

				ImGui::SameLine(0, 0);

				// ================================================================
				// 우측 패널 - Properties
				// ================================================================
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
				ImGui::BeginChild("RightPanel", ImVec2(rightWidth, mainAreaHeight), true);
				ImGui::PopStyleVar();

				// Panel header
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Indent(8.0f);
				ImGui::Text(bIsAnimationMode ? "Animation Properties" : "Bone Properties");
				ImGui::Unindent(8.0f);
				ImGui::PopStyleColor();

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// Bone Properties 편집 UI
				if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh)
				{
					const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
					if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
					{
						const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

						ImGui::Text("Bone Name");
						ImGui::SameLine(100.0f);
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
						ImGui::Text("%s", SelectedBone.Name.c_str());
						ImGui::PopStyleColor();

						ImGui::Spacing();

						if (ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_DefaultOpen))
						{
							if (!ActiveState->bBoneRotationEditing)
							{
								UpdateBoneTransformFromSkeleton(ActiveState);
							}

							float labelWidth = 70.0f;
							float inputWidth = (rightWidth - labelWidth - 40.0f) / 3.0f;

							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
							ImGui::Text("Bone");
							ImGui::PopStyleColor();
							ImGui::Spacing();

							// Location
							ImGui::Text("Location");
							ImGui::SameLine(labelWidth);
							ImGui::PushItemWidth(inputWidth);
							bool bLocationChanged = false;
							bLocationChanged |= ImGui::DragFloat("##BoneLocX", &ActiveState->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "%.2f");
							ImGui::SameLine();
							bLocationChanged |= ImGui::DragFloat("##BoneLocY", &ActiveState->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "%.2f");
							ImGui::SameLine();
							bLocationChanged |= ImGui::DragFloat("##BoneLocZ", &ActiveState->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "%.2f");
							ImGui::PopItemWidth();

							if (bLocationChanged)
							{
								ApplyBoneTransform(ActiveState);
								ActiveState->bBoneLinesDirty = true;
							}

							// Rotation
							ImGui::Text("Rotation");
							ImGui::SameLine(labelWidth);
							ImGui::PushItemWidth(inputWidth);
							bool bRotationChanged = false;

							if (ImGui::IsAnyItemActive())
							{
								ActiveState->bBoneRotationEditing = true;
							}

							bRotationChanged |= ImGui::DragFloat("##BoneRotX", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "%.2f");
							ImGui::SameLine();
							bRotationChanged |= ImGui::DragFloat("##BoneRotY", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "%.2f");
							ImGui::SameLine();
							bRotationChanged |= ImGui::DragFloat("##BoneRotZ", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "%.2f");
							ImGui::PopItemWidth();

							if (!ImGui::IsAnyItemActive())
							{
								ActiveState->bBoneRotationEditing = false;
							}

							if (bRotationChanged)
							{
								ApplyBoneTransform(ActiveState);
								ActiveState->bBoneLinesDirty = true;
							}

							// Scale
							ImGui::Text("Scale");
							ImGui::SameLine(labelWidth);
							ImGui::PushItemWidth(inputWidth);
							bool bScaleChanged = false;
							bScaleChanged |= ImGui::DragFloat("##BoneScaleX", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "%.2f");
							ImGui::SameLine();
							bScaleChanged |= ImGui::DragFloat("##BoneScaleY", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "%.2f");
							ImGui::SameLine();
							bScaleChanged |= ImGui::DragFloat("##BoneScaleZ", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "%.2f");
							ImGui::PopItemWidth();

							if (bScaleChanged)
							{
								ApplyBoneTransform(ActiveState);
								ActiveState->bBoneLinesDirty = true;
							}
						}
					}
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
					ImGui::TextWrapped("Select a bone from the hierarchy to edit its transform properties.");
					ImGui::PopStyleColor();
				}

				ImGui::EndChild();  // RightPanel

				ImGui::PopStyleVar();  // ItemSpacing

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
	// Animation 모드: EmbeddedAnimationEditor 업데이트
	if (EmbeddedAnimationEditor)
	{
		EmbeddedAnimationEditor->OnUpdate(DeltaSeconds);
		return;
	}

	// BlendSpace2D 모드: EmbeddedBlendSpace2DEditor 업데이트
	if (EmbeddedBlendSpace2DEditor)
	{
		EmbeddedBlendSpace2DEditor->OnUpdate(DeltaSeconds);
		return;
	}

	if (!ActiveState || !ActiveState->Viewport)
	{
		return;
	}

	// Tick the preview world
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
	// Animation 모드: EmbeddedAnimationEditor로 라우팅
	if (EmbeddedAnimationEditor)
	{
		EmbeddedAnimationEditor->OnMouseMove(MousePos);
		return;
	}

	// BlendSpace2D 모드: EmbeddedBlendSpace2DEditor로 라우팅
	if (EmbeddedBlendSpace2DEditor)
	{
		EmbeddedBlendSpace2DEditor->OnMouseMove(MousePos);
		return;
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
	// Animation 모드: EmbeddedAnimationEditor로 라우팅
	if (EmbeddedAnimationEditor)
	{
		EmbeddedAnimationEditor->OnMouseDown(MousePos, Button);
		return;
	}

	// BlendSpace2D 모드: EmbeddedBlendSpace2DEditor로 라우팅
	if (EmbeddedBlendSpace2DEditor)
	{
		EmbeddedBlendSpace2DEditor->OnMouseDown(MousePos, Button);
		return;
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
	// Animation 모드: EmbeddedAnimationEditor로 라우팅
	if (EmbeddedAnimationEditor)
	{
		EmbeddedAnimationEditor->OnMouseUp(MousePos, Button);
		return;
	}

	// BlendSpace2D 모드: EmbeddedBlendSpace2DEditor로 라우팅
	if (EmbeddedBlendSpace2DEditor)
	{
		EmbeddedBlendSpace2DEditor->OnMouseUp(MousePos, Button);
		return;
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

		// 탭 이름을 로드된 에셋 이름으로 업데이트
		FString TabName = Path;
		size_t LastSlash = Path.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = Path.substr(LastSlash + 1);
		}
		ActiveState->Name = FName(TabName.c_str());

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

		// 이미 로드된 모든 AnimSequence를 이 메쉬에 추가
		TArray<UAnimSequence*> AllAnimSequences = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		for (UAnimSequence* AnimSeq : AllAnimSequences)
		{
			if (AnimSeq && !AnimSeq->GetName().empty())
			{
				Mesh->AddAnimation(AnimSeq);
			}
		}
	}
}

void SDynamicEditorWindow::LoadAnimation(const FString& Path)
{
	if (Path.empty())
	{
		return;
	}

	// Animation 모드는 별도의 SAnimationWindow에서 처리
	USlateManager::GetInstance().OpenAnimationWindowWithFile(Path.c_str());
}

void SDynamicEditorWindow::LoadAnimGraph(const FString& Path)
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
		FString TabName = "AnimGraph";
		size_t LastSlash = Path.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = Path.substr(LastSlash + 1);
		}

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
	}
}

void SDynamicEditorWindow::LoadBlendSpace(const FString& Path)
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
		FString TabName = "BlendSpace";
		size_t LastSlash = Path.find_last_of("/\\");
		if (LastSlash != FString::npos)
		{
			TabName = Path.substr(LastSlash + 1);
		}

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
		FString TabName = InBlendSpace ? InBlendSpace->GetName() : "BlendSpace";

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
		TargetState->BlendSpace = InBlendSpace;
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
