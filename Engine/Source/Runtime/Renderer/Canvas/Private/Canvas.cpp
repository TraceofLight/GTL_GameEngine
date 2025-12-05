#include "pch.h"
#include "Canvas.h"
#include "CanvasRenderBackend_D2D.h"

FCanvas::FCanvas(ID2D1RenderTarget* InRenderTarget,
                 IDWriteFactory* InDWriteFactory,
                 const FMatrix& InViewMatrix,
                 const FMatrix& InProjectionMatrix,
                 const FViewportRect& InViewportRect)
	: ViewMatrix(InViewMatrix)
	, ProjectionMatrix(InProjectionMatrix)
	, ViewportRect(InViewportRect)
{
	// Direct2D 렌더링 백엔드 생성
	RenderBackend = std::make_unique<FCanvasRenderBackend_D2D>(InRenderTarget, InDWriteFactory);

	// 초기 Transform은 Identity
	CurrentTransform = FMatrix::Identity();
}

FCanvas::~FCanvas()
{
	// TUniquePtr가 자동 해제
}

// ────────────────────────────────────────────────────────
// Draw API
// ────────────────────────────────────────────────────────

void FCanvas::DrawLine(const FVector2D& Start, const FVector2D& End,
                       const FLinearColor& Color, float Thickness)
{
	// Transform 적용 (TODO: 2D Transform 지원 시 구현)
	FVector2D TransformedStart = Start;
	FVector2D TransformedEnd = End;

	// Direct2D 색상 변환
	D2D1_COLOR_F D2DColor = FCanvasRenderBackend_D2D::LinearColorToD2D(Color);

	// Command Buffer에 추가
	RenderBackend->AddLine(TransformedStart, TransformedEnd, D2DColor, Thickness);
}

void FCanvas::DrawBox(const FVector2D& Position, const FVector2D& Size,
                      const FLinearColor& Color, float Thickness)
{
	// Transform 적용 (TODO)
	FVector2D TransformedPosition = Position;

	D2D1_COLOR_F D2DColor = FCanvasRenderBackend_D2D::LinearColorToD2D(Color);

	// 4개 라인으로 박스 그리기
	FVector2D TopLeft = TransformedPosition;
	FVector2D TopRight = TransformedPosition + FVector2D(Size.X, 0);
	FVector2D BottomRight = TransformedPosition + Size;
	FVector2D BottomLeft = TransformedPosition + FVector2D(0, Size.Y);

	RenderBackend->AddLine(TopLeft, TopRight, D2DColor, Thickness);
	RenderBackend->AddLine(TopRight, BottomRight, D2DColor, Thickness);
	RenderBackend->AddLine(BottomRight, BottomLeft, D2DColor, Thickness);
	RenderBackend->AddLine(BottomLeft, TopLeft, D2DColor, Thickness);
}

void FCanvas::DrawFilledBox(const FVector2D& Position, const FVector2D& Size,
                             const FLinearColor& Color)
{
	// Transform 적용 (TODO)
	FVector2D TransformedPosition = Position;

	D2D1_COLOR_F D2DColor = FCanvasRenderBackend_D2D::LinearColorToD2D(Color);

	// 채워진 사각형 그리기
	RenderBackend->AddRectangle(TransformedPosition, Size, D2DColor, true, 0.0f);
}

void FCanvas::DrawEllipse(const FVector2D& Center, float Radius,
                          const FLinearColor& Color, bool bFilled,
                          float Thickness)
{
	// Transform 적용 (TODO)
	FVector2D TransformedCenter = Center;

	D2D1_COLOR_F D2DColor = FCanvasRenderBackend_D2D::LinearColorToD2D(Color);

	RenderBackend->AddEllipse(TransformedCenter, Radius, Radius, D2DColor, bFilled, Thickness);
}

