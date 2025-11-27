#include "pch.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleModuleRequired.h"
#include "ParticleModuleTypeDataBase.h"

UParticleEmitter::UParticleEmitter()
	: EmitterName("Emitter")
	, bDisabledLODsKeepEmitterAlive(false)
	, bCollapsed(false)
	, DetailMode(0)
	, EmitterRenderMode(EEmitterRenderMode::Normal)
	, EmitterEditorColor(1.0f, 1.0f, 1.0f, 1.0f)
	, InitialAllocationCount(0)
	, bIsSoloing(false)
	, bIsEnabled(true)
	, bWasEnabledBeforeSolo(true)
	, PeakActiveParticles(0)
	, RequiredBytes(0)
	, ReqInstanceBytes(0)
	, bRequiresLoopNotification(false)
	, bAxisLockEnabled(false)
	, LockAxisFlags(EParticleAxisLock::None)
	, TypeDataModule(nullptr)
{
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// 캐시 초기화
	PeakActiveParticles = 0;
	RequiredBytes = 0;
	ReqInstanceBytes = 0;
	bRequiresLoopNotification = false;
	bAxisLockEnabled = false;
	LockAxisFlags = EParticleAxisLock::None;
	TypeDataModule = nullptr;

	if (LODLevels.empty())
	{
		return;
	}

	// LOD 0을 기준으로 정보 캐싱
	UParticleLODLevel* LODLevel = LODLevels[0];
	if (!LODLevel)
	{
		return;
	}

	// 모듈 리스트 업데이트
	LODLevel->UpdateModuleLists();

	// TypeData 모듈 캐시
	TypeDataModule = LODLevel->TypeDataModule;

	// 최대 활성 파티클 수 계산
	PeakActiveParticles = LODLevel->CalculateMaxActiveParticleCount();

	// 필요 바이트 계산
	for (UParticleModule* Module : LODLevel->Modules)
	{
		if (Module)
		{
			RequiredBytes += Module->RequiredBytes(TypeDataModule);
			ReqInstanceBytes += Module->RequiredBytesPerInstance();
		}
	}

	// Required 모듈에서 추가 정보 캐싱
	if (LODLevel->RequiredModule)
	{
		// 루프 알림 필요 여부 (Duration이 0이 아닌 경우)
		if (LODLevel->RequiredModule->GetEmitterDurationValue() > 0.0f)
		{
			bRequiresLoopNotification = true;
		}

		// 축 고정 정보
		EParticleAxisLock AxisLock = LODLevel->RequiredModule->GetAxisLockOption();
		if (AxisLock != EParticleAxisLock::None)
		{
			bAxisLockEnabled = true;
			LockAxisFlags = AxisLock;
		}
	}

	// 모든 LOD 레벨의 PeakActiveParticles 계산
	for (int32 i = 1; i < static_cast<int32>(LODLevels.size()); ++i)
	{
		if (LODLevels[i])
		{
			LODLevels[i]->UpdateModuleLists();
			int32 LODPeak = LODLevels[i]->CalculateMaxActiveParticleCount();
			PeakActiveParticles = std::max(PeakActiveParticles, LODPeak);
		}
	}
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex)
{
	if (LODIndex >= 0 && LODIndex < static_cast<int32>(LODLevels.size()))
	{
		return LODLevels[LODIndex];
	}
	return nullptr;
}

void UParticleEmitter::SetEmitterName(const FString& InName)
{
	EmitterName = InName;
}

bool UParticleEmitter::AutogenerateLowestLODLevel(bool bDuplicateHighest)
{
	// 최소 1개의 LOD 레벨이 필요
	if (LODLevels.empty())
	{
		return false;
	}

	// 이미 여러 LOD 레벨이 있으면 생성하지 않음
	if (LODLevels.size() > 1)
	{
		return false;
	}

	// LOD 0을 기반으로 새 LOD 레벨 생성
	UParticleLODLevel* SourceLOD = LODLevels[0];
	if (!SourceLOD)
	{
		return false;
	}

	// bDuplicateHighest가 true면 100% 복제, false면 간소화 (UE: 10%)
	// 현재 간소화 로직이 없으므로 두 경우 모두 복제로 동작
	// TODO: 간소화 로직 구현 시 Percentage 사용

	// 새 LOD 레벨 생성
	UParticleLODLevel* NewLOD = new UParticleLODLevel();
	NewLOD->Level = static_cast<int32>(LODLevels.size());
	NewLOD->bEnabled = SourceLOD->bEnabled;

	// 필수 모듈 복사 (같은 인스턴스 공유)
	NewLOD->RequiredModule = SourceLOD->RequiredModule;
	NewLOD->SpawnModule = SourceLOD->SpawnModule;
	NewLOD->TypeDataModule = SourceLOD->TypeDataModule;

	// 모듈 복사
	for (UParticleModule* Module : SourceLOD->Modules)
	{
		if (Module)
		{
			NewLOD->Modules.Add(Module);
		}
	}

	NewLOD->SetLevelIndex(static_cast<int32>(LODLevels.size()));
	LODLevels.Add(NewLOD);

	return true;
}

