#include "pch.h"
#include "ParticleModuleDetailRenderer.h"
#include "ImGui/imgui.h"

#include "Source/Runtime/Engine/Particle/ParticleModule.h"
#include "Source/Runtime/Engine/Particle/ParticleModuleRequired.h"
#include "Source/Runtime/Engine/Particle/Spawn/ParticleModuleSpawn.h"
#include "Source/Runtime/Engine/Particle/Color/ParticleModuleColor.h"
#include "Source/Runtime/Engine/Particle/Lifetime/ParticleModuleLifetime.h"
#include "Source/Runtime/Engine/Particle/Location/ParticleModuleLocation.h"
#include "Source/Runtime/Engine/Particle/Size/ParticleModuleSize.h"
#include "Source/Runtime/Engine/Particle/Velocity/ParticleModuleVelocity.h"
#include "Source/Runtime/Engine/Particle/ParticleEmitter.h"
#include "Source/Runtime/Engine/Particle/ParticleTypes.h"

// ============================================================================
// 섹션 헬퍼
// ============================================================================

bool UParticleModuleDetailRenderer::BeginSection(const char* Label, bool bDefaultOpen)
{
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen * bDefaultOpen;
	return ImGui::CollapsingHeader(Label, Flags);
}

void UParticleModuleDetailRenderer::EndSection()
{
	// CollapsingHeader는 TreePop 필요 없음
}

// ============================================================================
// Distribution UI 헬퍼
// ============================================================================

bool UParticleModuleDetailRenderer::RenderFloatDistribution(const char* Label, FFloatDistribution& Dist)
{
	bool bChanged = false;

	ImGui::PushID(Label);

	// Use Range 체크박스
	bool bUseRange = !Dist.bIsUniform;
	if (ImGui::Checkbox("Use Range", &bUseRange))
	{
		Dist.bIsUniform = !bUseRange;
		if (Dist.bIsUniform)
		{
			Dist.Max = Dist.Min;
		}
		bChanged = true;
	}

	ImGui::SameLine();
	ImGui::Text("%s", Label);

	if (Dist.bIsUniform)
	{
		// 단일 값
		ImGui::SetNextItemWidth(-1);
		if (ImGui::DragFloat("##Value", &Dist.Min, 0.01f))
		{
			Dist.Max = Dist.Min;
			bChanged = true;
		}
	}
	else
	{
		// Min/Max 범위
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f - 4);
		if (ImGui::DragFloat("##Min", &Dist.Min, 0.01f))
		{
			bChanged = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::DragFloat("##Max", &Dist.Max, 0.01f))
		{
			bChanged = true;
		}
	}

	ImGui::PopID();
	return bChanged;
}

bool UParticleModuleDetailRenderer::RenderVectorDistribution(const char* Label, FVectorDistribution& Dist)
{
	bool bChanged = false;

	ImGui::PushID(Label);

	// Use Range 체크박스
	bool bUseRange = !Dist.bIsUniform;
	if (ImGui::Checkbox("Use Range", &bUseRange))
	{
		Dist.bIsUniform = !bUseRange;
		if (Dist.bIsUniform)
		{
			Dist.Max = Dist.Min;
		}
		bChanged = true;
	}

	ImGui::SameLine();
	ImGui::Text("%s", Label);

	if (Dist.bIsUniform)
	{
		// 단일 벡터
		float Vec[3] = { Dist.Min.X, Dist.Min.Y, Dist.Min.Z };
		if (ImGui::DragFloat3("##Value", Vec, 0.1f))
		{
			Dist.Min.X = Vec[0]; Dist.Min.Y = Vec[1]; Dist.Min.Z = Vec[2];
			Dist.Max = Dist.Min;
			bChanged = true;
		}
	}
	else
	{
		// Min
		float VecMin[3] = { Dist.Min.X, Dist.Min.Y, Dist.Min.Z };
		ImGui::Text("Min");
		ImGui::SameLine();
		if (ImGui::DragFloat3("##Min", VecMin, 0.1f))
		{
			Dist.Min.X = VecMin[0]; Dist.Min.Y = VecMin[1]; Dist.Min.Z = VecMin[2];
			bChanged = true;
		}

		// Max
		float VecMax[3] = { Dist.Max.X, Dist.Max.Y, Dist.Max.Z };
		ImGui::Text("Max");
		ImGui::SameLine();
		if (ImGui::DragFloat3("##Max", VecMax, 0.1f))
		{
			Dist.Max.X = VecMax[0]; Dist.Max.Y = VecMax[1]; Dist.Max.Z = VecMax[2];
			bChanged = true;
		}
	}

	ImGui::PopID();
	return bChanged;
}

