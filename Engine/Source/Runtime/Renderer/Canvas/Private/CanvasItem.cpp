#include "pch.h"
#include "CanvasItem.h"
#include "Canvas.h"

// ────────────────────────────────────────────────────────
// FCanvasLineItem
// ────────────────────────────────────────────────────────

void FCanvasLineItem::Draw(FCanvas& Canvas)
{
	Canvas.DrawLine(Position, EndPosition, Color, Thickness);
}

// ────────────────────────────────────────────────────────
// FCanvasBoxItem
// ────────────────────────────────────────────────────────

void FCanvasBoxItem::Draw(FCanvas& Canvas)
{
	Canvas.DrawBox(Position, Size, Color, Thickness);
}

// ────────────────────────────────────────────────────────
// FCanvasEllipseItem
// ────────────────────────────────────────────────────────

void FCanvasEllipseItem::Draw(FCanvas& Canvas)
{
	Canvas.DrawEllipse(Position, Radius, Color, bFilled, Thickness);
}

// ────────────────────────────────────────────────────────
// FCanvasTextItem
// ────────────────────────────────────────────────────────

void FCanvasTextItem::Draw(FCanvas& Canvas)
{
	// FString (std::string) → std::wstring 변환
	std::wstring WideText(Text.begin(), Text.end());
	Canvas.DrawText(WideText.c_str(), Position, Color, FontSize, FontName, bBold, false);
}

// ────────────────────────────────────────────────────────
// FCanvasAxisWidget (FutureEngine Axis Indicator 패턴)
// ────────────────────────────────────────────────────────

FVector2D FCanvasAxisWidget::ViewToScreen(const FVector4& ViewVec, float Scale) const
{
	// View 공간 → 2D 스크린 좌표
	// ViewVec.X (카메라 기준 오른쪽) → Screen X
	// ViewVec.Y (카메라 기준 위) → Screen -Y (화면 Y축 반전)
	float ScreenX = ViewVec.X * AxisSize * Scale;
	float ScreenY = -ViewVec.Y * AxisSize * Scale;

	return FVector2D(Position.X + ScreenX, Position.Y + ScreenY);
}

