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
#include <commdlg.h>  // Windows 파일 다이얼로그

#include "PlatformProcess.h"

// Helper function to get a clean display name from an animation sequence
static FString GetDisplayNameForAnimation(UAnimSequence* InAnimation)
{
	if (!InAnimation)
	{
		return "None";
	}

	FString FilePath = InAnimation->GetFilePath();
	FString DisplayName;

	// FBX 파일에 여러 애니메이션 스택이 있는 경우 "FileName#StackName" 형식
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
		// 일반 파일 경로
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
	// 이전 BlendSpace 정리 (필요한 경우)
	// 주의: 다른 곳에서 참조하고 있을 수 있으므로 일반적으로는 삭제하지 않음
	// if (EditingBlendSpace && EditingBlendSpace != InBlendSpace)
	// {
	//     delete EditingBlendSpace;
	// }

	EditingBlendSpace = InBlendSpace;
	SelectedSampleIndex = -1;
	bDraggingSample = false;

	// 기본 프리뷰 파라미터 설정 (중앙)
	if (EditingBlendSpace)
	{
		PreviewParameter.X = (EditingBlendSpace->XAxisMin + EditingBlendSpace->XAxisMax) * 0.5f;
		PreviewParameter.Y = (EditingBlendSpace->YAxisMin + EditingBlendSpace->YAxisMax) * 0.5f;

		// 저장된 스켈레톤 메시 경로가 있으면 자동으로 로드
		if (!EditingBlendSpace->EditorSkeletalMeshPath.empty())
		{
			USkeletalMesh* SavedMesh = UResourceManager::GetInstance().Get<USkeletalMesh>(EditingBlendSpace->EditorSkeletalMeshPath);
			if (SavedMesh)
			{
				// 애니메이션 목록 초기화
				AvailableAnimations.clear();

				// 스켈레톤 메시의 애니메이션 추가 (.anim 파일 제한)
				const TArray<UAnimSequence*>& Animations = SavedMesh->GetAnimations();
				for (UAnimSequence* Anim : Animations)
				{
					if (Anim && Anim->GetFilePath().ends_with(".anim"))
					{
						AvailableAnimations.Add(Anim);
					}
				}

				// 프리뷰 메시 설정
				if (PreviewState && PreviewState->PreviewActor)
				{
					PreviewState->PreviewActor->SetSkeletalMesh(EditingBlendSpace->EditorSkeletalMeshPath);
					PreviewState->CurrentMesh = SavedMesh;
					if (auto* SkelComp = PreviewState->PreviewActor->GetSkeletalMeshComponent())
					{
						SkelComp->SetVisibility(true);
					}
				}

				UE_LOG("Loaded %d animations from saved SkeletalMesh: %s",
					Animations.Num(),
					EditingBlendSpace->EditorSkeletalMeshPath.c_str());
			}
			else
			{
				UE_LOG("Warning: Failed to load saved SkeletalMesh: %s",
					EditingBlendSpace->EditorSkeletalMeshPath.c_str());
			}
		}
	}
}