bool UParticleModuleDetailRenderer::RenderColorDistribution(const char* Label, FColorDistribution& Dist)
{
	bool bChanged = false;

	ImGui::PushID(Label);

	// Use Range 체크박스
	bool bUseRange = !Dist.bIsUniform;
	if (ImGui::Checkbox("Use Range", &bUseRange))
	{
		Dist.bIsUniform = !bUseRange;
		if (Dist.bIsUniform)
		{
			Dist.Max = Dist.Min;
		}
		bChanged = true;
	}

	ImGui::SameLine();
	ImGui::Text("%s", Label);

	if (Dist.bIsUniform)
	{
		// 단일 색상
		float Color[4] = { Dist.Min.R, Dist.Min.G, Dist.Min.B, Dist.Min.A };
		if (ImGui::ColorEdit4("##Value", Color))
		{
			Dist.Min.R = Color[0]; Dist.Min.G = Color[1];
			Dist.Min.B = Color[2]; Dist.Min.A = Color[3];
			Dist.Max = Dist.Min;
			bChanged = true;
		}
	}
	else
	{
		// Min 색상
		float ColorMin[4] = { Dist.Min.R, Dist.Min.G, Dist.Min.B, Dist.Min.A };
		ImGui::Text("Min");
		ImGui::SameLine();
		if (ImGui::ColorEdit4("##Min", ColorMin))
		{
			Dist.Min.R = ColorMin[0]; Dist.Min.G = ColorMin[1];
			Dist.Min.B = ColorMin[2]; Dist.Min.A = ColorMin[3];
			bChanged = true;
		}

		// Max 색상
		float ColorMax[4] = { Dist.Max.R, Dist.Max.G, Dist.Max.B, Dist.Max.A };
		ImGui::Text("Max");
		ImGui::SameLine();
		if (ImGui::ColorEdit4("##Max", ColorMax))
		{
			Dist.Max.R = ColorMax[0]; Dist.Max.G = ColorMax[1];
			Dist.Max.B = ColorMax[2]; Dist.Max.A = ColorMax[3];
			bChanged = true;
		}
	}

	ImGui::PopID();
	return bChanged;
}

// ============================================================================
// Enum 콤보박스 헬퍼
// ============================================================================

bool UParticleModuleDetailRenderer::RenderScreenAlignmentCombo(const char* Label, EParticleScreenAlignment& Value)
{
	const char* Items[] = {
		"PSA Square",
		"PSA Rectangle",
		"PSA Velocity",
		"PSA TypeSpecific",
		"PSA FacingCameraPosition",
		"PSA FacingCameraDistanceBlend"
	};

	int CurrentItem = static_cast<int>(Value);
	if (ImGui::Combo(Label, &CurrentItem, Items, IM_ARRAYSIZE(Items)))
	{
		Value = static_cast<EParticleScreenAlignment>(CurrentItem);
		return true;
	}
	return false;
}

bool UParticleModuleDetailRenderer::RenderSortModeCombo(const char* Label, EParticleSortMode& Value)
{
	const char* Items[] = {
		"PSORTMODE None",
		"PSORTMODE ViewProjDepth",
		"PSORTMODE DistanceToView",
		"PSORTMODE Age_OldestFirst",
		"PSORTMODE Age_NewestFirst"
	};

	int CurrentItem = static_cast<int>(Value);
	if (ImGui::Combo(Label, &CurrentItem, Items, IM_ARRAYSIZE(Items)))
	{
		Value = static_cast<EParticleSortMode>(CurrentItem);
		return true;
	}
	return false;
}

bool UParticleModuleDetailRenderer::RenderBlendModeCombo(const char* Label, EParticleBlendMode& Value)
{
	const char* Items[] = {
		"Translucent",
		"Additive"
	};

	int CurrentItem = static_cast<int>(Value);
	if (ImGui::Combo(Label, &CurrentItem, Items, IM_ARRAYSIZE(Items)))
	{
		Value = static_cast<EParticleBlendMode>(CurrentItem);
		return true;
	}
	return false;
}

bool UParticleModuleDetailRenderer::RenderBurstMethodCombo(const char* Label, EParticleBurstMethod& Value)
{
	const char* Items[] = {
		"Instant",
		"Interpolated"
	};

	int CurrentItem = static_cast<int>(Value);
	if (ImGui::Combo(Label, &CurrentItem, Items, IM_ARRAYSIZE(Items)))
	{
		Value = static_cast<EParticleBurstMethod>(CurrentItem);
		return true;
	}
	return false;
}

// ============================================================================
// 메인 렌더링 함수
// ============================================================================

