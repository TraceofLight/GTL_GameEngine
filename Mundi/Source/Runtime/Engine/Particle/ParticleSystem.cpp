#include "pch.h"
#include "ParticleSystem.h"

#include "ObjectIterator.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "ParticleSystemComponent.h"
#include "TypeData/ParticleModuleTypeDataBase.h"
#include "JsonSerializer.h"

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

void UParticleSystem::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	if (bIsLoading)
	{
		if (InOutHandle.hasKey("UpdateTime_FPS")) UpdateTime_FPS = static_cast<float>(InOutHandle["UpdateTime_FPS"].ToFloat());
		if (InOutHandle.hasKey("UpdateTime_Delta")) UpdateTime_Delta = static_cast<float>(InOutHandle["UpdateTime_Delta"].ToFloat());
		if (InOutHandle.hasKey("WarmupTime")) WarmupTime = static_cast<float>(InOutHandle["WarmupTime"].ToFloat());
		if (InOutHandle.hasKey("WarmupTickRate")) WarmupTickRate = static_cast<int32>(InOutHandle["WarmupTickRate"].ToInt());
		if (InOutHandle.hasKey("bUseFixedRelativeBoundingBox")) bUseFixedRelativeBoundingBox = InOutHandle["bUseFixedRelativeBoundingBox"].ToBool();
		FJsonSerializer::ReadVector(InOutHandle, "FixedRelativeBoundingBox", FixedRelativeBoundingBox, FVector::Zero(), false);
		if (InOutHandle.hasKey("LODDistanceCheckTime")) LODDistanceCheckTime = static_cast<float>(InOutHandle["LODDistanceCheckTime"].ToFloat());
		if (InOutHandle.hasKey("LODMethod")) LODMethod = static_cast<EParticleSystemLODMethod>(InOutHandle["LODMethod"].ToInt());
		if (InOutHandle.hasKey("bRegenerateLODDuplicate")) bRegenerateLODDuplicate = InOutHandle["bRegenerateLODDuplicate"].ToBool();
		if (InOutHandle.hasKey("SystemUpdateMode")) SystemUpdateMode = static_cast<EParticleSystemUpdateMode>(InOutHandle["SystemUpdateMode"].ToInt());
		if (InOutHandle.hasKey("bOrientZAxisTowardCamera")) bOrientZAxisTowardCamera = InOutHandle["bOrientZAxisTowardCamera"].ToBool();
		if (InOutHandle.hasKey("SecondsBeforeInactive")) SecondsBeforeInactive = static_cast<float>(InOutHandle["SecondsBeforeInactive"].ToFloat());
		if (InOutHandle.hasKey("Delay")) Delay = static_cast<float>(InOutHandle["Delay"].ToFloat());
		if (InOutHandle.hasKey("bAutoDeactivate")) bAutoDeactivate = InOutHandle["bAutoDeactivate"].ToBool();

		// LODDistances
		LODDistances.clear();
		if (InOutHandle.hasKey("LODDistances") && InOutHandle["LODDistances"].JSONType() == JSON::Class::Array)
		{
			for (size_t i = 0; i < InOutHandle["LODDistances"].size(); ++i)
			{
				LODDistances.push_back(static_cast<float>(InOutHandle["LODDistances"][static_cast<int>(i)].ToFloat()));
			}
		}

		// Emitters 로드
		Emitters.clear();
		if (InOutHandle.hasKey("Emitters") && InOutHandle["Emitters"].JSONType() == JSON::Class::Array)
		{
			for (size_t i = 0; i < InOutHandle["Emitters"].size(); ++i)
			{
				JSON emitterJson = InOutHandle["Emitters"][static_cast<int>(i)];
				UParticleEmitter* NewEmitter = NewObject<UParticleEmitter>();
				NewEmitter->Serialize(true, emitterJson);
				Emitters.Add(NewEmitter);
			}
		}

		UpdateAllModuleLists();
	}
	else
	{
		InOutHandle["UpdateTime_FPS"] = UpdateTime_FPS;
		InOutHandle["UpdateTime_Delta"] = UpdateTime_Delta;
		InOutHandle["WarmupTime"] = WarmupTime;
		InOutHandle["WarmupTickRate"] = WarmupTickRate;
		InOutHandle["bUseFixedRelativeBoundingBox"] = bUseFixedRelativeBoundingBox;
		InOutHandle["FixedRelativeBoundingBox"] = FJsonSerializer::VectorToJson(FixedRelativeBoundingBox);
		InOutHandle["LODDistanceCheckTime"] = LODDistanceCheckTime;
		InOutHandle["LODMethod"] = static_cast<int32>(LODMethod);
		InOutHandle["bRegenerateLODDuplicate"] = bRegenerateLODDuplicate;
		InOutHandle["SystemUpdateMode"] = static_cast<int32>(SystemUpdateMode);
		InOutHandle["bOrientZAxisTowardCamera"] = bOrientZAxisTowardCamera;
		InOutHandle["SecondsBeforeInactive"] = SecondsBeforeInactive;
		InOutHandle["Delay"] = Delay;
		InOutHandle["bAutoDeactivate"] = bAutoDeactivate;

		// LODDistances
		JSON lodDistArray = JSON::Make(JSON::Class::Array);
		for (float Dist : LODDistances)
		{
			lodDistArray.append(Dist);
		}
		InOutHandle["LODDistances"] = lodDistArray;

		// Emitters 저장
		JSON emittersArray = JSON::Make(JSON::Class::Array);
		for (UParticleEmitter* Emitter : Emitters)
		{
			if (Emitter)
			{
				JSON emitterJson = JSON::Make(JSON::Class::Object);
				Emitter->Serialize(false, emitterJson);
				emittersArray.append(emitterJson);
			}
		}
		InOutHandle["Emitters"] = emittersArray;
	}
}

