#include "pch.h"
#include "ParticleModuleRequired.h"

#include "ParticleTypes.h"

UParticleModuleRequired::UParticleModuleRequired()
	: Material(nullptr)
	, EmitterDuration(1.0f)
	, EmitterDurationLow(0.0f)
	, EmitterLoops(0)
	, bDurationRecalcEachLoop(false)
	, ScreenAlignment(EParticleScreenAlignment::Square)
	, SubImages_Horizontal(1)
	, SubImages_Vertical(1)
	, SortMode(EParticleSortMode::None)
	, bUseLocalSpace(false)
	, bKillOnDeactivate(false)
	, bKillOnCompleted(false)
	, MaxDrawCount(500)
	, EmitterNormalsMode(EEmitterNormalsMode::CameraFacing)
	, InterpolationMethod(EParticleSubUVInterpMethod::None)
	, AxisLockOption(EParticleAxisLock::None)
{
	// Required 모듈은 Spawn/Update에 참여하지 않음 (설정만 제공)
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

/**
 * 실제 이미터 지속 시간 반환 (랜덤 범위 적용)
 * @return 이미터 지속 시간
 */
float UParticleModuleRequired::GetEmitterDuration() const
{
	if (EmitterDurationLow > 0.0f && EmitterDurationLow < EmitterDuration)
	{
		// 랜덤 범위
		float Alpha = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return EmitterDurationLow + Alpha * (EmitterDuration - EmitterDurationLow);
	}
	return EmitterDuration;
}

/**
 * 총 SubUV 이미지 수 반환
 * @return SubImages_Horizontal * SubImages_Vertical
 */
int32 UParticleModuleRequired::GetTotalSubImages() const
{
	return SubImages_Horizontal * SubImages_Vertical;
}