void UParticleModuleDetailRenderer::RenderModuleDetails(UParticleModule* Module)
{
	if (!Module)
	{
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a module to edit");
		return;
	}

	const char* ClassName = Module->GetClass() ? Module->GetClass()->Name : "Unknown";
	ImGui::Text("Module: %s", ClassName);
	ImGui::Separator();

	// 모듈 타입별 렌더링 분기
	if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
	{
		RenderRequiredModule(Required);
	}
	else if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
	{
		RenderSpawnModule(Spawn);
	}
	else if (UParticleModuleColor* Color = Cast<UParticleModuleColor>(Module))
	{
		RenderColorModule(Color);
	}
	else if (UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
	{
		RenderLifetimeModule(Lifetime);
	}
	else if (UParticleModuleLocation* Location = Cast<UParticleModuleLocation>(Module))
	{
		RenderLocationModule(Location);
	}
	else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
	{
		RenderSizeModule(Size);
	}
	else if (UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
	{
		RenderVelocityModule(Velocity);
	}
	else
	{
		// 기본 모듈 정보
		ImGui::Text("Spawn Module: %s", Module->IsSpawnModule() ? "Yes" : "No");
		ImGui::Text("Update Module: %s", Module->IsUpdateModule() ? "Yes" : "No");
		ImGui::Text("3D Draw Mode: %s", Module->Is3DDrawMode() ? "Yes" : "No");
	}
}

void UParticleModuleDetailRenderer::RenderEmitterDetails(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No emitter selected");
		return;
	}

	ImGui::Text("Emitter: %s", Emitter->EmitterName.c_str());
	ImGui::Separator();

	// 이미터 기본 정보
	if (BeginSection("Emitter Info", true))
	{
		ImGui::Text("LOD Levels: %d", Emitter->GetNumLODs());
		ImGui::Text("Peak Particles: %d", Emitter->GetPeakActiveParticles());

		// 에디터 색상
		float Color[4] = {
			Emitter->EmitterEditorColor.R,
			Emitter->EmitterEditorColor.G,
			Emitter->EmitterEditorColor.B,
			Emitter->EmitterEditorColor.A
		};
		if (ImGui::ColorEdit4("Editor Color", Color))
		{
			Emitter->EmitterEditorColor.R = Color[0];
			Emitter->EmitterEditorColor.G = Color[1];
			Emitter->EmitterEditorColor.B = Color[2];
			Emitter->EmitterEditorColor.A = Color[3];
		}

		// Solo 체크박스
		ImGui::Checkbox("Solo", &Emitter->bIsSoloing);
	}
}

// ============================================================================
// Required 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderRequiredModule(UParticleModuleRequired* Module)
{
	if (!Module)
	{
		return;
	}

	// Emitter 섹션
	if (BeginSection("Emitter", true))
	{
		// Material (TODO: Material 선택 UI)
		ImGui::Text("Material: %s", Module->GetMaterial() ? "Set" : "None");

		// Screen Alignment
		EParticleScreenAlignment Alignment = Module->GetScreenAlignment();
		if (RenderScreenAlignmentCombo("Screen Alignment", Alignment))
		{
			Module->SetScreenAlignment(Alignment);
		}

		// Sort Mode
		EParticleSortMode SortMode = Module->GetSortMode();
		if (RenderSortModeCombo("Sort Mode", SortMode))
		{
			Module->SetSortMode(SortMode);
		}

		// Blend Mode
		EParticleBlendMode BlendMode = Module->GetBlendMode();
		if (RenderBlendModeCombo("Blend Mode", BlendMode))
		{
			Module->SetBlendMode(BlendMode);
		}

		// Use Local Space
		bool bUseLocalSpace = Module->IsUseLocalSpace();
		if (ImGui::Checkbox("Use Local Space", &bUseLocalSpace))
		{
			Module->SetUseLocalSpace(bUseLocalSpace);
		}

		// Kill on Deactivate
		bool bKillOnDeactivate = Module->IsKillOnDeactivate();
		if (ImGui::Checkbox("Kill on Deactivate", &bKillOnDeactivate))
		{
			Module->SetKillOnDeactivate(bKillOnDeactivate);
		}

		// Kill on Completed
		bool bKillOnCompleted = Module->IsKillOnCompleted();
		if (ImGui::Checkbox("Kill on Completed", &bKillOnCompleted))
		{
			Module->SetKillOnCompleted(bKillOnCompleted);
		}
	}

	// Duration 섹션
	if (BeginSection("Duration", true))
	{
		// Emitter Loops
		int32 Loops = Module->GetEmitterLoops();
		if (ImGui::InputInt("Emitter Loops", &Loops))
		{
			Module->SetEmitterLoops(std::max(0, Loops));
		}

		// Emitter Duration
		float Duration = Module->GetEmitterDurationValue();
		if (ImGui::DragFloat("Emitter Duration", &Duration, 0.01f, 0.0f, 100.0f))
		{
			Module->SetEmitterDuration(Duration);
		}

		// Emitter Duration Low
		float DurationLow = Module->GetEmitterDurationLow();
		if (ImGui::DragFloat("Emitter Duration Low", &DurationLow, 0.01f, 0.0f, 100.0f))
		{
			Module->SetEmitterDurationLow(DurationLow);
		}

		// Duration Recalc Each Loop
		bool bRecalc = Module->IsDurationRecalcEachLoop();
		if (ImGui::Checkbox("Duration Recalc Each Loop", &bRecalc))
		{
			Module->SetDurationRecalcEachLoop(bRecalc);
		}
	}

	// Rendering 섹션
	if (BeginSection("Rendering", true))
	{
		// Max Draw Count
		int32 MaxDraw = Module->GetMaxDrawCount();
		if (ImGui::InputInt("Max Draw Count", &MaxDraw))
		{
			Module->SetMaxDrawCount(std::max(0, MaxDraw));
		}
	}

	// Sub UV 섹션
	if (BeginSection("Sub UV", false))
	{
		// SubImages Horizontal
		int32 SubH = Module->GetSubImagesHorizontal();
		if (ImGui::InputInt("SubImages Horizontal", &SubH))
		{
			Module->SetSubImagesHorizontal(std::max(1, SubH));
		}

		// SubImages Vertical
		int32 SubV = Module->GetSubImagesVertical();
		if (ImGui::InputInt("SubImages Vertical", &SubV))
		{
			Module->SetSubImagesVertical(std::max(1, SubV));
		}

		ImGui::Text("Total SubImages: %d", Module->GetTotalSubImages());
	}
}

// ============================================================================
// Spawn 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderSpawnModule(UParticleModuleSpawn* Module)
{
	if (!Module)
	{
		return;
	}

	// Spawn 섹션
	if (BeginSection("Spawn", true))
	{
		// Process Spawn Rate
		ImGui::Checkbox("Process Spawn Rate", &Module->bProcessSpawnRate);

		if (Module->bProcessSpawnRate)
		{
			// Rate
			RenderFloatDistribution("Spawn Rate", Module->Rate);

			// Rate Scale
			RenderFloatDistribution("Rate Scale", Module->RateScale);
		}
	}

	// Burst 섹션
	if (BeginSection("Burst", true))
	{
		// Process Burst List
		ImGui::Checkbox("Process Burst List", &Module->bProcessBurstList);

		if (Module->bProcessBurstList)
		{
			// Burst Method
			RenderBurstMethodCombo("Burst Method", Module->BurstMethod);

			// Burst Scale
			RenderFloatDistribution("Burst Scale", Module->BurstScale);

			// Burst List
			ImGui::Separator();
			ImGui::Text("Burst List:");

			if (ImGui::Button("+ Add Burst"))
			{
				Module->BurstList.push_back(FParticleBurst(10, 0.0f));
			}

			for (int32 i = 0; i < static_cast<int32>(Module->BurstList.size()); ++i)
			{
				ImGui::PushID(i);

				FParticleBurst& Burst = Module->BurstList[i];

				ImGui::SetNextItemWidth(60);
				ImGui::DragFloat("Time", &Burst.Time, 0.01f, 0.0f, 100.0f);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60);
				ImGui::DragInt("Count", &Burst.Count, 1, 0, 1000);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60);
				ImGui::DragInt("CountLow", &Burst.CountLow, 1, -1, 1000);
				ImGui::SameLine();

				if (ImGui::Button("X"))
				{
					Module->BurstList.erase(Module->BurstList.begin() + i);
					--i;
				}

				ImGui::PopID();
			}
		}
	}
}

