#include "pch.h"
#include "BlendSpace2DEditorWindow.h"
#include "Source/Runtime/Engine/Animation/BlendSpace2D.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimDataModel.h"
#include "Source/Runtime/InputCore/InputManager.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include"FViewportClient.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Math.h"

SBlendSpace2DEditorWindow::SBlendSpace2DEditorWindow()
	: CanvasPos(ImVec2(0, 0))
	, CanvasSize(ImVec2(600, 600))
{
}

SBlendSpace2DEditorWindow::~SBlendSpace2DEditorWindow()
{
	// ViewerState 정리 (SkeletalViewerBootstrap 사용)
	if (PreviewState)
	{
		SkeletalViewerBootstrap::DestroyViewerState(PreviewState);
	}
}

bool SBlendSpace2DEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
	Device = InDevice;
	SetRect(StartX, StartY, StartX + Width, StartY + Height);
	bIsOpen = true;

	// ViewerState 생성 (SkeletalViewerBootstrap 재활용)
	PreviewState = SkeletalViewerBootstrap::CreateViewerState("BlendSpace2D Preview", InWorld, InDevice);
	if (!PreviewState)
	{
		return false;
	}

	// 타임라인 아이콘 로드
	IconGoToFront = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_Front_24x.png");
	IconGoToFrontOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_Front_24x_OFF.png");
	IconStepBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Backwards_24x.png");
	IconStepBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Backwards_24x_OFF.png");
	IconBackwards = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Backwards_24x.png");
	IconBackwardsOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Backwards_24x_OFF.png");
	IconRecord = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Record_24x.png");
	IconPause = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Pause_24x.png");
	IconPauseOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Pause_24x_OFF.png");
	IconPlay = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Play_24x.png");
	IconPlayOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Play_24x_OFF.png");
	IconStepForward = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Forward_24x.png");
	IconStepForwardOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Step_Forward_24x_OFF.png");
	IconGoToEnd = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_End_24x.png");
	IconGoToEndOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Go_To_End_24x_OFF.png");
	IconLoop = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Loop_24x.png");
	IconLoopOff = UResourceManager::GetInstance().Load<UTexture>("Data/Icon/Loop_24x_OFF.png");

	return true;
}

void SBlendSpace2DEditorWindow::SetBlendSpace(UBlendSpace2D* InBlendSpace)
{
	EditingBlendSpace = InBlendSpace;
	SelectedSampleIndex = -1;
	bDraggingSample = false;

	// 기본 프리뷰 파라미터 설정 (중앙)
	if (EditingBlendSpace)
	{
		PreviewParameter.X = (EditingBlendSpace->XAxisMin + EditingBlendSpace->XAxisMax) * 0.5f;
		PreviewParameter.Y = (EditingBlendSpace->YAxisMin + EditingBlendSpace->YAxisMax) * 0.5f;
	}
}

void SBlendSpace2DEditorWindow::OnRender()
{
	if (!bIsOpen)
	{
		return;
	}

	if (!EditingBlendSpace)
	{
		// BlendSpace가 없으면 간단한 메시지만 표시
		if (ImGui::Begin("Blend Space 2D Editor", &bIsOpen))
		{
			ImGui::Text("No Blend Space loaded.");
			ImGui::Text("Please create or select a Blend Space 2D asset.");
		}
		ImGui::End();
		return;
	}

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Blend Space 2D Editor", &bIsOpen, flags))
	{
		// 툴바
		RenderToolbar();
		ImGui::Separator();

		// 전체 레이아웃 크기 계산
		ImVec2 ContentRegion = ImGui::GetContentRegionAvail();
		float TopHeight = ContentRegion.y * 0.6f;  // 상단 60%
		float BottomHeight = ContentRegion.y * 0.4f; // 하단 40%

		// ===== 상단: 애니메이션 프리뷰 뷰포트 (60%) =====
		ImGui::BeginChild("BlendSpace2DViewport", ImVec2(0, TopHeight), true, ImGuiWindowFlags_NoScrollbar);
		{
			RenderPreviewViewport();
		}
		ImGui::EndChild();

		// ===== 하단: 그리드 에디터 + 애니메이션 리스트 (40%) =====
		ImGui::BeginChild("BottomArea", ImVec2(0, BottomHeight), false, ImGuiWindowFlags_NoScrollbar);
		{
			float BottomLeftWidth = ContentRegion.x * 0.65f;  // 왼쪽 65%
			float BottomRightWidth = ContentRegion.x * 0.35f; // 오른쪽 35%

			// 왼쪽: 2D 블렌드 그리드
			ImGui::BeginChild("GridEditor", ImVec2(BottomLeftWidth, 0), true);
			{
				RenderGridEditor();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// 오른쪽: 애니메이션 시퀀스 목록
			ImGui::BeginChild("AnimationList", ImVec2(BottomRightWidth, 0), true);
			{
				RenderAnimationList();
			}
			ImGui::EndChild();
		}
		ImGui::EndChild();
	}
	ImGui::End();

	HandleKeyboardInput();
}

void SBlendSpace2DEditorWindow::RenderGrid()
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 그리드 라인 그리기
	for (float x = 0; x <= CanvasSize.x; x += GridCellSize)
	{
		ImVec2 p1(CanvasPos.x + x, CanvasPos.y);
		ImVec2 p2(CanvasPos.x + x, CanvasPos.y + CanvasSize.y);
		DrawList->AddLine(p1, p2, GridColor, 1.0f);
	}

	for (float y = 0; y <= CanvasSize.y; y += GridCellSize)
	{
		ImVec2 p1(CanvasPos.x, CanvasPos.y + y);
		ImVec2 p2(CanvasPos.x + CanvasSize.x, CanvasPos.y + y);
		DrawList->AddLine(p1, p2, GridColor, 1.0f);
	}

	// 중앙 축 (더 굵게)
	ImVec2 centerX(CanvasPos.x + CanvasSize.x * 0.5f, CanvasPos.y);
	ImVec2 centerXEnd(CanvasPos.x + CanvasSize.x * 0.5f, CanvasPos.y + CanvasSize.y);
	DrawList->AddLine(centerX, centerXEnd, AxisColor, 2.0f);

	ImVec2 centerY(CanvasPos.x, CanvasPos.y + CanvasSize.y * 0.5f);
	ImVec2 centerYEnd(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y * 0.5f);
	DrawList->AddLine(centerY, centerYEnd, AxisColor, 2.0f);
}

