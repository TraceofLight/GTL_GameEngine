#pragma once
#include "ParticleModule.h"

/**
 * @brief 빔 소스(시작점) 모듈
 * @details 빔의 시작 위치를 설정하는 모듈
 *
 * @param SourceOffset 소스 위치 오프셋 (에미터 기준)
 */
UCLASS()
class UParticleModuleBeamSource : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleBeamSource, UParticleModule)

public:
	/** 소스 위치 오프셋 (에미터 기준) */
	FVector SourceOffset;

	UParticleModuleBeamSource();
	~UParticleModuleBeamSource() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;
};
