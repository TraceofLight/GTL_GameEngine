#include "pch.h"
#include "AnimationWindow.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "ImGui/imgui.h"
#include <filesystem>

// ============================================================================
// Animation List Panel 렌더링
// ============================================================================

void SAnimationWindow::RenderAnimationListPanel()
{
	FAnimationTabState* State = ActiveState;
	if (!State)
	{
		return;
	}

	FRect PanelRect = AnimListRect;
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

	if (ImGui::Begin("Animation List##AnimList", nullptr, flags))
	{
		// 헤더
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		ImGui::Text("Animations");
		ImGui::PopStyleColor();

		// Save/Save As 버튼 (Animation List 헤더 우측)
		ImGui::SameLine();

		bool bCanSave = (State->CurrentAnimation != nullptr);

		// Save 버튼 (기존 파일 덮어쓰기)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.70f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.80f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.40f, 0.60f, 1.0f));

		if (!bCanSave)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}

		if (ImGui::SmallButton("Save") && bCanSave)
		{
			UAnimSequence* AnimToSave = State->CurrentAnimation;
			if (AnimToSave)
			{
				AnimToSave->Save();
			}
		}

		if (!bCanSave)
		{
			ImGui::PopStyleVar();
		}

		ImGui::PopStyleColor(3);

		// Save As 버튼
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.65f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.75f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.35f, 0.55f, 1.0f));

		if (!bCanSave)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}

		static char SaveAsFileName[256] = "";

		if (ImGui::SmallButton("Save As...") && bCanSave)
		{
			ImGui::OpenPopup("SaveAsMenu##AnimList");
			if (State->CurrentAnimation)
			{
				FString CurrentName = State->CurrentAnimation->GetName();
				strncpy_s(SaveAsFileName, CurrentName.c_str(), sizeof(SaveAsFileName) - 1);
			}
		}

		if (!bCanSave)
		{
			ImGui::PopStyleVar();
		}

		ImGui::PopStyleColor(3);

		// Save As 팝업 메뉴
		if (ImGui::BeginPopup("SaveAsMenu##AnimList"))
		{
			ImGui::Text("Enter new file name:");
			ImGui::SetNextItemWidth(250.0f);
			bool bEnterPressed = ImGui::InputTextWithHint("##SaveAsFileName", "e.g., MyAnimation", SaveAsFileName, sizeof(SaveAsFileName), ImGuiInputTextFlags_EnterReturnsTrue);

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Save to: Data/Animation/%s.anim", SaveAsFileName);
			ImGui::Spacing();

			if (ImGui::Button("Save", ImVec2(120, 0)) || bEnterPressed)
			{
				if (strlen(SaveAsFileName) > 0 && State->CurrentAnimation)
				{
					FString NewFileName = SaveAsFileName;
					FString SavePath = "Data/Animation/" + NewFileName + ".anim";

					UAnimSequence* SourceAnim = State->CurrentAnimation;

					// 원본 상태 저장
					FString OriginalName = SourceAnim->GetName();
					FString OriginalFilePath = SourceAnim->GetFilePath();

					// 임시로 Name을 변경하여 저장
					SourceAnim->SetName(NewFileName);

					if (SourceAnim->SaveToFile(SavePath))
					{
						// 원본 애니메이션 상태 복원
						SourceAnim->SetName(OriginalName);
						SourceAnim->SetFilePath(OriginalFilePath);

						// 새로운 AnimSequence 객체 생성 및 로드
						UAnimSequence* NewAnim = UResourceManager::GetInstance().Load<UAnimSequence>(SavePath);

						if (NewAnim && State->CurrentMesh)
						{
							// SkeletalMesh의 Animation List에 추가
							State->CurrentMesh->AddAnimation(NewAnim);
						}
					}
					else
					{
						// 저장 실패 시 원본 상태 복원
						SourceAnim->SetName(OriginalName);
						SourceAnim->SetFilePath(OriginalFilePath);
					}

					memset(SaveAsFileName, 0, sizeof(SaveAsFileName));
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				memset(SaveAsFileName, 0, sizeof(SaveAsFileName));
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::Spacing();

		// 애니메이션 목록 가져오기
		TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();
		TArray<UAnimSequence*> Animations;
		for (UAnimSequence* Anim : AllAnimations)
		{
			if (Anim && Anim->GetFilePath().ends_with(".anim"))
			{
				Animations.push_back(Anim);
			}
		}

		if (Animations.Num() > 0)
		{
			// 스크롤 가능한 리스트
			ImGui::BeginChild("AnimListScroll", ImVec2(0, 0), false);

			for (int32 i = 0; i < Animations.Num(); ++i)
			{
				UAnimSequence* Anim = Animations[i];
				if (!Anim)
				{
					continue;
				}

				bool bIsSelected = (State->SelectedAnimationIndex == i);

				// 애니메이션 이름 및 길이 표시
				FString DisplayName = Anim->GetName();
				if (DisplayName.empty())
				{
					DisplayName = "Anim " + std::to_string(i);
				}

				char LabelBuffer[256];
				UAnimDataModel* DataModel = Anim->GetDataModel();
				if (DataModel)
				{
					(void)snprintf(LabelBuffer, sizeof(LabelBuffer), "%s (%.1fs)",
					               DisplayName.c_str(), DataModel->GetPlayLength());
				}
				else
				{
					(void)snprintf(LabelBuffer, sizeof(LabelBuffer), "%s", DisplayName.c_str());
				}

				char SelectableID[512];
				(void)snprintf(SelectableID, sizeof(SelectableID), "%s##anim_%d", LabelBuffer, i);

				// 선택 가능한 항목
				if (ImGui::Selectable(SelectableID, bIsSelected))
				{
					State->SelectedAnimationIndex = i;
					State->CurrentAnimation = Anim;
					State->CurrentAnimationTime = 0.0f;

					// Bone 델타 초기화
					if (USkeletalMeshComponent* SkelComp = State->PreviewActor ?
						State->PreviewActor->GetSkeletalMeshComponent() : nullptr)
					{
						SkelComp->ClearAllBoneDeltas();
					}

					// Notify Track 재구성
					RebuildNotifyTracks(State);
					State->bIsPlaying = true;

					// 프레임 범위 설정
					if (State->CurrentAnimation && State->CurrentAnimation->GetDataModel())
					{
						int32 TotalFrames = State->CurrentAnimation->GetDataModel()->GetNumberOfFrames();
						State->WorkingRangeStartFrame = 0;
						State->WorkingRangeEndFrame = TotalFrames;
						State->ViewRangeStartFrame = 0;
						State->ViewRangeEndFrame = FMath::Min(60, TotalFrames);
					}

					// AnimInstance 재생 시작
					if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
					{
						USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
						SkelComp->ResetToReferencePose();

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
							AnimInst->PlayAnimation(Anim, State->PlaybackSpeed);
							State->bIsPlaying = true;
						}
					}

					// 탭 이름 업데이트
					State->TabName = FName(DisplayName.c_str());
					State->FilePath = Anim->GetFilePath();
				}

				// 우클릭 컨텍스트 메뉴
				char ContextMenuID[64];
				(void)snprintf(ContextMenuID, sizeof(ContextMenuID), "AnimContextMenu_%d", i);
				if (ImGui::BeginPopupContextItem(ContextMenuID))
				{
					if (ImGui::MenuItem("Delete"))
					{
						// 현재 재생 중인 애니메이션이면 정지
						if (State->CurrentAnimation == Anim)
						{
							State->CurrentAnimation = nullptr;
							State->SelectedAnimationIndex = -1;
							State->bIsPlaying = false;
							State->CurrentAnimationTime = 0.0f;

							// AnimInstance 정지
							if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
							{
								USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
								if (UAnimInstance* AnimInst = SkelComp->GetAnimInstance())
								{
									AnimInst->StopAnimation();
								}
								SkelComp->ResetToReferencePose();
							}
						}

						// 파일 경로 저장 (Unload 전에)
						FString FilePath = Anim->GetFilePath();

						// ResourceManager에서 제거
						UResourceManager::GetInstance().Unload<UAnimSequence>(FilePath);

						// 실제 파일 삭제
						if (!FilePath.empty())
						{
							std::error_code EC;
							if (std::filesystem::exists(FilePath, EC))
							{
								if (std::filesystem::remove(FilePath, EC))
								{
									UE_LOG("Animation file deleted: %s", FilePath.c_str());
								}
								else
								{
									UE_LOG("Failed to delete animation file: %s (error: %s)", FilePath.c_str(), EC.message().c_str());
								}
							}
						}

						ImGui::EndPopup();
						break;  // 리스트가 변경되었으므로 루프 종료
					}
					ImGui::EndPopup();
				}

				// Delete 키로 선택된 애니메이션 삭제
				if (bIsSelected && ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete))
				{
					// 현재 재생 중인 애니메이션이면 정지
					if (State->CurrentAnimation == Anim)
					{
						State->CurrentAnimation = nullptr;
						State->SelectedAnimationIndex = -1;
						State->bIsPlaying = false;
						State->CurrentAnimationTime = 0.0f;

						// AnimInstance 정지
						if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
						{
							USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
							if (UAnimInstance* AnimInst = SkelComp->GetAnimInstance())
							{
								AnimInst->StopAnimation();
							}
							SkelComp->ResetToReferencePose();
						}
					}

					// 파일 경로 저장 (Unload 전에)
					FString FilePath = Anim->GetFilePath();

					// ResourceManager에서 제거
					UResourceManager::GetInstance().Unload<UAnimSequence>(FilePath);

					// 실제 파일 삭제
					if (!FilePath.empty())
					{
						std::error_code EC;
						if (std::filesystem::exists(FilePath, EC))
						{
							if (std::filesystem::remove(FilePath, EC))
							{
								UE_LOG("Animation file deleted: %s", FilePath.c_str());
							}
							else
							{
								UE_LOG("Failed to delete animation file: %s (error: %s)", FilePath.c_str(), EC.message().c_str());
							}
						}
					}

					break;  // 리스트가 변경되었으므로 루프 종료
				}

				// 더블클릭으로 새 탭에서 열기
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					// 새 탭에서 열기 (현재 탭은 유지)
					// OpenNewTabWithAnimation(Anim, Anim->GetFilePath());
				}
			}

			ImGui::EndChild();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::TextWrapped("No animations loaded.");
			ImGui::Spacing();
			ImGui::TextWrapped("Drag and drop .anim or .fbx files onto the viewport to load animations.");
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();
}
