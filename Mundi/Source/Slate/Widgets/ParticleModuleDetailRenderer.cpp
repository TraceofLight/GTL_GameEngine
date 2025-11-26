#include "pch.h"
#include "ParticleModuleDetailRenderer.h"
#include "ImGui/imgui.h"
#include <algorithm>

#include "Source/Runtime/Engine/Particle/ParticleModule.h"
#include "Source/Runtime/Engine/Particle/ParticleModuleRequired.h"
#include "Source/Runtime/Engine/Particle/Spawn/ParticleModuleSpawn.h"
#include "Source/Runtime/Engine/Particle/Color/ParticleModuleColor.h"
#include "Source/Runtime/Engine/Particle/Lifetime/ParticleModuleLifetime.h"
#include "Source/Runtime/Engine/Particle/Location/ParticleModuleLocation.h"
#include "Source/Runtime/Engine/Particle/Size/ParticleModuleSize.h"
#include "Source/Runtime/Engine/Particle/Velocity/ParticleModuleVelocity.h"
#include "Source/Runtime/Engine/Particle/TypeData/ParticleModuleTypeDataBase.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleRotationRate.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleMeshRotation.h"
#include "Source/Runtime/Engine/Particle/Rotation/ParticleModuleMeshRotationRate.h"
#include "Source/Runtime/Engine/Particle/ParticleEmitter.h"
#include "Source/Runtime/Engine/Particle/ParticleTypes.h"
#include "Source/Runtime/Renderer/Material.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/StaticMesh.h"
#include "Source/Editor/PlatformProcess.h"

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
	// enum class EParticleBlendMode : None(0), Translucent(1), Additive(2)
	const char* Items[] = {
		"None",
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
	else if (UParticleModuleTypeDataMesh* TypeDataMesh = Cast<UParticleModuleTypeDataMesh>(Module))
	{
		RenderTypeDataMeshModule(TypeDataMesh);
	}
	else if (UParticleModuleTypeDataBeam* TypeDataBeam = Cast<UParticleModuleTypeDataBeam>(Module))
	{
		RenderTypeDataBeamModule(TypeDataBeam);
	}
	else if (UParticleModuleRotation* Rotation = Cast<UParticleModuleRotation>(Module))
	{
		RenderRotationModule(Rotation);
	}
	else if (UParticleModuleRotationRate* RotationRate = Cast<UParticleModuleRotationRate>(Module))
	{
		RenderRotationRateModule(RotationRate);
	}
	else if (UParticleModuleMeshRotation* MeshRotation = Cast<UParticleModuleMeshRotation>(Module))
	{
		RenderMeshRotationModule(MeshRotation);
	}
	else if (UParticleModuleMeshRotationRate* MeshRotationRate = Cast<UParticleModuleMeshRotationRate>(Module))
	{
		RenderMeshRotationRateModule(MeshRotationRate);
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
		// Material 선택 UI
		UMaterial* CurrentMaterial = Module->GetMaterial();
		FString MaterialName = "None";
		if (CurrentMaterial)
		{
			MaterialName = CurrentMaterial->GetFilePath();
			// 경로에서 파일명만 추출
			size_t LastSlash = MaterialName.find_last_of("/\\");
			if (LastSlash != FString::npos)
			{
				MaterialName = MaterialName.substr(LastSlash + 1);
			}
		}

		ImGui::Text("Material:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
		ImGui::InputText("##MaterialPath", const_cast<char*>(MaterialName.c_str()), MaterialName.size() + 1, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse##Material", ImVec2(70, 0)))
		{
			std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialogMultiExt(
				L"Data",
				L"*.dds;*.png;*.jpg;*.tga;*.bmp",
				L"Texture Files"
			);
			if (!SelectedPath.empty())
			{
				// 지원 확장자 체크
				FString Extension = SelectedPath.extension().string();
				std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

				if (Extension != ".dds" && Extension != ".png" && Extension != ".jpg" &&
				    Extension != ".jpeg" && Extension != ".tga" && Extension != ".bmp")
				{
					UE_LOG("ParticleEditor: Unsupported texture format '%s'. Supported: dds, png, jpg, tga, bmp", Extension.c_str());
				}
				else
				{
					FString TexturePath = SelectedPath.string();
					// 경로 정규화
					for (char& c : TexturePath)
					{
						if (c == '\\')
						{
							c = '/';
						}
					}
					// 텍스처를 머티리얼로 로드
					UMaterial* NewMaterial = UResourceManager::GetInstance().Load<UMaterial>(TexturePath);
					if (NewMaterial)
					{
						Module->SetMaterial(NewMaterial);
					}
					else
					{
						UE_LOG("ParticleEditor: Failed to load material from '%s'", TexturePath.c_str());
					}
				}
			}
		}
		if (CurrentMaterial)
		{
			ImGui::SameLine();
			if (ImGui::Button("X##ClearMaterial", ImVec2(20, 0)))
			{
				Module->SetMaterial(nullptr);
			}
		}

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

// ============================================================================
// TypeData Mesh 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderTypeDataMeshModule(UParticleModuleTypeDataMesh* Module)
{
	if (!Module)
	{
		return;
	}

	// Mesh 섹션
	if (BeginSection("Mesh Data", true))
	{
		// Mesh 선택 UI
		UStaticMesh* CurrentMesh = Module->Mesh;
		FString MeshName = "None";
		if (CurrentMesh)
		{
			MeshName = CurrentMesh->GetFilePath();
			// 경로에서 파일명만 추출
			size_t LastSlash = MeshName.find_last_of("/\\");
			if (LastSlash != FString::npos)
			{
				MeshName = MeshName.substr(LastSlash + 1);
			}
		}

		ImGui::Text("Mesh:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
		ImGui::InputText("##MeshPath", const_cast<char*>(MeshName.c_str()), MeshName.size() + 1, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse##Mesh", ImVec2(70, 0)))
		{
			std::filesystem::path SelectedPath = FPlatformProcess::OpenLoadFileDialogMultiExt(
				L"Data",
				L"*.obj;*.fbx",
				L"Mesh Files"
			);
			if (!SelectedPath.empty())
			{
				// 지원 확장자 체크
				FString Extension = SelectedPath.extension().string();
				std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

				if (Extension != ".obj" && Extension != ".fbx")
				{
					UE_LOG("ParticleEditor: Unsupported mesh format '%s'. Supported: obj, fbx", Extension.c_str());
				}
				else
				{
					FString MeshPath = SelectedPath.string();
					// 경로 정규화
					for (char& c : MeshPath)
					{
						if (c == '\\')
						{
							c = '/';
						}
					}
					// 메시 로드
					UStaticMesh* NewMesh = UResourceManager::GetInstance().Load<UStaticMesh>(MeshPath);
					if (NewMesh)
					{
						Module->Mesh = NewMesh;
						Module->OnMeshChanged();
					}
					else
					{
						UE_LOG("ParticleEditor: Failed to load mesh from '%s'", MeshPath.c_str());
					}
				}
			}
		}
		if (CurrentMesh)
		{
			ImGui::SameLine();
			if (ImGui::Button("X##ClearMesh", ImVec2(20, 0)))
			{
				Module->Mesh = nullptr;
				Module->OnMeshChanged();
			}
		}
	}

	// Rendering 섹션
	if (BeginSection("Rendering", true))
	{
		// Cast Shadows
		if (ImGui::Checkbox("Cast Shadows", &Module->bCastShadows))
		{
			Module->OnMeshChanged();
		}

		// Override Material
		if (ImGui::Checkbox("Override Material", &Module->bOverrideMaterial))
		{
			Module->OnMeshChanged();
		}

		// Mesh Alignment
		const char* AlignmentItems[] = { "None", "Z-Axis", "X-Axis", "Y-Axis" };
		int32 CurrentAlignment = static_cast<int32>(Module->MeshAlignment);
		if (ImGui::Combo("Mesh Alignment", &CurrentAlignment, AlignmentItems, IM_ARRAYSIZE(AlignmentItems)))
		{
			Module->MeshAlignment = static_cast<EParticleAxisLock>(CurrentAlignment);
			Module->OnMeshChanged();
		}
	}

	// Rotation Offset 섹션
	if (BeginSection("Rotation Offset", true))
	{
		if (ImGui::DragFloat("Pitch", &Module->Pitch, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::DragFloat("Yaw", &Module->Yaw, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::DragFloat("Roll", &Module->Roll, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
	}

	// Collision 섹션
	if (BeginSection("Collision", false))
	{
		ImGui::Checkbox("Do Collisions", &Module->DoCollisions);
	}
}

// ============================================================================
// Rotation 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderRotationModule(UParticleModuleRotation* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Initial Rotation", true))
	{
		RenderFloatDistribution("Start Rotation (Degrees)", Module->StartRotation);
	}
}

// ============================================================================
// Rotation Rate 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderRotationRateModule(UParticleModuleRotationRate* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Rotation Rate", true))
	{
		RenderFloatDistribution("Start Rotation Rate (Deg/s)", Module->StartRotationRate);
	}
}

// ============================================================================
// Mesh Rotation 모듈 렌더링 (3D 회전)
// ============================================================================

void UParticleModuleDetailRenderer::RenderMeshRotationModule(UParticleModuleMeshRotation* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Mesh Rotation (3D)", true))
	{
		RenderVectorDistribution("Start Rotation (Degrees)", Module->StartRotation);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "X=Pitch, Y=Yaw, Z=Roll");
	}
}

// ============================================================================
// Mesh Rotation Rate 모듈 렌더링 (3D 회전 속도)
// ============================================================================

void UParticleModuleDetailRenderer::RenderMeshRotationRateModule(UParticleModuleMeshRotationRate* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Mesh Rotation Rate (3D)", true))
	{
		RenderVectorDistribution("Start Rotation Rate (Deg/s)", Module->StartRotationRate);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "X=Pitch, Y=Yaw, Z=Roll");
	}
}

// ============================================================================
// TypeData Beam 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderTypeDataBeamModule(UParticleModuleTypeDataBeam* Module)
{
	if (!Module)
	{
		return;
	}

	// Beam Endpoint 섹션
	if (BeginSection("Beam Endpoints", true))
	{
		// Source Offset
		float SrcVec[3] = { Module->SourceOffset.X, Module->SourceOffset.Y, Module->SourceOffset.Z };
		if (ImGui::DragFloat3("Source Offset", SrcVec, 1.0f))
		{
			Module->SourceOffset.X = SrcVec[0];
			Module->SourceOffset.Y = SrcVec[1];
			Module->SourceOffset.Z = SrcVec[2];
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Offset from emitter location (start)");

		// Target Offset
		float TgtVec[3] = { Module->TargetOffset.X, Module->TargetOffset.Y, Module->TargetOffset.Z };
		if (ImGui::DragFloat3("Target Offset", TgtVec, 1.0f))
		{
			Module->TargetOffset.X = TgtVec[0];
			Module->TargetOffset.Y = TgtVec[1];
			Module->TargetOffset.Z = TgtVec[2];
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Offset from emitter location (end)");
	}

	// Beam Shape 섹션
	if (BeginSection("Beam Shape", true))
	{
		// Beam Width
		RenderFloatDistribution("Beam Width", Module->BeamWidth);

		// Segments
		int32 Segments = Module->Segments;
		if (ImGui::InputInt("Segments", &Segments))
		{
			Module->Segments = std::max(1, Segments);
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Number of segments (more = smoother)");

		// Taper
		ImGui::Checkbox("Taper Beam", &Module->bTaperBeam);
		if (Module->bTaperBeam)
		{
			ImGui::DragFloat("Taper Factor", &Module->TaperFactor, 0.01f, 0.0f, 1.0f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "0 = full taper, 1 = no taper");
		}
	}

	// Rendering 섹션
	if (BeginSection("Rendering", true))
	{
		// Sheets
		int32 Sheets = Module->Sheets;
		if (ImGui::InputInt("Sheets", &Sheets))
		{
			Module->Sheets = std::max(1, Sheets);
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Number of sheets (for visibility from all angles)");

		// UV Tiling
		ImGui::DragFloat("UV Tiling", &Module->UVTiling, 0.1f, 0.1f, 100.0f);
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Texture repeat along beam length");
	}
}
