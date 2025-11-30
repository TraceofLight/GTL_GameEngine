#include "pch.h"
#include "AnimationWindow.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "ImGui/imgui.h"

// ============================================================================
// Properties Panel 렌더링
// ============================================================================

void SAnimationWindow::RenderPropertiesPanel()
{
	FAnimationTabState* State = ActiveState;
	if (!State)
	{
		return;
	}

	FRect PanelRect = PropertiesRect;
	if (PanelRect.GetWidth() <= 0 || PanelRect.GetHeight() <= 0)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(PanelRect.Left, PanelRect.Top));
	ImGui::SetNextWindowSize(ImVec2(PanelRect.GetWidth(), PanelRect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("Properties##AnimProperties", nullptr, flags))
	{
		// 헤더
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		ImGui::Text("Bone Properties");
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::Spacing();

		// Bone이 선택되었는지 확인
		if (State->SelectedBoneIndex >= 0 && State->CurrentMesh)
		{
			const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
			if (Skeleton && State->SelectedBoneIndex < Skeleton->Bones.size())
			{
				const FBone& SelectedBone = Skeleton->Bones[State->SelectedBoneIndex];

				// Bone 이름
				ImGui::Text("Bone Name");
				ImGui::SameLine(100.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
				ImGui::Text("%s", SelectedBone.Name.c_str());
				ImGui::PopStyleColor();

				ImGui::Spacing();

				// Transforms 섹션
				if (ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_DefaultOpen))
				{
					// 편집 중이 아닐 때만 스켈레톤에서 값 업데이트
					if (!State->bBoneRotationEditing)
					{
						UpdateBoneTransformFromSkeleton(State);
					}

					float labelWidth = 70.0f;
					float panelWidth = PanelRect.GetWidth();
					float inputWidth = (panelWidth - labelWidth - 40.0f) / 3.0f;

					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
					ImGui::Text("Bone");
					ImGui::PopStyleColor();
					ImGui::Spacing();

					// Location
					ImGui::Text("Location");
					ImGui::SameLine(labelWidth);
					ImGui::PushItemWidth(inputWidth);
					bool bLocationChanged = false;
					bLocationChanged |= ImGui::DragFloat("##BoneLocX", &State->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "%.2f");
					ImGui::SameLine();
					bLocationChanged |= ImGui::DragFloat("##BoneLocY", &State->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "%.2f");
					ImGui::SameLine();
					bLocationChanged |= ImGui::DragFloat("##BoneLocZ", &State->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "%.2f");
					ImGui::PopItemWidth();

					if (bLocationChanged)
					{
						ApplyBoneTransform(State);
						State->bBoneLinesDirty = true;
					}

					// Rotation
					ImGui::Text("Rotation");
					ImGui::SameLine(labelWidth);
					ImGui::PushItemWidth(inputWidth);
					bool bRotationChanged = false;

					if (ImGui::IsAnyItemActive())
					{
						State->bBoneRotationEditing = true;
					}

					bRotationChanged |= ImGui::DragFloat("##BoneRotX", &State->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "%.2f");
					ImGui::SameLine();
					bRotationChanged |= ImGui::DragFloat("##BoneRotY", &State->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "%.2f");
					ImGui::SameLine();
					bRotationChanged |= ImGui::DragFloat("##BoneRotZ", &State->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "%.2f");
					ImGui::PopItemWidth();

					if (!ImGui::IsAnyItemActive())
					{
						State->bBoneRotationEditing = false;
					}

					if (bRotationChanged)
					{
						ApplyBoneTransform(State);
						State->bBoneLinesDirty = true;
					}

					// Scale
					ImGui::Text("Scale");
					ImGui::SameLine(labelWidth);
					ImGui::PushItemWidth(inputWidth);
					bool bScaleChanged = false;
					bScaleChanged |= ImGui::DragFloat("##BoneScaleX", &State->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "%.2f");
					ImGui::SameLine();
					bScaleChanged |= ImGui::DragFloat("##BoneScaleY", &State->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "%.2f");
					ImGui::SameLine();
					bScaleChanged |= ImGui::DragFloat("##BoneScaleZ", &State->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "%.2f");
					ImGui::PopItemWidth();

					if (bScaleChanged)
					{
						ApplyBoneTransform(State);
						State->bBoneLinesDirty = true;
					}
				}

				// Display Options 섹션
				ImGui::Spacing();
				if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Checkbox("Show Mesh", &State->bShowMesh))
					{
						if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
						{
							State->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(State->bShowMesh);
						}
					}

					if (ImGui::Checkbox("Show Bones", &State->bShowBones))
					{
						if (State->PreviewActor && State->PreviewActor->GetBoneLineComponent())
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
		}
		else
		{
			// Bone 미선택 상태
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::TextWrapped("Select a bone from the viewport to edit its transform properties.");
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Display Options (Bone 미선택 상태에서도 표시)
			if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Checkbox("Show Mesh", &State->bShowMesh))
				{
					if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
					{
						State->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(State->bShowMesh);
					}
				}

				if (ImGui::Checkbox("Show Bones", &State->bShowBones))
				{
					if (State->PreviewActor && State->PreviewActor->GetBoneLineComponent())
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
	}
	ImGui::End();
}