void UParticleSystem::DuplicateFrom(const UParticleSystem* Source)
{
	if (!Source)
	{
		return;
	}

	UpdateTime_FPS = Source->UpdateTime_FPS;
	UpdateTime_Delta = Source->UpdateTime_Delta;
	WarmupTime = Source->WarmupTime;
	WarmupTickRate = Source->WarmupTickRate;
	bUseFixedRelativeBoundingBox = Source->bUseFixedRelativeBoundingBox;
	FixedRelativeBoundingBox = Source->FixedRelativeBoundingBox;
	LODDistanceCheckTime = Source->LODDistanceCheckTime;
	LODMethod = Source->LODMethod;
	LODDistances = Source->LODDistances;
	bRegenerateLODDuplicate = Source->bRegenerateLODDuplicate;
	SystemUpdateMode = Source->SystemUpdateMode;
	bOrientZAxisTowardCamera = Source->bOrientZAxisTowardCamera;
	SecondsBeforeInactive = Source->SecondsBeforeInactive;
	Delay = Source->Delay;
	bAutoDeactivate = Source->bAutoDeactivate;

	// Emitters 복제
	Emitters.clear();
	for (UParticleEmitter* SrcEmitter : Source->Emitters)
	{
		if (SrcEmitter)
		{
			UParticleEmitter* NewEmitter = NewObject<UParticleEmitter>();
			NewEmitter->DuplicateFrom(SrcEmitter);
			Emitters.Add(NewEmitter);
		}
	}

	UpdateAllModuleLists();
}

bool UParticleSystem::SaveToFile(const FString& InFilePath)
{
	JSON json = JSON::Make(JSON::Class::Object);
	json["FileType"] = FString("ParticleSystem");
	json["Version"] = 1;

	Serialize(false, json);

	FWideString WidePath(InFilePath.begin(), InFilePath.end());
	if (FJsonSerializer::SaveJsonToFile(json, WidePath))
	{
		SetFilePath(InFilePath);
		return true;
	}
	return false;
}

void UParticleSystem::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	// ResourceManager 패턴 호환용 Load
	// InDevice는 파티클 시스템에서 사용하지 않음
	LoadFromFileInternal(InFilePath);
}

bool UParticleSystem::LoadFromFileInternal(const FString& InFilePath)
{
	FWideString WidePath(InFilePath.begin(), InFilePath.end());
	JSON json;
	if (!FJsonSerializer::LoadJsonFromFile(json, WidePath))
	{
		UE_LOG("ParticleSystem: Failed to load file: %s", InFilePath.c_str());
		return false;
	}

	// 파일 타입 확인
	if (!json.hasKey("FileType") || json["FileType"].ToString() != "ParticleSystem")
	{
		UE_LOG("ParticleSystem: Invalid file type: %s", InFilePath.c_str());
		return false;
	}

	Serialize(true, json);
	return true;
}
