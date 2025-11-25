#include "pch.h"
#include "ParticleSystem.h"

#include "ObjectIterator.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "ParticleSystemComponent.h"
#include "TypeData/ParticleModuleTypeDataBase.h"

UParticleSystem::UParticleSystem()
	: UpdateTime_FPS(0.0f)
	, UpdateTime_Delta(0.0f)
	, WarmupTime(0.0f)
	, WarmupTickRate(0)
	, bUseFixedRelativeBoundingBox(false)
	, FixedRelativeBoundingBox(FVector::Zero())
	, LODDistanceCheckTime(0.25f)
	, LODMethod(EParticleSystemLODMethod::Automatic)
	, bRegenerateLODDuplicate(false)
	, SystemUpdateMode(EParticleSystemUpdateMode::RealTime)
	, bOrientZAxisTowardCamera(false)
	, SecondsBeforeInactive(1.0f)
	, Delay(0.0f)
	, bAutoDeactivate(true)
	, bHasGPUEmitter(false)
	, MaxDuration(0.0f)
{
}

void UParticleSystem::UpdateAllModuleLists()
{
	// 캐시 초기화
	bHasGPUEmitter = false;
	MaxDuration = 0.0f;

	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		// 각 이미터의 모듈 정보 캐싱
		Emitter->CacheEmitterModuleInfo();

		// GPU 이미터 확인
		UParticleModuleTypeDataBase* TypeData = Emitter->GetTypeDataModule();
		if (TypeData && TypeData->IsGPUSprites())
		{
			bHasGPUEmitter = true;
		}

		// 최대 Duration 계산
		UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
		if (LODLevel && LODLevel->RequiredModule)
		{
			float EmitterDuration = LODLevel->RequiredModule->GetEmitterDurationValue();
			int32 EmitterLoops = LODLevel->RequiredModule->GetEmitterLoops();

			// 루프가 0이면 무한 (매우 큰 값 사용)
			if (EmitterLoops == 0)
			{
				MaxDuration = FLT_MAX;
			}
			else
			{
				float TotalDuration = EmitterDuration * static_cast<float>(EmitterLoops);
				MaxDuration = std::max(MaxDuration, TotalDuration);
			}
		}
	}
}

bool UParticleSystem::ContainsEmitterType(EDynamicEmitterType EmitterType)
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		UParticleModuleTypeDataBase* TypeData = Emitter->GetTypeDataModule();
		if (!TypeData)
		{
			// TypeData가 없으면 기본 Sprite
			if (EmitterType == EDynamicEmitterType::Sprite)
			{
				return true;
			}
		}
		else
		{
			// TypeData 클래스 이름으로 타입 확인
			const char* ClassName = TypeData->GetClass()->Name;
			if (ClassName)
			{
				if (EmitterType == EDynamicEmitterType::Sprite &&
					strcmp(ClassName, "UParticleModuleTypeDataSprite") == 0)
				{
					return true;
				}
				else if (EmitterType == EDynamicEmitterType::Mesh &&
					strcmp(ClassName, "UParticleModuleTypeDataMesh") == 0)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void UParticleSystem::BuildEmitters()
{
	// 모든 이미터 초기화 및 모듈 정보 캐싱
	UpdateAllModuleLists();

	// LOD 거리 배열 초기화 (필요한 경우)
	if (LODDistances.empty())
	{
		// 기본 LOD 거리 설정
		int32 NumLODs = GetNumLODs();
		LODDistances.resize(NumLODs);
		for (int32 i = 0; i < NumLODs; ++i)
		{
			LODDistances[i] = 1000.0f * static_cast<float>(i + 1);
		}
	}
}

void UParticleSystem::SetupSoloing()
{
	// 솔로 모드 설정
	// 하나라도 솔로인 경우, 솔로가 아닌 이미터는 비활성화
	bool bHasSolo = false;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter && Emitter->bIsSoloing)
		{
			bHasSolo = true;
			break;
		}
	}

	if (bHasSolo)
	{
		// 솔로인 이미터만 활성화
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (!Emitter)
			{
				continue;
			}

			for (int32 LODIndex = 0; LODIndex < Emitter->GetNumLODs(); ++LODIndex)
			{
				UParticleLODLevel* LODLevel = Emitter->GetLODLevel(LODIndex);
				if (LODLevel)
				{
					LODLevel->bEnabled = Emitter->bIsSoloing;
				}
			}
		}
	}
}

int32 UParticleSystem::GetNumLODs() const
{
	int32 MaxLODs = 0;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			MaxLODs = std::max(MaxLODs, Emitter->GetNumLODs());
		}
	}
	return MaxLODs;
}

float UParticleSystem::GetLODDistance(int32 LODLevelIndex) const
{
	if (LODLevelIndex >= 0 && LODLevelIndex < static_cast<int32>(LODDistances.size()))
	{
		return LODDistances[LODLevelIndex];
	}
	return 0.0f;
}

UParticleEmitter* UParticleSystem::GetEmitter(int32 Index)
{
	if (Index >= 0 && Index < static_cast<int32>(Emitters.size()))
	{
		return Emitters[Index];
	}
	return nullptr;
}

/**
 * 모듈 프로퍼티가 변경되었을 때 호출
 * 이 Template을 사용하는 모든 PSC에 UpdateInstances 호출
 */
void UParticleSystem::OnModuleChanged()
{
	// TObjectIterator로 모든 PSC를 순회하여 이 Template을 사용하는 것 탐색
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* PSC = *It;
		if (PSC && PSC->Template == this)
		{
			PSC->UpdateInstances(true);
		}
	}
}
