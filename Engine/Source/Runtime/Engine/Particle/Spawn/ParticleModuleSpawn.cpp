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
 * @brief Rate 기반으로 이번 프레임에 생성할 파티클 수 계산
 * @details SpawnFraction 누적 방식으로 부드러운 파티클 생성 보장
 *
 * [왜 SpawnFraction이 필요한가?]
 *
 * 예를 들어 Rate=30 (초당 30개), 60fps (DeltaTime=0.016초)인 경우:
 *   - 이번 프레임 생성량 = 30 * 0.016 = 0.48개
 *   - 그냥 int로 변환하면 0개 → 파티클이 매우 끊겨서 나옴
 *
 * [SpawnFraction 누적 방식]
 *
 * SpawnFraction에 소수점을 누적 저장하고, 1.0 이상이 되면 그때 생성:
 *   - Frame 1: SpawnCount = 0 + 0.48 = 0.48 → 0개 생성, Fraction = 0.48 저장
 *   - Frame 2: SpawnCount = 0.48 + 0.48 = 0.96 → 0개 생성, Fraction = 0.96 저장
 *   - Frame 3: SpawnCount = 0.96 + 0.48 = 1.44 → 1개 생성, Fraction = 0.44 저장
 *   - Frame 4: SpawnCount = 0.44 + 0.48 = 0.92 → 0개 생성, Fraction = 0.92 저장
 *   - Frame 5: SpawnCount = 0.92 + 0.48 = 1.40 → 1개 생성, Fraction = 0.40 저장
 *   - ...
 *
 * 이렇게 하면 장기적으로 정확히 초당 30개가 생성되고,
 * 프레임마다 부드럽게 파티클이 나옴
 *
 * @param DeltaTime 이번 프레임의 경과 시간 (초)
 * @param OutNumber [출력] 이번 프레임에 생성할 파티클 개수 (정수)
 * @param OutRate [출력] 현재 초당 생성률 (Rate * RateScale)
 * @param InOutSpawnFraction [입출력] 소수점 누적값
 *                           - 입력: 이전 프레임까지의 누적 소수점
 *                           - 출력: 이번 프레임 후 남은 소수점
 * @return 파티클을 1개 이상 생성해야 하면 true
 */
bool UParticleModuleSpawn::GetSpawnAmount(float DeltaTime, int32& OutNumber, float& OutRate, float& InOutSpawnFraction)
{
	// ========== 1단계: 출력값 초기화 ==========
	OutNumber = 0;
	OutRate = 0.0f;

	// Rate 처리가 비활성화되어 있으면 스킵
	if (!bProcessSpawnRate)
	{
		return false;
	}

	// ========== 2단계: 현재 생성률 계산 ==========
	// Rate와 RateScale 모두 Distribution이므로 매번 다른 값이 나올 수 있음
	// 예: Rate(Min=20, Max=40) * RateScale(1.0) → 20~40 사이 랜덤
	float CurrentRate = Rate.GetValue() * RateScale.GetValue();

	// 생성률이 0 이하면 생성 안 함
	if (CurrentRate <= 0.0f)
	{
		return false;
	}

	// 초당 생성률 출력 (디버깅/통계용)
	OutRate = CurrentRate;

	// ========== 3단계: SpawnFraction 누적 방식으로 생성 개수 계산 ==========

	// 이번 프레임 생성량 = (이전까지 누적된 소수점) + (이번 프레임 생성량)
	// 예: InOutSpawnFraction=0.96, CurrentRate=30, DeltaTime=0.016
	//     SpawnCount = 0.96 + (30 * 0.016) = 0.96 + 0.48 = 1.44
	float SpawnCount = InOutSpawnFraction + (CurrentRate * DeltaTime);

	// 정수 부분 = 실제로 생성할 개수
	// 예: SpawnCount=1.44 → OutNumber=1
	OutNumber = static_cast<int32>(SpawnCount);

	// 소수 부분 = 다음 프레임으로 이월할 찌꺼기
	// 예: SpawnCount=1.44, OutNumber=1 → InOutSpawnFraction=0.44
	InOutSpawnFraction = SpawnCount - static_cast<float>(OutNumber);

	// 1개 이상 생성할 게 있으면 true 반환
	return OutNumber > 0;
}

/**
 * 버스트 시간에 맞는 버스트 카운트 반환
 * @param OldTime 이전 시간 (AccumulatedTime - DeltaTime)
 * @param NewTime 현재 시간 (AccumulatedTime)
 * @param Duration 이미터 지속 시간 (루프 계산용)
 * @return 버스트로 생성할 파티클 수
 */
