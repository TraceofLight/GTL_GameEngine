#include "pch.h"
#include "AnimationWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Slate/Widgets/ViewportToolbarWidget.h"

// ============================================================================
// Viewport Panel 렌더링
// ============================================================================

void SAnimationWindow::RenderViewportPanel()
{
	FAnimationTabState* State = ActiveState;
	if (!State)
	{
		return;
	}

	// 현재 패널의 영역 가져오기
	FRect PanelRect = ViewportRect;
	if (PanelRect.GetWidth() <= 0 || PanelRect.GetHeight() <= 0)
	{
		return;
	}

	// ImGui 윈도우 설정
	ImGui::SetNextWindowPos(ImVec2(PanelRect.Left, PanelRect.Top));
	ImGui::SetNextWindowSize(ImVec2(PanelRect.GetWidth(), PanelRect.GetHeight()));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

	if (ImGui::Begin("##AnimViewport", nullptr, flags))
	{
		// Viewport Toolbar 렌더링
		if (ViewportToolbar)
		{
			AGizmoActor* GizmoActor = nullptr;
			if (State->Client && State->Client->GetWorld())
			{
				GizmoActor = State->Client->GetWorld()->GetGizmoActor();
			}
			ViewportToolbar->Render(State->Client, GizmoActor, false);
		}

		ImVec2 ContentSize = ImGui::GetContentRegionAvail();
		uint32 NewWidth = static_cast<uint32>(ContentSize.x);
		uint32 NewHeight = static_cast<uint32>(ContentSize.y);

		if (NewWidth > 0 && NewHeight > 0)
		{
			// 렌더 타겟 업데이트
			UpdateViewportRenderTarget(NewWidth, NewHeight);
			RenderToPreviewRenderTarget();

			ID3D11ShaderResourceView* PreviewSRV = GetPreviewShaderResourceView();
			if (PreviewSRV)
			{
				ImVec2 ImagePos = ImGui::GetCursorScreenPos();

				// 뷰포트 이미지 렌더링
				ImTextureID TextureID = reinterpret_cast<ImTextureID>(PreviewSRV);
				ImGui::Image(TextureID, ContentSize);

				// 드래그 앤 드랍 타겟 (Content Browser에서 파일 드롭)
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
					{
						const char* filePath = static_cast<const char*>(payload->Data);
						FString path(filePath);

						if (path.ends_with(".anim"))
						{
							// .anim 파일 드롭: 애니메이션 로드
							UAnimSequence* Anim = UResourceManager::GetInstance().Load<UAnimSequence>(path);
							if (Anim && State->CurrentMesh)
							{
								State->CurrentMesh->AddAnimation(Anim);

								TArray<UAnimSequence*> Animations = State->CurrentMesh->GetAnimations();
								for (int32 i = 0; i < Animations.Num(); ++i)
								{
									if (Animations[i] == Anim)
									{
										State->SelectedAnimationIndex = i;
										State->CurrentAnimation = Anim;
										State->CurrentAnimationTime = 0.0f;
										break;
									}
								}

								// 탭 이름 업데이트
								State->TabName = FName(path.substr(path.find_last_of("/\\") + 1).c_str());
								State->FilePath = path;

								RebuildNotifyTracks(State);

								// AnimInstance 생성 및 재생
								if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
								{
									USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
									UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
									if (!AnimInst)
									{
										UAnimSingleNodeInstance* SingleNodeInst = NewObject<UAnimSingleNodeInstance>();
										AnimInst = SingleNodeInst;
										SkelComp->SetAnimInstance(AnimInst);
									}
									if (AnimInst)
									{
										AnimInst->PlayAnimation(Anim, State->PlaybackSpeed);
										State->bIsPlaying = true;
									}
								}
							}
						}
						else if (path.ends_with(".fbx") || path.ends_with(".FBX"))
						{
							// .fbx 파일 드롭: 스켈레탈 메쉬 또는 애니메이션 로드
							if (State->CurrentMesh && State->CurrentMesh->GetSkeleton())
							{
								// 기존 메쉬가 있으면 애니메이션으로 로드 시도
								TArray<UAnimSequence*> AnimSequences = UFbxLoader::GetInstance().LoadAllFbxAnimations(
									path, *State->CurrentMesh->GetSkeleton());

								if (AnimSequences.Num() > 0)
								{
									for (UAnimSequence* AnimSeq : AnimSequences)
									{
										if (AnimSeq)
										{
											State->CurrentMesh->AddAnimation(AnimSeq);
										}
									}

									TArray<UAnimSequence*> Animations = State->CurrentMesh->GetAnimations();
									if (Animations.Num() > 0)
									{
										State->SelectedAnimationIndex = Animations.Num() - AnimSequences.Num();
										State->CurrentAnimation = Animations[State->SelectedAnimationIndex];
										State->CurrentAnimationTime = 0.0f;

										// AnimInstance 재생
										if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent())
										{
											USkeletalMeshComponent* SkelComp = State->PreviewActor->GetSkeletalMeshComponent();
											UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
											if (!AnimInst)
											{
												UAnimSingleNodeInstance* SingleNodeInst = NewObject<UAnimSingleNodeInstance>();
												AnimInst = SingleNodeInst;
												SkelComp->SetAnimInstance(AnimInst);
											}
											if (AnimInst)
											{
												AnimInst->PlayAnimation(State->CurrentAnimation, State->PlaybackSpeed);
												State->bIsPlaying = true;
											}
										}
									}
								}
							}
							else
							{
								// 메쉬가 없으면 스켈레탈 메쉬로 로드
								LoadSkeletalMesh(path);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
		}

		// Recording 상태 오버레이
		if (State->bIsRecording)
		{
			ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
			ImGui::SetCursorPos(ImVec2(ContentAvail.x - 180, ContentAvail.y - 40));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.05f, 0.05f, 0.85f));
			ImGui::BeginChild("RecordingOverlay", ImVec2(170, 30), true, ImGuiWindowFlags_NoScrollbar);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
			ImGui::Text("REC");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			ImGui::Text("%d frames", State->RecordedFrames.Num());
			ImGui::PopStyleColor();
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}