void SBlendSpace2DEditorWindow::OnRender()
{
	if (!bIsOpen)
	{
		return;
	}

	// 첫 프레임에만 윈도우 크기 설정
	static bool bFirstFrame = true;
	if (bFirstFrame)
	{
		ImGui::SetNextWindowSize(ImVec2(1600.0f, 1000.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(160.0f, 90.0f), ImGuiCond_FirstUseEver);
		bFirstFrame = false;
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
		float TopHeight = ContentRegion.y * 0.55f;     // 상단 55%
		float BottomHeight = ContentRegion.y * 0.43f;  // 하단 43%

		// ===== 상단: 애니메이션 프리뷰 뷰포트 (55%) =====
		ImGui::BeginChild("BlendSpace2DViewport", ImVec2(0, TopHeight), true, ImGuiWindowFlags_NoScrollbar);
		{
			RenderPreviewViewport();
		}
		ImGui::EndChild();

		// ===== 하단: 3단 레이아웃 (Properties/Samples | Grid | Animations) (43%) =====
		ImGui::BeginChild("BottomArea", ImVec2(0, BottomHeight), false, ImGuiWindowFlags_NoScrollbar);
		{
			float LeftPanelWidth = ContentRegion.x * 0.22f;    // 왼쪽 22%
			float CenterPanelWidth = ContentRegion.x * 0.53f;  // 중앙 53%
			float RightPanelWidth = ContentRegion.x * 0.23f;   // 오른쪽 23%

			// 왼쪽: Properties (위) + Samples (아래)
			ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, 0), false, ImGuiWindowFlags_NoScrollbar);
			{
				float PropertiesHeight = BottomHeight * 0.48f;  // 위 48%
				float SamplesHeight = BottomHeight * 0.48f;     // 아래 48%

				// Properties (위)
				ImGui::BeginChild("PropertiesPanel", ImVec2(0, PropertiesHeight), true);
				{
					RenderProperties();
				}
				ImGui::EndChild();

				// Samples (아래)
				ImGui::BeginChild("SamplesPanel", ImVec2(0, SamplesHeight), true);
				{
					RenderSampleList();
				}
				ImGui::EndChild();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// 중앙: 2D 블렌드 그리드
			ImGui::BeginChild("GridEditor", ImVec2(CenterPanelWidth, 0), true);
			{
				RenderGridEditor();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// 오른쪽: 애니메이션 시퀀스 목록
			ImGui::BeginChild("AnimationsPanel", ImVec2(RightPanelWidth, 0), true);
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
		FString DisplayName = GetDisplayNameForAnimation(Sample.Animation);

		ImVec2 TextPos(ScreenPos.x + SamplePointRadius + 5, ScreenPos.y - 8);
		DrawList->AddText(TextPos, IM_COL32(255, 255, 255, 255), DisplayName.c_str());
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

	// 축 이름 표시
	ImU32 AxisNameColor = IM_COL32(150, 200, 255, 255);
	float AxisNameFontSize = ImGui::GetFontSize() * 1.1f;

	// X축 이름 (하단 중앙)
	if (!EditingBlendSpace->XAxisName.empty())
	{
		ImVec2 TextSize = ImGui::CalcTextSize(EditingBlendSpace->XAxisName.c_str());
		ImVec2 XAxisNamePos(
			CanvasPos.x + CanvasSize.x * 0.5f - TextSize.x * 0.5f,
			CanvasPos.y + CanvasSize.y + 35
		);
		DrawList->AddText(XAxisNamePos, AxisNameColor, EditingBlendSpace->XAxisName.c_str());
	}

	// Y축 이름 (왼쪽 중앙, 회전)
	if (!EditingBlendSpace->YAxisName.empty())
	{
		// 텍스트 회전은 복잡하므로 일단 세로로 표시
		ImVec2 YAxisNamePos(CanvasPos.x - 65, CanvasPos.y + CanvasSize.y * 0.5f - 30);
		DrawList->AddText(YAxisNamePos, AxisNameColor, EditingBlendSpace->YAxisName.c_str());
	}

	// 눈금 개수 (그리드 크기에 따라 적절히 조정)
	const int NumXTicks = 8;  // X축 눈금 개수
	const int NumYTicks = 8;  // Y축 눈금 개수

	ImU32 TickColor = IM_COL32(180, 180, 180, 255);
	ImU32 MajorTickColor = IM_COL32(220, 220, 220, 255);

	// ===== X축 눈금 (하단) =====
	float XRange = EditingBlendSpace->XAxisMax - EditingBlendSpace->XAxisMin;
	float XStep = XRange / NumXTicks;

	for (int i = 0; i <= NumXTicks; ++i)
	{
		float ParamValue = EditingBlendSpace->XAxisMin + XStep * i;
		ImVec2 ScreenPos = ParamToScreen(FVector2D(ParamValue, EditingBlendSpace->YAxisMin));

		// 눈금 선 (아래로)
		bool bIsMajor = (i % 2 == 0);  // 짝수 인덱스는 주요 눈금
		ImU32 CurrentTickColor = bIsMajor ? MajorTickColor : TickColor;
		float TickLength = bIsMajor ? 8.0f : 5.0f;

		DrawList->AddLine(
			ImVec2(ScreenPos.x, CanvasPos.y + CanvasSize.y),
			ImVec2(ScreenPos.x, CanvasPos.y + CanvasSize.y + TickLength),
			CurrentTickColor,
			bIsMajor ? 2.0f : 1.0f
		);

		// 숫자 레이블 (주요 눈금에만)
		if (bIsMajor)
		{
			char Label[32];
			sprintf_s(Label, "%.0f", ParamValue);
			ImVec2 TextSize = ImGui::CalcTextSize(Label);
			ImVec2 TextPos(ScreenPos.x - TextSize.x * 0.5f, CanvasPos.y + CanvasSize.y + 12);
			DrawList->AddText(TextPos, MajorTickColor, Label);
		}
	}

	// ===== Y축 눈금 (왼쪽) =====
	float YRange = EditingBlendSpace->YAxisMax - EditingBlendSpace->YAxisMin;
	float YStep = YRange / NumYTicks;

	for (int i = 0; i <= NumYTicks; ++i)
	{
		float ParamValue = EditingBlendSpace->YAxisMin + YStep * i;
		ImVec2 ScreenPos = ParamToScreen(FVector2D(EditingBlendSpace->XAxisMin, ParamValue));

		// 눈금 선 (왼쪽으로)
		bool bIsMajor = (i % 2 == 0);
		ImU32 CurrentTickColor = bIsMajor ? MajorTickColor : TickColor;
		float TickLength = bIsMajor ? 8.0f : 5.0f;

		DrawList->AddLine(
			ImVec2(CanvasPos.x, ScreenPos.y),
			ImVec2(CanvasPos.x - TickLength, ScreenPos.y),
			CurrentTickColor,
			bIsMajor ? 2.0f : 1.0f
		);

		// 숫자 레이블 (주요 눈금에만)
		if (bIsMajor)
		{
			char Label[32];
			sprintf_s(Label, "%.0f", ParamValue);
			ImVec2 TextSize = ImGui::CalcTextSize(Label);
			ImVec2 TextPos(CanvasPos.x - TickLength - TextSize.x - 4, ScreenPos.y - TextSize.y * 0.5f);
			DrawList->AddText(TextPos, MajorTickColor, Label);
		}
	}
}

/**
 * @brief Delaunay 삼각분할 시각화
 */
void SBlendSpace2DEditorWindow::RenderTriangulation()
{
	if (!EditingBlendSpace)
		return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 삼각형 렌더링
	for (const FBlendTriangle& Tri : EditingBlendSpace->Triangles)
	{
		if (!Tri.IsValid())
			continue;

		// 인덱스 유효성 검사
		if (Tri.Index0 < 0 || Tri.Index0 >= EditingBlendSpace->GetNumSamples() ||
			Tri.Index1 < 0 || Tri.Index1 >= EditingBlendSpace->GetNumSamples() ||
			Tri.Index2 < 0 || Tri.Index2 >= EditingBlendSpace->GetNumSamples())
		{
			continue;
		}

		// 3개 정점의 화면 좌표
		ImVec2 P0 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index0].Position);
		ImVec2 P1 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index1].Position);
		ImVec2 P2 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index2].Position);

		// 삼각형 경계선 (얇은 노란색)
		ImU32 TriColor = IM_COL32(255, 255, 0, 100);  // 반투명 노란색
		DrawList->AddLine(P0, P1, TriColor, 1.5f);
		DrawList->AddLine(P1, P2, TriColor, 1.5f);
		DrawList->AddLine(P2, P0, TriColor, 1.5f);

		// 삼각형 내부 (매우 투명한 채우기 - 선택적)
		// DrawList->AddTriangleFilled(P0, P1, P2, IM_COL32(255, 255, 0, 20));
	}
}

