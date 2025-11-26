#pragma once
#include "ParticleEmitterInstance.h"
#include "Particle.h"

// Forward declarations
class UParticleModuleTypeDataBeam;
class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;
struct FDynamicEmitterDataBase;
struct FDynamicEmitterReplayDataBase;

/**
 * 빔 파티클 에미터 인스턴스
 *
 * @note Source에서 Target까지 직선 빔을 렌더링
 * @note 레이저, 번개 등의 효과에 사용
 */
struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
	// ============== 멤버 변수 ==============

	/** TypeData 모듈 참조 (빔 렌더링 설정) */
	UParticleModuleTypeDataBeam* BeamTypeData;

	/** 빔 소스 모듈 참조 (시작점 설정) */
	UParticleModuleBeamSource* BeamSourceModule;

	/** 빔 타겟 모듈 참조 (끝점 설정) */
	UParticleModuleBeamTarget* BeamTargetModule;

	/** 빔 노이즈 모듈 참조 (번개 효과) */
	UParticleModuleBeamNoise* BeamNoiseModule;

	/** 빔 포인트 배열 (Source에서 Target까지) */
	TArray<FBeamPoint> BeamPoints;

	/** GPU 버퍼 */
	ID3D11Buffer* BeamVertexBuffer;
	ID3D11Buffer* BeamIndexBuffer;

	/** 노이즈 애니메이션용 타이머 */
	float NoiseTime;

	// ============== 생성자/소멸자 ==============

	FParticleBeamEmitterInstance();
	~FParticleBeamEmitterInstance() override;

	// ============== 핵심 오버라이드 ==============

	/**
	 * 에미터 인스턴스 초기화
	 */
	virtual void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;

	/**
	 * 파티클 Tick (빔은 위치 고정이므로 물리 시뮬레이션 없음)
	 */
	void Tick(float DeltaTime);

	/**
	 * 렌더링을 위한 동적 데이터가 필요한지 체크
	 */
	virtual bool IsDynamicDataRequired() const override;

	/**
	 * 렌더링용 동적 데이터 가져오기
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;

	/**
	 * 리플레이 데이터 가져오기
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData() override;

	/**
	 * 할당된 메모리 크기 반환
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax) override;

	// ============== 빔 전용 메서드 ==============

	/**
	 * 빔 포인트 계산 (Source에서 Target까지 선형 보간)
	 */
	void CalculateBeamPoints();

	/**
	 * 월드 공간 Source 위치 반환
	 */
	FVector GetSourcePosition() const;

	/**
	 * 월드 공간 Target 위치 반환
	 */
	FVector GetTargetPosition() const;

	/**
	 * 빔 포인트에 노이즈 적용 (번개 효과)
	 */
	void ApplyNoiseToBeamPoints();

protected:
	/**
	 * 리플레이 데이터 채우기
	 */
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
