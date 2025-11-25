#pragma once

#include "ParticleTypes.h"
#include "ParticleModule.h"

#include "UParticleModuleSpawn.generated.h"

/**
 * @brief 파티클의 생성 빈도와 수량을 제어하는 모듈
 * @details 초당 생성률, 버스트 생성 등을 관리
 *
 * @param Rate 초당 파티클 생성 개수
 * @param RateScale 생성률 스케일
 * @param BurstMethod 버스트 생성 방식
 * @param BurstList 버스트 목록
 */
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	FFloatDistribution Rate;
	FFloatDistribution RateScale;
	EParticleBurstMethod BurstMethod;
	TArray<FParticleBurst> BurstList;
	FFloatDistribution BurstScale;
	bool bProcessSpawnRate;
	bool bProcessBurstList;

	UParticleModuleSpawn();
	~UParticleModuleSpawn() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	bool GetSpawnAmount(float OldTime, float DeltaTime, int32& OutNumber, float& OutRate);
	int32 GetBurstCount(float OldTime, float NewTime, float Duration);
};