void SBlendSpace2DEditorWindow::RenderSamplePoints()
{
	if (!EditingBlendSpace)
		return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
	{
		const FBlendSample& Sample = EditingBlendSpace->Samples[i];
		ImVec2 ScreenPos = ParamToScreen(Sample.Position);

		ImU32 Color = (i == SelectedSampleIndex) ? SelectedSampleColor : SampleColor;

		// 샘플 포인트 (원)
		DrawList->AddCircleFilled(ScreenPos, SamplePointRadius, Color);
		DrawList->AddCircle(ScreenPos, SamplePointRadius, IM_COL32(255, 255, 255, 255), 0, 2.0f);

		// 샘플 이름 (애니메이션 이름)
		if (Sample.Animation)
		{
			FString AnimName = Sample.Animation->GetName();
			ImVec2 TextPos(ScreenPos.x + SamplePointRadius + 5, ScreenPos.y - 8);
			DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), AnimName.c_str());
		}
	}
}

void SBlendSpace2DEditorWindow::RenderPreviewMarker()
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 PreviewPos = ParamToScreen(PreviewParameter);

	// 프리뷰 마커 (속이 빈 원)
	DrawList->AddCircle(PreviewPos, PreviewMarkerRadius, PreviewColor, 0, 3.0f);

	// 십자가 마커
	DrawList->AddLine(
		ImVec2(PreviewPos.x - PreviewMarkerRadius, PreviewPos.y),
		ImVec2(PreviewPos.x + PreviewMarkerRadius, PreviewPos.y),
		PreviewColor, 2.0f);

	DrawList->AddLine(
		ImVec2(PreviewPos.x, PreviewPos.y - PreviewMarkerRadius),
		ImVec2(PreviewPos.x, PreviewPos.y + PreviewMarkerRadius),
		PreviewColor, 2.0f);
}

void SBlendSpace2DEditorWindow::RenderAxisLabels()
{
	if (!EditingBlendSpace)
		return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// X축 레이블
	char LabelMin[64], LabelMax[64];
	sprintf_s(LabelMin, "%.0f", EditingBlendSpace->XAxisMin);
	sprintf_s(LabelMax, "%.0f", EditingBlendSpace->XAxisMax);

	ImVec2 XMinPos(CanvasPos.x - 20, CanvasPos.y + CanvasSize.y + 10);
	ImVec2 XMaxPos(CanvasPos.x + CanvasSize.x - 20, CanvasPos.y + CanvasSize.y + 10);

	DrawList->AddText(XMinPos, IM_COL32(200, 200, 200, 255), LabelMin);
	DrawList->AddText(XMaxPos, IM_COL32(200, 200, 200, 255), LabelMax);

	// Y축 레이블
	sprintf_s(LabelMin, "%.0f", EditingBlendSpace->YAxisMin);
	sprintf_s(LabelMax, "%.0f", EditingBlendSpace->YAxisMax);

	ImVec2 YMinPos(CanvasPos.x - 40, CanvasPos.y + CanvasSize.y - 10);
	ImVec2 YMaxPos(CanvasPos.x - 40, CanvasPos.y);

	DrawList->AddText(YMinPos, IM_COL32(200, 200, 200, 255), LabelMin);
	DrawList->AddText(YMaxPos, IM_COL32(200, 200, 200, 255), LabelMax);
}

