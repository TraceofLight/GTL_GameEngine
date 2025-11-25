#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

#include "UParticleModuleVelocity.generated.h"

/**
 * @brief 파티클의 초기 속도와 방향을 설정하는 모듈
 * @details Spawn 시 파티클의 Velocity 및 BaseVelocity 값을 설정
 *
 * @param StartVelocity 파티클 초기 속도 분포
 * @param StartVelocityRadial 방사형 초기 속도 분포
 */
UCLASS()
class UParticleModuleVelocity :
	public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleVelocity();
	~UParticleModuleVelocity() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 초기 속도 분포
	FVectorDistribution StartVelocity;

	// 방사형 초기 속도 분포 (중심에서 바깥으로)
	FFloatDistribution StartVelocityRadial;

	// 속도에 위치 기반 방향 적용 여부
	bool bInWorldSpace;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
};
