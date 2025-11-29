#include "pch.h"
#include "DynamicEditorPanels.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/AssetManagement/Texture.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/BlendSpace2D.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Editor/PlatformProcess.h"
#include "SelectionManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include <filesystem>

extern const FString GDataDir;

// ============================================================================
// SAssetPanel
// ============================================================================

void SAssetPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Asset##DynamicAsset", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
			ImGui::Text("Skeletal Mesh");
			ImGui::Separator();

			// 메시 경로 입력
			ImGui::Text("Mesh Path:");
			ImGui::PushItemWidth(-1.0f);
			ImGui::InputTextWithHint("##MeshPath", "Browse for FBX...", State->MeshPathBuffer, sizeof(State->MeshPathBuffer));
			ImGui::PopItemWidth();

			ImGui::Spacing();

			// 로드 버튼
			if (ImGui::Button("Browse...", ImVec2(-1, 30)))
			{
				std::filesystem::path widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
				if (!widePath.empty())
				{
					std::string s = widePath.string();
					strncpy_s(State->MeshPathBuffer, s.c_str(), sizeof(State->MeshPathBuffer) - 1);
				}
			}

			if (strlen(State->MeshPathBuffer) > 0)
			{
				if (ImGui::Button("Load Mesh", ImVec2(-1, 30)))
				{
					Owner->LoadSkeletalMesh(State->MeshPathBuffer);
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 표시 옵션
			ImGui::Text("Display Options:");
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

// ============================================================================
// SViewportPreviewPanel
// ============================================================================

void SViewportPreviewPanel::OnRender()
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

	if (ImGui::Begin("##DynamicViewport", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
			ImVec2 vpMin = ImGui::GetCursorScreenPos();
			ImVec2 vpSize = ImGui::GetContentRegionAvail();

			uint32 vpWidth = (uint32)std::max(1.0f, vpSize.x);
			uint32 vpHeight = (uint32)std::max(1.0f, vpSize.y);

			if (vpWidth > 0 && vpHeight > 0)
			{
				Owner->UpdateViewportRenderTarget(vpWidth, vpHeight);
				Owner->RenderToPreviewRenderTarget();

				ID3D11ShaderResourceView* srv = Owner->GetPreviewShaderResourceView();
				if (srv)
				{
					ImGui::Image((ImTextureID)srv, vpSize);
				}
				else
				{
					ImGui::Dummy(vpSize);
				}

				ContentRect = FRect(vpMin.x, vpMin.y, vpMin.x + vpSize.x, vpMin.y + vpSize.y);
			}
		}
	}
	ImGui::End();
}

void SViewportPreviewPanel::OnUpdate(float DeltaSeconds)
{
	// 추가 업데이트 로직 (필요시)
}

// ============================================================================
// SBoneTreePanel
// ============================================================================

void SBoneTreePanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Skeleton Tree##DynamicBoneTree", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
			RenderBoneTree(State);
		}
	}
	ImGui::End();
}

void SBoneTreePanel::RenderBoneTree(FEditorTabState* State)
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

// ============================================================================
// SBoneDetailPanel
// ============================================================================

void SBoneDetailPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoTitleBar;

	if (ImGui::Begin("Details##DynamicDetail", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
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

void SBoneDetailPanel::RenderAssetDetailsTab(FEditorTabState* State)
{
	if (!State->CurrentMesh)
	{
		ImGui::TextDisabled("No skeletal mesh loaded.");
		return;
	}

	RenderMaterialSlotsSection(State);
	RenderLODPickerSection(State);
}

void SBoneDetailPanel::RenderMaterialSlotsSection(FEditorTabState* State)
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

				ImGui::Text("Start Index: %u", Group.StartIndex);
				ImGui::Text("Index Count: %u", Group.IndexCount);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
}

void SBoneDetailPanel::RenderLODPickerSection(FEditorTabState* State)
{
	if (ImGui::CollapsingHeader("LOD Picker", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int32 CurrentLOD = 0;
		const char* LODOptions[] = { "Auto (LOD 0)", "LOD 0", "LOD 1", "LOD 2" };

		ImGui::Text("LOD");
		ImGui::SameLine(100);
		ImGui::SetNextItemWidth(-1);
		ImGui::Combo("##LODCombo", &CurrentLOD, LODOptions, IM_ARRAYSIZE(LODOptions));
	}
}

void SBoneDetailPanel::RenderDetailsTab(FEditorTabState* State)
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

void SBoneDetailPanel::RenderBoneSection(FEditorTabState* State)
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

void SBoneDetailPanel::RenderTransformsSection(FEditorTabState* State)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

	float buttonWidth = (ImGui::GetContentRegionAvail().x - 4) / 3.0f;

	bool bBoneSelected = (State->BoneTransformModeFlags & 0x1) != 0;
	bool bRefSelected = (State->BoneTransformModeFlags & 0x2) != 0;
	bool bMeshSelected = (State->BoneTransformModeFlags & 0x4) != 0;

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
			State->BoneTransformModeFlags ^= 0x1;
			if (State->BoneTransformModeFlags == 0)
			{
				State->BoneTransformModeFlags = 0x1;
			}
		}
		else
		{
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
			RenderTransformEditor("RefEdit", State->ReferenceBoneLocation, State->ReferenceBoneRotation, State->ReferenceBoneScale, true);
		}
	}

	if (State->BoneTransformModeFlags & 0x4)
	{
		if (ImGui::CollapsingHeader("Mesh Relative##MeshTransform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderTransformEditor("MeshEdit", State->MeshRelativeBoneLocation, State->MeshRelativeBoneRotation, State->MeshRelativeBoneScale, true);
		}
	}
}