void SBlendSpace2DEditorWindow::RenderToolbar()
{
	static char SavePath[256] = "Data/Animation/MyBlendSpace.blend2d";
	static char LoadPath[256] = "Data/Animation/MyBlendSpace.blend2d";

	// 저장 버튼
	if (ImGui::Button("Save"))
	{
		if (EditingBlendSpace)
		{
			ImGui::OpenPopup("Save BlendSpace");
		}
	}

	// 저장 팝업
	if (ImGui::BeginPopupModal("Save BlendSpace", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Save BlendSpace2D to file:");
		ImGui::InputText("Path", SavePath, sizeof(SavePath));

		if (ImGui::Button("Save", ImVec2(120, 0)))
		{
			if (EditingBlendSpace)
			{
				EditingBlendSpace->SaveToFile(SavePath);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();

	// 로드 버튼
	if (ImGui::Button("Load"))
	{
		ImGui::OpenPopup("Load BlendSpace");
	}

	// 로드 팝업
	if (ImGui::BeginPopupModal("Load BlendSpace", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Load BlendSpace2D from file:");
		ImGui::InputText("Path", LoadPath, sizeof(LoadPath));

		if (ImGui::Button("Load", ImVec2(120, 0)))
		{
			UBlendSpace2D* LoadedBS = UBlendSpace2D::LoadFromFile(LoadPath);
			if (LoadedBS)
			{
				SetBlendSpace(LoadedBS);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	if (ImGui::Button("Add Sample"))
	{
		// 중앙에 샘플 추가
		if (EditingBlendSpace)
		{
			FVector2D CenterPos(
				(EditingBlendSpace->XAxisMin + EditingBlendSpace->XAxisMax) * 0.5f,
				(EditingBlendSpace->YAxisMin + EditingBlendSpace->YAxisMax) * 0.5f
			);
			AddSampleAtPosition(CenterPos);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Remove Selected"))
	{
		RemoveSelectedSample();
	}

	ImGui::SameLine();

	if (ImGui::Button("Clear All"))
	{
		if (EditingBlendSpace)
		{
			EditingBlendSpace->ClearSamples();
			SelectedSampleIndex = -1;
		}
	}
}

void SBlendSpace2DEditorWindow::RenderSampleList()
{
	if (!EditingBlendSpace)
		return;

	ImGui::Text("Sample Points (%d)", EditingBlendSpace->GetNumSamples());
	ImGui::Separator();

	for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
	{
		const FBlendSample& Sample = EditingBlendSpace->Samples[i];

		char Label[256];
		FString AnimName = Sample.Animation ? Sample.Animation->GetName() : "None";
		sprintf_s(Label, "[%d] %.1f, %.1f - %s",
			i,
			Sample.Position.X,
			Sample.Position.Y,
			AnimName.c_str());

		bool bSelected = (i == SelectedSampleIndex);
		if (ImGui::Selectable(Label, bSelected))
		{
			SelectSample(i);
		}
	}
}

void SBlendSpace2DEditorWindow::RenderProperties()
{
	if (!EditingBlendSpace)
		return;

	ImGui::Text("Blend Space Properties");
	ImGui::Separator();

	// 축 범위 편집
	ImGui::InputFloat("X Min", &EditingBlendSpace->XAxisMin);
	ImGui::InputFloat("X Max", &EditingBlendSpace->XAxisMax);
	ImGui::InputFloat("Y Min", &EditingBlendSpace->YAxisMin);
	ImGui::InputFloat("Y Max", &EditingBlendSpace->YAxisMax);

	ImGui::Separator();

	// 선택된 샘플 속성
	if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
	{
		ImGui::Text("Selected Sample Properties");
		FBlendSample& Sample = EditingBlendSpace->Samples[SelectedSampleIndex];

		ImGui::InputFloat("Position X", &Sample.Position.X);
		ImGui::InputFloat("Position Y", &Sample.Position.Y);

		if (Sample.Animation)
		{
			ImGui::Text("Animation: %s", Sample.Animation->GetName().c_str());
			ImGui::Text("Duration: %.2fs", Sample.Animation->GetPlayLength());
		}
		else
		{
			ImGui::Text("Animation: None");
		}
	}
}

ImVec2 SBlendSpace2DEditorWindow::ParamToScreen(FVector2D Param) const
{
	if (!EditingBlendSpace)
		return CanvasPos;

	// 파라미터를 0~1 범위로 정규화
	float NormX = (Param.X - EditingBlendSpace->XAxisMin) /
		(EditingBlendSpace->XAxisMax - EditingBlendSpace->XAxisMin);
	float NormY = (Param.Y - EditingBlendSpace->YAxisMin) /
		(EditingBlendSpace->YAxisMax - EditingBlendSpace->YAxisMin);

	// 0~1을 스크린 좌표로 변환 (Y축은 반전)
	float ScreenX = CanvasPos.x + NormX * CanvasSize.x;
	float ScreenY = CanvasPos.y + (1.0f - NormY) * CanvasSize.y;

	return ImVec2(ScreenX, ScreenY);
}

FVector2D SBlendSpace2DEditorWindow::ScreenToParam(ImVec2 ScreenPos) const
{
	if (!EditingBlendSpace)
		return FVector2D::Zero();

	// 스크린 좌표를 0~1 범위로 정규화
	float NormX = (ScreenPos.x - CanvasPos.x) / CanvasSize.x;
	float NormY = (CanvasPos.y + CanvasSize.y - ScreenPos.y) / CanvasSize.y;

	// 범위 클램핑
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);

	// 0~1을 파라미터 범위로 변환
	float ParamX = EditingBlendSpace->XAxisMin +
		NormX * (EditingBlendSpace->XAxisMax - EditingBlendSpace->XAxisMin);
	float ParamY = EditingBlendSpace->YAxisMin +
		NormY * (EditingBlendSpace->YAxisMax - EditingBlendSpace->YAxisMin);

	return FVector2D(ParamX, ParamY);
}

void SBlendSpace2DEditorWindow::HandleMouseInput()
{
	if (!EditingBlendSpace)
		return;

	ImVec2 MousePos = ImGui::GetMousePos();
	bool bMouseInCanvas = ImGui::IsMouseHoveringRect(
		CanvasPos,
		ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y)
	);

	if (!bMouseInCanvas)
		return;

	// 왼쪽 클릭: PreviewMarker, 샘플 선택 또는 드래그 시작
	if (ImGui::IsMouseClicked(0))
	{
		// 먼저 PreviewMarker를 클릭했는지 확인
		ImVec2 PreviewScreenPos = ParamToScreen(PreviewParameter);
		float DistToPreview = sqrtf(
			(MousePos.x - PreviewScreenPos.x) * (MousePos.x - PreviewScreenPos.x) +
			(MousePos.y - PreviewScreenPos.y) * (MousePos.y - PreviewScreenPos.y)
		);

		if (DistToPreview <= PreviewMarkerRadius + 5.0f)
		{
			// PreviewMarker 드래그 시작
			bDraggingPreviewMarker = true;
			SelectedSampleIndex = -1;
		}
		else
		{
			// 클릭한 위치에 샘플이 있는지 확인
			bool bFoundSample = false;
			for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
			{
				ImVec2 SampleScreenPos = ParamToScreen(EditingBlendSpace->Samples[i].Position);
				float Dist = sqrtf(
					(MousePos.x - SampleScreenPos.x) * (MousePos.x - SampleScreenPos.x) +
					(MousePos.y - SampleScreenPos.y) * (MousePos.y - SampleScreenPos.y)
				);

				if (Dist <= SamplePointRadius + 5.0f)
				{
					SelectSample(i);
					bDraggingSample = true;
					bFoundSample = true;
					break;
				}
			}

			if (!bFoundSample)
			{
				SelectedSampleIndex = -1;
			}
		}
	}

	// PreviewMarker 드래그 중
	if (bDraggingPreviewMarker && ImGui::IsMouseDragging(0))
	{
		FVector2D NewPos = ScreenToParam(MousePos);

		// 축 범위 내로 제한
		NewPos.X = FMath::Clamp(NewPos.X, EditingBlendSpace->XAxisMin, EditingBlendSpace->XAxisMax);
		NewPos.Y = FMath::Clamp(NewPos.Y, EditingBlendSpace->YAxisMin, EditingBlendSpace->YAxisMax);

		PreviewParameter = NewPos;
	}

	// 샘플 드래그 중
	if (bDraggingSample && ImGui::IsMouseDragging(0))
	{
		if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
		{
			FVector2D NewPos = ScreenToParam(MousePos);
			EditingBlendSpace->Samples[SelectedSampleIndex].Position = NewPos;
		}
	}

	// 드래그 종료
	if (ImGui::IsMouseReleased(0))
	{
		bDraggingSample = false;
		bDraggingPreviewMarker = false;
	}

	// 더블 클릭: 새 샘플 추가
	if (ImGui::IsMouseDoubleClicked(0))
	{
		FVector2D ClickPos = ScreenToParam(MousePos);
		AddSampleAtPosition(ClickPos);
	}
}

void SBlendSpace2DEditorWindow::HandleKeyboardInput()
{
	// Delete 키: 선택된 샘플 삭제
	if (INPUT.IsKeyPressed(VK_DELETE))
	{
		RemoveSelectedSample();
	}
}

void SBlendSpace2DEditorWindow::AddSampleAtPosition(FVector2D Position)
{
	if (!EditingBlendSpace)
		return;

	// TODO: 애니메이션 선택 다이얼로그 열기
	// 지금은 nullptr로 추가
	EditingBlendSpace->AddSample(Position, nullptr);

	// 새로 추가된 샘플 선택
	SelectedSampleIndex = EditingBlendSpace->GetNumSamples() - 1;
}

void SBlendSpace2DEditorWindow::RemoveSelectedSample()
{
	if (!EditingBlendSpace)
		return;

	if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
	{
		EditingBlendSpace->RemoveSample(SelectedSampleIndex);
		SelectedSampleIndex = -1;
	}
}

void SBlendSpace2DEditorWindow::SelectSample(int32 Index)
{
	if (Index >= 0 && Index < EditingBlendSpace->GetNumSamples())
	{
		SelectedSampleIndex = Index;
	}
}

// ===== 새로운 레이아웃 렌더 함수들 =====

/**
 * @brief 상단: 애니메이션 프리뷰 뷰포트
 */
void SBlendSpace2DEditorWindow::RenderPreviewViewport()
{
	ImGui::Text("Animation Preview");
	ImGui::Separator();

	// 뷰포트 렌더링 영역 (타임라인을 위해 하단 여백 확보)
	float TimelineHeight = 120.0f;  // 타임라인 높이
	ImGui::BeginChild("ViewportRenderArea", ImVec2(0, -(TimelineHeight + 10)), false, ImGuiWindowFlags_NoScrollbar);
	ImVec2 childPos = ImGui::GetWindowPos();
	ImVec2 childSize = ImGui::GetWindowSize();
	ImVec2 rectMin = childPos;
	ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);

	// 뷰포트 영역 저장 (SkeletalMeshViewer의 CenterRect와 동일)
	PreviewViewportRect.Left = rectMin.x;
	PreviewViewportRect.Top = rectMin.y;
	PreviewViewportRect.Right = rectMax.x;
	PreviewViewportRect.Bottom = rectMax.y;
	PreviewViewportRect.UpdateMinMax();

	ImGui::EndChild();

	ImGui::Separator();

	// 타임라인 렌더링
	RenderTimelineControls();

	// 재생/일시정지 버튼
	if (bIsPlaying)
	{
		if (ImGui::Button("Pause", ImVec2(80, 0)))
		{
			bIsPlaying = false;
		}
	}
	else
	{
		if (ImGui::Button("Play", ImVec2(80, 0)))
		{
			bIsPlaying = true;
		}
	}

	ImGui::SameLine();

	// 루프 토글
	ImGui::Checkbox("Loop", &bLoopAnimation);

	ImGui::SameLine();

	// 재생 속도
	ImGui::SetNextItemWidth(100);
	ImGui::DragFloat("Speed", &PlaybackSpeed, 0.1f, 0.1f, 5.0f, "%.1fx");

	ImGui::Spacing();

	// 블렌드 파라미터 슬라이더
	ImGui::SliderFloat("X Axis", &PreviewParameter.X,
		EditingBlendSpace->XAxisMin,
		EditingBlendSpace->XAxisMax);

	// Y축 슬라이더를 별도로 (범위가 다를 수 있음)
	ImGui::SliderFloat("Y Axis", &PreviewParameter.Y,
		EditingBlendSpace->YAxisMin,
		EditingBlendSpace->YAxisMax);
}

/**
 * @brief 하단 왼쪽: 2D 블렌드 그리드 에디터
 */
void SBlendSpace2DEditorWindow::RenderGridEditor()
{
	ImGui::Text("Blend Space 2D Grid");
	ImGui::Separator();

	// 축 정보 표시
	ImGui::Text("X: %s (%.1f ~ %.1f)  |  Y: %s (%.1f ~ %.1f)",
		EditingBlendSpace->XAxisName.c_str(),
		EditingBlendSpace->XAxisMin,
		EditingBlendSpace->XAxisMax,
		EditingBlendSpace->YAxisName.c_str(),
		EditingBlendSpace->YAxisMin,
		EditingBlendSpace->YAxisMax);

	ImGui::Separator();

	// 그리드 캔버스
	CanvasPos = ImGui::GetCursorScreenPos();
	ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
	CanvasSize = ImVec2(FMath::Min(AvailableSize.x - 20, 500.0f), FMath::Min(AvailableSize.y - 20, 500.0f));

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 캔버스 배경
	DrawList->AddRectFilled(CanvasPos,
		ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y),
		IM_COL32(30, 30, 30, 255));

	// 그리드 렌더링
	RenderGrid();

	// 축 라벨 렌더링
	RenderAxisLabels();

	// 샘플 포인트 렌더링
	RenderSamplePoints();

	// 프리뷰 마커 렌더링
	RenderPreviewMarker();

	// 입력 처리
	HandleMouseInput();

	// 캔버스 영역 확보 (클릭 감지를 위해)
	ImGui::Dummy(CanvasSize);
}

/**
 * @brief 하단 오른쪽: 애니메이션 시퀀스 목록
 */
void SBlendSpace2DEditorWindow::RenderAnimationList()
{
	ImGui::Text("Animation Sequences");
	ImGui::Separator();

	// 애니메이션 로드 버튼
	if (ImGui::Button("Load Animations", ImVec2(-1, 30)))
	{
		// TODO: FBX 파일에서 애니메이션 로드
		ImGui::OpenPopup("LoadAnimationsPopup");
	}

	// 로드 팝업
	if (ImGui::BeginPopupModal("LoadAnimationsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Select Skeletal Mesh:");
		ImGui::Separator();

		// ResourceManager에서 로드된 SkeletalMesh들 가져오기
		TArray<USkeletalMesh*> LoadedMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();

		static int SelectedMeshIndex = -1;
		USkeletalMesh* SelectedMesh = nullptr;

		ImGui::Text("Loaded Skeletal Meshes: %d", LoadedMeshes.Num());
		ImGui::Spacing();

		// SkeletalMesh 리스트 표시 (스크롤 가능한 영역)
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
		ImGui::BeginChild("MeshList", ImVec2(500, 350), true, ImGuiWindowFlags_None);
		{
			if (LoadedMeshes.Num() == 0)
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Skeletal Meshes loaded.");
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Please load a Skeletal Mesh first.");
			}
			else
			{
				for (int32 i = 0; i < LoadedMeshes.Num(); ++i)
				{
					USkeletalMesh* Mesh = LoadedMeshes[i];
					if (!Mesh) continue;

					const FSkeleton* Skeleton = Mesh->GetSkeleton();
					if (!Skeleton) continue;

					bool bIsSelected = (i == SelectedMeshIndex);

					// 선택 가능한 항목 - Skeleton 이름 또는 파일 경로 표시
					FString DisplayName;
					if (!Skeleton->Name.empty())
					{
						DisplayName = Skeleton->Name;
					}
					else
					{
						DisplayName = Mesh->GetFilePath();
					}

					// 선택 하이라이트 색상
					if (bIsSelected)
					{
						ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
					}

					if (ImGui::Selectable(DisplayName.c_str(), bIsSelected, ImGuiSelectableFlags_None, ImVec2(0, 25)))
					{
						SelectedMeshIndex = i;
						SelectedMesh = Mesh;
					}

					if (bIsSelected)
					{
						ImGui::PopStyleColor();
					}

					// 더블클릭으로 바로 로드
					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
					{
						SelectedMesh = Mesh;
						// 아래 Load 버튼 로직 실행
						goto LoadSelectedMesh;
					}

					// 툴팁: 자세한 정보 표시
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Skeleton: %s", Skeleton->Name.c_str());
						ImGui::Text("File: %s", Mesh->GetFilePath().c_str());
						ImGui::Text("Bones: %d", Skeleton->Bones.Num());
						ImGui::Text("Animations: %d", Mesh->GetAnimations().Num());
						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Double-click to load animations");
						ImGui::EndTooltip();
					}
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		ImGui::Separator();

		if (ImGui::Button("Load", ImVec2(120, 0)))
		{
			LoadSelectedMesh:
			if (SelectedMeshIndex >= 0 && SelectedMeshIndex < LoadedMeshes.Num())
			{
				USkeletalMesh* Mesh = LoadedMeshes[SelectedMeshIndex];
				if (Mesh)
				{
					// 기존 리스트 초기화 (선택된 스켈레톤의 애니메이션만 표시)
					AvailableAnimations.clear();

					// SkeletalMesh에 저장된 애니메이션들을 가져옴
					const TArray<UAnimSequence*>& Animations = Mesh->GetAnimations();

					// 모든 애니메이션 추가
					for (UAnimSequence* Anim : Animations)
					{
						if (Anim)
						{
							AvailableAnimations.Add(Anim);
						}
					}

					// 프리뷰 메시 설정
					if (PreviewState && PreviewState->PreviewActor)
					{
						PreviewState->PreviewActor->SetSkeletalMesh(Mesh->GetFilePath());
						PreviewState->CurrentMesh = Mesh;
						if (auto* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent())
						{
							SkelComp->SetVisibility(true);
						}
					}

					UE_LOG("Loaded %d animations from SkeletalMesh: %s", Animations.Num(), Mesh->GetName().c_str());
				}
			}

			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	// SkeletalMeshViewer 스타일로 헤더 표시
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
	ImGui::Text("Animation List (%d)", AvailableAnimations.Num());
	ImGui::PopStyleColor();
	ImGui::Separator();
	ImGui::Spacing();

	// 애니메이션 리스트
	if (AvailableAnimations.Num() == 0)
	{
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No animations loaded.");
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a Skeletal Mesh to load animations.");
	}
	else
	{
		ImGui::BeginChild("AnimListScroll", ImVec2(0, 0), false);
		for (int32 i = 0; i < AvailableAnimations.Num(); ++i)
		{
			UAnimSequence* Anim = AvailableAnimations[i];
			if (!Anim) continue;

			UAnimDataModel* DataModel = Anim->GetDataModel();
			if (!DataModel) continue;

			bool bIsSelected = (i == SelectedAnimationIndex);

			// FilePath에서 "파일명#애니메이션이름" 형식으로 표시
			FString FilePath = Anim->GetFilePath();
			FString DisplayName;

			size_t HashPos = FilePath.find('#');
			if (HashPos != FString::npos)
			{
				FString FullPath = FilePath.substr(0, HashPos); // "path/to/file.fbx"
				FString AnimStackName = FilePath.substr(HashPos + 1); // "AnimStackName"

				// 파일명만 추출 (경로 제거)
				size_t LastSlash = FullPath.find_last_of("/\\");
				FString FileName;
				if (LastSlash != FString::npos)
				{
					FileName = FullPath.substr(LastSlash + 1);
				}
				else
				{
					FileName = FullPath;
				}

				// 확장자 제거
				size_t DotPos = FileName.find_last_of('.');
				if (DotPos != FString::npos)
				{
					FileName = FileName.substr(0, DotPos);
				}

				DisplayName = FileName + "#" + AnimStackName;
			}
			else
			{
				// FilePath에 #이 없으면 GetName() 사용
				DisplayName = Anim->GetName();
				if (DisplayName.empty())
				{
					DisplayName = "Anim " + std::to_string(i);
				}
			}

			// "애니메이션이름 (재생시간)" 형식으로 표시
			char LabelBuffer[256];
			snprintf(LabelBuffer, sizeof(LabelBuffer), "%s (%.1fs)", DisplayName.c_str(), DataModel->GetPlayLength());
			FString Label = LabelBuffer;

			// 선택 가능한 항목
			if (ImGui::Selectable(Label.c_str(), bIsSelected))
			{
				SelectedAnimationIndex = i;
			}

			// 더블 클릭으로 샘플에 추가
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				// 선택된 그리드 샘플에 애니메이션 할당
				if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
				{
					EditingBlendSpace->Samples[SelectedSampleIndex].Animation = Anim;
				}
			}

			// 우클릭 메뉴
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Add to Grid"))
				{
					// 중앙에 새 샘플 추가
					FVector2D CenterPos(
						(EditingBlendSpace->XAxisMin + EditingBlendSpace->XAxisMax) * 0.5f,
						(EditingBlendSpace->YAxisMin + EditingBlendSpace->YAxisMax) * 0.5f
					);

					EditingBlendSpace->AddSample(CenterPos, Anim);
				}

				if (ImGui::MenuItem("Preview"))
				{
					// TODO: 이 애니메이션을 프리뷰 뷰포트에서 재생
				}

				ImGui::EndPopup();
			}

			// 애니메이션 정보 툴팁
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Name: %s", Anim->GetName().c_str());
				if (Anim->GetDataModel())
				{
					ImGui::Text("Duration: %.2f seconds", Anim->GetDataModel()->GetPlayLength());
					ImGui::Text("Frames: %d", Anim->GetDataModel()->GetNumberOfFrames());
				}
				ImGui::Text("Double-click to assign to selected sample");
				ImGui::Text("Right-click for more options");
				ImGui::EndTooltip();
			}
		}
		ImGui::EndChild();
	}
}

void SBlendSpace2DEditorWindow::OnUpdate(float DeltaSeconds)
{
	if (!PreviewState || !PreviewState->World)
	{
		return;
	}

	// ViewportClient Tick (카메라 입력 처리)
	if (PreviewState->Client)
	{
		PreviewState->Client->Tick(DeltaSeconds);
	}

	// BlendSpace2D 애니메이션 블렌딩 및 재생
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp && EditingBlendSpace->GetNumSamples() > 0)
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
					if (BlendNode->GetBlendSpace() != EditingBlendSpace)
					{
						BlendNode->SetBlendSpace(EditingBlendSpace);
						BlendNode->SetAutoCalculateParameter(false);  // 수동으로 파라미터 설정
					}

					// 현재 PreviewParameter 설정
					BlendNode->SetBlendParameter(PreviewParameter);

					// 참고: Update()와 Evaluate()는 SkeletalMeshComponent::TickComponent()에서 자동 호출됨 (Unreal 방식)
				}
			}
		}
	}

	// PlaybackSpeed를 World TimeDilation으로 적용 (Unreal Engine 방식)
	if (bIsPlaying)
	{
		PreviewState->World->SetTimeDilation(PlaybackSpeed) ;
	}
	else
	{
		PreviewState->World->SetTimeDilation(PlaybackSpeed);  // 일시정지
	}

	// PreviewWorld 업데이트 (SkeletalMeshViewer와 동일)
	PreviewState->World->Tick(DeltaSeconds);
}

void SBlendSpace2DEditorWindow::OnMouseMove(FVector2D MousePos)
{
	if (!PreviewState || !PreviewState->Viewport) return;

	if (PreviewViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(PreviewViewportRect.Left, PreviewViewportRect.Top);
		PreviewState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
	}
}

void SBlendSpace2DEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!PreviewState || !PreviewState->Viewport) return;

	if (PreviewViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(PreviewViewportRect.Left, PreviewViewportRect.Top);
		PreviewState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

void SBlendSpace2DEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!PreviewState || !PreviewState->Viewport) return;

	if (PreviewViewportRect.Contains(MousePos))
	{
		FVector2D LocalPos = MousePos - FVector2D(PreviewViewportRect.Left, PreviewViewportRect.Top);
		PreviewState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
	}
}

