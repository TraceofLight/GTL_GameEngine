#include "pch.h"
#include "CanvasRenderBackend_D2D.h"
#include <algorithm>

FCanvasRenderBackend_D2D::FCanvasRenderBackend_D2D(ID2D1RenderTarget* InRenderTarget, IDWriteFactory* InDWriteFactory)
	: D2DRenderTarget(InRenderTarget)
	, DWriteFactory(InDWriteFactory)
{
}

FCanvasRenderBackend_D2D::~FCanvasRenderBackend_D2D()
{
	// Brush 캐시 해제
	for (auto& Pair : BrushCache)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	BrushCache.clear();

	// TextFormat 캐시 해제
	for (auto& Pair : TextFormatCache)
	{
		if (Pair.second)
		{
			Pair.second->Release();
		}
	}
	TextFormatCache.clear();
}

// ────────────────────────────────────────────────────────
// Command Buffer API
// ────────────────────────────────────────────────────────

void FCanvasRenderBackend_D2D::AddLine(const FVector2D& Start, const FVector2D& End,
                                        const D2D1_COLOR_F& Color, float Thickness)
{
	FLineCommand Cmd;
	Cmd.Start = Vector2DToD2D(Start);
	Cmd.End = Vector2DToD2D(End);
	Cmd.Color = Color;
	Cmd.Thickness = Thickness;
	LineCommands.Add(Cmd);
}

void FCanvasRenderBackend_D2D::AddEllipse(const FVector2D& Center, float RadiusX, float RadiusY,
                                          const D2D1_COLOR_F& Color, bool bFilled, float Thickness)
{
	FEllipseCommand Cmd;
	Cmd.Center = Vector2DToD2D(Center);
	Cmd.RadiusX = RadiusX;
	Cmd.RadiusY = RadiusY;
	Cmd.Color = Color;
	Cmd.bFilled = bFilled;
	Cmd.Thickness = Thickness;
	EllipseCommands.Add(Cmd);
}

void FCanvasRenderBackend_D2D::AddRectangle(const FVector2D& Position, const FVector2D& Size,
                                            const D2D1_COLOR_F& Color, bool bFilled, float Thickness)
{
	FRectangleCommand Cmd;
	Cmd.Rect = D2D1::RectF(Position.X, Position.Y, Position.X + Size.X, Position.Y + Size.Y);
	Cmd.Color = Color;
	Cmd.bFilled = bFilled;
	Cmd.Thickness = Thickness;
	RectangleCommands.Add(Cmd);
}

void FCanvasRenderBackend_D2D::AddText(const wchar_t* Text, const FVector2D& Position,
                                       const D2D1_COLOR_F& Color, float FontSize,
                                       const wchar_t* FontName, bool bBold, bool bCenterAlign)
{
	FTextCommand Cmd;
	Cmd.Text = Text;
	Cmd.Position = Vector2DToD2D(Position);
	Cmd.Color = Color;
	Cmd.FontSize = FontSize;
	Cmd.FontName = FontName;
	Cmd.bBold = bBold;
	Cmd.bCenterAlign = bCenterAlign;
	TextCommands.Add(Cmd);
}

// ────────────────────────────────────────────────────────
// 배치 렌더링 실행
// ────────────────────────────────────────────────────────

void FCanvasRenderBackend_D2D::BeginDraw()
{
	if (D2DRenderTarget)
	{
		D2DRenderTarget->BeginDraw();
	}
}

void FCanvasRenderBackend_D2D::EndDraw()
{
	if (D2DRenderTarget)
	{
		D2DRenderTarget->EndDraw();
	}
}

