#include "pch.h"
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <algorithm>
#include "StatsOverlayD2D.h"
#include "UIManager.h"
#include "MemoryManager.h"
#include "Picking.h"
#include "PlatformTime.h"
#include "DecalStatManager.h"
#include "GPUProfiler.h"
#include "TileCullingStats.h"
#include "LightStats.h"
#include "ShadowStats.h"
#include "ParticleStats.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

static inline void SafeRelease(IUnknown* p) { if (p) p->Release(); }

UStatsOverlayD2D& UStatsOverlayD2D::Get()
{
	static UStatsOverlayD2D Instance;
	return Instance;
}

void UStatsOverlayD2D::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain)
{
	D3DDevice = InDevice;
	D3DContext = InContext;
	SwapChain = InSwapChain;
	bInitialized = (D3DDevice && D3DContext && SwapChain);

	if (!bInitialized)
	{
		return;
	}

	D2D1_FACTORY_OPTIONS FactoryOpts{};
#ifdef _DEBUG
	FactoryOpts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &FactoryOpts, (void**)&D2DFactory)))
	{
		return;
	}

	IDXGIDevice* DxgiDevice = nullptr;
	if (FAILED(D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice)))
	{
		return;
	}

	if (FAILED(D2DFactory->CreateDevice(DxgiDevice, &D2DDevice)))
	{
		SafeRelease(DxgiDevice);
		return;
	}
	SafeRelease(DxgiDevice);

	if (FAILED(D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DContext)))
	{
		return;
	}

	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&DWriteFactory)))
	{
		return;
	}

	if (DWriteFactory)
	{
		DWriteFactory->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			16.0f,
			L"en-us",
			&TextFormat);

		if (TextFormat)
		{
			TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		}
	}

	EnsureInitialized();
}

void UStatsOverlayD2D::Shutdown()
{
	ReleaseD2DResources();

	D3DDevice = nullptr;
	D3DContext = nullptr;
	SwapChain = nullptr;
	bInitialized = false;
}

void UStatsOverlayD2D::EnsureInitialized()
{
	if (!D2DContext)
	{
		return;
	}

	SafeRelease(BrushYellow);
	SafeRelease(BrushSkyBlue);
	SafeRelease(BrushLightGreen);
	SafeRelease(BrushOrange);
	SafeRelease(BrushCyan);
	SafeRelease(BrushViolet);
	SafeRelease(BrushDeepPink);
	SafeRelease(BrushBlack);

	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &BrushYellow);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::SkyBlue), &BrushSkyBlue);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGreen), &BrushLightGreen);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), &BrushOrange);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Cyan), &BrushCyan);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Violet), &BrushViolet);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DeepPink), &BrushDeepPink);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.6f), &BrushBlack);
}

void UStatsOverlayD2D::ReleaseD2DResources()
{
	SafeRelease(BrushBlack);
	SafeRelease(BrushDeepPink);
	SafeRelease(BrushViolet);
	SafeRelease(BrushCyan);
	SafeRelease(BrushOrange);
	SafeRelease(BrushLightGreen);
	SafeRelease(BrushSkyBlue);
	SafeRelease(BrushYellow);

	SafeRelease(TextFormat);
	SafeRelease(DWriteFactory);
	SafeRelease(D2DContext);
	SafeRelease(D2DDevice);
	SafeRelease(D2DFactory);
}

static void DrawTextBlock(
	ID2D1DeviceContext* InD2dCtx,
	IDWriteTextFormat* InTextFormat,
	const wchar_t* InText,
	const D2D1_RECT_F& InRect,
	ID2D1SolidColorBrush* InBgBrush,
	ID2D1SolidColorBrush* InTextBrush)
{
	if (!InD2dCtx || !InTextFormat || !InText || !InBgBrush || !InTextBrush)
	{
		return;
	}

	InD2dCtx->FillRectangle(InRect, InBgBrush);
	InD2dCtx->DrawTextW(
		InText,
		static_cast<UINT32>(wcslen(InText)),
		InTextFormat,
		InRect,
		InTextBrush);
}

