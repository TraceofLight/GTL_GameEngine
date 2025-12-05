#include "pch.h"
#include "StatsOverlayD2D.h"
#include "Canvas.h"
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
#include "SkinningStats.h"

// Stats 패널 색상 (FutureEngine 패턴)
namespace StatsColors
{
	static const FLinearColor Yellow(1.0f, 1.0f, 0.0f, 1.0f);          // FPS
	static const FLinearColor SkyBlue(0.529f, 0.808f, 0.922f, 1.0f);   // Picking
	static const FLinearColor LightGreen(0.565f, 0.933f, 0.565f, 1.0f);// Memory
	static const FLinearColor Orange(1.0f, 0.647f, 0.0f, 1.0f);        // Decal, GPU
	static const FLinearColor Cyan(0.0f, 1.0f, 1.0f, 1.0f);            // TileCulling
	static const FLinearColor Violet(0.933f, 0.510f, 0.933f, 1.0f);    // Lights
	static const FLinearColor DeepPink(1.0f, 0.078f, 0.576f, 1.0f);    // Shadow
	static const FLinearColor Black(0.0f, 0.0f, 0.0f, 0.6f);           // 배경
}

UStatsOverlayD2D& UStatsOverlayD2D::Get()
{
	static UStatsOverlayD2D Instance;
	return Instance;
}

void UStatsOverlayD2D::Initialize()
{
	bInitialized = true;
}

void UStatsOverlayD2D::Shutdown()
{
	bInitialized = false;
}

// 헬퍼 함수: 배경 박스 + 텍스트 그리기
static void DrawTextPanel(FCanvas& Canvas, const wchar_t* Text,
                          float X, float Y, float Width, float Height,
                          const FLinearColor& TextColor)
{
	// 배경 사각형
	Canvas.DrawFilledBox(FVector2D(X, Y), FVector2D(Width, Height), StatsColors::Black);

	// 텍스트 (좌상단 기준으로 약간 오프셋)
	const float TextPadding = 4.0f;
	Canvas.DrawText(Text, FVector2D(X + TextPadding, Y + TextPadding),
	                TextColor, 16.0f, L"Segoe UI", false);
}

