#include "pch.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleModuleRequired.h"
#include "Spawn/ParticleModuleSpawn.h"
#include "TypeData/ParticleModuleTypeDataBase.h"
#include "Lifetime/ParticleModuleLifetime.h"
#include "Velocity/ParticleModuleVelocity.h"
#include "Color/ParticleModuleColor.h"
#include "Size/ParticleModuleSize.h"
#include "Location/ParticleModuleLocation.h"
#include "Rotation/ParticleModuleRotation.h"
#include "Rotation/ParticleModuleRotationRate.h"
#include "Rotation/ParticleModuleMeshRotation.h"
#include "Rotation/ParticleModuleMeshRotationRate.h"

UParticleLODLevel::UParticleLODLevel()
	: Level(0)
	, bEnabled(true)
	, RequiredModule(nullptr)
	, TypeDataModule(nullptr)
	, SpawnModule(nullptr)
	, PeakActiveParticles(0)
{
}

void UParticleLODLevel::UpdateModuleLists()
{
	// 캐시 초기화
	SpawnModules.clear();
	UpdateModules.clear();

	// 모듈들을 순회하며 분류
	for (UParticleModule* Module : Modules)
	{
		if (!Module)
		{
			continue;
		}

		// 이 LOD 레벨에서 유효한지 확인
		if (!Module->IsValidForLODLevel(Level))
		{
			continue;
		}

		// Spawn 모듈 캐시
		if (Module->IsSpawnModule())
		{
			SpawnModules.Add(Module);
		}

		// Update 모듈 캐시
		if (Module->IsUpdateModule())
		{
			UpdateModules.Add(Module);
		}
	}
}

int32 UParticleLODLevel::CalculateMaxActiveParticleCount()
{
	if (!RequiredModule || !SpawnModule)
	{
		return 0;
	}

	// 기본 계산: 생성률 * 수명
	float Duration = RequiredModule->GetEmitterDurationValue();
	float SpawnRate = SpawnModule->Rate.Max;
	float Lifetime = 1.0f;  // 기본 수명

	// Lifetime 모듈에서 최대 수명 찾기
	for (UParticleModule* Module : Modules)
	{
		// UParticleModuleLifetime 체크 (간단히 이름으로)
		if (Module && Module->GetClass()->Name &&
			strcmp(Module->GetClass()->Name, "UParticleModuleLifetime") == 0)
		{
			// Lifetime 모듈의 최대값 사용
			// 실제로는 캐스팅해서 값을 가져와야 함
			Lifetime = 5.0f;  // 임시 값
			break;
		}
	}

	// 버스트 파티클 수 추가
	int32 BurstTotal = 0;
	for (const FParticleBurst& Burst : SpawnModule->BurstList)
	{
		BurstTotal += Burst.Count;
	}

	// 최대 파티클 수 계산
	int32 MaxCount = static_cast<int32>(SpawnRate * Lifetime) + BurstTotal;

	// 안전 마진 추가 (20%)
	MaxCount = static_cast<int32>(static_cast<float>(MaxCount) * 1.2f);

	// 최소값 보장 및 MaxDrawCount 제한
	MaxCount = std::max(10, MaxCount);

	int32 MaxDrawCount = RequiredModule->GetMaxDrawCount();
	if (MaxDrawCount > 0)
	{
		MaxCount = std::min(MaxDrawCount, MaxCount);
	}

	PeakActiveParticles = MaxCount;
	return MaxCount;
}

int32 UParticleLODLevel::GetModuleIndex(UParticleModule* InModule)
{
	for (int32 i = 0; i < static_cast<int32>(Modules.size()); ++i)
	{
		if (Modules[i] == InModule)
		{
			return i;
		}
	}
	return -1;
}

UParticleModule* UParticleLODLevel::GetModuleAtIndex(int32 InIndex)
{
	if (InIndex >= 0 && InIndex < static_cast<int32>(Modules.size()))
	{
		return Modules[InIndex];
	}
	return nullptr;
}

