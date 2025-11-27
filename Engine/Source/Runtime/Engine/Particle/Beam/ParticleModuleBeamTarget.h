#pragma once
#include "ParticleModule.h"

/**
 * @brief 빔 타겟(끝점) 모듈
 * @details 빔의 끝 위치를 설정하는 모듈
 *
 * @param TargetOffset 타겟 위치 오프셋 (에미터 기준)
 */
UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleBeamTarget, UParticleModule)

public:
	/** 타겟 위치 오프셋 (에미터 기준) */
	FVector TargetOffset;

	UParticleModuleBeamTarget();
	~UParticleModuleBeamTarget() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;
};