bool SBoneDetailPanel::RenderTransformEditor(const char* Label, FVector& Location, FVector& Rotation, FVector& Scale, bool bReadOnly)
{
	bool bChanged = false;

	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	ImGui::PushID(Label);

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

void SBoneDetailPanel::SyncBoneTransformFromViewport(FEditorTabState* State)
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

	FTransform LocalTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);
	State->EditBoneLocation = LocalTransform.Translation;
	State->EditBoneRotation = LocalTransform.Rotation.ToEulerZYXDeg();
	State->EditBoneScale = LocalTransform.Scale3D;

	const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
	if (Skeleton && State->SelectedBoneIndex < Skeleton->RefLocalPose.Num())
	{
		const FTransform& RefPose = Skeleton->RefLocalPose[State->SelectedBoneIndex];
		State->ReferenceBoneLocation = RefPose.Translation;
		State->ReferenceBoneRotation = RefPose.Rotation.ToEulerZYXDeg();
		State->ReferenceBoneScale = RefPose.Scale3D;
	}

	FTransform ComponentTransform = SkelComp->GetBoneWorldTransform(State->SelectedBoneIndex);
	FTransform ActorTransform = State->PreviewActor->GetActorTransform();
	FVector RelativeLocation = ComponentTransform.Translation - ActorTransform.Translation;
	RelativeLocation = ActorTransform.Rotation.Inverse().RotateVector(RelativeLocation);

	State->MeshRelativeBoneLocation = RelativeLocation;
	State->MeshRelativeBoneRotation = (ActorTransform.Rotation.Inverse() * ComponentTransform.Rotation).ToEulerZYXDeg();
	State->MeshRelativeBoneScale = ComponentTransform.Scale3D;
}

void SBoneDetailPanel::ApplyBoneTransformToViewport(FEditorTabState* State)
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

	FTransform NewTransform;
	NewTransform.Translation = State->EditBoneLocation;
	NewTransform.Rotation = FQuat::MakeFromEulerZYX(State->EditBoneRotation);
	NewTransform.Scale3D = State->EditBoneScale;

	// Animation 재생 중일 때 Delta로 처리
	if (State->CurrentAnimation && State->bIsPlaying)
	{
		FTransform AnimTransform = SkelComp->GetBoneLocalTransform(State->SelectedBoneIndex);

		const FTransform* ExistingDelta = SkelComp->GetBoneDelta(State->SelectedBoneIndex);
		FTransform BasePose = AnimTransform;
		if (ExistingDelta)
		{
			FTransform InverseDelta;
			InverseDelta.Translation = -ExistingDelta->Translation;
			InverseDelta.Rotation = ExistingDelta->Rotation.Inverse();
			InverseDelta.Scale3D = FVector(1.0f / ExistingDelta->Scale3D.X, 1.0f / ExistingDelta->Scale3D.Y, 1.0f / ExistingDelta->Scale3D.Z);

			BasePose.Translation = AnimTransform.Translation + InverseDelta.Translation;
			BasePose.Rotation = InverseDelta.Rotation * AnimTransform.Rotation;
			BasePose.Scale3D = AnimTransform.Scale3D * InverseDelta.Scale3D;
		}

		FTransform Delta;
		Delta.Translation = NewTransform.Translation - BasePose.Translation;
		Delta.Rotation = NewTransform.Rotation * BasePose.Rotation.Inverse();
		Delta.Scale3D = FVector(NewTransform.Scale3D.X / BasePose.Scale3D.X, NewTransform.Scale3D.Y / BasePose.Scale3D.Y, NewTransform.Scale3D.Z / BasePose.Scale3D.Z);

		SkelComp->SetBoneDelta(State->SelectedBoneIndex, Delta);
	}

	SkelComp->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
	State->bBoneLinesDirty = true;
}