void UParticleLODLevel::SetLevelIndex(int32 InLevelIndex)
{
	Level = InLevelIndex;

	// 모든 모듈의 LOD 유효성 업데이트
	if (RequiredModule)
	{
		RequiredModule->SetLODValidity(Level, true);
	}

	if (SpawnModule)
	{
		SpawnModule->SetLODValidity(Level, true);
	}

	if (TypeDataModule)
	{
		TypeDataModule->SetLODValidity(Level, true);
	}

	for (UParticleModule* Module : Modules)
	{
		if (Module)
		{
			Module->SetLODValidity(Level, true);
		}
	}
}

bool UParticleLODLevel::IsModuleEditable(UParticleModule* InModule)
{
	if (!InModule)
	{
		return false;
	}

	// 이 LOD 레벨에서 유효한지 확인
	return InModule->IsValidForLODLevel(Level);
}

// 모듈 타입 이름으로 모듈 생성
static UParticleModule* CreateModuleByTypeName(const FString& TypeName)
{
	if (TypeName == "UParticleModuleRequired") return NewObject<UParticleModuleRequired>();
	if (TypeName == "UParticleModuleSpawn") return NewObject<UParticleModuleSpawn>();
	if (TypeName == "UParticleModuleLifetime") return NewObject<UParticleModuleLifetime>();
	if (TypeName == "UParticleModuleVelocity") return NewObject<UParticleModuleVelocity>();
	if (TypeName == "UParticleModuleColor") return NewObject<UParticleModuleColor>();
	if (TypeName == "UParticleModuleSize") return NewObject<UParticleModuleSize>();
	if (TypeName == "UParticleModuleLocation") return NewObject<UParticleModuleLocation>();
	if (TypeName == "UParticleModuleTypeDataSprite") return NewObject<UParticleModuleTypeDataSprite>();
	if (TypeName == "UParticleModuleTypeDataMesh") return NewObject<UParticleModuleTypeDataMesh>();
	if (TypeName == "UParticleModuleRotation") return NewObject<UParticleModuleRotation>();
	if (TypeName == "UParticleModuleRotationRate") return NewObject<UParticleModuleRotationRate>();
	if (TypeName == "UParticleModuleMeshRotation") return NewObject<UParticleModuleMeshRotation>();
	if (TypeName == "UParticleModuleMeshRotationRate") return NewObject<UParticleModuleMeshRotationRate>();
	return nullptr;
}