void FCanvasAxisWidget::Draw(FCanvas& Canvas)
{
	// FutureEngine Axis.cpp 로직 통합

	// 월드 고정 축 (1,0,0), (0,1,0), (0,0,1)을 View 공간으로 변환
	FVector4 WorldX(1.0f, 0.0f, 0.0f, 0.0f);
	FVector4 WorldY(0.0f, 1.0f, 0.0f, 0.0f);
	FVector4 WorldZ(0.0f, 0.0f, 1.0f, 0.0f);

	// View 행렬로 변환 (방향 벡터이므로 W=0)
	FVector4 ViewAxisX(
		WorldX.X * ViewMatrix.M[0][0] + WorldX.Y * ViewMatrix.M[1][0] + WorldX.Z * ViewMatrix.M[2][0],
		WorldX.X * ViewMatrix.M[0][1] + WorldX.Y * ViewMatrix.M[1][1] + WorldX.Z * ViewMatrix.M[2][1],
		WorldX.X * ViewMatrix.M[0][2] + WorldX.Y * ViewMatrix.M[1][2] + WorldX.Z * ViewMatrix.M[2][2],
		0.0f
	);

	FVector4 ViewAxisY(
		WorldY.X * ViewMatrix.M[0][0] + WorldY.Y * ViewMatrix.M[1][0] + WorldY.Z * ViewMatrix.M[2][0],
		WorldY.X * ViewMatrix.M[0][1] + WorldY.Y * ViewMatrix.M[1][1] + WorldY.Z * ViewMatrix.M[2][1],
		WorldY.X * ViewMatrix.M[0][2] + WorldY.Y * ViewMatrix.M[1][2] + WorldY.Z * ViewMatrix.M[2][2],
		0.0f
	);

	FVector4 ViewAxisZ(
		WorldZ.X * ViewMatrix.M[0][0] + WorldZ.Y * ViewMatrix.M[1][0] + WorldZ.Z * ViewMatrix.M[2][0],
		WorldZ.X * ViewMatrix.M[0][1] + WorldZ.Y * ViewMatrix.M[1][1] + WorldZ.Z * ViewMatrix.M[2][1],
		WorldZ.X * ViewMatrix.M[0][2] + WorldZ.Y * ViewMatrix.M[1][2] + WorldZ.Z * ViewMatrix.M[2][2],
		0.0f
	);

	// 축 끝점 계산
	FVector2D AxisEndX = ViewToScreen(ViewAxisX);
	FVector2D AxisEndY = ViewToScreen(ViewAxisY);
	FVector2D AxisEndZ = ViewToScreen(ViewAxisZ);

	// 축 색상 (FutureEngine 패턴)
	const FLinearColor ColorX(1.0f, 0.0f, 0.0f, 1.0f);  // 빨강
	const FLinearColor ColorY(0.0f, 1.0f, 0.0f, 1.0f);  // 초록
	const FLinearColor ColorZ(0.0f, 0.4f, 1.0f, 1.0f);  // 파랑
	const FLinearColor ColorCenter(0.2f, 0.2f, 0.2f, 1.0f);  // 회색

	// X, Y, Z 축 라인 그리기
	Canvas.DrawLine(Position, AxisEndX, ColorX, LineThickness);
	Canvas.DrawLine(Position, AxisEndY, ColorY, LineThickness);
	Canvas.DrawLine(Position, AxisEndZ, ColorZ, LineThickness);

	// 중심점 그리기 (작은 원 2개)
	Canvas.DrawEllipse(Position, 3.0f, ColorCenter, true);
	Canvas.DrawEllipse(Position, 1.5f, ColorCenter, true);

	// 텍스트 레이블 위치 계산 (축 끝점보다 25% 더 멀리)
	constexpr float TextOffset = 1.25f;
	FVector2D TextPosX = ViewToScreen(ViewAxisX, TextOffset);
	FVector2D TextPosY = ViewToScreen(ViewAxisY, TextOffset);
	FVector2D TextPosZ = ViewToScreen(ViewAxisZ, TextOffset);

	// 축이 카메라에 수직인지 확인 (스크린 공간에서 길이가 매우 짧으면 수직)
	constexpr float MinAxisLengthThreshold = 5.0f;  // 픽셀 단위

	float AxisLengthX = std::sqrt((AxisEndX.X - Position.X) * (AxisEndX.X - Position.X) +
	                              (AxisEndX.Y - Position.Y) * (AxisEndX.Y - Position.Y));
	float AxisLengthY = std::sqrt((AxisEndY.X - Position.X) * (AxisEndY.X - Position.X) +
	                              (AxisEndY.Y - Position.Y) * (AxisEndY.Y - Position.Y));
	float AxisLengthZ = std::sqrt((AxisEndZ.X - Position.X) * (AxisEndZ.X - Position.X) +
	                              (AxisEndZ.Y - Position.Y) * (AxisEndZ.Y - Position.Y));

	// 축 레이블 그리기 (카메라에 수직이 아닌 축만 표시, 중앙 정렬)
	if (AxisLengthX > MinAxisLengthThreshold)
	{
		Canvas.DrawText(L"X", TextPosX, ColorX, 13.0f, L"Consolas", true, true);
	}
	if (AxisLengthY > MinAxisLengthThreshold)
	{
		Canvas.DrawText(L"Y", TextPosY, ColorY, 13.0f, L"Consolas", true, true);
	}
	if (AxisLengthZ > MinAxisLengthThreshold)
	{
		Canvas.DrawText(L"Z", TextPosZ, ColorZ, 13.0f, L"Consolas", true, true);
	}
}
