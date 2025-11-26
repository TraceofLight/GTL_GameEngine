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
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamSource.h"
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamTarget.h"
#include "Source/Runtime/Engine/Particle/Beam/ParticleModuleBeamNoise.h"
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Min~Max 랜덤 범위 사용 (체크 해제 시 상수값)");
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Min~Max 랜덤 범위 사용 (각 요소별 독립적, 체크 해제 시 상수값)");
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
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Min~Max 랜덤 범위 사용 (각 채널별 독립적, 체크 해제 시 고정 색상)");
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
	else if (UParticleModuleBeamSource* BeamSource = Cast<UParticleModuleBeamSource>(Module))
	{
		RenderBeamSourceModule(BeamSource);
	}
	else if (UParticleModuleBeamTarget* BeamTarget = Cast<UParticleModuleBeamTarget>(Module))
	{
		RenderBeamTargetModule(BeamTarget);
	}
	else if (UParticleModuleBeamNoise* BeamNoise = Cast<UParticleModuleBeamNoise>(Module))
	{
		RenderBeamNoiseModule(BeamNoise);
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 렌더링에 사용할 텍스처 (지원: DDS, PNG, JPG, TGA, BMP)\n기본값: None");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클이 카메라를 향하는 방식\n"
				"Square: 카메라 정면 빌보드 (1:1 비율)\n"
				"Rectangle: 커스텀 비율 빌보드\n"
				"Velocity: 파티클 속도 방향으로 정렬\n"
				"TypeSpecific: Emitter 타입의 기본 정렬 사용\n"
				"FacingCameraPosition: 항상 카메라 위치를 향함\n"
				"FacingCameraDistanceBlend: 거리 기반 블렌딩\n"
				"기본값: Square");
		}

		// Sort Mode
		EParticleSortMode SortMode = Module->GetSortMode();
		if (RenderSortModeCombo("Sort Mode", SortMode))
		{
			Module->SetSortMode(SortMode);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 렌더링 순서 정렬 방식\n"
				"None: 정렬 안 함 (가장 빠름)\n"
				"ViewProjDepth: 뷰 투영 깊이 기준 정렬\n"
				"DistanceToView: 카메라 거리 기준 정렬\n"
				"Age_OldestFirst: 오래된 파티클부터 렌더링\n"
				"Age_NewestFirst: 최신 파티클부터 렌더링\n"
				"기본값: None");
		}

		// Blend Mode
		EParticleBlendMode BlendMode = Module->GetBlendMode();
		if (RenderBlendModeCombo("Blend Mode", BlendMode))
		{
			Module->SetBlendMode(BlendMode);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 블렌딩 모드\n"
				"None: 불투명 (투명도 없음)\n"
				"Translucent: 알파 블렌딩 (연기, 먼지 등)\n"
				"Additive: 가산 블렌딩 (불, 발광 효과 등)\n"
				"기본값: None");
		}

		// Use Local Space
		bool bUseLocalSpace = Module->IsUseLocalSpace();
		if (ImGui::Checkbox("Use Local Space", &bUseLocalSpace))
		{
			Module->SetUseLocalSpace(bUseLocalSpace);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클을 Emitter의 로컬 공간에서 시뮬레이션\n"
				"true: 파티클이 Emitter 이동을 따라감\n"
				"false: 파티클이 생성 후 월드 위치에 고정\n"
				"기본값: false");
		}

		// Kill on Deactivate
		bool bKillOnDeactivate = Module->IsKillOnDeactivate();
		if (ImGui::Checkbox("Kill on Deactivate", &bKillOnDeactivate))
		{
			Module->SetKillOnDeactivate(bKillOnDeactivate);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Emitter 비활성화 시 모든 활성 파티클 제거\n"
				"기본값: false");
		}

		// Kill on Completed
		bool bKillOnCompleted = Module->IsKillOnCompleted();
		if (ImGui::Checkbox("Kill on Completed", &bKillOnCompleted))
		{
			Module->SetKillOnCompleted(bKillOnCompleted);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Emitter 지속 시간 완료 시 모든 활성 파티클 제거\n"
				"기본값: false");
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Emitter가 반복할 횟수 (0 = 무한 반복)\n"
				"기본값: 0");
		}

		// Emitter Duration
		float Duration = Module->GetEmitterDurationValue();
		if (ImGui::DragFloat("Emitter Duration", &Duration, 0.01f, 0.0f, 100.0f))
		{
			Module->SetEmitterDuration(Duration);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Emitter가 파티클을 생성하는 최대 지속 시간 (초)\n"
				"DurationLow > 0이면 실제 지속 시간 = random(DurationLow, Duration)\n"
				"기본값: 1.0");
		}

		// Emitter Duration Low
		float DurationLow = Module->GetEmitterDurationLow();
		if (ImGui::DragFloat("Emitter Duration Low", &DurationLow, 0.01f, 0.0f, 100.0f))
		{
			Module->SetEmitterDurationLow(DurationLow);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("랜덤 지속 시간 범위의 최소값\n"
				"0이면 고정값 Emitter Duration 사용\n"
				"기본값: 0.0");
		}

		// Duration Recalc Each Loop
		bool bRecalc = Module->IsDurationRecalcEachLoop();
		if (ImGui::Checkbox("Duration Recalc Each Loop", &bRecalc))
		{
			Module->SetDurationRecalcEachLoop(bRecalc);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("각 루프마다 랜덤 지속 시간 재계산\n"
				"DurationLow > 0일 때만 적용됨\n"
				"기본값: false");
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("프레임당 렌더링할 최대 파티클 개수\n"
				"많은 파티클 수에서 렌더링 비용 제한\n"
				"0 = 제한 없음\n"
				"기본값: 500");
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("가로 방향 서브 이미지 개수 (texture atlas)\n"
				"스프라이트 시트 애니메이션용 (TBD)\n"
				"기본값: 1");
		}

		// SubImages Vertical
		int32 SubV = Module->GetSubImagesVertical();
		if (ImGui::InputInt("SubImages Vertical", &SubV))
		{
			Module->SetSubImagesVertical(std::max(1, SubV));
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("세로 방향 서브 이미지 개수 (texture atlas)\n"
				"스프라이트 시트 애니메이션용 (TBD)\n"
				"기본값: 1");
		}

		ImGui::Text("Total SubImages: %d", Module->GetTotalSubImages());
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("스프라이트 시트의 총 프레임 수 (가로 * 세로)\n"
				"서브 이미지 애니메이션은 아직 미구현 (TBD)");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Spawn Rate 기반 연속적인 파티클 생성 활성화\n"
				"기본값: true");
		}

		if (Module->bProcessSpawnRate)
		{
			// Rate
			RenderFloatDistribution("Spawn Rate", Module->Rate);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("초당 생성할 파티클 개수\n"
					"소수점 누적 방식으로 부드러운 생성 보장\n"
					"기본값: 10.0");
			}

			// Rate Scale
			RenderFloatDistribution("Rate Scale", Module->RateScale);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Spawn Rate에 곱해지는 배율 (1.0 = 100%, 0.5 = 50%)\n"
					"기본값: 1.0");
			}
		}
	}

	// Burst 섹션
	if (BeginSection("Burst", true))
	{
		// Process Burst List
		ImGui::Checkbox("Process Burst List", &Module->bProcessBurstList);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Burst 생성 활성화 (특정 시간에 대량 파티클 생성)\n"
				"기본값: true");
		}

		if (Module->bProcessBurstList)
		{
			// Burst Method
			RenderBurstMethodCombo("Burst Method", Module->BurstMethod);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Burst 생성 방식\n"
					"Instant: 한 프레임에 모든 burst 파티클 생성\n"
					"Interpolated: 프레임 내에서 burst 파티클 분산 생성 (TBD)\n"
					"기본값: Instant");
			}

			// Burst Scale
			RenderFloatDistribution("Burst Scale", Module->BurstScale);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("모든 Burst 개수에 곱해지는 배율\n"
					"기본값: 1.0");
			}

			// Burst List
			ImGui::Separator();
			ImGui::Text("Burst List:");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Burst 이벤트 목록 (Time, Count, CountLow)\n"
					"각 Burst는 Emitter 수명 중 지정된 시간에 파티클 생성");
			}

			if (ImGui::Button("+ Add Burst"))
			{
				Module->BurstList.push_back(FParticleBurst(10, 0.0f));
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("목록에 새 Burst 이벤트 추가");
			}

			for (int32 i = 0; i < static_cast<int32>(Module->BurstList.size()); ++i)
			{
				ImGui::PushID(i);

				FParticleBurst& Burst = Module->BurstList[i];

				ImGui::SetNextItemWidth(60);
				ImGui::DragFloat("Time", &Burst.Time, 0.01f, 0.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Burst가 발생하는 시간 (초 단위, Emitter 시작 기준)");
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60);
				ImGui::DragInt("Count", &Burst.Count, 1, 0, 1000);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("생성할 파티클 개수\n"
						"CountLow가 -1이면 고정값, 그 외에는 최대값");
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60);
				ImGui::DragInt("CountLow", &Burst.CountLow, 1, -1, 1000);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("최소 파티클 개수\n"
						"-1: Count 고정값으로 사용\n"
						"0 이상: CountLow~Count 범위에서 랜덤 생성");
				}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 초기 색상 (RGB 채널)\n"
				"기본값: 흰색 (1, 1, 1)");
		}

		// Start Alpha
		RenderFloatDistribution("Start Alpha", Module->StartAlpha);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 초기 알파/불투명도 (0.0 = 투명, 1.0 = 불투명)\n"
				"기본값: 1.0");
		}

		// Clamp Alpha
		ImGui::Checkbox("Clamp Alpha", &Module->bClampAlpha);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("알파값을 [0.0, 1.0] 범위로 제한\n"
				"기본값: true");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 수명 (초 단위, 각 파티클이 존재하는 시간)\n"
				"기본값: 1.0");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 생성 위치 오프셋 (Emitter 기준 상대 좌표)\n"
				"기본값: (0, 0, 0)");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("파티클 초기 크기 (X, Y, Z 스케일)\n"
				"스프라이트의 경우 균등 스케일링은 (X, X, 1.0) 형태 사용\n"
				"기본값: (1, 1, 1)");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 속도 벡터 (초당 이동 거리)\n"
				"'In World Space' 활성화하지 않으면 로컬 공간에서 적용\n"
				"기본값: (0, 0, 0)");
		}

		// Radial Velocity
		RenderFloatDistribution("Start Velocity Radial", Module->StartVelocityRadial);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 방사형 속도 (Emitter 중심에서 바깥쪽 방향)\n"
				"양수 = 바깥으로 확장, 음수 = 안쪽으로 수축\n"
				"기본값: 0.0");
		}

		// World Space
		ImGui::Checkbox("In World Space", &Module->bInWorldSpace);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("속도를 Emitter 로컬 공간 대신 월드 공간에서 적용\n"
				"기본값: false");
		}
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
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Mesh 파티클은 스프라이트 대신 3D 메시 렌더링");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("TypeData Mesh: 2D 스프라이트 대신 3D 메시를 파티클로 사용\n"
				"파편, 발사체, 복잡한 형태에 유용");
		}

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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("각 파티클에 사용할 Static Mesh (지원: .obj, .fbx)\n"
				"기본값: None");
		}
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
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Mesh 파티클의 그림자 생성 활성화\n"
				"기본값: false (TBD - Mesh 파티클 그림자 미완성)");
		}

		// Override Material
		if (ImGui::Checkbox("Override Material", &Module->bOverrideMaterial))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Mesh 원본 Material 대신 Required 모듈의 Material 사용\n"
				"기본값: false");
		}

		// Mesh Alignment
		const char* AlignmentItems[] = { "None", "Z-Axis", "X-Axis", "Y-Axis" };
		int32 CurrentAlignment = static_cast<int32>(Module->MeshAlignment);
		if (ImGui::Combo("Mesh Alignment", &CurrentAlignment, AlignmentItems, IM_ARRAYSIZE(AlignmentItems)))
		{
			Module->MeshAlignment = static_cast<EParticleAxisLock>(CurrentAlignment);
			Module->OnMeshChanged();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Mesh 회전을 특정 축으로 고정\n"
				"None: 자유 회전\n"
				"Z-Axis/X-Axis/Y-Axis: 해당 축 중심 회전만 허용\n"
				"기본값: None (TBD - 축 고정 기능 미완성 가능성)");
		}
	}

	// Rotation Offset 섹션
	if (BeginSection("Rotation Offset", true))
	{
		if (ImGui::DragFloat("Pitch", &Module->Pitch, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("모든 Mesh 인스턴스에 적용되는 고정 Pitch 회전 오프셋 (도)\n"
				"기본값: 0.0");
		}

		if (ImGui::DragFloat("Yaw", &Module->Yaw, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("모든 Mesh 인스턴스에 적용되는 고정 Yaw 회전 오프셋 (도)\n"
				"기본값: 0.0");
		}

		if (ImGui::DragFloat("Roll", &Module->Roll, 1.0f, -180.0f, 180.0f, "%.1f"))
		{
			Module->OnMeshChanged();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("모든 Mesh 인스턴스에 적용되는 고정 Roll 회전 오프셋 (도)\n"
				"기본값: 0.0");
		}
	}

	// Collision 섹션
	if (BeginSection("Collision", false))
	{
		ImGui::Checkbox("Do Collisions", &Module->DoCollisions);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Mesh 파티클의 충돌 감지 활성화\n"
				"기본값: false (TBD - Mesh 파티클 충돌 미구현)");
		}
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
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "2D 스프라이트 Z축 회전");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Rotation 모듈: 스프라이트 파티클이 화면 방향 축(Z) 중심 회전\n"
				"3D Mesh 회전은 MeshRotation 모듈 사용");
		}

		RenderFloatDistribution("Start Rotation (Degrees)", Module->StartRotation);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 2D 회전 각도 (도, 시계방향)\n"
				"기본값: 0.0");
		}
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
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "2D 스프라이트 회전 속도");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Rotation Rate 모듈: 스프라이트 파티클의 연속 회전\n"
				"3D Mesh 회전 속도는 MeshRotationRate 모듈 사용");
		}

		RenderFloatDistribution("Start Rotation Rate (Deg/s)", Module->StartRotationRate);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 2D 회전 속도 (초당 도)\n"
				"양수 = 시계방향, 음수 = 반시계방향\n"
				"기본값: 0.0");
		}
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
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "3D Mesh 회전 (Pitch, Yaw, Roll)");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("MeshRotation 모듈: Mesh 파티클의 완전한 3D 회전\n"
				"TypeData Mesh 모듈이 활성화되어야 함");
		}

		RenderVectorDistribution("Start Rotation (Degrees)", Module->StartRotation);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 3D 회전 (X=Pitch, Y=Yaw, Z=Roll) 각도 (도)\n"
				"기본값: (0, 0, 0)");
		}

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
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "3D Mesh 회전 속도");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("MeshRotationRate 모듈: Mesh 파티클의 연속 3D 회전\n"
				"TypeData Mesh 모듈이 활성화되어야 함");
		}

		RenderVectorDistribution("Start Rotation Rate (Deg/s)", Module->StartRotationRate);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("초기 3D 회전 속도 (X=Pitch, Y=Yaw, Z=Roll) (초당 도)\n"
				"기본값: (0, 0, 0)");
		}

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

	ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "빔 파티클 렌더링 설정");
	ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "빔 위치는 Source/Target 모듈 사용");

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