void UParticleLODLevel::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	if (bIsLoading)
	{
		if (InOutHandle.hasKey("Level")) Level = static_cast<int32>(InOutHandle["Level"].ToInt());
		if (InOutHandle.hasKey("bEnabled")) bEnabled = InOutHandle["bEnabled"].ToBool();
		if (InOutHandle.hasKey("PeakActiveParticles")) PeakActiveParticles = static_cast<int32>(InOutHandle["PeakActiveParticles"].ToInt());

		// RequiredModule 로드
		if (InOutHandle.hasKey("RequiredModule") && InOutHandle["RequiredModule"].JSONType() == JSON::Class::Object)
		{
			RequiredModule = NewObject<UParticleModuleRequired>();
			JSON reqJson = InOutHandle["RequiredModule"];
			RequiredModule->Serialize(true, reqJson);
		}

		// SpawnModule 로드
		if (InOutHandle.hasKey("SpawnModule") && InOutHandle["SpawnModule"].JSONType() == JSON::Class::Object)
		{
			SpawnModule = NewObject<UParticleModuleSpawn>();
			JSON spawnJson = InOutHandle["SpawnModule"];
			SpawnModule->Serialize(true, spawnJson);
		}

		// TypeDataModule 로드
		if (InOutHandle.hasKey("TypeDataModule") && InOutHandle["TypeDataModule"].JSONType() == JSON::Class::Object)
		{
			JSON typeJson = InOutHandle["TypeDataModule"];
			FString TypeName;
			if (typeJson.hasKey("ModuleType")) TypeName = typeJson["ModuleType"].ToString();

			UParticleModule* Module = CreateModuleByTypeName(TypeName);
			if (Module)
			{
				Module->Serialize(true, typeJson);
				TypeDataModule = static_cast<UParticleModuleTypeDataBase*>(Module);
			}
		}

		// Modules 배열 로드
		Modules.clear();
		if (InOutHandle.hasKey("Modules") && InOutHandle["Modules"].JSONType() == JSON::Class::Array)
		{
			for (size_t i = 0; i < InOutHandle["Modules"].size(); ++i)
			{
				JSON moduleJson = InOutHandle["Modules"][static_cast<int>(i)];
				FString TypeName;
				if (moduleJson.hasKey("ModuleType")) TypeName = moduleJson["ModuleType"].ToString();

				UParticleModule* Module = CreateModuleByTypeName(TypeName);
				if (Module)
				{
					Module->Serialize(true, moduleJson);
					Modules.Add(Module);
				}
			}
		}

		UpdateModuleLists();
	}
	else
	{
		InOutHandle["Level"] = Level;
		InOutHandle["bEnabled"] = bEnabled;
		InOutHandle["PeakActiveParticles"] = PeakActiveParticles;

		// RequiredModule 저장
		if (RequiredModule)
		{
			JSON reqJson = JSON::Make(JSON::Class::Object);
			reqJson["ModuleType"] = FString("UParticleModuleRequired");
			RequiredModule->Serialize(false, reqJson);
			InOutHandle["RequiredModule"] = reqJson;
		}

		// SpawnModule 저장
		if (SpawnModule)
		{
			JSON spawnJson = JSON::Make(JSON::Class::Object);
			spawnJson["ModuleType"] = FString("UParticleModuleSpawn");
			SpawnModule->Serialize(false, spawnJson);
			InOutHandle["SpawnModule"] = spawnJson;
		}

		// TypeDataModule 저장
		if (TypeDataModule)
		{
			JSON typeJson = JSON::Make(JSON::Class::Object);
			typeJson["ModuleType"] = FString(TypeDataModule->GetClass()->Name);
			TypeDataModule->Serialize(false, typeJson);
			InOutHandle["TypeDataModule"] = typeJson;
		}

		// Modules 배열 저장
		JSON modulesArray = JSON::Make(JSON::Class::Array);
		for (UParticleModule* Module : Modules)
		{
			if (Module)
			{
				JSON moduleJson = JSON::Make(JSON::Class::Object);
				moduleJson["ModuleType"] = FString(Module->GetClass()->Name);
				Module->Serialize(false, moduleJson);
				modulesArray.append(moduleJson);
			}
		}
		InOutHandle["Modules"] = modulesArray;
	}
}

void UParticleLODLevel::DuplicateFrom(const UParticleLODLevel* Source)
{
	if (!Source)
	{
		return;
	}

	Level = Source->Level;
	bEnabled = Source->bEnabled;
	PeakActiveParticles = Source->PeakActiveParticles;

	// RequiredModule 복제
	if (Source->RequiredModule)
	{
		RequiredModule = NewObject<UParticleModuleRequired>();
		RequiredModule->DuplicateFrom(Source->RequiredModule);
	}

	// SpawnModule 복제
	if (Source->SpawnModule)
	{
		SpawnModule = NewObject<UParticleModuleSpawn>();
		SpawnModule->DuplicateFrom(Source->SpawnModule);
	}

	// TypeDataModule 복제
	if (Source->TypeDataModule)
	{
		const char* TypeName = Source->TypeDataModule->GetClass()->Name;
		UParticleModule* NewModule = CreateModuleByTypeName(TypeName);
		if (NewModule)
		{
			NewModule->DuplicateFrom(Source->TypeDataModule);
			TypeDataModule = static_cast<UParticleModuleTypeDataBase*>(NewModule);
		}
	}

	// Modules 복제
	Modules.clear();
	for (UParticleModule* SrcModule : Source->Modules)
	{
		if (SrcModule)
		{
			const char* TypeName = SrcModule->GetClass()->Name;
			UParticleModule* NewModule = CreateModuleByTypeName(TypeName);
			if (NewModule)
			{
				NewModule->DuplicateFrom(SrcModule);
				Modules.Add(NewModule);
			}
		}
	}

	UpdateModuleLists();
}