void SBlendSpace2DEditorWindow::RenderSamplePoints_Enhanced(const TArray<int32>& InSampleIndices, const TArray<float>& InWeights)
{
	if (!EditingBlendSpace)
		return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 가중치 정보를 빠른 조회를 위해 맵으로 변환
	TMap<int32, float> WeightMap;
	for (int32 i = 0; i < InSampleIndices.Num(); ++i)
	{
		WeightMap.Add(InSampleIndices[i], InWeights[i]);
	}

	for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
	{
		const FBlendSample& Sample = EditingBlendSpace->Samples[i];
		ImVec2 ScreenPos = ParamToScreen(Sample.Position);

		// 활성 샘플인지 확인 (가중치가 있는 샘플)
		bool bIsActive = WeightMap.Contains(i) && WeightMap[i] > 0.001f;
		float Weight = bIsActive ? WeightMap[i] : 0.0f;

		// 색상 및 크기 결정
		ImU32 Color;
		float Radius = SamplePointRadius;

		if (i == SelectedSampleIndex)
		{
			// 선택된 샘플
			Color = SelectedSampleColor;
			Radius = SamplePointRadius * 1.2f;
		}
		else if (bIsActive)
		{
			// 활성 샘플 (가중치에 따라 색상 변경)
			// 가중치가 높을수록 밝은 녹색
			float Intensity = FMath::Lerp(0.5f, 1.0f, Weight);
			Color = IM_COL32(
				static_cast<uint8_t>(100 * Intensity),
				static_cast<uint8_t>(255 * Intensity),
				static_cast<uint8_t>(100 * Intensity),
				255
			);
			Radius = SamplePointRadius * (1.0f + Weight * 0.5f);  // 가중치에 따라 크기 증가
		}
		else
		{
			// 일반 샘플
			Color = SampleColor;
		}

		// RateScale이 1.0이 아닌 샘플 표시 (테두리 색상 변경)
		ImU32 OutlineColor = IM_COL32(255, 255, 255, 255);
		if (Sample.RateScale != 1.0f)
		{
			OutlineColor = IM_COL32(255, 150, 0, 255);  // 주황색 테두리
		}

		// 샘플 포인트 (원)
		DrawList->AddCircleFilled(ScreenPos, Radius, Color);
		DrawList->AddCircle(ScreenPos, Radius, OutlineColor, 0, 2.0f);

		// 샘플 이름 (애니메이션 이름)
		FString DisplayName = GetDisplayNameForAnimation(Sample.Animation);
		ImVec2 TextPos(ScreenPos.x + Radius + 5, ScreenPos.y - 8);

		// 활성 샘플은 더 밝게 표시
		ImU32 TextColor = bIsActive ? IM_COL32(150, 255, 150, 255) : IM_COL32(255, 255, 255, 200);
		DrawList->AddText(TextPos, TextColor, DisplayName.c_str());

		// 블렌드 가중치 표시
		if (bIsActive)
		{
			char WeightText[32];
			sprintf_s(WeightText, "%.1f%%", Weight * 100.0f);

			ImVec2 WeightTextSize = ImGui::CalcTextSize(WeightText);
			ImVec2 WeightTextPos(ScreenPos.x - WeightTextSize.x * 0.5f, ScreenPos.y - Radius - WeightTextSize.y - 2);

			// 가중치 텍스트 배경
			ImVec2 BgMin(WeightTextPos.x - 2, WeightTextPos.y - 2);
			ImVec2 BgMax(WeightTextPos.x + WeightTextSize.x + 2, WeightTextPos.y + WeightTextSize.y + 2);
			DrawList->AddRectFilled(BgMin, BgMax, IM_COL32(0, 0, 0, 180));

			// 가중치가 클수록 텍스트를 더 밝게 표시
			ImVec4 WeightTextColor = ImVec4(0.9f, 1.0f, 0.9f, 1.0f);
			DrawList->AddText(WeightTextPos, ImGui::ColorConvertFloat4ToU32(WeightTextColor), WeightText);
		}

		// RateScale 표시 (1.0이 아닌 경우)
		if (Sample.RateScale != 1.0f)
		{
			char RateText[32];
			sprintf_s(RateText, "x%.2f", Sample.RateScale);
			ImVec2 RateTextPos(ScreenPos.x + Radius + 5, ScreenPos.y + 8);
			DrawList->AddText(RateTextPos, IM_COL32(255, 200, 100, 255), RateText);
		}
	}
}