void SBlendSpace2DEditorWindow::OnRenderViewport()
{
	// 윈도우가 닫혀있으면 렌더링하지 않음
	if (!bIsOpen)
	{
		return;
	}

	// SkeletalMeshViewer와 동일한 방식으로 뷰포트 렌더링
	if (PreviewState && PreviewState->Viewport && PreviewViewportRect.GetWidth() > 0 && PreviewViewportRect.GetHeight() > 0)
	{
		const uint32 NewStartX = static_cast<uint32>(PreviewViewportRect.Left);
		const uint32 NewStartY = static_cast<uint32>(PreviewViewportRect.Top);
		const uint32 NewWidth = static_cast<uint32>(PreviewViewportRect.Right - PreviewViewportRect.Left);
		const uint32 NewHeight = static_cast<uint32>(PreviewViewportRect.Bottom - PreviewViewportRect.Top);

		PreviewState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

		// 뷰포트 렌더링
		PreviewState->Viewport->Render();
	}
	else
	{
		// 뷰포트 영역이 없으면 리셋
		PreviewViewportRect = FRect(0, 0, 0, 0);
		PreviewViewportRect.UpdateMinMax();
	}
}

// ========================================
// Timeline Implementation
// ========================================

void SBlendSpace2DEditorWindow::RenderTimelineControls()
{
	// BlendSpace2D에서 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation)
	{
		ImGui::TextDisabled("No animation playing");
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();

	// === Timeline 렌더링 영역 ===
	ImGui::BeginChild("TimelineArea", ImVec2(0, -40), true);
	{
		RenderTimeline();
	}
	ImGui::EndChild();

	ImGui::Separator();

	// === 재생 컨트롤 버튼들 ===
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

	float ButtonSize = 20.0f;
	ImVec2 ButtonSizeVec(ButtonSize, ButtonSize);

	// ToFront |<<
	if (IconGoToFront && IconGoToFront->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToFront", IconGoToFront->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToFront();
		}
	}
	else
	{
		if (ImGui::Button("|<<", ButtonSizeVec))
		{
			TimelineToFront();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("To Front");
	}

	ImGui::SameLine();

	// ToPrevious |<
	if (IconStepBackwards && IconStepBackwards->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepBackwards", IconStepBackwards->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToPrevious();
		}
	}
	else
	{
		if (ImGui::Button("|<", ButtonSizeVec))
		{
			TimelineToPrevious();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Previous Frame");
	}

	ImGui::SameLine();

	// Reverse <<
	if (IconBackwards && IconBackwards->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Backwards", IconBackwards->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineReverse();
		}
	}
	else
	{
		if (ImGui::Button("<<", ButtonSizeVec))
		{
			TimelineReverse();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Reverse");
	}

	ImGui::SameLine();

	// Play/Pause
	if (bIsPlaying)
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

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Pause");
		}

		if (bPauseClicked)
		{
			if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
			{
				UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
				if (AnimInst)
				{
					AnimInst->StopAnimation();
				}
			}
			bIsPlaying = false;
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

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Play");
		}

		if (bPlayClicked)
		{
			TimelinePlay();
		}
	}

	ImGui::SameLine();

	// ToNext >|
	if (IconStepForward && IconStepForward->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##StepForward", IconStepForward->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToNext();
		}
	}
	else
	{
		if (ImGui::Button(">|", ButtonSizeVec))
		{
			TimelineToNext();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Next Frame");
	}

	ImGui::SameLine();

	// ToEnd >>|
	if (IconGoToEnd && IconGoToEnd->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ToEnd", IconGoToEnd->GetShaderResourceView(), ButtonSizeVec))
		{
			TimelineToEnd();
		}
	}
	else
	{
		if (ImGui::Button(">>|", ButtonSizeVec))
		{
			TimelineToEnd();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("To End");
	}

	ImGui::SameLine();

	// Loop 토글
	bool bWasLooping = bLoopAnimation;
	UTexture* LoopIcon = bWasLooping ? IconLoop : IconLoopOff;
	if (LoopIcon && LoopIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##Loop", LoopIcon->GetShaderResourceView(), ButtonSizeVec))
		{
			bLoopAnimation = !bLoopAnimation;
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
			bLoopAnimation = !bLoopAnimation;
		}
		if (bWasLooping)
		{
			ImGui::PopStyleColor();
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Loop");
	}

	ImGui::SameLine();

	// 재생 속도
	ImGui::Text("Speed:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::DragFloat("##Speed", &PlaybackSpeed, 0.05f, 0.1f, 5.0f, "%.2fx");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Drag or click to edit playback speed");
	}

	ImGui::PopStyleVar(2);
}

void SBlendSpace2DEditorWindow::RenderTimeline()
{
	// BlendSpace2D에서 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();

	// 타임라인 영역 크기 설정
	ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
	ImVec2 TimelineSize = ImVec2(ContentAvail.x, ContentAvail.y);

	ImVec2 CursorPos = ImGui::GetCursorScreenPos();
	ImVec2 TimelineMin = CursorPos;
	ImVec2 TimelineMax = ImVec2(CursorPos.x + TimelineSize.x, CursorPos.y + TimelineSize.y);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 전체 범위 표시
	float StartTime = 0.0f;
	float EndTime = MaxTime;

	// 눈금자 영역 (상단 30픽셀)
	const float RulerHeight = 30.0f;
	ImVec2 RulerMin = TimelineMin;
	ImVec2 RulerMax = ImVec2(TimelineMax.x, TimelineMin.y + RulerHeight);

	// 눈금자 렌더링
	DrawTimelineRuler(DrawList, RulerMin, RulerMax, StartTime, EndTime);

	// 타임라인 영역 (눈금자 아래)
	ImVec2 ScrollTimelineMin = ImVec2(TimelineMin.x, RulerMax.y);
	ImVec2 ScrollTimelineMax = TimelineMax;
	float ScrollTimelineWidth = ScrollTimelineMax.x - ScrollTimelineMin.x;
	float ScrollTimelineHeight = ScrollTimelineMax.y - ScrollTimelineMin.y;

	// 배경
	DrawList->AddRectFilled(ScrollTimelineMin, ScrollTimelineMax, IM_COL32(40, 40, 40, 255));

	// 세로 그리드 (프레임 단위)
	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	for (int32 FrameIndex = 0; FrameIndex <= TotalFrames; ++FrameIndex)
	{
		float FrameTime = (FrameRate > 0.0f) ? (FrameIndex / FrameRate) : 0.0f;
		float NormalizedX = (FrameTime - StartTime) / (EndTime - StartTime);
		float ScreenX = ScrollTimelineMin.x + NormalizedX * ScrollTimelineWidth;

		DrawList->AddLine(
			ImVec2(ScreenX, ScrollTimelineMin.y),
			ImVec2(ScreenX, ScrollTimelineMax.y),
			IM_COL32(70, 70, 70, 255),
			1.0f
		);
	}

	// Playhead 렌더링 (빨간 세로 바)
	DrawTimelinePlayhead(DrawList, ScrollTimelineMin, ScrollTimelineMax, CurrentAnimationTime, StartTime, EndTime);

	// 마우스 입력 처리 (타임라인 스크러빙)
	ImGui::SetCursorScreenPos(ScrollTimelineMin);
	ImGui::InvisibleButton("##Timeline", ImVec2(ScrollTimelineWidth, ScrollTimelineHeight));

	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		ImVec2 MousePos = ImGui::GetMousePos();
		float NormalizedX = (MousePos.x - ScrollTimelineMin.x) / ScrollTimelineWidth;
		float MouseTime = FMath::Lerp(StartTime, EndTime, FMath::Clamp(NormalizedX, 0.0f, 1.0f));

		CurrentAnimationTime = FMath::Clamp(MouseTime, 0.0f, MaxTime);

		// AnimInstance 시간 업데이트
		if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
		{
			UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
			if (AnimInst)
			{
				AnimInst->SetPosition(CurrentAnimationTime);
			}
		}
	}
}

void SBlendSpace2DEditorWindow::DrawTimelineRuler(ImDrawList* DrawList, const ImVec2& RulerMin, const ImVec2& RulerMax, float StartTime, float EndTime)
{
	// 눈금자 배경
	DrawList->AddRectFilled(RulerMin, RulerMax, IM_COL32(50, 50, 50, 255));

	// Ruler 하단 테두리
	DrawList->AddLine(
		ImVec2(RulerMin.x, RulerMax.y),
		ImVec2(RulerMax.x, RulerMax.y),
		IM_COL32(60, 60, 60, 255),
		1.0f
	);

	// BlendSpace2D에서 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation || !CurrentAnimation->GetDataModel())
	{
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	if (FrameRate <= 0.0f)
	{
		return;
	}

	int32 TotalFrames = DataModel->GetNumberOfFrames();
	float RulerWidth = RulerMax.x - RulerMin.x;

	// 프레임마다 세로 줄 그리기
	for (int32 FrameIndex = 0; FrameIndex <= TotalFrames; ++FrameIndex)
	{
		float FrameTime = FrameIndex / FrameRate;
		float NormalizedX = (FrameTime - StartTime) / (EndTime - StartTime);
		float ScreenX = RulerMin.x + NormalizedX * RulerWidth;

		// 프레임 번호 표시 (일정 간격마다)
		float PixelsPerFrame = RulerWidth / TotalFrames;
		int32 LabelInterval = 1;

		if (PixelsPerFrame < 10.0f)
		{
			LabelInterval = 10;
		}
		else if (PixelsPerFrame < 20.0f)
		{
			LabelInterval = 5;
		}
		else if (PixelsPerFrame < 40.0f)
		{
			LabelInterval = 2;
		}

		// 라벨 표시
		if (FrameIndex % LabelInterval == 0)
		{
			char FrameLabel[32];
			snprintf(FrameLabel, sizeof(FrameLabel), "%d", FrameIndex);

			ImVec2 TextSize = ImGui::CalcTextSize(FrameLabel);
			float RulerCenterY = (RulerMin.y + RulerMax.y) * 0.5f - TextSize.y * 0.5f;
			DrawList->AddText(
				ImVec2(ScreenX - TextSize.x * 0.5f, RulerCenterY),
				IM_COL32(220, 220, 220, 255),
				FrameLabel
			);
		}
	}
}

void SBlendSpace2DEditorWindow::DrawTimelinePlayhead(ImDrawList* DrawList, const ImVec2& TimelineMin, const ImVec2& TimelineMax, float CurrentTime, float StartTime, float EndTime)
{
	float Duration = EndTime - StartTime;
	if (Duration <= 0.0f)
	{
		return;
	}

	float NormalizedX = (CurrentTime - StartTime) / Duration;
	if (NormalizedX < 0.0f || NormalizedX > 1.0f)
	{
		return; // 화면 밖
	}

	float ScreenX = TimelineMin.x + NormalizedX * (TimelineMax.x - TimelineMin.x);

	// 빨간 세로 바
	DrawList->AddLine(
		ImVec2(ScreenX, TimelineMin.y),
		ImVec2(ScreenX, TimelineMax.y),
		IM_COL32(255, 50, 50, 255),
		2.0f
	);

	// 상단 삼각형 핸들
	float TriangleSize = 6.0f;
	DrawList->AddTriangleFilled(
		ImVec2(ScreenX, TimelineMin.y),
		ImVec2(ScreenX - TriangleSize, TimelineMin.y + TriangleSize * 1.5f),
		ImVec2(ScreenX + TriangleSize, TimelineMin.y + TriangleSize * 1.5f),
		IM_COL32(255, 50, 50, 255)
	);
}

// Timeline 컨트롤 기능
void SBlendSpace2DEditorWindow::TimelineToFront()
{
	CurrentAnimationTime = 0.0f;

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPosition(CurrentAnimationTime);
		}
	}
}

