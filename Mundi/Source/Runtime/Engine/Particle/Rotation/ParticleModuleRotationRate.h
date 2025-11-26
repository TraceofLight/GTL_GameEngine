#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"
#include "UParticleModuleRotationRate.generated.h"

/**
 * @brief 파티클의 회전 속도를 설정하는 모듈
 * @details Spawn 시 파티클의 RotationRate 값을 설정
 *          에디터에서는 Degrees/Second로 입력, 내부적으로 Radians/Second로 변환
 *
 * @param StartRotationRate 파티클 회전 속도 분포 (Degrees/Second)
 */
UCLASS()
class UParticleModuleRotationRate : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleRotationRate();
	~UParticleModuleRotationRate() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 회전 속도 분포 (Degrees/Second)
	FFloatDistribution StartRotationRate;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