void FCanvasRenderBackend_D2D::Flush()
{
	if (!D2DRenderTarget)
	{
		return;
	}

	BeginDraw();

	// 1. 모든 라인 렌더링
	for (const FLineCommand& Cmd : LineCommands)
	{
		ID2D1SolidColorBrush* Brush = GetOrCreateBrush(Cmd.Color);
		if (Brush)
		{
			D2DRenderTarget->DrawLine(Cmd.Start, Cmd.End, Brush, Cmd.Thickness);
		}
	}

	// 2. 모든 원 렌더링
	for (const FEllipseCommand& Cmd : EllipseCommands)
	{
		ID2D1SolidColorBrush* Brush = GetOrCreateBrush(Cmd.Color);
		if (Brush)
		{
			D2D1_ELLIPSE Ellipse = D2D1::Ellipse(Cmd.Center, Cmd.RadiusX, Cmd.RadiusY);
			if (Cmd.bFilled)
			{
				D2DRenderTarget->FillEllipse(Ellipse, Brush);
			}
			else
			{
				D2DRenderTarget->DrawEllipse(Ellipse, Brush, Cmd.Thickness);
			}
		}
	}

	// 3. 모든 사각형 렌더링
	for (const FRectangleCommand& Cmd : RectangleCommands)
	{
		ID2D1SolidColorBrush* Brush = GetOrCreateBrush(Cmd.Color);
		if (Brush)
		{
			if (Cmd.bFilled)
			{
				D2DRenderTarget->FillRectangle(Cmd.Rect, Brush);
			}
			else
			{
				D2DRenderTarget->DrawRectangle(Cmd.Rect, Brush, Cmd.Thickness);
			}
		}
	}

	// 4. 모든 텍스트 렌더링
	if (DWriteFactory && !TextCommands.IsEmpty())
	{
		for (const FTextCommand& Cmd : TextCommands)
		{
			ID2D1SolidColorBrush* Brush = GetOrCreateBrush(Cmd.Color);
			IDWriteTextFormat* TextFormat = GetOrCreateTextFormat(Cmd.FontName.c_str(), Cmd.FontSize, Cmd.bBold);

			if (Brush && TextFormat)
			{
				// 텍스트 크기 측정
				IDWriteTextLayout* TextLayout = nullptr;
				DWriteFactory->CreateTextLayout(
					Cmd.Text.c_str(),
					static_cast<uint32>(Cmd.Text.length()),
					TextFormat,
					10000.0f,  // maxWidth
					10000.0f,  // maxHeight
					&TextLayout
				);

				if (TextLayout)
				{
					DWRITE_TEXT_METRICS Metrics;
					TextLayout->GetMetrics(&Metrics);

					// 텍스트 렌더링 (정렬 모드에 따라 분기)
					D2D1_RECT_F TextRect;
					if (Cmd.bCenterAlign)
					{
						// 중앙 정렬 (Axis 레이블용)
						TextRect = D2D1::RectF(
							Cmd.Position.x - Metrics.width * 0.5f,
							Cmd.Position.y - Metrics.height * 0.5f,
							Cmd.Position.x + Metrics.width * 0.5f,
							Cmd.Position.y + Metrics.height * 0.5f
						);
					}
					else
					{
						// 좌상단 정렬 (Stats용)
						TextRect = D2D1::RectF(
							Cmd.Position.x,
							Cmd.Position.y,
							Cmd.Position.x + Metrics.width,
							Cmd.Position.y + Metrics.height
						);
					}

					D2DRenderTarget->DrawText(
						Cmd.Text.c_str(),
						static_cast<uint32>(Cmd.Text.length()),
						TextFormat,
						TextRect,
						Brush
					);

					TextLayout->Release();
				}
			}
		}
	}

	EndDraw();

	// 명령 버퍼 클리어
	LineCommands.Empty();
	EllipseCommands.Empty();
	RectangleCommands.Empty();
	TextCommands.Empty();
}

// ────────────────────────────────────────────────────────
// Resource Caching
// ────────────────────────────────────────────────────────

ID2D1SolidColorBrush* FCanvasRenderBackend_D2D::GetOrCreateBrush(const D2D1_COLOR_F& Color)
{
	FColorKey Key = { Color.r, Color.g, Color.b, Color.a };

	// 캐시에서 찾기
	auto It = BrushCache.find(Key);
	if (It != BrushCache.end())
	{
		return It->second;
	}

	// 새로 생성
	ID2D1SolidColorBrush* NewBrush = nullptr;
	if (D2DRenderTarget)
	{
		D2DRenderTarget->CreateSolidColorBrush(Color, &NewBrush);
		if (NewBrush)
		{
			BrushCache[Key] = NewBrush;
		}
	}

	return NewBrush;
}

IDWriteTextFormat* FCanvasRenderBackend_D2D::GetOrCreateTextFormat(const wchar_t* FontName, float FontSize, bool bBold)
{
	FTextFormatKey Key;
	Key.FontName = FontName;
	Key.FontSize = FontSize;
	Key.bBold = bBold;

	// 캐시에서 찾기
	auto It = TextFormatCache.find(Key);
	if (It != TextFormatCache.end())
	{
		return It->second;
	}

	// 새로 생성
	IDWriteTextFormat* NewFormat = nullptr;
	if (DWriteFactory)
	{
		DWriteFactory->CreateTextFormat(
			FontName,
			nullptr,
			bBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			FontSize,
			L"en-us",
			&NewFormat
		);

		if (NewFormat)
		{
			NewFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			NewFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
			TextFormatCache[Key] = NewFormat;
		}
	}

	return NewFormat;
}

// ────────────────────────────────────────────────────────
// 헬퍼 함수
// ────────────────────────────────────────────────────────

D2D1_COLOR_F FCanvasRenderBackend_D2D::LinearColorToD2D(const FLinearColor& Color)
{
	return D2D1::ColorF(Color.R, Color.G, Color.B, Color.A);
}

D2D1_POINT_2F FCanvasRenderBackend_D2D::Vector2DToD2D(const FVector2D& Vec)
{
	return D2D1::Point2F(Vec.X, Vec.Y);
}