// ============================================================================
// Color 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderColorModule(UParticleModuleColor* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Initial Color", true))
	{
		// Start Color
		RenderColorDistribution("Start Color", Module->StartColor);

		// Start Alpha
		RenderFloatDistribution("Start Alpha", Module->StartAlpha);

		// Clamp Alpha
		ImGui::Checkbox("Clamp Alpha", &Module->bClampAlpha);
	}
}

// ============================================================================
// Lifetime 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderLifetimeModule(UParticleModuleLifetime* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Lifetime", true))
	{
		RenderFloatDistribution("Lifetime", Module->Lifetime);
	}
}

// ============================================================================
// Location 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderLocationModule(UParticleModuleLocation* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Initial Location", true))
	{
		RenderVectorDistribution("Start Location", Module->StartLocation);
	}
}

// ============================================================================
// Size 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderSizeModule(UParticleModuleSize* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Initial Size", true))
	{
		RenderVectorDistribution("Start Size", Module->StartSize);
	}
}

// ============================================================================
// Velocity 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderVelocityModule(UParticleModuleVelocity* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Initial Velocity", true))
	{
		// Start Velocity
		RenderVectorDistribution("Start Velocity", Module->StartVelocity);

		// Radial Velocity
		RenderFloatDistribution("Start Velocity Radial", Module->StartVelocityRadial);

		// World Space
		ImGui::Checkbox("In World Space", &Module->bInWorldSpace);
	}
}