void SBlendSpace2DEditorWindow::TimelineToPrevious()
{
	// 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	// 프레임 단위로 스냅하여 이동
	const FFrameRate& FrameRate = DataModel->GetFrameRate();
	float TimePerFrame = 1.0f / FrameRate.AsDecimal();

	// 현재 시간을 프레임으로 변환 (반올림)
	int32 CurrentFrame = static_cast<int32>(CurrentAnimationTime / TimePerFrame + 0.5f);

	// 이전 프레임으로 이동
	CurrentFrame = FMath::Max(0, CurrentFrame - 1);

	// 프레임을 시간으로 변환
	CurrentAnimationTime = static_cast<float>(CurrentFrame) * TimePerFrame;

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPosition(CurrentAnimationTime);
		}
	}
}

void SBlendSpace2DEditorWindow::TimelineReverse()
{
	// 역재생 (음수 속도)
	PlaybackSpeed = -FMath::Abs(PlaybackSpeed);

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPlayRate(PlaybackSpeed);
			AnimInst->ResumeAnimation();
			bIsPlaying = true;
		}
	}
}

void SBlendSpace2DEditorWindow::TimelinePlay()
{
	// 정방향 재생
	PlaybackSpeed = FMath::Abs(PlaybackSpeed);

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPlayRate(PlaybackSpeed);
			AnimInst->ResumeAnimation();
			bIsPlaying = true;
		}
	}
}