void SBlendSpace2DEditorWindow::RenderTriangulation_Enhanced(int32 InActiveTriangle)
{
	if (!EditingBlendSpace)
		return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 삼각형 렌더링
	for (int32 i = 0; i < EditingBlendSpace->Triangles.Num(); ++i)
	{
		const FBlendTriangle& Tri = EditingBlendSpace->Triangles[i];

		if (!Tri.IsValid())
			continue;

		// 인덱스 유효성 검사
		if (Tri.Index0 < 0 || Tri.Index0 >= EditingBlendSpace->GetNumSamples() ||
			Tri.Index1 < 0 || Tri.Index1 >= EditingBlendSpace->GetNumSamples() ||
			Tri.Index2 < 0 || Tri.Index2 >= EditingBlendSpace->GetNumSamples())
		{
			continue;
		}

		// 3개 정점의 화면 좌표
		ImVec2 P0 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index0].Position);
		ImVec2 P1 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index1].Position);
		ImVec2 P2 = ParamToScreen(EditingBlendSpace->Samples[Tri.Index2].Position);

		// 활성 삼각형인 경우 채우기 및 강조
		if (i == InActiveTriangle)
		{
			DrawList->AddTriangleFilled(P0, P1, P2, IM_COL32(0, 255, 100, 40));
			DrawList->AddLine(P0, P1, IM_COL32(150, 255, 150, 200), 2.0f);
			DrawList->AddLine(P1, P2, IM_COL32(150, 255, 150, 200), 2.0f);
			DrawList->AddLine(P2, P0, IM_COL32(150, 255, 150, 200), 2.0f);
		}
		else
		{
			// 삼각형 경계선 (기본 색상)
			ImU32 TriColor = IM_COL32(255, 255, 0, 80);  // 반투명 노란색
			DrawList->AddLine(P0, P1, TriColor, 1.0f);
			DrawList->AddLine(P1, P2, TriColor, 1.0f);
			DrawList->AddLine(P2, P0, TriColor, 1.0f);
		}
	}
}