// ============================================================================
// Beam Source 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderBeamSourceModule(UParticleModuleBeamSource* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Beam Source", true))
	{
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "빔 시작점 설정");

		float Offset[3] = { Module->SourceOffset.X, Module->SourceOffset.Y, Module->SourceOffset.Z };
		if (ImGui::DragFloat3("Source Offset", Offset, 1.0f))
		{
			Module->SourceOffset = FVector(Offset[0], Offset[1], Offset[2]);
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Offset from emitter location (start)");
	}
}

// ============================================================================
// Beam Target 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderBeamTargetModule(UParticleModuleBeamTarget* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Beam Target", true))
	{
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "빔 끝점 설정");

		float Offset[3] = { Module->TargetOffset.X, Module->TargetOffset.Y, Module->TargetOffset.Z };
		if (ImGui::DragFloat3("Target Offset", Offset, 1.0f))
		{
			Module->TargetOffset = FVector(Offset[0], Offset[1], Offset[2]);
		}
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Offset from emitter location (end)");
	}
}

// ============================================================================
// Beam Noise 모듈 렌더링
// ============================================================================

void UParticleModuleDetailRenderer::RenderBeamNoiseModule(UParticleModuleBeamNoise* Module)
{
	if (!Module)
	{
		return;
	}

	if (BeginSection("Beam Noise", true))
	{
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "노이즈 효과");

		ImGui::Checkbox("Enabled", &Module->bEnabled);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("노이즈 효과 활성화/비활성화");
		}

		if (Module->bEnabled)
		{
			ImGui::DragFloat("Strength", &Module->Strength, 0.5f, 0.0f, 100.0f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Noise amplitude (displacement amount)");

			ImGui::DragFloat("Frequency", &Module->Frequency, 0.1f, 0.1f, 10.0f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Spatial frequency (how often noise changes)");

			ImGui::DragFloat("Speed", &Module->Speed, 0.1f, 0.0f, 10.0f);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Animation speed");

			ImGui::Checkbox("Lock Endpoints", &Module->bLockEndpoints);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Keep source/target positions fixed");
		}
	}
}
