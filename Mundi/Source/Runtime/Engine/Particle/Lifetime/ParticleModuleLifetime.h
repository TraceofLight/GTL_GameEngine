#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

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
	DECLARE_CLASS(UParticleModuleLifetime, UParticleModule)

public:
	// 파티클 수명 분포
	FFloatDistribution Lifetime;

	UParticleModuleLifetime();
	~UParticleModuleLifetime() override = default;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
};
