#pragma once

#include "Vector.h"
#include "Color.h"

// Forward declarations
class FCanvas;

/**
 * @brief Canvas 렌더링 아이템의 추상 베이스 클래스
 * @details Unreal Engine의 FCanvasItem 패턴을 따름
 *
 * UE Reference: Engine/Source/Runtime/Engine/Public/CanvasItem.h
 *
 * @param Position 2D 위치 (스크린 좌표)
 * @param Color 색상
 */
class FCanvasItem
{
public:
	// 생성자/소멸자
	FCanvasItem(const FVector2D& InPosition, const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		: Position(InPosition)
		, Color(InColor)
	{
	}
	virtual ~FCanvasItem() = default;

	// ────────────────────────────────────────────────────────
	// Draw API (순수 가상 함수)
	// ────────────────────────────────────────────────────────

	/**
	 * @brief Canvas에 아이템 렌더링 (서브클래스에서 구현)
	 * @param Canvas 렌더링 대상 Canvas
	 */
	virtual void Draw(FCanvas& Canvas) = 0;

	// ────────────────────────────────────────────────────────
	// Getters/Setters
	// ────────────────────────────────────────────────────────

	void SetPosition(const FVector2D& InPosition) { Position = InPosition; }
	void SetColor(const FLinearColor& InColor) { Color = InColor; }

	const FVector2D& GetPosition() const { return Position; }
	const FLinearColor& GetColor() const { return Color; }

protected:
	FVector2D Position;
	FLinearColor Color;
};

// ────────────────────────────────────────────────────────
// FCanvasLineItem - 2D 라인
// ────────────────────────────────────────────────────────

/**
 * @brief 2D 라인 렌더링 아이템
 *
 * @param Position 시작점
 * @param EndPosition 끝점
 * @param Color 색상
 * @param Thickness 선 두께
 */
class FCanvasLineItem : public FCanvasItem
{
public:
	FCanvasLineItem(const FVector2D& InStartPosition, const FVector2D& InEndPosition,
	                const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), float InThickness = 1.0f)
		: FCanvasItem(InStartPosition, InColor)
		, EndPosition(InEndPosition)
		, Thickness(InThickness)
	{
	}

	void Draw(FCanvas& Canvas) override;

	void SetEndPosition(const FVector2D& InEndPosition) { EndPosition = InEndPosition; }
	void SetThickness(float InThickness) { Thickness = InThickness; }

private:
	FVector2D EndPosition;
	float Thickness;
};

// ────────────────────────────────────────────────────────
// FCanvasBoxItem - 2D 박스 (외곽선)
// ────────────────────────────────────────────────────────

/**
 * @brief 2D 박스 렌더링 아이템 (외곽선만)
 *
 * @param Position 좌상단 위치
 * @param Size 박스 크기
 * @param Color 색상
 * @param Thickness 선 두께
 */
class FCanvasBoxItem : public FCanvasItem
{
public:
	FCanvasBoxItem(const FVector2D& InPosition, const FVector2D& InSize,
	               const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), float InThickness = 1.0f)
		: FCanvasItem(InPosition, InColor)
		, Size(InSize)
		, Thickness(InThickness)
	{
	}

	void Draw(FCanvas& Canvas) override;

	void SetSize(const FVector2D& InSize) { Size = InSize; }
	void SetThickness(float InThickness) { Thickness = InThickness; }

private:
	FVector2D Size;
	float Thickness;
};

// ────────────────────────────────────────────────────────
// FCanvasEllipseItem - 2D 원/타원
// ────────────────────────────────────────────────────────

/**
 * @brief 2D 원/타원 렌더링 아이템
 *
 * @param Position 중심점
 * @param Radius 반지름
 * @param Color 색상
 * @param bFilled 채워진 원 여부
 * @param Thickness 외곽선 두께 (bFilled=false일 때)
 */
class FCanvasEllipseItem : public FCanvasItem
{
public:
	FCanvasEllipseItem(const FVector2D& InCenter, float InRadius,
	                   const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
	                   bool bInFilled = true, float InThickness = 1.0f)
		: FCanvasItem(InCenter, InColor)
		, Radius(InRadius)
		, bFilled(bInFilled)
		, Thickness(InThickness)
	{
	}

	void Draw(FCanvas& Canvas) override;

	void SetRadius(float InRadius) { Radius = InRadius; }
	void SetFilled(bool bInFilled) { bFilled = bInFilled; }
	void SetThickness(float InThickness) { Thickness = InThickness; }

private:
	float Radius;
	bool bFilled;
	float Thickness;
};

// ────────────────────────────────────────────────────────
// FCanvasTextItem - 2D 텍스트
// ────────────────────────────────────────────────────────

/**
 * @brief 2D 텍스트 렌더링 아이템
 *
 * @param Position 텍스트 위치
 * @param Text 텍스트 문자열
 * @param Color 색상
 * @param FontSize 폰트 크기
 * @param FontName 폰트 이름
 * @param bBold 굵게 표시 여부
 */
class FCanvasTextItem : public FCanvasItem
{
public:
	FCanvasTextItem(const FVector2D& InPosition, const FString& InText,
	                const FLinearColor& InColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
	                float InFontSize = 14.0f, const wchar_t* InFontName = L"Arial",
	                bool bInBold = false)
		: FCanvasItem(InPosition, InColor)
		, Text(InText)
		, FontSize(InFontSize)
		, FontName(InFontName)
		, bBold(bInBold)
	{
	}

	void Draw(FCanvas& Canvas) override;

	void SetText(const FString& InText) { Text = InText; }
	void SetFontSize(float InFontSize) { FontSize = InFontSize; }
	void SetFontName(const wchar_t* InFontName) { FontName = InFontName; }
	void SetBold(bool bInBold) { bBold = bInBold; }

private:
	FString Text;
	float FontSize;
	const wchar_t* FontName;
	bool bBold;
};

// ────────────────────────────────────────────────────────
// FCanvasAxisWidget - 카메라 축 인디케이터 (FutureEngine 패턴)
// ────────────────────────────────────────────────────────

/**
 * @brief 뷰포트에 월드 축 방향을 표시하는 2D 오버레이 위젯
 * @details FutureEngine의 FAxis 로직을 통합하여 구현
 *
 * FutureEngine Reference: Engine/Source/Editor/Private/Axis.cpp
 *
 * @param Position 위젯 위치 (뷰포트 기준)
 * @param ViewMatrix 카메라 View 행렬
 * @param AxisSize 축 라인 길이 (픽셀)
 * @param LineThickness 라인 두께 (픽셀)
 */
class FCanvasAxisWidget : public FCanvasItem
{
public:
	FCanvasAxisWidget(const FVector2D& InPosition, const FMatrix& InViewMatrix,
	                  float InAxisSize = 40.0f, float InLineThickness = 3.0f)
		: FCanvasItem(InPosition, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, ViewMatrix(InViewMatrix)
		, AxisSize(InAxisSize)
		, LineThickness(InLineThickness)
	{
	}

	void Draw(FCanvas& Canvas) override;

	void SetViewMatrix(const FMatrix& InViewMatrix) { ViewMatrix = InViewMatrix; }
	void SetAxisSize(float InAxisSize) { AxisSize = InAxisSize; }
	void SetLineThickness(float InLineThickness) { LineThickness = InLineThickness; }

private:
	FMatrix ViewMatrix;
	float AxisSize;
	float LineThickness;

	// FutureEngine 패턴: View 공간 벡터를 2D 스크린 좌표로 변환
	FVector2D ViewToScreen(const FVector4& ViewVec, float Scale = 1.0f) const;
};
