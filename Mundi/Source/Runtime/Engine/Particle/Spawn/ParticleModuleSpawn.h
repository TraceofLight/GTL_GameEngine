#pragma once

#include "ParticleTypes.h"
#include "ParticleModule.h"

#include "UParticleModuleSpawn.generated.h"

/**
 * @brief 파티클의 생성 빈도와 수량을 제어하는 모듈
 * @details 초당 생성률(Rate) 기반 연속 생성과 버스트(Burst) 기반 대량 생성을 관리
 *
 * [Rate 기반 생성]
 * - Rate: 초당 생성할 파티클 개수 (예: 30이면 초당 30개)
 * - RateScale: Rate에 곱해지는 스케일 값 (예: 0.5면 절반 속도)
 * - SpawnFraction을 통해 소수점 누적 → 부드러운 생성
 *
 * [Burst 기반 생성]
 * - BurstList: 특정 시간에 대량 생성할 파티클 정보 배열
 * - BurstScale: 버스트 개수에 곱해지는 스케일 값
 * - BurstMethod: 버스트 생성 방식 (Instant/Interpolated)
 */
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	// ==================== Rate 기반 생성 설정 ====================

	/** 초당 파티클 생성 개수 (Distribution으로 Min~Max 범위 설정 가능) */
	FFloatDistribution Rate;

	/** Rate에 곱해지는 스케일 값 (1.0 = 100%, 0.5 = 50%) */
	FFloatDistribution RateScale;

	/** Rate 기반 생성 활성화 여부 (false면 Rate 무시) */
	bool bProcessSpawnRate;

	// ==================== Burst 기반 생성 설정 ====================

	/** 버스트 생성 방식 (Instant: 한꺼번에, Interpolated: 프레임 내 분산) */
	EParticleBurstMethod BurstMethod;

	/** 버스트 목록 - 각 항목은 (시간, 개수, 최소개수) 정보 포함 */
	TArray<FParticleBurst> BurstList;

	/** 버스트 개수에 곱해지는 스케일 값 */
	FFloatDistribution BurstScale;

	/** Burst 기반 생성 활성화 여부 (false면 BurstList 무시) */
	bool bProcessBurstList;

	// ==================== 생성자/소멸자 ====================

	UParticleModuleSpawn();
	~UParticleModuleSpawn() override = default;

	// ==================== 직렬화 ====================

	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// ==================== 스폰 계산 함수 ====================

	/**
	 * @brief Rate 기반으로 이번 프레임에 생성할 파티클 수 계산
	 * @details SpawnFraction 누적 방식으로 부드러운 파티클 생성 보장
	 *
	 * [SpawnFraction 누적 방식 설명]
	 * 문제 상황:
	 *   - Rate = 30 (초당 30개), DeltaTime = 0.016초 (60fps)
	 *   - 이번 프레임 생성량 = 30 * 0.016 = 0.48개
	 *   - 정수로 변환하면 0개 → 파티클이 안 나옴!
	 *
	 * 해결 방법 (SpawnFraction 누적):
	 *   - Frame 1: 0.48개 → 0개 생성, SpawnFraction = 0.48 저장
	 *   - Frame 2: 0.48 + 0.48 = 0.96개 → 0개 생성, SpawnFraction = 0.96
	 *   - Frame 3: 0.96 + 0.48 = 1.44개 → 1개 생성, SpawnFraction = 0.44
	 *   - Frame 4: 0.44 + 0.48 = 0.92개 → 0개 생성, SpawnFraction = 0.92
	 *   - ...이런 식으로 누적해서 자연스럽게 생성
	 *
	 * @param DeltaTime 이번 프레임의 경과 시간 (초)
	 * @param OutNumber [출력] 이번 프레임에 생성할 파티클 개수 (정수)
	 * @param OutRate [출력] 현재 초당 생성률 (Rate * RateScale)
	 * @param InOutSpawnFraction [입출력] 소수점 누적값 (EmitterInstance에서 관리)
	 *                           - 입력: 이전 프레임까지의 누적 소수점
	 *                           - 출력: 이번 프레임 후 남은 소수점
	 * @return 파티클을 1개 이상 생성해야 하면 true
	 */
	bool GetSpawnAmount(float DeltaTime, int32& OutNumber, float& OutRate, float& InOutSpawnFraction);

	/**
	 * @brief Burst 기반으로 이번 프레임에 생성할 파티클 수 계산
	 * @details BurstList에서 OldTime~NewTime 구간에 해당하는 버스트 개수 합산
	 *
	 * [버스트 동작 예시]
	 * BurstList = [(Time=0.0, Count=50), (Time=2.0, Count=100)]
	 *   - t=0.0초: 50개 폭발 생성
	 *   - t=2.0초: 100개 폭발 생성
	 *
	 * @param OldTime 이전 프레임 시간 (AccumulatedTime - DeltaTime)
	 * @param NewTime 현재 프레임 시간 (AccumulatedTime)
	 * @param Duration 이미터 총 지속 시간 (루프 계산용)
	 * @return 이번 프레임에 버스트로 생성할 파티클 총 개수
	 */
	int32 GetBurstCount(float OldTime, float NewTime, float Duration);
};
