#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

#include "UParticleModuleLifetime.generated.h"

/**
 * @brief 파티클의 수명을 설정하는 모듈
 * @details Spawn 시 파티클의 Lifetime 값을 설정
 *
 * @param Lifetime 파티클 수명 분포 (Min~Max)
 */
UCLASS()
class UParticleModuleLifetime :
	public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	// 파티클 수명 분포
	FFloatDistribution Lifetime;

	UParticleModuleLifetime();
	~UParticleModuleLifetime() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