void SBlendSpace2DEditorWindow::RenderToolbar()
{
	// 저장 버튼
	if (ImGui::Button("Save"))
	{
		if (EditingBlendSpace)
		{
			const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Animation";
			const FWideString Extension = L".blend2d";
			const FWideString Description = L"BlendSpace2D Files";

			// 플랫폼 공용 다이얼로그 호출 (SelectedPath는 ABSOLUTE PATH)
			std::filesystem::path SelectedPath = FPlatformProcess::OpenSaveFileDialog(BaseDir, Extension, Description);

			if (!SelectedPath.empty())
			{
				// 현재 프리뷰 상태 저장
				EditingBlendSpace->EditorPreviewParameter = PreviewParameter;

				// 현재 사용 중인 스켈레톤 메시 경로 저장
				if (PreviewState && PreviewState->CurrentMesh)
				{
					EditingBlendSpace->EditorSkeletalMeshPath = PreviewState->CurrentMesh->GetFilePath();
				}

				// 절대 경로를 상대 경로로 변환
				FWideString AbsolutePath = SelectedPath.wstring();
				FString FinalPathStr = ResolveAssetRelativePath(WideToUTF8(AbsolutePath), "");

				// 저장
				EditingBlendSpace->SaveToFile(FinalPathStr);
				UE_LOG("[BlendSpace2D] Saved to: %s", FinalPathStr.c_str());
			}
		}
	}

	ImGui::SameLine();
	// 로드 버튼
	if (ImGui::Button("Load"))
	{
		const FWideString BaseDir = UTF8ToWide(GDataDir) + L"/Animation";
		const FWideString Extension = L".blend2d";
		const FWideString Description = L"BlendSpace2D Files";

		// 2. 플랫폼 공용 다이얼로그 호출 (SelectedPath는 ABSOLUTE PATH)
		std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialog(BaseDir, Extension, Description);

		if (!SelectedPath.empty())
		{
			FWideString AbsolutePath = SelectedPath.wstring();
			FString FinalPathStr = ResolveAssetRelativePath(WideToUTF8(AbsolutePath), "");
			UBlendSpace2D* LoadedBS = UBlendSpace2D::LoadFromFile(FinalPathStr);

			if (LoadedBS)
			{
				SetBlendSpace(LoadedBS);
			}
			else
			{
				UE_LOG("[Error] Failed to load BlendSpace2D: %S", AbsolutePath.c_str());
			}
		}
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

	// ===== Grid Snapping =====
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	ImGui::Checkbox("Grid Snap", &bEnableGridSnapping);
	if (bEnableGridSnapping)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("##SnapSize", &GridSnapSize, 1.0f, 1.0f, 50.0f, "%.1f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Grid snap interval");
		}
	}

	// ===== Zoom Control =====
	ImGui::SameLine();
	ImGui::Text("Zoom:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::SliderFloat("##Zoom", &ZoomLevel, 0.5f, 3.0f, "%.1fx"))
	{
		// Zoom 변경 시
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset"))
	{
		ZoomLevel = 1.0f;
		PanOffset = FVector2D(0.0f, 0.0f);
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

		// 헬퍼 함수를 사용하여 애니메이션 이름 가져오기
		FString AnimName = GetDisplayNameForAnimation(Sample.Animation);

		char Label[256];
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

	// 선택된 샘플이 있으면 샘플 속성 표시
	if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
	{
		ImGui::Text("Sample Properties [%d]", SelectedSampleIndex);
		ImGui::Separator();

		FBlendSample& Sample = EditingBlendSpace->Samples[SelectedSampleIndex];

		// 위치
		ImGui::Text("Position");
		ImGui::InputFloat("X##SamplePosX", &Sample.Position.X);
		ImGui::InputFloat("Y##SamplePosY", &Sample.Position.Y);

		ImGui::Separator();

		// RateScale (재생 속도 배율)
		ImGui::Text("Playback");
		ImGui::SliderFloat("Rate Scale", &Sample.RateScale, 0.1f, 3.0f, "%.2f");
		ImGui::SameLine();
		if (ImGui::Button("Reset##RateScale"))
		{
			Sample.RateScale = 1.0f;
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
			"Playback speed multiplier");

		ImGui::Separator();

		// 애니메이션 정보
		ImGui::Text("Animation");
		if (Sample.Animation)
		{
			ImGui::Text("Name: %s", Sample.Animation->GetName().c_str());

			// Sync Markers 섹션
			ImGui::Separator();
			ImGui::Text("Sync Markers (%d)", Sample.Animation->GetSyncMarkers().Num());

			// Sync Marker 목록
			const TArray<FAnimSyncMarker>& Markers = Sample.Animation->GetSyncMarkers();
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

			ImGui::Separator();

			ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f),
				"%s", Sample.Animation->GetName().c_str());
			ImGui::Text("Duration: %.2fs", Sample.Animation->GetPlayLength());
			ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
				"Effective: %.2fs (with RateScale)",
				Sample.Animation->GetPlayLength() / Sample.RateScale);
		}
		else
		{
			ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "No Animation");
		}
	}
	else
	{
		// 샘플이 선택되지 않았으면 BlendSpace 전체 속성 표시
		ImGui::Text("Blend Space Properties");
		ImGui::Separator();

		// 축 이름 편집
		ImGui::Text("Axis Names");
		static char XAxisNameBuffer[128];
		static char YAxisNameBuffer[128];

		if (EditingBlendSpace->XAxisName.length() < 128)
		{
			strcpy_s(XAxisNameBuffer, EditingBlendSpace->XAxisName.c_str());
		}
		if (EditingBlendSpace->YAxisName.length() < 128)
		{
			strcpy_s(YAxisNameBuffer, EditingBlendSpace->YAxisName.c_str());
		}

		if (ImGui::InputText("X Axis Name", XAxisNameBuffer, 128))
		{
			EditingBlendSpace->XAxisName = XAxisNameBuffer;
		}
		if (ImGui::InputText("Y Axis Name", YAxisNameBuffer, 128))
		{
			EditingBlendSpace->YAxisName = YAxisNameBuffer;
		}

		ImGui::Separator();

		// 축 범위
		ImGui::Text("Axis Ranges");
		ImGui::InputFloat("X Min", &EditingBlendSpace->XAxisMin);
		ImGui::InputFloat("X Max", &EditingBlendSpace->XAxisMax);
		ImGui::InputFloat("Y Min", &EditingBlendSpace->YAxisMin);
		ImGui::InputFloat("Y Max", &EditingBlendSpace->YAxisMax);

		ImGui::Separator();

		// 축별 블렌드 가중치
		ImGui::Text("Axis Blend Weights");
		ImGui::SliderFloat("X Weight", &EditingBlendSpace->XAxisBlendWeight, 0.1f, 3.0f, "%.2f");
		ImGui::SliderFloat("Y Weight", &EditingBlendSpace->YAxisBlendWeight, 0.1f, 3.0f, "%.2f");
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
			"Higher = more important");

		ImGui::Separator();

		// Sync Group 설정
		ImGui::Text("Sync Group");

		// Sync Marker 사용 체크박스
		ImGui::Checkbox("Use Sync Markers", &EditingBlendSpace->bUseSyncMarkers);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
			"Sync animations using markers");

		// Sync Group 이름
		static char SyncGroupBuffer[128] = "Default";
		if (EditingBlendSpace->SyncGroupName.length() < 128)
		{
			strcpy_s(SyncGroupBuffer, EditingBlendSpace->SyncGroupName.c_str());
		}

		if (ImGui::InputText("Group Name", SyncGroupBuffer, 128))
		{
			EditingBlendSpace->SyncGroupName = SyncGroupBuffer;
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
			"Animations in same group sync together");

		ImGui::Separator();

		// Delaunay 삼각분할 정보
		ImGui::Text("Triangulation");
		ImGui::Text("Triangles: %d", EditingBlendSpace->Triangles.Num());
		if (ImGui::Button("Regenerate Triangulation"))
		{
			EditingBlendSpace->GenerateTriangulation();
		}

		ImGui::Separator();

		// 통계
		ImGui::Text("Statistics");
		ImGui::Text("Total Samples: %d", EditingBlendSpace->GetNumSamples());
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
			"Select a sample to edit");
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

	// Pan 오프셋 적용
	NormX += PanOffset.X;
	NormY += PanOffset.Y;

	// 0~1을 스크린 좌표로 변환 (Y축은 반전) + Zoom 적용
	float ScreenX = CanvasPos.x + NormX * CanvasSize.x * ZoomLevel;
	float ScreenY = CanvasPos.y + (1.0f - NormY) * CanvasSize.y * ZoomLevel;

	return ImVec2(ScreenX, ScreenY);
}