// ============================================================================
// STimelinePanel
// ============================================================================

STimelinePanel::STimelinePanel(SDynamicEditorWindow* InOwner) : SEditorPanel(InOwner)
{
	// 타임라인 아이콘 로드
	IconGoToFront = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/ToFront.dds");
	IconGoToFrontOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/ToFront_Off.dds");
	IconStepBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/StepBackwards.dds");
	IconStepBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/StepBackwards_Off.dds");
	IconBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Backwards.dds");
	IconBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Backwards_Off.dds");
	IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Record.dds");
	IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Pause.dds");
	IconPauseOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Pause_Off.dds");
	IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Play.dds");
	IconPlayOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Play_Off.dds");
	IconStepForward = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/StepForward.dds");
	IconStepForwardOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/StepForward_Off.dds");
	IconGoToEnd = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/ToEnd.dds");
	IconGoToEndOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/ToEnd_Off.dds");
	IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Loop.dds");
	IconLoopOff = UResourceManager::GetInstance().Load<UTexture>("Data/Textures/UI/Animation/Loop_Off.dds");

	ScanNotifyLibrary();
}

STimelinePanel::~STimelinePanel()
{
}

void STimelinePanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Timeline##DynamicTimeline", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
			RenderTimelineControls(State);
		}
	}
	ImGui::End();
}

void STimelinePanel::OnUpdate(float DeltaSeconds)
{
	FEditorTabState* State = Owner->GetActiveState();
	if (!State || !State->CurrentAnimation)
	{
		return;
	}

	// 재생 중이면 시간 업데이트
	if (State->bIsPlaying && State->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			if (UAnimInstance* AnimInst = SkelComp->GetAnimInstance())
			{
				State->CurrentAnimationTime = AnimInst->GetCurrentTime();
			}
		}
	}
}