void UStatsOverlayD2D::Draw(FCanvas& Canvas)
{
	if (!bInitialized || (!bShowFPS && !bShowMemory && !bShowPicking && !bShowDecal &&
	                      !bShowTileCulling && !bShowLights && !bShowShadow && !bShowGPU &&
	                      !bShowSkinning && !bShowParticles && !bShowPhysicsAsset))
	{
		return;
	}

	const float Margin = 12.0f;
	const float Space = 8.0f;
	const float PanelWidth = 250.0f;
	const float PanelHeight = 48.0f;
	float NextY = 120.0f;  // UI 툴바 아래로 위치 조정

	// FPS
	if (bShowFPS)
	{
		float Dt = UUIManager::GetInstance().GetDeltaTime();
		float Fps = Dt > 0.0f ? (1.0f / Dt) : 0.0f;
		float Ms = Dt * 1000.0f;

		wchar_t Buf[128];
		swprintf_s(Buf, L"FPS: %.1f\nFrame time: %.2f ms", Fps, Ms);

		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, PanelHeight, StatsColors::Yellow);
		NextY += PanelHeight + Space;
	}

	// Picking
	if (bShowPicking)
	{
		wchar_t Buf[256];
		double LastMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetLastPickTime());
		double TotalMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetTotalPickTime());
		uint32 Count = CPickingSystem::GetPickCount();
		double AvgMs = (Count > 0) ? (TotalMs / (double)Count) : 0.0;
		swprintf_s(Buf, L"Pick Count: %u\nLast: %.3f ms\nAvg: %.3f ms\nTotal: %.3f ms",
		           Count, LastMs, AvgMs, TotalMs);

		const float PickPanelHeight = 96.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, PickPanelHeight, StatsColors::SkyBlue);
		NextY += PickPanelHeight + Space;
	}

	// Memory
	if (bShowMemory)
	{
		double Mb = static_cast<double>(FMemoryManager::TotalAllocationBytes) / (1024.0 * 1024.0);

		wchar_t Buf[128];
		swprintf_s(Buf, L"Memory: %.1f MB\nAllocs: %u", Mb, FMemoryManager::TotalAllocationCount);

		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, PanelHeight, StatsColors::LightGreen);
		NextY += PanelHeight + Space;
	}

	// Decal
	if (bShowDecal)
	{
		uint32_t TotalCount = FDecalStatManager::GetInstance().GetTotalDecalCount();
		uint32_t AffectedMeshCount = FDecalStatManager::GetInstance().GetAffectedMeshCount();
		double TotalTime = FDecalStatManager::GetInstance().GetDecalPassTimeMS();
		double AverageTimePerDecal = FDecalStatManager::GetInstance().GetAverageTimePerDecalMS();
		double AverageTimePerDraw = FDecalStatManager::GetInstance().GetAverageTimePerDrawMS();

		wchar_t Buf[256];
		swprintf_s(Buf, L"[Decal Stats]\nTotal: %u\nAffectedMesh: %u\n전체 소요 시간: %.3f ms\nAvg/Decal: %.3f ms\nAvg/Mesh: %.3f ms",
		           TotalCount, AffectedMeshCount, TotalTime, AverageTimePerDecal, AverageTimePerDraw);

		const float DecalPanelHeight = 140.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, DecalPanelHeight, StatsColors::Orange);
		NextY += DecalPanelHeight + Space;
	}

	// Tile Culling
	if (bShowTileCulling)
	{
		const FTileCullingStats& TileStats = FTileCullingStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Tile Culling Stats]\nTiles: %u x %u (%u)\nLights: %u (P:%u S:%u)\nMin/Avg/Max: %u / %.1f / %u\nCulling Eff: %.1f%%\nBuffer: %u KB",
		           TileStats.TileCountX, TileStats.TileCountY, TileStats.TotalTileCount,
		           TileStats.TotalLights, TileStats.TotalPointLights, TileStats.TotalSpotLights,
		           TileStats.MinLightsPerTile, TileStats.AvgLightsPerTile, TileStats.MaxLightsPerTile,
		           TileStats.CullingEfficiency, TileStats.LightIndexBufferSizeBytes / 1024);

		const float TilePanelHeight = 160.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, TilePanelHeight, StatsColors::Cyan);
		NextY += TilePanelHeight + Space;
	}

	// Lights
	if (bShowLights)
	{
		const FLightStats& LightStats = FLightStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Light Stats]\nTotal Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n  Ambient: %u",
		           LightStats.TotalLights, LightStats.TotalPointLights, LightStats.TotalSpotLights,
		           LightStats.TotalDirectionalLights, LightStats.TotalAmbientLights);

		const float LightPanelHeight = 140.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, LightPanelHeight, StatsColors::Violet);
		NextY += LightPanelHeight + Space;
	}

	// Shadow
	if (bShowShadow)
	{
		const FShadowStats& ShadowStats = FShadowStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Shadow Stats]\nShadow Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n\nAtlas 2D: %u x %u (%.1f MB)\nAtlas Cube: %u x %u x %u (%.1f MB)\n\nTotal Memory: %.1f MB",
		           ShadowStats.TotalShadowCastingLights, ShadowStats.ShadowCastingPointLights,
		           ShadowStats.ShadowCastingSpotLights, ShadowStats.ShadowCastingDirectionalLights,
		           ShadowStats.ShadowAtlas2DSize, ShadowStats.ShadowAtlas2DSize, ShadowStats.ShadowAtlas2DMemoryMB,
		           ShadowStats.ShadowAtlasCubeSize, ShadowStats.ShadowAtlasCubeSize, ShadowStats.ShadowCubeArrayCount,
		           ShadowStats.ShadowAtlasCubeMemoryMB, ShadowStats.TotalShadowMemoryMB);

		const float ShadowPanelHeight = 260.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, ShadowPanelHeight, StatsColors::DeepPink);
		NextY += ShadowPanelHeight + Space;

		// Shadow Time Profile
		wchar_t TimeBuf[128];
		swprintf_s(TimeBuf, L"ShadowMapPass: %.3f ms",
		           FScopeCycleCounter::GetTimeProfile("ShadowMapPass").Milliseconds);
		DrawTextPanel(Canvas, TimeBuf, Margin, NextY, PanelWidth, 40.0f, StatsColors::DeepPink);
		NextY += 40.0f + Space;
	}

	// GPU
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
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, GPUPanelHeight, StatsColors::Orange);
		NextY += GPUPanelHeight + Space;
	}

	// Skinning
	if (bShowSkinning && GPUTimer)
	{
		const FSkinningStats& SkinStats = FSkinningStatManager::GetInstance().GetStats();
		double SkinningGPUTime = GPUTimer->GetTime("Skinning");

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Skinning Stats]\nSkeletal Meshes: %u\nTotal Bones: %u\nAvg Bones/Mesh: %.1f\nGPU Time: %.3f ms",
		           SkinStats.TotalSkeletalMeshCount, SkinStats.TotalBoneCount,
		           SkinStats.AvgBonesPerMesh, SkinningGPUTime);

		const float SkinPanelHeight = 120.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, SkinPanelHeight, StatsColors::LightGreen);
		NextY += SkinPanelHeight + Space;
	}

	// Particles
	if (bShowParticles)
	{
		const FParticleStats& ParticleStats = FParticleStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Particle Stats]\nEmitters: %u\nParticles: %u\nSprite: %u\nMesh: %u",
		           ParticleStats.TotalEmitters, ParticleStats.TotalParticles,
		           ParticleStats.SpriteEmitters, ParticleStats.MeshEmitters);

		const float ParticlePanelHeight = 120.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, ParticlePanelHeight, StatsColors::Cyan);
		NextY += ParticlePanelHeight + Space;
	}

	// PhysicsAsset
	if (bShowPhysicsAsset)
	{
		wchar_t Buf[512];
		swprintf_s(Buf, L"[PhysicsAsset Stats]\nBodies: %d\nConstraints: %d\nShapes: %d\nMode: %S\nSimulating: %s",
		           PhysicsAssetBodies, PhysicsAssetConstraints, PhysicsAssetShapes,
		           PhysicsAssetMode.c_str(), bPhysicsAssetSimulating ? L"Yes" : L"No");

		const float PhysicsPanelHeight = 140.0f;
		DrawTextPanel(Canvas, Buf, Margin, NextY, PanelWidth, PhysicsPanelHeight, StatsColors::Violet);
		NextY += PhysicsPanelHeight + Space;
	}
}
