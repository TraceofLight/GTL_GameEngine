#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"
#include "UParticleModuleSize.generated.h"

/**
 * @brief 파티클의 크기를 설정하는 모듈
 * @details Spawn 시 파티클의 Size 값을 설정
 *
 * @param StartSize 파티클 초기 크기 분포
 */
UCLASS()
class UParticleModuleSize : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleSize();
	~UParticleModuleSize() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 초기 크기 분포
	FVectorDistribution StartSize;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
