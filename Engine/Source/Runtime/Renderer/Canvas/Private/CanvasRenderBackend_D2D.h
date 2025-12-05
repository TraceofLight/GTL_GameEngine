#pragma once

#include "Vector.h"
#include "UEContainer.h"
#include <d2d1.h>
#include <dwrite.h>

/**
 * @brief Direct2D 기반 Canvas 렌더링 백엔드 (Command Buffer 패턴)
 * @details FutureEngine의 FD2DOverlayManager 패턴을 참고한 배치 렌더링 시스템
 *
 * FutureEngine Reference: Engine/Source/Render/UI/Overlay/D2DOverlayManager.cpp
 *
 * @param LineCommands 라인 렌더링 명령 버퍼
 * @param EllipseCommands 원 렌더링 명령 버퍼
 * @param RectangleCommands 사각형 렌더링 명령 버퍼
 * @param TextCommands 텍스트 렌더링 명령 버퍼
 * @param BrushCache 색상별 Brush 캐시 (성능 최적화)
 * @param TextFormatCache 폰트별 TextFormat 캐시 (성능 최적화)
 */
class FCanvasRenderBackend_D2D
{
public:
	// 생성자/소멸자
	FCanvasRenderBackend_D2D(ID2D1RenderTarget* InRenderTarget, IDWriteFactory* InDWriteFactory);
	~FCanvasRenderBackend_D2D();

	// ────────────────────────────────────────────────────────
	// Command Buffer API (FCanvas에서 호출)
	// ────────────────────────────────────────────────────────

	void AddLine(const FVector2D& Start, const FVector2D& End,
	             const D2D1_COLOR_F& Color, float Thickness);

	void AddEllipse(const FVector2D& Center, float RadiusX, float RadiusY,
	                const D2D1_COLOR_F& Color, bool bFilled, float Thickness);

	void AddRectangle(const FVector2D& Position, const FVector2D& Size,
	                  const D2D1_COLOR_F& Color, bool bFilled, float Thickness);

	void AddText(const wchar_t* Text, const FVector2D& Position,
	             const D2D1_COLOR_F& Color, float FontSize,
	             const wchar_t* FontName, bool bBold, bool bCenterAlign);

	// ────────────────────────────────────────────────────────
	// 배치 렌더링 실행
	// ────────────────────────────────────────────────────────

	void BeginDraw();
	void EndDraw();
	void Flush();

	// ────────────────────────────────────────────────────────
	// 헬퍼 함수
	// ────────────────────────────────────────────────────────

	static D2D1_COLOR_F LinearColorToD2D(const FLinearColor& Color);
	static D2D1_POINT_2F Vector2DToD2D(const FVector2D& Vec);

private:
	// ────────────────────────────────────────────────────────
	// Command Structures (FutureEngine 패턴)
	// ────────────────────────────────────────────────────────

	struct FLineCommand
	{
		D2D1_POINT_2F Start;
		D2D1_POINT_2F End;
		D2D1_COLOR_F Color;
		float Thickness;
	};

	struct FEllipseCommand
	{
		D2D1_POINT_2F Center;
		float RadiusX;
		float RadiusY;
		D2D1_COLOR_F Color;
		bool bFilled;
		float Thickness;
	};

	struct FRectangleCommand
	{
		D2D1_RECT_F Rect;
		D2D1_COLOR_F Color;
		bool bFilled;
		float Thickness;
	};

	struct FTextCommand
	{
		std::wstring Text;
		D2D1_POINT_2F Position;
		D2D1_COLOR_F Color;
		float FontSize;
		std::wstring FontName;
		bool bBold;
		bool bCenterAlign;
	};

	// Command Buffers
	TArray<FLineCommand> LineCommands;
	TArray<FEllipseCommand> EllipseCommands;
	TArray<FRectangleCommand> RectangleCommands;
	TArray<FTextCommand> TextCommands;

	// ────────────────────────────────────────────────────────
	// Resource Caching (성능 최적화)
	// ────────────────────────────────────────────────────────

	struct FColorKey
	{
		float R, G, B, A;

		bool operator==(const FColorKey& Other) const
		{
			return R == Other.R && G == Other.G && B == Other.B && A == Other.A;
		}
	};

	struct FColorKeyHash
	{
		size_t operator()(const FColorKey& Key) const
		{
			size_t Hash = 0;
			Hash = Hash * 31 + *reinterpret_cast<const uint32*>(&Key.R);
			Hash = Hash * 31 + *reinterpret_cast<const uint32*>(&Key.G);
			Hash = Hash * 31 + *reinterpret_cast<const uint32*>(&Key.B);
			Hash = Hash * 31 + *reinterpret_cast<const uint32*>(&Key.A);
			return Hash;
		}
	};

	struct FTextFormatKey
	{
		std::wstring FontName;
		float FontSize;
		bool bBold;

		bool operator==(const FTextFormatKey& Other) const
		{
			return FontName == Other.FontName && FontSize == Other.FontSize && bBold == Other.bBold;
		}
	};

	struct FTextFormatKeyHash
	{
		size_t operator()(const FTextFormatKey& Key) const
		{
			size_t Hash = 0;
			for (wchar_t Ch : Key.FontName)
			{
				Hash = Hash * 31 + static_cast<size_t>(Ch);
			}
			Hash = Hash * 31 + *reinterpret_cast<const uint32*>(&Key.FontSize);
			Hash = Hash * 31 + (Key.bBold ? 1 : 0);
			return Hash;
		}
	};

	std::unordered_map<FColorKey, ID2D1SolidColorBrush*, FColorKeyHash> BrushCache;
	std::unordered_map<FTextFormatKey, IDWriteTextFormat*, FTextFormatKeyHash> TextFormatCache;

	ID2D1SolidColorBrush* GetOrCreateBrush(const D2D1_COLOR_F& Color);
	IDWriteTextFormat* GetOrCreateTextFormat(const wchar_t* FontName, float FontSize, bool bBold);

	// ────────────────────────────────────────────────────────
	// Direct2D Resources
	// ────────────────────────────────────────────────────────

	ID2D1RenderTarget* D2DRenderTarget;
	IDWriteFactory* DWriteFactory;
};
