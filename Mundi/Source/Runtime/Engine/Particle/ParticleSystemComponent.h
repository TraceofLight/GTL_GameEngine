#pragma once

#include "Source/Runtime/Engine/Components/PrimitiveComponent.h"
#include "UParticleSystemComponent.generated.h"

// Forward declarations
class UParticleSystem;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

/**
 * Component that manages and renders a particle system
 * Handles emitter instances and their rendering data
 */
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent();

	/** Particle system template that defines the emitters and their properties */
	// 사용할 파티클의 시스템 에셋 (설계도)
	UParticleSystem* Template;

	/** Array of emitter instances, one for each emitter in the template */
	// 파티클 에셋의 에미터들
	// PSC는 USceneComponent 상속받아서 Transform 정보 있음.
	// World Matrix를 인스턴스에게 넘겨서 파티클이 원하는 월드 위치에 표시되게 함.
	TArray<FParticleEmitterInstance*> EmitterInstances;

	/** Array of render data for each emitter, used by the rendering system */
	// 렌더 스레드로 보낼 데이터 캐시
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// ============== Lifecycle ==============
	/**
	 * Initialize the component
	 * 컴포넌트 초기화 - EmitterInstance 생성 및 설정
	 */
	void InitializeComponent() override;

	/**
	 * Tick function to update particle system
	 * 매 프레임 파티클 시스템 업데이트
	 */
	void TickComponent(float DeltaTime) override;

	// ============== System Control ==============
	/**
	 * Activate the particle system
	 * 파티클 시스템 활성화
	 *
	 * @param bReset - If true, reset all emitters to initial state
	 */
	void ActivateSystem(bool bReset = false);

	/**
	 * Deactivate the particle system
	 * 파티클 시스템 비활성화
	 */
	void DeactivateSystem();

	/**
	 * Set the particle system template
	 * 파티클 시스템 템플릿 설정
	 *
	 * @param NewTemplate - New particle system template to use
	 */
	void SetTemplate(UParticleSystem* NewTemplate);

	/**
	 * Reset all particles in all emitters
	 * 모든 에미터의 파티클 리셋
	 */
	void ResetParticles();

	// ============== Emitter Management ==============
	/**
	 * Initialize emitter instances from template
	 * 템플릿으로부터 에미터 인스턴스 초기화
	 */
	void InitializeEmitters();

	/**
	 * Update all emitter instances
	 * 모든 에미터 인스턴스 업데이트
	 *
	 * @param DeltaTime - Time elapsed since last update
	 */
	void UpdateEmitters(float DeltaTime);

	// ============== State ==============
	/** Whether the particle system is currently active */
	// 파티클 시스템이 현재 활성화되어 있는지 여부
	bool bIsActive;

	/** Accumulated time for warmup */
	// 웜업을 위한 누적 시간
	float AccumulatedTime;

protected:
	/**
	 * Create emitter instances from template
	 * 템플릿으로부터 에미터 인스턴스 생성 (내부 헬퍼)
	 */
	void CreateEmitterInstances();

	/**
	 * Clear all emitter instances
	 * 모든 에미터 인스턴스 정리 (내부 헬퍼)
	 */
	void ClearEmitterInstances();
};
