#pragma once
#include "ParticleModule.h"

/**
 * @brief 빔 노이즈 모듈
 * @details 빔에 번개 같은 지지직거리는 효과를 주는 모듈
 *
 * @param bEnabled 노이즈 활성화 여부
 * @param Strength 노이즈 강도 (흔들림 크기)
 * @param Frequency 노이즈 주파수 (공간적 밀도)
 * @param Speed 애니메이션 속도
 * @param bLockEndpoints 시작점/끝점 고정 여부
 */
UCLASS()
class UParticleModuleBeamNoise : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleBeamNoise, UParticleModule)

public:
	/** 노이즈 활성화 */
	bool bEnabled;

	/** 노이즈 강도 (흔들림 크기) */
	float Strength;

	/** 노이즈 주파수 (공간적 밀도) */
	float Frequency;

	/** 애니메이션 속도 */
	float Speed;

	/** 시작점/끝점 고정 (기본: true) */
	bool bLockEndpoints;

	UParticleModuleBeamNoise();
	~UParticleModuleBeamNoise() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;
};
