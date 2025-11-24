#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

#include "UParticleModuleColor.generated.h"

/**
 * @brief 파티클의 색상을 정의하는 모듈
 * @details Spawn 시 파티클의 Color 값을 설정
 *
 * @param StartColor 파티클 초기 색상 분포
 * @param StartAlpha 파티클 초기 알파 분포
 * @param bClampAlpha 알파 값을 0~1로 클램프할지 여부
 */
UCLASS()
class UParticleModuleColor : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	FColorDistribution StartColor;
	FFloatDistribution StartAlpha;
	bool bClampAlpha;

	UParticleModuleColor();
	~UParticleModuleColor() override = default;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
};
