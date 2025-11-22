#include "pch.h"
#include "ParticleModuleSpawn.h"

#include "ParticleTypes.h"

UParticleModuleSpawn::UParticleModuleSpawn()
	: Rate(10.0f)
	, RateScale(1.0f)
	, BurstMethod(EParticleBurstMethod::Instant)
	, BurstScale(1.0f)
	, bProcessSpawnRate(true)
	, bProcessBurstList(true)
{
	// Spawn 모듈은 Spawn/Update에 참여하지 않음 (스폰 로직은 EmitterInstance에서 처리)
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

/**
 * 주어진 시간 동안 생성할 파티클 수 계산
 * @param OldTime 이전 시간
 * @param DeltaTime 델타 타임
 * @param OutNumber 생성할 파티클 수 (출력)
 * @param OutRate 실제 생성률 (출력)
 * @return 파티클을 생성해야 하면 true
 */
bool UParticleModuleSpawn::GetSpawnAmount(float OldTime, float DeltaTime, int32& OutNumber, float& OutRate)
{
	OutNumber = 0;
	OutRate = 0.0f;

	if (!bProcessSpawnRate)
	{
		return false;
	}

	// 생성률 계산
	float CurrentRate = Rate.GetValue() * RateScale.GetValue();
	if (CurrentRate <= 0.0f)
	{
		return false;
	}

	OutRate = CurrentRate;

	// 델타 타임 동안 생성할 파티클 수
	float SpawnCount = CurrentRate * DeltaTime;
	OutNumber = static_cast<int32>(SpawnCount);

	// 소수점 부분은 확률적으로 처리
	float Fraction = SpawnCount - static_cast<float>(OutNumber);
	if (Fraction > 0.0f)
	{
		float Random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		if (Random < Fraction)
		{
			++OutNumber;
		}
	}

	return OutNumber > 0;
}

/**
 * 버스트 시간에 맞는 버스트 카운트 반환
 * @param OldTime 이전 시간
 * @param NewTime 현재 시간
 * @param Duration 이미터 지속 시간
 * @return 버스트로 생성할 파티클 수
 */
int32 UParticleModuleSpawn::GetBurstCount(float OldTime, float NewTime, float Duration)
{
	if (!bProcessBurstList || BurstList.empty())
	{
		return 0;
	}

	int32 TotalBurst = 0;
	float Scale = BurstScale.GetValue();

	for (const FParticleBurst& Burst : BurstList)
	{
		// 버스트 시간이 OldTime과 NewTime 사이에 있는지 확인
		if (Burst.Time >= OldTime && Burst.Time < NewTime)
		{
			int32 Count = Burst.Count;

			// 랜덤 범위 적용
			if (Burst.CountLow >= 0 && Burst.CountLow < Burst.Count)
			{
				int32 Range = Burst.Count - Burst.CountLow;
				Count = Burst.CountLow + (rand() % (Range + 1));
			}

			// 스케일 적용
			Count = static_cast<int32>(static_cast<float>(Count) * Scale);
			TotalBurst += Count;
		}
	}

	return TotalBurst;
}