void UParticleEmitter::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	if (bIsLoading)
	{
		if (InOutHandle.hasKey("EmitterName")) EmitterName = InOutHandle["EmitterName"].ToString();
		if (InOutHandle.hasKey("bDisabledLODsKeepEmitterAlive")) bDisabledLODsKeepEmitterAlive = InOutHandle["bDisabledLODsKeepEmitterAlive"].ToBool();
		if (InOutHandle.hasKey("bCollapsed")) bCollapsed = InOutHandle["bCollapsed"].ToBool();
		if (InOutHandle.hasKey("DetailMode")) DetailMode = static_cast<int32>(InOutHandle["DetailMode"].ToInt());
		if (InOutHandle.hasKey("EmitterRenderMode")) EmitterRenderMode = static_cast<EEmitterRenderMode>(InOutHandle["EmitterRenderMode"].ToInt());
		if (InOutHandle.hasKey("InitialAllocationCount")) InitialAllocationCount = static_cast<int32>(InOutHandle["InitialAllocationCount"].ToInt());
		if (InOutHandle.hasKey("bIsSoloing")) bIsSoloing = InOutHandle["bIsSoloing"].ToBool();
		if (InOutHandle.hasKey("bIsEnabled")) bIsEnabled = InOutHandle["bIsEnabled"].ToBool();
		if (InOutHandle.hasKey("ThumbnailData")) ThumbnailData = InOutHandle["ThumbnailData"].ToString();

		// EmitterEditorColor
		if (InOutHandle.hasKey("EmitterEditorColor") && InOutHandle["EmitterEditorColor"].JSONType() == JSON::Class::Array && InOutHandle["EmitterEditorColor"].size() == 4)
		{
			EmitterEditorColor.R = static_cast<float>(InOutHandle["EmitterEditorColor"][0].ToFloat());
			EmitterEditorColor.G = static_cast<float>(InOutHandle["EmitterEditorColor"][1].ToFloat());
			EmitterEditorColor.B = static_cast<float>(InOutHandle["EmitterEditorColor"][2].ToFloat());
			EmitterEditorColor.A = static_cast<float>(InOutHandle["EmitterEditorColor"][3].ToFloat());
		}

		// LODLevels 로드
		LODLevels.clear();
		if (InOutHandle.hasKey("LODLevels") && InOutHandle["LODLevels"].JSONType() == JSON::Class::Array)
		{
			for (size_t i = 0; i < InOutHandle["LODLevels"].size(); ++i)
			{
				JSON lodJson = InOutHandle["LODLevels"][static_cast<int>(i)];
				UParticleLODLevel* NewLOD = NewObject<UParticleLODLevel>();
				NewLOD->Serialize(true, lodJson);
				LODLevels.Add(NewLOD);
			}
		}

		CacheEmitterModuleInfo();
	}
	else
	{
		InOutHandle["EmitterName"] = EmitterName;
		InOutHandle["bDisabledLODsKeepEmitterAlive"] = bDisabledLODsKeepEmitterAlive;
		InOutHandle["bCollapsed"] = bCollapsed;
		InOutHandle["DetailMode"] = DetailMode;
		InOutHandle["EmitterRenderMode"] = static_cast<int32>(EmitterRenderMode);
		InOutHandle["InitialAllocationCount"] = InitialAllocationCount;
		InOutHandle["bIsSoloing"] = bIsSoloing;
		InOutHandle["bIsEnabled"] = bIsEnabled;
		InOutHandle["ThumbnailData"] = ThumbnailData;

		// EmitterEditorColor
		JSON colorArray = JSON::Make(JSON::Class::Array);
		colorArray.append(EmitterEditorColor.R, EmitterEditorColor.G, EmitterEditorColor.B, EmitterEditorColor.A);
		InOutHandle["EmitterEditorColor"] = colorArray;

		// LODLevels 저장
		JSON lodArray = JSON::Make(JSON::Class::Array);
		for (UParticleLODLevel* LOD : LODLevels)
		{
			if (LOD)
			{
				JSON lodJson = JSON::Make(JSON::Class::Object);
				LOD->Serialize(false, lodJson);
				lodArray.append(lodJson);
			}
		}
		InOutHandle["LODLevels"] = lodArray;
	}
}

void UParticleEmitter::DuplicateFrom(const UParticleEmitter* Source)
{
	if (!Source)
	{
		return;
	}

	EmitterName = Source->EmitterName;
	bDisabledLODsKeepEmitterAlive = Source->bDisabledLODsKeepEmitterAlive;
	bCollapsed = Source->bCollapsed;
	DetailMode = Source->DetailMode;
	EmitterRenderMode = Source->EmitterRenderMode;
	EmitterEditorColor = Source->EmitterEditorColor;
	InitialAllocationCount = Source->InitialAllocationCount;
	bIsSoloing = Source->bIsSoloing;
	bIsEnabled = Source->bIsEnabled;
	bWasEnabledBeforeSolo = Source->bWasEnabledBeforeSolo;
	ThumbnailData = Source->ThumbnailData;

	// LODLevels 복제
	LODLevels.clear();
	for (UParticleLODLevel* SrcLOD : Source->LODLevels)
	{
		if (SrcLOD)
		{
			UParticleLODLevel* NewLOD = NewObject<UParticleLODLevel>();
			NewLOD->DuplicateFrom(SrcLOD);
			LODLevels.Add(NewLOD);
		}
	}

	CacheEmitterModuleInfo();
}
