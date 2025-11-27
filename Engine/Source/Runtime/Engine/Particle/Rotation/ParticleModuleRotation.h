#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"
#include "UParticleModuleRotation.generated.h"

/**
 * @brief 파티클의 초기 회전값을 설정하는 모듈
 * @details Spawn 시 파티클의 Rotation 값을 설정
 *          에디터에서는 Degrees로 입력, 내부적으로 Radians로 변환
 *
 * @param StartRotation 파티클 초기 회전 분포 (Degrees)
 */
UCLASS()
class UParticleModuleRotation : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleRotation();
	~UParticleModuleRotation() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 초기 회전 분포 (Degrees)
	FFloatDistribution StartRotation;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
