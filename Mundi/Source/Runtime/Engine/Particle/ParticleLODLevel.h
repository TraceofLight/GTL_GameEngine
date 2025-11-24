#pragma once

#include "UParticleLODLevel.generated.h"

// Forward declarations
class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

/**
 * @brief 파티클 이미터의 LOD 레벨
 * @details 특정 LOD 레벨에서 사용할 모듈들과 설정을 관리
 *
 * @param Level LOD 레벨 인덱스
 * @param bEnabled LOD 레벨 활성화 여부
 * @param RequiredModule 필수 모듈 (Material, Duration 등)
 * @param Modules 일반 모듈 배열
 * @param TypeDataModule 타입 데이터 모듈 (Sprite/Mesh)
 * @param SpawnModule 스폰 모듈
 * @param SpawnModules Spawn 시 호출할 모듈 캐시
 * @param UpdateModules Update 시 호출할 모듈 캐시
 * @param PeakActiveParticles 최대 활성 파티클 수
 */
UCLASS()
class UParticleLODLevel :
	public UObject
{
	GENERATED_REFLECTION_BODY()

public:
	int32 Level;
	bool bEnabled;
	UParticleModuleRequired* RequiredModule;
	TArray<UParticleModule*> Modules;
	UParticleModuleTypeDataBase* TypeDataModule;
	UParticleModuleSpawn* SpawnModule;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	int32 PeakActiveParticles;

	UParticleLODLevel();
	~UParticleLODLevel() override = default;

	void UpdateModuleLists();
	int32 CalculateMaxActiveParticleCount();
	int32 GetModuleIndex(UParticleModule* InModule);
	UParticleModule* GetModuleAtIndex(int32 InIndex);
	void SetLevelIndex(int32 InLevelIndex);
	bool IsModuleEditable(UParticleModule* InModule);
};
