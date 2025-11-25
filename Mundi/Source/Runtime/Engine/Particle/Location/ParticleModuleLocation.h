#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

/**
 * @brief 파티클의 초기 위치를 결정하는 모듈
 * @details Spawn 시 파티클의 Location 값을 설정
 *
 * @param StartLocation 파티클 초기 위치 분포
 */
UCLASS()
class UParticleModuleLocation :
	public UParticleModule
{
	DECLARE_CLASS(UParticleModuleLocation, UParticleModule)

public:
	// 파티클 초기 위치 분포
	FVectorDistribution StartLocation;

	UParticleModuleLocation();
	~UParticleModuleLocation() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
};