void STimelinePanel::RenderTimelineControls(FEditorTabState* State)
{
	if (!State)
	{
		return;
	}

	// 애니메이션이 없을 때 안내 메시지
	if (!State->CurrentAnimation)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		float availHeight = ImGui::GetContentRegionAvail().y;
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availHeight * 0.4f);

		const char* message = "Select an animation from the list to view timeline";
		float textWidth = ImGui::CalcTextSize(message).x;
		float windowWidth = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (windowWidth - textWidth) * 0.5f);
		ImGui::Text("%s", message);
		ImGui::PopStyleColor();
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();

	// Working Range 기본값 설정
	if (State->WorkingRangeEndFrame < 0)
	{
		State->WorkingRangeEndFrame = TotalFrames;
	}
	if (State->ViewRangeEndFrame < 0)
	{
		State->ViewRangeEndFrame = TotalFrames;
	}

	// Timeline 영역
	ImGui::BeginChild("TimelineInner", ImVec2(0, -40), true);
	{
		RenderTimeline(State);
	}
	ImGui::EndChild();

	ImGui::Separator();

	// 재생 컨트롤 버튼들
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

	float ButtonSize = 20.0f;
	ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);

	// ToFront |<<
	if (IconGoToFront && IconGoToFront->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToFront", IconGoToFront->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToFront(State);
		}
	}
	else
	{
		if (ImGui::Button("|<<", ButtonSizeVec))
		{
			TimelineToFront(State);
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("To Front"); }
	ImGui::SameLine();

	// ToPrevious |<
	if (IconStepBackwards && IconStepBackwards->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepBackwards", IconStepBackwards->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToPrevious(State);
		}
	}
	else
	{
		if (ImGui::Button("|<", ButtonSizeVec))
		{
			TimelineToPrevious(State);
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Previous Frame"); }
	ImGui::SameLine();

	// Reverse <<
	if (IconBackwards && IconBackwards->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Backwards", IconBackwards->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineReverse(State);
		}
	}
	else
	{
		if (ImGui::Button("<<", ButtonSizeVec))
		{
			TimelineReverse(State);
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reverse"); }
	ImGui::SameLine();

	// Record
	bool bWasRecording = State->bIsRecording;
	if (bWasRecording)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
	}
	if (IconRecord && IconRecord->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Record", IconRecord->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineRecord(State);
		}
	}
	else
	{
		if (ImGui::Button("O", ButtonSizeVec))
		{
			TimelineRecord(State);
		}
	}
	if (bWasRecording)
	{
		ImGui::PopStyleColor(3);
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip(State->bIsRecording ? "Stop Recording" : "Record"); }
	ImGui::SameLine();

	// Play/Pause
	if (State->bIsPlaying)
	{
		bool bPauseClicked = false;
		if (IconPause && IconPause->GetShaderResourceView())
		{
			bPauseClicked = ImGui::ImageButton("##Pause", IconPause->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bPauseClicked = ImGui::Button("||", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Pause"); }

		if (bPauseClicked)
		{
			if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
			{
				UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
				if (AnimInst)
				{
					AnimInst->StopAnimation();
				}
			}
			State->bIsPlaying = false;
		}
	}
	else
	{
		bool bPlayClicked = false;
		if (IconPlay && IconPlay->GetShaderResourceView())
		{
			bPlayClicked = ImGui::ImageButton("##Play", IconPlay->GetShaderResourceView(), ButtonSizeVec);
		}
		else
		{
			bPlayClicked = ImGui::Button(">", ButtonSizeVec);
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Play"); }

		if (bPlayClicked)
		{
			TimelinePlay(State);
		}
	}
	ImGui::SameLine();

	// ToNext >|
	if (IconStepForward && IconStepForward->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepForward", IconStepForward->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToNext(State);
		}
	}
	else
	{
		if (ImGui::Button(">|", ButtonSizeVec))
		{
			TimelineToNext(State);
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Next Frame"); }
	ImGui::SameLine();

	// ToEnd >>|
	if (IconGoToEnd && IconGoToEnd->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToEnd", IconGoToEnd->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToEnd(State);
		}
	}
	else
	{
		if (ImGui::Button(">>|", ButtonSizeVec))
		{
			TimelineToEnd(State);
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("To End"); }
	ImGui::SameLine();

	// Loop 토글
	bool bWasLooping = State->bLoopAnimation;
	UTexture* LoopIcon = bWasLooping ? IconLoop : IconLoopOff;
	if (LoopIcon && LoopIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Loop", LoopIcon->GetShaderResourceView(), ButtonSizeVec))
		{
			State->bLoopAnimation = !State->bLoopAnimation;
			if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
			{
				if (UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance())
				{
					if (UAnimSingleNodeInstance* SingleNodeInst = dynamic_cast<UAnimSingleNodeInstance*>(AnimInst))
					{
						SingleNodeInst->SetLooping(State->bLoopAnimation);
					}
				}
			}
		}
	}
	else
	{
		if (bWasLooping)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
		}
		if (ImGui::Button("Loop", ButtonSizeVec))
		{
			State->bLoopAnimation = !State->bLoopAnimation;
			if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
			{
				if (UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance())
				{
					if (UAnimSingleNodeInstance* SingleNodeInst = dynamic_cast<UAnimSingleNodeInstance*>(AnimInst))
					{
						SingleNodeInst->SetLooping(State->bLoopAnimation);
					}
				}
			}
		}
		if (bWasLooping)
		{
			ImGui::PopStyleColor();
		}
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Loop"); }
	ImGui::SameLine();

	ImGui::Dummy(ImVec2(20, 0));
	ImGui::SameLine();

	// Speed 컨트롤
	ImGui::Text("Speed:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::DragFloat("##Speed", &State->PlaybackSpeed, 0.05f, 0.1f, 5.0f, "%.2fx");
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Playback speed"); }

	ImGui::PopStyleVar(2);
}

void STimelinePanel::RenderTimeline(FEditorTabState* State)
{
	if (!State || !State->CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();
	float FrameRate = DataModel->GetFrameRate().AsDecimal();

	int32 ViewStartFrame = State->ViewRangeStartFrame;
	int32 ViewEndFrame = (State->ViewRangeEndFrame < 0) ? TotalFrames : State->ViewRangeEndFrame;
	float StartTime = (FrameRate > 0.0f) ? ViewStartFrame / FrameRate : 0.0f;
	float EndTime = (FrameRate > 0.0f) ? ViewEndFrame / FrameRate : 0.0f;

	ImVec2 ContentMin = ImGui::GetCursorScreenPos();
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 룰러 높이
	float RulerHeight = 25.0f;
	ImVec2 RulerMin = ContentMin;
	ImVec2 RulerMax = ImVec2(ContentMin.x + ContentSize.x, ContentMin.y + RulerHeight);

	// 타임라인 영역
	ImVec2 TimelineMin = ImVec2(ContentMin.x, RulerMax.y);
	ImVec2 TimelineMax = ImVec2(ContentMin.x + ContentSize.x, ContentMin.y + ContentSize.y);

	// 배경
	DrawList->AddRectFilled(RulerMin, RulerMax, IM_COL32(40, 40, 45, 255));
	DrawList->AddRectFilled(TimelineMin, TimelineMax, IM_COL32(30, 30, 35, 255));

	// 룰러 눈금
	DrawTimelineRuler(DrawList, RulerMin, RulerMax, StartTime, EndTime, State);

	// Playhead
	DrawTimelinePlayhead(DrawList, TimelineMin, TimelineMax, State->CurrentAnimationTime, StartTime, EndTime);

	// 클릭으로 시간 이동
	ImGui::SetCursorScreenPos(ContentMin);
	ImGui::InvisibleButton("##TimelineClick", ContentSize);
	if (ImGui::IsItemClicked() || (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
	{
		ImVec2 MousePos = ImGui::GetMousePos();
		float NormX = (MousePos.x - ContentMin.x) / ContentSize.x;
		NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
		float NewTime = StartTime + NormX * (EndTime - StartTime);
		NewTime = FMath::Clamp(NewTime, 0.0f, MaxTime);

		State->CurrentAnimationTime = NewTime;

		// AnimInstance에 시간 설정
		if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
		{
			if (UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance())
			{
				AnimInst->SetPosition(NewTime);
			}
		}
	}
}

void STimelinePanel::DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime, FEditorTabState* State)
{
	float Width = RulerMax.x - RulerMin.x;
	float Duration = EndTime - StartTime;

	if (Duration <= 0.0f)
	{
		return;
	}

	// 시간 간격 계산
	float Interval = 0.1f;
	if (Duration > 10.0f) { Interval = 1.0f; }
	else if (Duration > 5.0f) { Interval = 0.5f; }
	else if (Duration > 2.0f) { Interval = 0.2f; }

	float FirstTick = std::ceil(StartTime / Interval) * Interval;
	for (float Time = FirstTick; Time <= EndTime; Time += Interval)
	{
		float NormX = (Time - StartTime) / Duration;
		float X = RulerMin.x + NormX * Width;

		// 눈금
		DrawList->AddLine(ImVec2(X, RulerMax.y - 10), ImVec2(X, RulerMax.y), IM_COL32(150, 150, 150, 200));

		// 시간 텍스트
		char Buf[16];
		snprintf(Buf, sizeof(Buf), "%.1fs", Time);
		DrawList->AddText(ImVec2(X + 2, RulerMin.y + 2), IM_COL32(200, 200, 200, 255), Buf);
	}
}

void STimelinePanel::DrawPlaybackRange(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State)
{
	// (필요시 구현)
}

void STimelinePanel::DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime)
{
	float Duration = EndTime - StartTime;
	if (Duration <= 0.0f)
	{
		return;
	}

	if (CurrentTime < StartTime || CurrentTime > EndTime)
	{
		return;
	}

	float NormX = (CurrentTime - StartTime) / Duration;
	float X = TimelineMin.x + NormX * (TimelineMax.x - TimelineMin.x);

	// Playhead 라인
	DrawList->AddLine(ImVec2(X, TimelineMin.y), ImVec2(X, TimelineMax.y), IM_COL32(255, 100, 100, 255), 2.0f);

	// 삼각형 핸들
	float TriSize = 8.0f;
	DrawList->AddTriangleFilled(
		ImVec2(X, TimelineMin.y),
		ImVec2(X - TriSize, TimelineMin.y - TriSize),
		ImVec2(X + TriSize, TimelineMin.y - TriSize),
		IM_COL32(255, 100, 100, 255)
	);
}

void STimelinePanel::DrawKeyframeMarkers(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float StartTime, float EndTime, FEditorTabState* State)
{
	// (필요시 구현)
}

void STimelinePanel::DrawNotifyTracksPanel(FEditorTabState* State, float StartTime, float EndTime)
{
	// (필요시 구현)
}

void STimelinePanel::RebuildNotifyTracks(FEditorTabState* State)
{
	// (필요시 구현)
}

void STimelinePanel::ScanNotifyLibrary()
{
	// (필요시 구현)
}

void STimelinePanel::CreateNewNotifyScript(const FString& ScriptName, bool bIsNotifyState)
{
	// (필요시 구현)
}

void STimelinePanel::OpenNotifyScriptInEditor(const FString& NotifyClassName, bool bIsNotifyState)
{
	// (필요시 구현)
}

void STimelinePanel::TimelineToFront(FEditorTabState* State)
{
	State->CurrentAnimationTime = 0.0f;
	RefreshAnimationFrame(State);
}

void STimelinePanel::TimelineToPrevious(FEditorTabState* State)
{
	if (!State->CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	float FrameTime = (FrameRate > 0.0f) ? 1.0f / FrameRate : 0.0f;
	State->CurrentAnimationTime = std::max(0.0f, State->CurrentAnimationTime - FrameTime);
	RefreshAnimationFrame(State);
}

void STimelinePanel::TimelineReverse(FEditorTabState* State)
{
	State->PlaybackSpeed = -std::abs(State->PlaybackSpeed);
	TimelinePlay(State);
}

void STimelinePanel::TimelineRecord(FEditorTabState* State)
{
	State->bIsRecording = !State->bIsRecording;
	// 녹화 로직 (필요시 구현)
}

void STimelinePanel::TimelinePlay(FEditorTabState* State)
{
	if (!State->PreviewActor || !State->CurrentAnimation)
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
		return;
	}

	AnimInst->SetPlayRate(State->PlaybackSpeed);
	AnimInst->PlayAnimation(State->CurrentAnimation, State->PlaybackSpeed);
	AnimInst->SetPosition(State->CurrentAnimationTime);
	State->bIsPlaying = true;
}

void STimelinePanel::TimelineToNext(FEditorTabState* State)
{
	if (!State->CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	float FrameTime = (FrameRate > 0.0f) ? 1.0f / FrameRate : 0.0f;
	float MaxTime = DataModel->GetPlayLength();
	State->CurrentAnimationTime = std::min(MaxTime, State->CurrentAnimationTime + FrameTime);
	RefreshAnimationFrame(State);
}

void STimelinePanel::TimelineToEnd(FEditorTabState* State)
{
	if (!State->CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = State->CurrentAnimation->GetDataModel();
	if (DataModel)
	{
		State->CurrentAnimationTime = DataModel->GetPlayLength();
		RefreshAnimationFrame(State);
	}
}

void STimelinePanel::RefreshAnimationFrame(FEditorTabState* State)
{
	if (!State->PreviewActor || !State->CurrentAnimation)
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

// ============================================================================
// SAnimationListPanel
// ============================================================================

void SAnimationListPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Animations##DynamicAnimList", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State)
		{
			RenderAnimationList(State);
		}
	}
	ImGui::End();
}

void SAnimationListPanel::RenderAnimationList(FEditorTabState* State)
{
	if (!State->CurrentMesh)
	{
		ImGui::TextDisabled("No skeletal mesh loaded.");
		return;
	}

	// 애니메이션 목록 가져오기
	const TArray<UAnimSequence*>& Animations = State->CurrentMesh->GetAnimations();

	if (Animations.IsEmpty())
	{
		ImGui::TextDisabled("No animations available.");
		ImGui::TextDisabled("Load a mesh with animations.");
		return;
	}

	ImGui::Text("Animations (%d)", Animations.size());
	ImGui::Separator();

	ImGui::BeginChild("AnimList", ImVec2(0, 0), false);
	for (int32 i = 0; i < Animations.size(); ++i)
	{
		UAnimSequence* Anim = Animations[i];
		if (!Anim)
		{
			continue;
		}

		bool bSelected = (State->SelectedAnimationIndex == i);
		FString AnimName = Anim->GetName();
		if (AnimName.empty())
		{
			AnimName = "Animation " + std::to_string(i);
		}

		if (ImGui::Selectable(AnimName.c_str(), bSelected))
		{
			State->SelectedAnimationIndex = i;
			State->CurrentAnimation = Anim;
			State->CurrentAnimationTime = 0.0f;
			State->bIsPlaying = false;

			// Working/View range 리셋
			if (UAnimDataModel* DataModel = Anim->GetDataModel())
			{
				int32 TotalFrames = DataModel->GetNumberOfFrames();
				State->WorkingRangeStartFrame = 0;
				State->WorkingRangeEndFrame = TotalFrames;
				State->ViewRangeStartFrame = 0;
				State->ViewRangeEndFrame = TotalFrames;
			}
		}

		// 더블클릭으로 재생
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		{
			State->SelectedAnimationIndex = i;
			State->CurrentAnimation = Anim;
			State->CurrentAnimationTime = 0.0f;

			// 애니메이션 재생 시작
			if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
			{
				if (UAnimInstance* AnimInst = State->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance())
				{
					AnimInst->SetPlayRate(State->PlaybackSpeed);
					AnimInst->PlayAnimation(Anim, State->PlaybackSpeed);
					State->bIsPlaying = true;
				}
			}
		}
	}
	ImGui::EndChild();
}

// ============================================================================
// SAnimGraphNodePanel - 노드 에디터 (중앙 패널)
// ============================================================================

void SAnimGraphNodePanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoTitleBar;

	if (ImGui::Begin("##AnimGraphNodePanel", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State && State->StateMachine)
		{
			ImGui::TextDisabled("AnimGraph Node Editor");
			ImGui::TextDisabled("State Machine: %s", State->StateMachine->GetName().c_str());
			ImGui::Separator();

			// 간단한 노드 리스트 표시 (실제 노드 에디터는 별도 작업 필요)
			TMap<FName, FAnimStateNode>& Nodes = State->StateMachine->GetNodes();
			int32 i = 0;
			for (auto& Pair : Nodes)
			{
				ImGui::Text("State %d: %s", i++, Pair.first.ToString().c_str());
			}
		}
		else
		{
			ImGui::TextDisabled("No AnimStateMachine loaded.");
			ImGui::TextDisabled("Use AnimGraph mode with a loaded state machine.");
		}
	}
	ImGui::End();
}

// ============================================================================
// SAnimGraphStateListPanel - 좌측 상태 목록 패널
// ============================================================================

void SAnimGraphStateListPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("States##AnimGraphStates", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State && State->StateMachine)
		{
			ImGui::Text("States");
			ImGui::Separator();

			TMap<FName, FAnimStateNode>& Nodes = State->StateMachine->GetNodes();
			for (auto& Pair : Nodes)
			{
				if (ImGui::Selectable(Pair.first.ToString().c_str()))
				{
					// 상태 선택 처리
				}
			}

			ImGui::Spacing();
			if (ImGui::Button("+ Add State", ImVec2(-1, 30)))
			{
				// 새 상태 추가
			}
		}
		else
		{
			ImGui::TextDisabled("No state machine.");
		}
	}
	ImGui::End();
}

// ============================================================================
// SAnimGraphDetailsPanel - 우측 상세 속성 패널
// ============================================================================

void SAnimGraphDetailsPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Details##AnimGraphDetails", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State && State->StateMachine)
		{
			ImGui::Text("State Details");
			ImGui::Separator();

			// 선택된 상태/전환 속성 표시
			ImGui::TextDisabled("Select a state or transition to view details.");
		}
		else
		{
			ImGui::TextDisabled("No state machine.");
		}
	}
	ImGui::End();
}

// ============================================================================
// SBlendSpaceGridPanel - 블렌드 스페이스 그리드 패널
// ============================================================================

void SBlendSpaceGridPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoTitleBar;

	if (ImGui::Begin("##BlendSpaceGrid", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State && State->BlendSpace)
		{
			ImVec2 ContentMin = ImGui::GetCursorScreenPos();
			ImVec2 ContentSize = ImGui::GetContentRegionAvail();
			ImDrawList* DrawList = ImGui::GetWindowDrawList();

			// 그리드 배경
			DrawList->AddRectFilled(ContentMin,
				ImVec2(ContentMin.x + ContentSize.x, ContentMin.y + ContentSize.y),
				IM_COL32(40, 40, 45, 255));

			// 그리드 라인
			float GridStep = 50.0f;
			for (float x = 0; x < ContentSize.x; x += GridStep)
			{
				DrawList->AddLine(
					ImVec2(ContentMin.x + x, ContentMin.y),
					ImVec2(ContentMin.x + x, ContentMin.y + ContentSize.y),
					IM_COL32(60, 60, 65, 255));
			}
			for (float y = 0; y < ContentSize.y; y += GridStep)
			{
				DrawList->AddLine(
					ImVec2(ContentMin.x, ContentMin.y + y),
					ImVec2(ContentMin.x + ContentSize.x, ContentMin.y + y),
					IM_COL32(60, 60, 65, 255));
			}

			// 축 레이블 - 직접 멤버 접근
			UBlendSpace2D* BS = State->BlendSpace;
			float XMin = BS->XAxisMin;
			float XMax = BS->XAxisMax;
			float YMin = BS->YAxisMin;
			float YMax = BS->YAxisMax;
			float XRange = XMax - XMin;
			float YRange = YMax - YMin;
			if (XRange < 0.001f) { XRange = 1.0f; }
			if (YRange < 0.001f) { YRange = 1.0f; }

			char LabelBuf[64];
			snprintf(LabelBuf, sizeof(LabelBuf), "%s", BS->XAxisName.c_str());
			DrawList->AddText(
				ImVec2(ContentMin.x + ContentSize.x * 0.5f - 20, ContentMin.y + ContentSize.y - 20),
				IM_COL32(200, 200, 200, 255), LabelBuf);

			snprintf(LabelBuf, sizeof(LabelBuf), "%s", BS->YAxisName.c_str());
			DrawList->AddText(
				ImVec2(ContentMin.x + 5, ContentMin.y + ContentSize.y * 0.5f),
				IM_COL32(200, 200, 200, 255), LabelBuf);

			// 샘플 포인트 그리기
			const TArray<FBlendSample>& Samples = BS->Samples;
			for (int32 i = 0; i < static_cast<int32>(Samples.size()); ++i)
			{
				const FBlendSample& Sample = Samples[i];

				// 정규화된 위치 계산
				float NormX = (Sample.Position.X - XMin) / XRange;
				float NormY = 1.0f - (Sample.Position.Y - YMin) / YRange;

				float ScreenX = ContentMin.x + NormX * ContentSize.x;
				float ScreenY = ContentMin.y + NormY * ContentSize.y;

				bool bSelected = (State->SelectedSampleIndex == i);
				ImU32 Color = bSelected ? IM_COL32(255, 200, 100, 255) : IM_COL32(100, 200, 255, 255);

				DrawList->AddCircleFilled(ImVec2(ScreenX, ScreenY), 8.0f, Color);
				DrawList->AddCircle(ImVec2(ScreenX, ScreenY), 8.0f, IM_COL32(255, 255, 255, 200), 0, 2.0f);
			}

			// Preview 포인트
			float PreviewNormX = (State->PreviewBlendPosition.X - XMin) / XRange;
			float PreviewNormY = 1.0f - (State->PreviewBlendPosition.Y - YMin) / YRange;
			float PreviewScreenX = ContentMin.x + PreviewNormX * ContentSize.x;
			float PreviewScreenY = ContentMin.y + PreviewNormY * ContentSize.y;
			DrawList->AddCircleFilled(ImVec2(PreviewScreenX, PreviewScreenY), 6.0f, IM_COL32(255, 100, 100, 255));

			// 마우스 클릭 처리
			ImGui::SetCursorScreenPos(ContentMin);
			ImGui::InvisibleButton("##BlendSpaceGridClick", ContentSize);
			if (ImGui::IsItemClicked() || (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))
			{
				ImVec2 MousePos = ImGui::GetMousePos();
				float NormX = (MousePos.x - ContentMin.x) / ContentSize.x;
				float NormY = 1.0f - (MousePos.y - ContentMin.y) / ContentSize.y;

				State->PreviewBlendPosition.X = XMin + NormX * XRange;
				State->PreviewBlendPosition.Y = YMin + NormY * YRange;
			}
		}
		else
		{
			ImGui::TextDisabled("No BlendSpace loaded.");
		}
	}
	ImGui::End();
}

// ============================================================================
// SBlendSpaceSampleListPanel - 블렌드 스페이스 샘플 목록 패널
// ============================================================================

void SBlendSpaceSampleListPanel::OnRender()
{
	ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
	ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Samples##BlendSpaceSamples", nullptr, flags))
	{
		FEditorTabState* State = Owner->GetActiveState();
		if (State && State->BlendSpace)
		{
			ImGui::Text("Blend Samples");
			ImGui::Separator();

			const TArray<FBlendSample>& Samples = State->BlendSpace->Samples;
			for (int32 i = 0; i < static_cast<int32>(Samples.size()); ++i)
			{
				const FBlendSample& Sample = Samples[i];
				bool bSelected = (State->SelectedSampleIndex == i);

				char Label[128];
				FString AnimName = Sample.Animation ? Sample.Animation->GetName() : "None";
				snprintf(Label, sizeof(Label), "%s (%.1f, %.1f)", AnimName.c_str(), Sample.Position.X, Sample.Position.Y);

				if (ImGui::Selectable(Label, bSelected))
				{
					State->SelectedSampleIndex = i;
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Preview Position
			ImGui::Text("Preview Position");
			ImGui::DragFloat("X", &State->PreviewBlendPosition.X, 0.01f);
			ImGui::DragFloat("Y", &State->PreviewBlendPosition.Y, 0.01f);
		}
		else
		{
			ImGui::TextDisabled("No BlendSpace loaded.");
		}
	}
	ImGui::End();
}
