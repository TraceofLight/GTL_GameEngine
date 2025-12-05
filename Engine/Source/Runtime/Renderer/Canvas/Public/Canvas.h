#pragma once

#include "Vector.h"
#include "Color.h"
#include "UEContainer.h"
#include "SceneView.h"  // FViewportRect
#include <d2d1.h>
#include <dwrite.h>
#include <memory>

// Forward declarations
class FCanvasRenderBackend_D2D;

/**
 * @brief Unreal Engine 스타일 2D Canvas 렌더링 시스템
 * @details Direct2D를 백엔드로 사용하며, Transform 스택 및 배치 렌더링 지원
 *
 * UE Reference: Engine/Source/Runtime/Engine/Public/CanvasTypes.h
 *
 * @param RenderBackend Direct2D 렌더링 백엔드 (Command Buffer 패턴)
 * @param TransformStack Transform 스택 (계층적 좌표 변환)
 * @param ViewMatrix 카메라 View 행렬
 * @param ProjectionMatrix 카메라 Projection 행렬
 * @param ViewportRect 뷰포트 영역
 */
class FCanvas
{
public:
	// 생성자/소멸자
	FCanvas(ID2D1RenderTarget* InRenderTarget,
	        IDWriteFactory* InDWriteFactory,
	        const FMatrix& InViewMatrix,
	        const FMatrix& InProjectionMatrix,
	        const FViewportRect& InViewportRect);
	~FCanvas();

	// ────────────────────────────────────────────────────────
	// Draw API (Unreal Engine 스타일)
	// ────────────────────────────────────────────────────────

	/**
	 * @brief 2D 라인 그리기
	 * @param Start 시작점 (스크린 좌표)
	 * @param End 끝점 (스크린 좌표)
	 * @param Color 색상
	 * @param Thickness 선 두께 (픽셀)
	 */
	void DrawLine(const FVector2D& Start, const FVector2D& End,
	              const FLinearColor& Color, float Thickness = 1.0f);

	/**
	 * @brief 2D 박스 그리기 (외곽선만)
	 * @param Position 좌상단 위치 (스크린 좌표)
	 * @param Size 박스 크기 (픽셀)
	 * @param Color 색상
	 * @param Thickness 선 두께 (픽셀)
	 */
	void DrawBox(const FVector2D& Position, const FVector2D& Size,
	             const FLinearColor& Color, float Thickness = 1.0f);

	/**
	 * @brief 2D 채워진 박스 그리기
	 * @param Position 좌상단 위치 (스크린 좌표)
	 * @param Size 박스 크기 (픽셀)
	 * @param Color 색상
	 */
	void DrawFilledBox(const FVector2D& Position, const FVector2D& Size,
	                    const FLinearColor& Color);

	/**
	 * @brief 2D 원 그리기
	 * @param Center 중심점 (스크린 좌표)
	 * @param Radius 반지름 (픽셀)
	 * @param Color 색상
	 * @param bFilled true=채워진 원, false=외곽선만
	 * @param Thickness 외곽선 두께 (bFilled=false일 때)
	 */
	void DrawEllipse(const FVector2D& Center, float Radius,
	                 const FLinearColor& Color, bool bFilled = true,
	                 float Thickness = 1.0f);

	/**
	 * @brief 2D 텍스트 그리기
	 * @param Text 텍스트 문자열 (유니코드)
	 * @param Position 위치 (스크린 좌표)
	 * @param Color 색상
	 * @param FontSize 폰트 크기 (픽셀)
	 * @param FontName 폰트 이름 (기본: L"Arial")
	 * @param bBold 굵게 표시 여부
	 * @param bCenterAlign 중앙 정렬 여부 (기본: false = 좌상단 정렬)
	 */
	void DrawText(const wchar_t* Text, const FVector2D& Position,
	              const FLinearColor& Color, float FontSize = 14.0f,
	              const wchar_t* FontName = L"Arial", bool bBold = false,
	              bool bCenterAlign = false);

	// ────────────────────────────────────────────────────────
	// Transform Stack (Unreal Engine 스타일)
	// ────────────────────────────────────────────────────────

	/**
	 * @brief 절대 Transform Push (현재 Transform 무시)
	 * @param Transform 새 Transform 행렬
	 */
	void PushAbsoluteTransform(const FMatrix& Transform);

	/**
	 * @brief 상대 Transform Push (현재 Transform에 곱셈)
	 * @param Transform 상대 Transform 행렬
	 */
	void PushRelativeTransform(const FMatrix& Transform);

	/**
	 * @brief Transform Pop (이전 Transform 복원)
	 */
	void PopTransform();

	/**
	 * @brief 현재 Transform 조회
	 * @return 현재 Transform 행렬
	 */
	const FMatrix& GetTransform() const;

	// ────────────────────────────────────────────────────────
	// 좌표 변환 (World ↔ Screen)
	// ────────────────────────────────────────────────────────

	/**
	 * @brief 3D 월드 좌표를 2D 스크린 좌표로 변환
	 * @param WorldLocation 월드 공간 좌표
	 * @return 스크린 공간 좌표 (픽셀)
	 */
	FVector2D Project(const FVector& WorldLocation) const;

	/**
	 * @brief 2D 스크린 좌표를 3D 월드 레이로 역변환
	 * @param ScreenPosition 스크린 공간 좌표 (픽셀)
	 * @param OutWorldOrigin 레이 시작점 (월드 공간)
	 * @param OutWorldDirection 레이 방향 (월드 공간)
	 */
	void Deproject(const FVector2D& ScreenPosition,
	               FVector& OutWorldOrigin, FVector& OutWorldDirection) const;

	// ────────────────────────────────────────────────────────
	// 배치 렌더링
	// ────────────────────────────────────────────────────────

	/**
	 * @brief 배치 렌더링 실행 (Direct2D BeginDraw/EndDraw)
	 */
	void Flush();

	// ────────────────────────────────────────────────────────
	// Getters
	// ────────────────────────────────────────────────────────

	const FMatrix& GetViewMatrix() const { return ViewMatrix; }
	const FMatrix& GetProjectionMatrix() const { return ProjectionMatrix; }
	const FViewportRect& GetViewportRect() const { return ViewportRect; }

private:
	// Direct2D 렌더링 백엔드
	std::unique_ptr<FCanvasRenderBackend_D2D> RenderBackend;

	// Transform 스택
	TArray<FMatrix> TransformStack;
	FMatrix CurrentTransform;

	// Camera/Viewport 정보
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FViewportRect ViewportRect;
};