FVector2D SBlendSpace2DEditorWindow::ScreenToParam(ImVec2 ScreenPos) const
{
	if (!EditingBlendSpace)
		return FVector2D::Zero();

	// 스크린 좌표를 0~1 범위로 정규화 (Zoom 적용)
	float NormX = (ScreenPos.x - CanvasPos.x) / (CanvasSize.x * ZoomLevel);
	float NormY = (CanvasPos.y + CanvasSize.y * ZoomLevel - ScreenPos.y) / (CanvasSize.y * ZoomLevel);

	// Pan 오프셋 제거
	NormX -= PanOffset.X;
	NormY -= PanOffset.Y;

	// 범위 클램핑 (Pan/Zoom으로 범위 밖으로 갈 수 있음)
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

	// ===== Zoom (마우스 휠) =====
	ImGuiIO& io = ImGui::GetIO();
	if (io.MouseWheel != 0.0f)
	{
		float ZoomDelta = io.MouseWheel * 0.1f;  // 10% per wheel tick
		ZoomLevel = FMath::Clamp(ZoomLevel + ZoomDelta, 0.5f, 3.0f);  // 50% ~ 300%
	}

	// ===== Pan (마우스 중간 버튼 드래그) =====
	if (ImGui::IsMouseClicked(2))  // Middle button
	{
		bPanning = true;
		PanStartMousePos = MousePos;
	}

	if (bPanning && ImGui::IsMouseDragging(2))
	{
		ImVec2 Delta = ImVec2(MousePos.x - PanStartMousePos.x, MousePos.y - PanStartMousePos.y);
		PanOffset.X += Delta.x / (CanvasSize.x * ZoomLevel);
		PanOffset.Y -= Delta.y / (CanvasSize.y * ZoomLevel);  // Y축 반전
		PanStartMousePos = MousePos;
	}

	if (ImGui::IsMouseReleased(2))
	{
		bPanning = false;
	}

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

					// Ctrl 키를 누른 상태면 복제 모드
					if (io.KeyCtrl)
					{
						bDuplicatingMode = true;
						// 샘플 복제
						FBlendSample OriginalSample = EditingBlendSpace->Samples[i];
						EditingBlendSpace->AddSample(OriginalSample.Position, OriginalSample.Animation);
						// RateScale 복사
						if (EditingBlendSpace->GetNumSamples() > 0)
						{
							EditingBlendSpace->Samples[EditingBlendSpace->GetNumSamples() - 1].RateScale = OriginalSample.RateScale;
						}
						SelectedSampleIndex = EditingBlendSpace->GetNumSamples() - 1;
					}

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

	// 우클릭: 컨텍스트 메뉴
	if (ImGui::IsMouseClicked(1))
	{
		ContextMenuPos = MousePos;
		ContextMenuSampleIndex = -1;

		// 샘플 위에서 우클릭했는지 확인
		for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
		{
			ImVec2 SampleScreenPos = ParamToScreen(EditingBlendSpace->Samples[i].Position);
			float Dist = sqrtf(
				(MousePos.x - SampleScreenPos.x) * (MousePos.x - SampleScreenPos.x) +
				(MousePos.y - SampleScreenPos.y) * (MousePos.y - SampleScreenPos.y)
			);

			if (Dist <= SamplePointRadius + 5.0f)
			{
				ContextMenuSampleIndex = i;
				break;
			}
		}

		bShowContextMenu = true;
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

			// Grid Snapping 적용
			if (bEnableGridSnapping)
			{
				NewPos.X = floorf(NewPos.X / GridSnapSize + 0.5f) * GridSnapSize;
				NewPos.Y = floorf(NewPos.Y / GridSnapSize + 0.5f) * GridSnapSize;
			}

			EditingBlendSpace->Samples[SelectedSampleIndex].Position = NewPos;
		}
	}

	// 드래그 종료
	if (ImGui::IsMouseReleased(0))
	{
		// 샘플 드래그가 끝났으면 삼각분할 재생성
		if (bDraggingSample && EditingBlendSpace)
		{
			EditingBlendSpace->GenerateTriangulation();
		}

		bDraggingSample = false;
		bDraggingPreviewMarker = false;
		bDuplicatingMode = false;
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
	ImGuiIO& io = ImGui::GetIO();

	// Delete 키: 선택된 샘플 삭제
	if (ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		RemoveSelectedSample();
	}

	// Space 키: 애니메이션 재생/일시정지
	if (ImGui::IsKeyPressed(ImGuiKey_Space))
	{
		bIsPlaying = !bIsPlaying;
	}

	// Ctrl+S: 저장
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
	{
		if (EditingBlendSpace)
		{
			// 마지막 저장 경로가 있으면 그대로 저장
			if (!EditingBlendSpace->GetFilePath().empty())
			{
				EditingBlendSpace->EditorPreviewParameter = PreviewParameter;
				if (PreviewState && PreviewState->CurrentMesh)
				{
					EditingBlendSpace->EditorSkeletalMeshPath = PreviewState->CurrentMesh->GetFilePath();
				}
				EditingBlendSpace->SaveToFile(EditingBlendSpace->GetFilePath());
				UE_LOG("[BlendSpace2D] Saved to: %s", EditingBlendSpace->GetFilePath().c_str());
			}
		}
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

	// 삼각분할 자동 재생성
	if (EditingBlendSpace->GetNumSamples() >= 3)
	{
		EditingBlendSpace->GenerateTriangulation();
	}
}

void SBlendSpace2DEditorWindow::RemoveSelectedSample()
{
	if (!EditingBlendSpace)
		return;

	if (SelectedSampleIndex >= 0 && SelectedSampleIndex < EditingBlendSpace->GetNumSamples())
	{
		EditingBlendSpace->RemoveSample(SelectedSampleIndex);
		SelectedSampleIndex = -1;

		// 삼각분할 자동 재생성
		if (EditingBlendSpace->GetNumSamples() >= 3)
		{
			EditingBlendSpace->GenerateTriangulation();
		}
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

	// 전체 가용 영역 계산
	ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
	float TimelineAndControlsHeight = 0.0f;  // 타임라인 컨트롤 높이 (파라미터 슬라이더 제거로 축소)
	float ViewportHeight = ContentAvail.y - TimelineAndControlsHeight;

	// 뷰포트 렌더링 영역
	ImGui::BeginChild("ViewportRenderArea", ImVec2(0, ViewportHeight), false, ImGuiWindowFlags_NoScrollbar);
	{
		ImVec2 childPos = ImGui::GetWindowPos();
		ImVec2 childSize = ImGui::GetWindowSize();
		ImVec2 rectMin = childPos;
		ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);

		// 뷰포트 영역 저장
		PreviewViewportRect.Left = rectMin.x;
		PreviewViewportRect.Top = rectMin.y;
		PreviewViewportRect.Right = rectMax.x;
		PreviewViewportRect.Bottom = rectMax.y;
		PreviewViewportRect.UpdateMinMax();
	}
	ImGui::EndChild();

	ImGui::Separator();

	// 타임라인과 컨트롤 영역
	ImGui::BeginChild("TimelineAndControls", ImVec2(0, TimelineAndControlsHeight), true);
	{
		// 타임라인 렌더링
		RenderTimelineControls();
	}
	ImGui::EndChild();
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

	// ===== Validation Warnings =====
	bool bHasWarnings = false;
	TArray<FString> Warnings;

	// 1. 샘플이 너무 가까운지 체크
	for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
	{
		for (int32 j = i + 1; j < EditingBlendSpace->GetNumSamples(); ++j)
		{
			FVector2D Pos1 = EditingBlendSpace->Samples[i].Position;
			FVector2D Pos2 = EditingBlendSpace->Samples[j].Position;
			float Distance = sqrtf((Pos2.X - Pos1.X) * (Pos2.X - Pos1.X) + (Pos2.Y - Pos1.Y) * (Pos2.Y - Pos1.Y));

			if (Distance < 5.0f)
			{
				char WarningMsg[256];
				sprintf_s(WarningMsg, "Samples %d and %d are too close (%.1f)", i, j, Distance);
				Warnings.Add(WarningMsg);
				bHasWarnings = true;
			}
		}
	}

	// 2. 삼각분할 실패 체크
	if (EditingBlendSpace->GetNumSamples() >= 3 && EditingBlendSpace->Triangles.Num() == 0)
	{
		Warnings.Add("Triangulation failed! Check sample positions.");
		bHasWarnings = true;
	}

	// 경고 표시
	if (bHasWarnings)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.0f, 1.0f));
		for (const FString& Warning : Warnings)
		{
			ImGui::TextWrapped("%s", Warning.c_str());
		}
		ImGui::PopStyleColor();
	}

	ImGui::Separator();

	// 그리드 캔버스 (전체 영역 사용 - 직사각형)
	ImVec2 AvailableSize = ImGui::GetContentRegionAvail();

	// 축 레이블을 위한 여백 (왼쪽 70px, 하단 50px)
	const float LeftMargin = 70.0f;
	const float BottomMargin = 50.0f;
	const float RightMargin = 10.0f;
	const float TopMargin = 10.0f;

	// 캔버스 위치와 크기 설정 (여백 고려)
	CanvasPos = ImGui::GetCursorScreenPos();
	CanvasPos.x += LeftMargin;
	CanvasPos.y += TopMargin;
	CanvasSize = ImVec2(
		AvailableSize.x - LeftMargin - RightMargin,
		AvailableSize.y - TopMargin - BottomMargin
	);

	// 현재 프리뷰 파라미터로 블렌딩 정보 가져오기
	TArray<int32> CurrentSampleIndices;
	TArray<float> CurrentWeights;
	int32 ActiveTriangle = -1;

	if (EditingBlendSpace && EditingBlendSpace->GetNumSamples() >= 3)
	{
		// 1. 블렌딩에 사용할 샘플과 가중치를 가져옵니다.
		EditingBlendSpace->GetBlendWeights(PreviewParameter, CurrentSampleIndices, CurrentWeights);

		// 2. 반환된 샘플 인덱스들을 사용하여 현재 활성 삼각형을 찾습니다.
		if (CurrentSampleIndices.Num() > 0)
		{
			for (int32 i = 0; i < EditingBlendSpace->Triangles.Num(); ++i)
			{
				const FBlendTriangle& Tri = EditingBlendSpace->Triangles[i];

				// GetBlendWeights는 3개의 샘플을 반환하는 것을 가정
				if (CurrentSampleIndices.Num() == 3)
				{
					// 3개의 인덱스가 모두 삼각형의 정점과 일치하는지 확인
					bool bHasV0 = (CurrentSampleIndices.Contains(Tri.Index0));
					bool bHasV1 = (CurrentSampleIndices.Contains(Tri.Index1));
					bool bHasV2 = (CurrentSampleIndices.Contains(Tri.Index2));

					if (bHasV0 && bHasV1 && bHasV2)
					{
						ActiveTriangle = i;
						break;
					}
				}
			}
		}
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 캔버스 배경
	DrawList->AddRectFilled(CanvasPos,
		ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y),
		IM_COL32(30, 30, 30, 255));

	// 그리드 렌더링
	RenderGrid();

	// 축 라벨 렌더링
	RenderAxisLabels();

	// Delaunay 삼각분할 렌더링 (활성 삼각형 전달)
	RenderTriangulation_Enhanced(ActiveTriangle);

	// 샘플 포인트 렌더링 (가중치 정보 전달)
	RenderSamplePoints_Enhanced(CurrentSampleIndices, CurrentWeights);

	// 프리뷰 마커 렌더링
	RenderPreviewMarker();

	// 입력 처리
	HandleMouseInput();

	// 캔버스 영역 확보 (클릭 감지를 위해) - 여백 포함한 전체 크기
	ImGui::Dummy(ImVec2(
		CanvasSize.x + LeftMargin + RightMargin,
		CanvasSize.y + TopMargin + BottomMargin
	));

	// ===== 컨텍스트 메뉴 =====
	if (bShowContextMenu)
	{
		ImGui::OpenPopup("SampleContextMenu");
		bShowContextMenu = false;
	}

	if (ImGui::BeginPopup("SampleContextMenu"))
	{
		if (ContextMenuSampleIndex >= 0)
		{
			// 샘플 위에서 우클릭
			ImGui::Text("Sample %d", ContextMenuSampleIndex);
			ImGui::Separator();

			if (ImGui::MenuItem("Delete"))
			{
				if (ContextMenuSampleIndex < EditingBlendSpace->GetNumSamples())
				{
					EditingBlendSpace->RemoveSample(ContextMenuSampleIndex);
					SelectedSampleIndex = -1;

					// 삼각분할 재생성
					if (EditingBlendSpace->GetNumSamples() >= 3)
					{
						EditingBlendSpace->GenerateTriangulation();
					}
				}
			}

			if (ImGui::MenuItem("Duplicate"))
			{
				if (ContextMenuSampleIndex < EditingBlendSpace->GetNumSamples())
				{
					FBlendSample OriginalSample = EditingBlendSpace->Samples[ContextMenuSampleIndex];
					// 약간 오프셋 추가
					FVector2D NewPos = OriginalSample.Position;
					NewPos.X += 10.0f;
					NewPos.Y += 10.0f;
					EditingBlendSpace->AddSample(NewPos, OriginalSample.Animation);

					// RateScale 복사
					if (EditingBlendSpace->GetNumSamples() > 0)
					{
						EditingBlendSpace->Samples[EditingBlendSpace->GetNumSamples() - 1].RateScale = OriginalSample.RateScale;
					}

					SelectedSampleIndex = EditingBlendSpace->GetNumSamples() - 1;

					// 삼각분할 재생성
					if (EditingBlendSpace->GetNumSamples() >= 3)
					{
						EditingBlendSpace->GenerateTriangulation();
					}
				}
			}
		}
		else
		{
			// 빈 공간에서 우클릭
			if (ImGui::MenuItem("Add Sample Here"))
			{
				FVector2D ClickPos = ScreenToParam(ContextMenuPos);
				AddSampleAtPosition(ClickPos);
			}
		}

		ImGui::EndPopup();
	}
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

					// .anim 파일만 추가
					for (UAnimSequence* Anim : Animations)
					{
						if (Anim && Anim->GetFilePath().ends_with(".anim"))
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

					// BlendSpace에 스켈레톤 메시 경로 저장
					if (EditingBlendSpace)
					{
						EditingBlendSpace->EditorSkeletalMeshPath = Mesh->GetFilePath();
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

			// 헬퍼 함수를 사용하여 애니메이션 이름 가져오기
			FString DisplayName = GetDisplayNameForAnimation(Anim);

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

						// 첫 번째 유효한 샘플 애니메이션을 재생 (타임라인 표시용)
						/*for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
						{
							if (EditingBlendSpace->Samples[i].Animation)
							{
								AnimInst->PlayAnimation(EditingBlendSpace->Samples[i].Animation, PlaybackSpeed);
								break;
							}
						}*/
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
			UAnimInstance* AnimInst = SkelComp->GetAnimInstance(); for (int32 i = 0; i < EditingBlendSpace->GetNumSamples(); ++i)
			if (AnimInst)
			{
				UAnimSequenceBase* AnimBase = AnimInst->GetCurrentAnimation();
				CurrentAnimation = dynamic_cast<UAnimSequence*>(AnimBase);
			}
		}
	}

	UAnimDataModel* DataModel = nullptr;
	if (CurrentAnimation)
	{
		DataModel = CurrentAnimation->GetDataModel();
	}

	if (!DataModel)
	{
		return;
	}

	float MaxTime = DataModel->GetPlayLength();
	int32 TotalFrames = DataModel->GetNumberOfFrames();

	// === Timeline 렌더링 영역 ===
	float TimelineAreaHeight = 100.0f;
	ImGui::BeginChild("TimelineArea", ImVec2(0, TimelineAreaHeight), true, ImGuiWindowFlags_NoScrollbar);
	{
		RenderTimeline();
	}
	ImGui::EndChild();

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
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No animation playing");
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

		// 메이저/마이너 그리드 구분
		bool bIsMajor = (FrameIndex % 10 == 0);
		ImU32 LineColor = bIsMajor ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255);
		float LineThickness = bIsMajor ? 1.5f : 1.0f;

		DrawList->AddLine(
			ImVec2(ScreenX, ScrollTimelineMin.y),
			ImVec2(ScreenX, ScrollTimelineMax.y),
			LineColor,
			LineThickness
		);
	}

	// 현재 시간 표시 (하단)
	char TimeText[64];
	sprintf_s(TimeText, "%.2fs / %.2fs", CurrentAnimationTime, MaxTime);
	ImVec2 TimeTextSize = ImGui::CalcTextSize(TimeText);
	ImVec2 TimeTextPos = ImVec2(
		ScrollTimelineMin.x + 5,
		ScrollTimelineMax.y - TimeTextSize.y - 5
	);
	DrawList->AddRectFilled(
		ImVec2(TimeTextPos.x - 2, TimeTextPos.y - 2),
		ImVec2(TimeTextPos.x + TimeTextSize.x + 2, TimeTextPos.y + TimeTextSize.y + 2),
		IM_COL32(0, 0, 0, 180)
	);
	DrawList->AddText(TimeTextPos, IM_COL32(255, 255, 255, 255), TimeText);

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
