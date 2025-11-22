#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

/**
 * @brief 파티클의 크기를 설정하는 모듈
 * @details Spawn 시 파티클의 Size 값을 설정
 *
 * @param StartSize 파티클 초기 크기 분포
 */
UCLASS()
class UParticleModuleSize : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSize, UParticleModule)

public:
	UParticleModuleSize();
	~UParticleModuleSize() override = default;

	// 파티클 초기 크기 분포
	FVectorDistribution StartSize;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
};