void SBlendSpace2DEditorWindow::TimelineToNext()
{
	// 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	// 프레임 단위로 스냅하여 이동
	const FFrameRate& FrameRate = DataModel->GetFrameRate();
	float TimePerFrame = 1.0f / FrameRate.AsDecimal();
	int32 TotalFrames = DataModel->GetNumberOfFrames();

	// 현재 시간을 프레임으로 변환 (반올림)
	int32 CurrentFrame = static_cast<int32>(CurrentAnimationTime / TimePerFrame + 0.5f);

	// 다음 프레임으로 이동
	CurrentFrame = FMath::Min(TotalFrames, CurrentFrame + 1);

	// 프레임을 시간으로 변환
	CurrentAnimationTime = static_cast<float>(CurrentFrame) * TimePerFrame;

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPosition(CurrentAnimationTime);
		}
	}
}

void SBlendSpace2DEditorWindow::TimelineToEnd()
{
	// 현재 재생중인 애니메이션 가져오기
	UAnimSequence* CurrentAnimation = nullptr;
	if (EditingBlendSpace && PreviewState->PreviewActor)
	{
		USkeletalMeshComponent* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent();
		if (SkelComp)
		{
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance();
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	if (!CurrentAnimation)
	{
		return;
	}

	UAnimDataModel* DataModel = CurrentAnimation->GetDataModel();
	if (!DataModel)
	{
		return;
	}

	CurrentAnimationTime = DataModel->GetPlayLength();

	if (PreviewState->PreviewActor && PreviewState->PreviewActor->GetSkeletalMeshComponent())
	{
		UAnimInstance* AnimInst = PreviewState->PreviewActor->GetSkeletalMeshComponent()->GetAnimInstance();
		if (AnimInst)
		{
			AnimInst->SetPosition(CurrentAnimationTime);
		}
	}
}