int32 UParticleModuleSpawn::GetBurstCount(float OldTime, float NewTime, float Duration)
{
	if (!bProcessBurstList || BurstList.empty())
	{
		return 0;
	}

	// Duration이 0 이하면 루핑 불가능
	if (Duration <= 0.0f)
	{
		Duration = 1.0f;
	}

	int32 TotalBurst = 0;
	float Scale = BurstScale.GetValue();

	// 루핑 이미터를 위해 시간을 Duration 내로 wrap
	float WrappedOldTime = fmod(OldTime, Duration);
	float WrappedNewTime = fmod(NewTime, Duration);

	// 음수 방지 (fmod가 음수 결과를 반환할 수 있음)
	if (WrappedOldTime < 0.0f)
	{
		WrappedOldTime += Duration;
	}
	if (WrappedNewTime < 0.0f)
	{
		WrappedNewTime += Duration;
	}

	for (const FParticleBurst& Burst : BurstList)
	{
		bool bShouldBurst = false;

		// 시간이 wrap되지 않은 경우 (일반 케이스)
		// 예: OldTime=0.3, NewTime=0.6, Burst.Time=0.5 → true
		if (WrappedOldTime <= WrappedNewTime)
		{
			bShouldBurst = (Burst.Time >= WrappedOldTime && Burst.Time < WrappedNewTime);
		}
		// 시간이 wrap된 경우 (루프 경계를 넘은 케이스)
		// 예: Duration=1.0, OldTime=0.9, NewTime=0.1 (wrap됨)
		//     Burst.Time=0.0 또는 0.95 → 둘 다 true여야 함
		else
		{
			// Burst.Time이 [WrappedOldTime, Duration) 또는 [0, WrappedNewTime) 구간에 있는지 체크
			bShouldBurst = (Burst.Time >= WrappedOldTime) || (Burst.Time < WrappedNewTime);
		}

		if (bShouldBurst)
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

void UParticleModuleSpawn::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("Rate")) Rate = JsonToFloatDistribution(InOutHandle["Rate"]);
		if (InOutHandle.hasKey("RateScale")) RateScale = JsonToFloatDistribution(InOutHandle["RateScale"]);
		if (InOutHandle.hasKey("BurstMethod")) BurstMethod = static_cast<EParticleBurstMethod>(InOutHandle["BurstMethod"].ToInt());
		if (InOutHandle.hasKey("BurstScale")) BurstScale = JsonToFloatDistribution(InOutHandle["BurstScale"]);
		if (InOutHandle.hasKey("bProcessSpawnRate")) bProcessSpawnRate = InOutHandle["bProcessSpawnRate"].ToBool();
		if (InOutHandle.hasKey("bProcessBurstList")) bProcessBurstList = InOutHandle["bProcessBurstList"].ToBool();

		// BurstList
		BurstList.clear();
		if (InOutHandle.hasKey("BurstList") && InOutHandle["BurstList"].JSONType() == JSON::Class::Array)
		{
			for (size_t i = 0; i < InOutHandle["BurstList"].size(); ++i)
			{
				BurstList.push_back(JsonToParticleBurst(InOutHandle["BurstList"][static_cast<int>(i)]));
			}
		}
	}
	else
	{
		InOutHandle["Rate"] = FloatDistributionToJson(Rate);
		InOutHandle["RateScale"] = FloatDistributionToJson(RateScale);
		InOutHandle["BurstMethod"] = static_cast<int32>(BurstMethod);
		InOutHandle["BurstScale"] = FloatDistributionToJson(BurstScale);
		InOutHandle["bProcessSpawnRate"] = bProcessSpawnRate;
		InOutHandle["bProcessBurstList"] = bProcessBurstList;

		// BurstList
		JSON BurstArray = JSON::Make(JSON::Class::Array);
		for (const FParticleBurst& Burst : BurstList)
		{
			BurstArray.append(ParticleBurstToJson(Burst));
		}
		InOutHandle["BurstList"] = BurstArray;
	}
}

void UParticleModuleSpawn::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleSpawn* SrcSpawn = static_cast<const UParticleModuleSpawn*>(Source);
	if (!SrcSpawn)
	{
		return;
	}

	Rate = SrcSpawn->Rate;
	RateScale = SrcSpawn->RateScale;
	BurstMethod = SrcSpawn->BurstMethod;
	BurstList = SrcSpawn->BurstList;
	BurstScale = SrcSpawn->BurstScale;
	bProcessSpawnRate = SrcSpawn->bProcessSpawnRate;
	bProcessBurstList = SrcSpawn->bProcessBurstList;
}
