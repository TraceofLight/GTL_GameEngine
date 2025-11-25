#pragma once
#include "ParticleEmitterInstance.h"
#include "DynamicEmitterDataBase.h"

/**
 * Sprite particle emitter instance
 * 스프라이트 파티클 이미터 인스턴스
 *
 * @note This is the most common particle type (billboard sprites)
 * @note Inherits all base particle management from FParticleEmitterInstance
 */
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	/**
	 * Constructor
	 */
	FParticleSpriteEmitterInstance();

	/**
	 * Destructor
	 */
	virtual ~FParticleSpriteEmitterInstance();

	virtual void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;

	/**
	 * Retrieves the dynamic data for the emitter (render thread data)
	 * 에미터의 동적 데이터를 가져옴 (렌더 스레드용 데이터)
	 *
	 * @param bSelected - Whether the emitter is selected in the editor
	 * @return FDynamicEmitterDataBase* - The dynamic sprite emitter data, or nullptr if not required
	 *
	 * @note Caller is responsible for deleting the returned pointer
	 * @note Creates FDynamicSpriteEmitterData with all rendering information
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;


	/**
	 * Retrieves replay data for the emitter
	 * 에미터의 리플레이 데이터를 가져옴
	 *
	 * @return FDynamicEmitterReplayDataBase* - The replay data, or nullptr if not available
	 *
	 * @note Caller is responsible for deleting the returned pointer
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData() override;

	/**
	 * Retrieve the allocated size of this instance
	 * 이 인스턴스가 할당한 메모리 크기 반환
	 *
	 * @param OutNum - The size of this instance (currently used)
	 * @param OutMax - The maximum size of this instance (allocated)
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax) override;

protected:
	/**
	 * Fill replay data with sprite-specific particle information
	 * 스프라이트 전용 파티클 정보로 리플레이 데이터를 채움
	 *
	 * @param OutData - Output replay data to fill
	 * @return true if successful, false if no data to fill
	 *
	 * @note Calls parent FParticleEmitterInstance::FillReplayData first!
	 * @note Then adds sprite-specific data (material, SubImages, alignment, etc.)
	 */
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