void UStatsOverlayD2D::Draw()
{
	if (!bInitialized || (!bShowFPS && !bShowMemory && !bShowPicking && !bShowDecal && !bShowTileCulling && !bShowLights && !bShowShadow && !bShowGPU && !bShowSkinning && !bShowParticles && !bShowPhysicsAsset) || !SwapChain)
	{
		return;
	}

	if (!D2DContext || !TextFormat)
	{
		return;
	}

	IDXGISurface* Surface = nullptr;
	if (FAILED(SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface)))
	{
		return;
	}

	D2D1_BITMAP_PROPERTIES1 BmpProps = {};
	BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	BmpProps.dpiX = 96.0f;
	BmpProps.dpiY = 96.0f;
	BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	ID2D1Bitmap1* TargetBmp = nullptr;
	if (FAILED(D2DContext->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp)))
	{
		SafeRelease(Surface);
		return;
	}

	D2DContext->SetTarget(TargetBmp);

	D2DContext->BeginDraw();
	const float Margin = 12.0f;
	const float Space = 8.0f;   // 패널간의 간격
	const float PanelWidth = 250.0f;
	const float PanelHeight = 48.0f;
	float NextY = 120.0f;  // UI 툴바 아래로 위치 조정

	if (bShowFPS)
	{
		float Dt = UUIManager::GetInstance().GetDeltaTime();
		float Fps = Dt > 0.0f ? (1.0f / Dt) : 0.0f;
		float Ms = Dt * 1000.0f;

		wchar_t Buf[128];
		swprintf_s(Buf, L"FPS: %.1f\nFrame time: %.2f ms", Fps, Ms);

		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushYellow);

		NextY += PanelHeight + Space;
	}

	if (bShowPicking)
	{
		wchar_t Buf[256];
		double LastMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetLastPickTime());
		double TotalMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetTotalPickTime());
		uint32 Count = CPickingSystem::GetPickCount();
		double AvgMs = (Count > 0) ? (TotalMs / (double)Count) : 0.0;
		swprintf_s(Buf, L"Pick Count: %u\nLast: %.3f ms\nAvg: %.3f ms\nTotal: %.3f ms", Count, LastMs, AvgMs, TotalMs);

		const float PickPanelHeight = 96.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PickPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushSkyBlue);

		NextY += PickPanelHeight + Space;
	}

	if (bShowMemory)
	{
		double Mb = static_cast<double>(FMemoryManager::TotalAllocationBytes) / (1024.0 * 1024.0);

		wchar_t Buf[128];
		swprintf_s(Buf, L"Memory: %.1f MB\nAllocs: %u", Mb, FMemoryManager::TotalAllocationCount);

		D2D1_RECT_F Rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, Rc, BrushBlack, BrushLightGreen);

		NextY += PanelHeight + Space;
	}

	if (bShowDecal)
	{
		// 1. FDecalStatManager로부터 통계 데이터를 가져옵니다.
		uint32_t TotalCount = FDecalStatManager::GetInstance().GetTotalDecalCount();
		//uint32_t VisibleDecalCount = FDecalStatManager::GetInstance().GetVisibleDecalCount();
		uint32_t AffectedMeshCount = FDecalStatManager::GetInstance().GetAffectedMeshCount();
		double TotalTime = FDecalStatManager::GetInstance().GetDecalPassTimeMS();
		double AverageTimePerDecal = FDecalStatManager::GetInstance().GetAverageTimePerDecalMS();
		double AverageTimePerDraw = FDecalStatManager::GetInstance().GetAverageTimePerDrawMS();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[256];
		swprintf_s(Buf, L"[Decal Stats]\nTotal: %u\nAffectedMesh: %u\n전체 소요 시간: %.3f ms\nAvg/Decal: %.3f ms\nAvg/Mesh: %.3f ms",
			TotalCount,
			AffectedMeshCount,
			TotalTime,
			AverageTimePerDecal,
			AverageTimePerDraw);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float decalPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + decalPanelHeight);

		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushOrange);

		NextY += decalPanelHeight + Space;
	}

	if (bShowTileCulling)
	{
		const FTileCullingStats& TileStats = FTileCullingStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Tile Culling Stats]\nTiles: %u x %u (%u)\nLights: %u (P:%u S:%u)\nMin/Avg/Max: %u / %.1f / %u\nCulling Eff: %.1f%%\nBuffer: %u KB",
			TileStats.TileCountX,
			TileStats.TileCountY,
			TileStats.TotalTileCount,
			TileStats.TotalLights,
			TileStats.TotalPointLights,
			TileStats.TotalSpotLights,
			TileStats.MinLightsPerTile,
			TileStats.AvgLightsPerTile,
			TileStats.MaxLightsPerTile,
			TileStats.CullingEfficiency,
			TileStats.LightIndexBufferSizeBytes / 1024);

		const float tilePanelHeight = 160.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + tilePanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushCyan);

		NextY += tilePanelHeight + Space;
	}

	if (bShowLights)
	{
		const FLightStats& LightStats = FLightStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Light Stats]\nTotal Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n  Ambient: %u",
			LightStats.TotalLights,
			LightStats.TotalPointLights,
			LightStats.TotalSpotLights,
			LightStats.TotalDirectionalLights,
			LightStats.TotalAmbientLights);

		const float lightPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + lightPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushViolet);

		NextY += lightPanelHeight + Space;
	}

	if (bShowShadow)
	{
		const FShadowStats& ShadowStats = FShadowStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Shadow Stats]\nShadow Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n\nAtlas 2D: %u x %u (%.1f MB)\nAtlas Cube: %u x %u x %u (%.1f MB)\n\nTotal Memory: %.1f MB",
			ShadowStats.TotalShadowCastingLights,
			ShadowStats.ShadowCastingPointLights,
			ShadowStats.ShadowCastingSpotLights,
			ShadowStats.ShadowCastingDirectionalLights,
			ShadowStats.ShadowAtlas2DSize,
			ShadowStats.ShadowAtlas2DSize,
			ShadowStats.ShadowAtlas2DMemoryMB,
			ShadowStats.ShadowAtlasCubeSize,
			ShadowStats.ShadowAtlasCubeSize,
			ShadowStats.ShadowCubeArrayCount,
			ShadowStats.ShadowAtlasCubeMemoryMB,
			ShadowStats.TotalShadowMemoryMB);

		const float shadowPanelHeight = 260.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + shadowPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushDeepPink);

		NextY += shadowPanelHeight + Space;

		rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + 40);
		DrawTextBlock(D2DContext, TextFormat, FScopeCycleCounter::GetTimeProfile("ShadowMapPass").GetConstWChar_tWithKey("ShadowMapPass"), rc, BrushBlack, BrushDeepPink);

		NextY += shadowPanelHeight + Space;
	}

	if (bShowGPU && GPUTimer)
	{
		wchar_t Buf[512];
		swprintf_s(Buf,
			L"=== GPU Timings ===\n"
			L"RenderLitPath: %.3f ms\n"
			L"ShadowMaps: %.3f ms\n"
			L"OpaquePass: %.3f ms\n"
			L"DecalPass: %.3f ms\n"
			L"HeightFog: %.3f ms\n"
			L"EditorPrimitives: %.3f ms\n"
			L"DebugPass: %.3f ms\n"
			L"OverlayPrimitives: %.3f ms",
			GPUTimer->GetTime("RenderLitPath"),
			GPUTimer->GetTime("ShadowMaps"),
			GPUTimer->GetTime("OpaquePass"),
			GPUTimer->GetTime("DecalPass"),
			GPUTimer->GetTime("HeightFog"),
			GPUTimer->GetTime("EditorPrimitives"),
			GPUTimer->GetTime("DebugPass"),
			GPUTimer->GetTime("OverlayPrimitives"));

		constexpr float GPUPanelHeight = 200.0f;
		D2D1_RECT_F Rect = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + GPUPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, Rect, BrushBlack, BrushOrange);

		NextY += GPUPanelHeight + Space;
	}

	if (bShowSkinning && GPUTimer)
	{
		const ESkinningMode SkinningMode = GWorld->GetRenderSettings().GetSkinningMode();

		const FTimeProfile& CpuProfile = FScopeCycleCounter::GetTimeProfile("SKINNING_CPU_TASK");
		double CpuSkinningTime = CpuProfile.Milliseconds;

		double GpuSkinningTime = GPUTimer->GetTime("SKINNING_GPU_TASK");
		GpuSkinningTime = std::max(GpuSkinningTime, 0.0);

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Skinning Stats]\n"
		   L"Mode: %s\n\n"
		   L"--- Timings ---\n"
		   L"SKINNING_CPU_TASK: %.4f ms\n"
		   L"SKINNING_GPU_TASK: %.4f ms",
		   (SkinningMode == ESkinningMode::GPU) ? L"GPU" : L"CPU",
		   CpuSkinningTime,
		   GpuSkinningTime
		);

		constexpr float SkinningPanelHeight = 130.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + SkinningPanelHeight);

		ID2D1SolidColorBrush* BrushLawnGreen = nullptr;
		D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LawnGreen), &BrushLawnGreen);

		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushLawnGreen);

		SafeRelease(BrushLawnGreen);

		NextY += SkinningPanelHeight + Space;
	}

	if (bShowParticles)
	{
		const FParticleStats& ParticleStats = FParticleStatManager::GetInstance().GetStats();

		// GPU 시간을 GPUTimer에서 가져오기
		double GpuTimeMS = 0.0;
		if (GPUTimer)
		{
			GpuTimeMS = GPUTimer->GetTime("ParticlePass");
			GpuTimeMS = std::max(GpuTimeMS, 0.0);
		}

		wchar_t Buf[1024];
		swprintf_s(Buf, 
			L"[Particle Stats]\n"
			L"Systems: %u / %u\n"
			L"Emitters: %u / %u\n"
			L"  Sprite: %u | Mesh: %u\n"
			L"Particles: %u\n"
			L"Inserted Vertices: %u\n"
			L"Inserted Instances: %u\n"
			L"Draw Calls: %u\n"
			L"Drawed Triangles: %u\n"
			L"Drawed Vertices: %u\n"
			L"\n"
			L"--- Timings ---\n"
			L"CPU: %.3f ms\n"
			L"GPU: %.3f ms\n"
			L"Avg/System: %.3f ms\n"
			L"Avg/Emitter: %.3f ms\n"
			L"Avg/Particle: %.3f µs\n"
			L"\n"
			L"--- Memory ---\n"
			L"Total: %.2f MB\n"
			L"  VB: %.2f MB\n"
			L"  IB: %.2f MB\n"
			L"  Instance: %.2f MB",
			ParticleStats.VisibleParticleSystems,
			ParticleStats.TotalParticleSystems,
			ParticleStats.VisibleEmitters,
			ParticleStats.TotalEmitters,
			ParticleStats.SpriteEmitters,
			ParticleStats.MeshEmitters,
			ParticleStats.RenderedParticles,
			ParticleStats.TotalInsertedVertices,
			ParticleStats.TotalInsertedInstances,
			ParticleStats.TotalDrawCalls,
			ParticleStats.TotalDrawedTriangles,
			ParticleStats.TotalDrawedVertices,
			ParticleStats.CpuTimeMS,
			GpuTimeMS,
			ParticleStats.AverageTimePerSystem,
			ParticleStats.AverageTimePerEmitter,
			ParticleStats.AverageTimePerParticle,
			ParticleStats.GetTotalMemoryMB(),
			static_cast<float>(ParticleStats.VertexBufferMemoryBytes) / (1024.0f * 1024.0f),
			static_cast<float>(ParticleStats.IndexBufferMemoryBytes) / (1024.0f * 1024.0f),
			static_cast<float>(ParticleStats.InstanceBufferMemoryBytes) / (1024.0f * 1024.0f)
		);

		constexpr float ParticlePanelHeight = 500.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + ParticlePanelHeight);
		
		// 고유한 색상으로 파티클 통계 표시 (LimeGreen)
		ID2D1SolidColorBrush* BrushLimeGreen = nullptr;
		D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LimeGreen), &BrushLimeGreen);
		
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushLimeGreen);
		
		SafeRelease(BrushLimeGreen);

		NextY += ParticlePanelHeight + Space;
	}

	if (bShowPhysicsAsset)
	{
		wchar_t Buf[256];
		if (bPhysicsAssetSimulating)
		{
			swprintf_s(Buf, L"[Physics Asset]\nBodies: %d | Constraints: %d\nShapes: %d | Mode: %S\nSIMULATING",
				PhysicsAssetBodies, PhysicsAssetConstraints, PhysicsAssetShapes, PhysicsAssetMode.c_str());
		}
		else
		{
			swprintf_s(Buf, L"[Physics Asset]\nBodies: %d | Constraints: %d\nShapes: %d | Mode: %S",
				PhysicsAssetBodies, PhysicsAssetConstraints, PhysicsAssetShapes, PhysicsAssetMode.c_str());
		}

		const float PhysicsAssetPanelHeight = 96.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PhysicsAssetPanelHeight);

		ID2D1SolidColorBrush* BrushGold = nullptr;
		D2DContext->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.843f, 0.0f, 1.0f), &BrushGold);

		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushGold);

		SafeRelease(BrushGold);

		NextY += PhysicsAssetPanelHeight + Space;
	}

	D2DContext->EndDraw();
	D2DContext->SetTarget(nullptr);

	FScopeCycleCounter::TimeProfileInit();

	SafeRelease(TargetBmp);
	SafeRelease(Surface);
}