void FCanvas::DrawText(const wchar_t* Text, const FVector2D& Position,
                       const FLinearColor& Color, float FontSize,
                       const wchar_t* FontName, bool bBold, bool bCenterAlign)
{
	// Transform 적용 (TODO)
	FVector2D TransformedPosition = Position;

	D2D1_COLOR_F D2DColor = FCanvasRenderBackend_D2D::LinearColorToD2D(Color);

	RenderBackend->AddText(Text, TransformedPosition, D2DColor, FontSize, FontName, bBold, bCenterAlign);
}

// ────────────────────────────────────────────────────────
// Transform Stack
// ────────────────────────────────────────────────────────

void FCanvas::PushAbsoluteTransform(const FMatrix& Transform)
{
	TransformStack.Add(CurrentTransform);
	CurrentTransform = Transform;
}

void FCanvas::PushRelativeTransform(const FMatrix& Transform)
{
	TransformStack.Add(CurrentTransform);
	CurrentTransform = CurrentTransform * Transform;
}

void FCanvas::PopTransform()
{
	if (!TransformStack.IsEmpty())
	{
		CurrentTransform = TransformStack.Last();
		TransformStack.RemoveAt(TransformStack.Num() - 1);
	}
}

const FMatrix& FCanvas::GetTransform() const
{
	return CurrentTransform;
}

// ────────────────────────────────────────────────────────
// 좌표 변환 (World ↔ Screen)
// ────────────────────────────────────────────────────────

FVector2D FCanvas::Project(const FVector& WorldLocation) const
{
	// 1. World → Clip Space (View * Projection)
	FMatrix ViewProjection = ViewMatrix * ProjectionMatrix;
	FVector4 ClipSpace = ViewProjection.TransformPositionVector4(WorldLocation);

	// 2. Perspective Divide (Clip → NDC)
	if (std::abs(ClipSpace.W) > KINDA_SMALL_NUMBER)
	{
		ClipSpace.X /= ClipSpace.W;
		ClipSpace.Y /= ClipSpace.W;
		ClipSpace.Z /= ClipSpace.W;
	}

	// 3. NDC → Screen Space
	// NDC: X[-1, 1], Y[-1, 1]
	// Screen: X[0, Width], Y[0, Height]
	float ScreenX = (ClipSpace.X + 1.0f) * 0.5f * ViewportRect.Width();
	float ScreenY = (1.0f - ClipSpace.Y) * 0.5f * ViewportRect.Height();  // Y축 반전

	return FVector2D(ScreenX, ScreenY);
}

void FCanvas::Deproject(const FVector2D& ScreenPosition,
                        FVector& OutWorldOrigin, FVector& OutWorldDirection) const
{
	// 1. Screen → NDC
	float NdcX = (ScreenPosition.X / ViewportRect.Width()) * 2.0f - 1.0f;
	float NdcY = 1.0f - (ScreenPosition.Y / ViewportRect.Height()) * 2.0f;  // Y축 반전

	// 2. NDC → Clip Space (W=1 가정, Near Plane)
	FVector4 ClipNear(NdcX, NdcY, 0.0f, 1.0f);  // Z=0 (Near Plane)
	FVector4 ClipFar(NdcX, NdcY, 1.0f, 1.0f);   // Z=1 (Far Plane)

	// 3. Clip → World Space (Inverse ViewProjection)
	FMatrix ViewProjection = ViewMatrix * ProjectionMatrix;
	FMatrix InvViewProjection = ViewProjection.Inverse();

	FVector4 WorldNear = InvViewProjection.TransformPositionVector4(FVector(ClipNear.X, ClipNear.Y, ClipNear.Z));
	FVector4 WorldFar = InvViewProjection.TransformPositionVector4(FVector(ClipFar.X, ClipFar.Y, ClipFar.Z));

	// 4. Ray 생성
	OutWorldOrigin = FVector(WorldNear.X, WorldNear.Y, WorldNear.Z);
	OutWorldDirection = (FVector(WorldFar.X, WorldFar.Y, WorldFar.Z) - OutWorldOrigin).GetNormalized();
}

// ────────────────────────────────────────────────────────
// 배치 렌더링
// ────────────────────────────────────────────────────────

void FCanvas::Flush()
{
	RenderBackend->Flush();
}
