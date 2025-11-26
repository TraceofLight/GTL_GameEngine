#pragma once

#include "Source/Runtime/Engine/Components/PrimitiveComponent.h"
#include "UParticleSystemComponent.generated.h"

// Forward declarations
class UParticleSystem;
class UBillboardComponent;
class UGizmoArrowComponent;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;
struct FParticleDynamicData;

/**
 * Component that manages and renders a particle system
 * Handles emitter instances and their rendering data
 */
UCLASS(DisplayName="파티클 시스템 컴포넌트", Description="파티클 효과를 재생하는 컴포넌트입니다")
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent();

	/** Particle system template that defines the emitters and their properties */
	// 사용할 파티클의 시스템 에셋 (설계도)
	UPROPERTY(EditAnywhere, Category="Particle")
	UParticleSystem* Template;

	/** Array of emitter instances, one for each emitter in the template */
	// 파티클 에셋의 에미터들
	// PSC는 USceneComponent 상속받아서 Transform 정보 있음.
	// World Matrix를 인스턴스에게 넘겨서 파티클이 원하는 월드 위치에 표시되게 함.
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// ============== Lifecycle ==============
	// Serialize/Duplicate
	void Serialize(const bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateSubObjects() override;

	/**
	 * Called when component is registered to world
	 * 월드에 등록될 때 호출 - 에디터 전용 아이콘 생성
	 */
	void OnRegister(UWorld* InWorld) override;

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

	/**
	 * EmitterInstance 업데이트 (Editor에서 프로퍼티 변경 시 호출)
	 * EmitterInstance를 재생성하여 변경사항 반영
	 *
	 * @param bEmptyInstances - true면 기존 Instance 완전히 파괴 후 재생성
	 */
	void UpdateInstances(bool bEmptyInstances = true);

	// ============== Particle System Creation ==============
	/**
	 * Create a flare particle system with random rotation, velocity, and lifetime
	 * flare0.dds 텍스처를 사용하여 랜덤한 회전, 속도, 수명을 가진 파티클 시스템 생성
	 *
	 * @return UParticleSystem* - Created particle system template
	 */
	static UParticleSystem* CreateFlareParticleSystem();

	/**
	 * Create a mesh particle system using bitten apple model
	 * 물린 사과 메시를 사용하는 메시 파티클 시스템 생성
	 *
	 * @return UParticleSystem* - Created particle system template
	 */
	static UParticleSystem* CreateAppleMeshParticleSystem();

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

	// ============== Rendering Data ==============
	/**
	 * Update dynamic render data for all emitters
	 * 모든 에미터의 동적 렌더 데이터 업데이트 (렌더쪽에서 호출)
	 *
	 * @note Called by render thread to get latest particle data
	 */
	void UpdateDynamicData();

	/**
	 * Get current dynamic data for rendering
	 * 렌더링을 위한 현재 동적 데이터 가져오기
	 *
	 * @return FParticleDynamicData* - Current render data (can be nullptr)
	 */
	FParticleDynamicData* GetCurrentDynamicData() const { return CurrentDynamicData; }

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

	FParticleDynamicData* CurrentDynamicData;

	/** Editor-only sprite component for visualization (not serialized, PIE excluded) */
	UBillboardComponent* SpriteComponent = nullptr;

	/** Editor-only direction gizmo for showing particle system direction */
	UGizmoArrowComponent* DirectionGizmo = nullptr;

	/** Update direction gizmo to match component transform */
	void UpdateDirectionGizmo();
};
