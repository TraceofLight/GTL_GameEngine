#include "pch.h"
#include "ParticleModule.h"

UParticleModule::UParticleModule()
	: bSpawnModule(false)
	, bUpdateModule(false)
	, bFinalUpdateModule(false)
	, bSupported3DDrawMode(false)
	, b3DDrawMode(false)
	, LODValidity(0xFF)  // 기본적으로 모든 LOD에서 유효
{
}

/**
 * 파티클 Spawn 시 호출
 * @param Owner 이미터 인스턴스
 * @param Offset 모듈 데이터 오프셋
 * @param SpawnTime 스폰 시간
 * @param ParticleBase 스폰되는 파티클
 */
void UParticleModule::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// 기본 구현은 아무것도 하지 않음
	// 파생 클래스에서 오버라이드
}

/**
 * 파티클 Update 시 호출
 * @param Owner 이미터 인스턴스
 * @param Offset 모듈 데이터 오프셋
 * @param DeltaTime 델타 타임
 */
void UParticleModule::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// 기본 구현은 아무것도 하지 않음
	// 파생 클래스에서 오버라이드
}

/**
 * 파티클 FinalUpdate 시 호출 (렌더링 직전)
 * @param Owner 이미터 인스턴스
 * @param Offset 모듈 데이터 오프셋
 * @param DeltaTime 델타 타임
 */
void UParticleModule::FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// 기본 구현은 아무것도 하지 않음
	// 파생 클래스에서 오버라이드
}

/**
 * 모듈이 필요로 하는 추가 바이트 수 반환
 * @param TypeData 타입 데이터 모듈
 * @return 필요한 바이트 수
 */
uint32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	// 기본적으로 추가 데이터 없음
	return 0;
}

/**
 * 모듈이 필요로 하는 인스턴스별 바이트 수 반환
 * @return 필요한 바이트 수
 */
uint32 UParticleModule::RequiredBytesPerInstance()
{
	// 기본적으로 인스턴스별 추가 데이터 없음
	return 0;
}

/**
 * 모듈이 특정 LOD 레벨에서 유효한지 확인
 * @param LODLevel LOD 레벨 인덱스
 * @return 유효 여부
 */
bool UParticleModule::IsValidForLODLevel(int32 LODLevel) const
{
	if (LODLevel < 0 || LODLevel >= 8)
	{
		return false;
	}
	return (LODValidity & (1 << LODLevel)) != 0;
}

/**
 * 특정 LOD 레벨의 유효성 설정
 * @param LODLevel LOD 레벨 인덱스
 * @param bValid 유효성 여부
 */
void UParticleModule::SetLODValidity(int32 LODLevel, bool bValid)
{
	if (LODLevel < 0 || LODLevel >= 8)
	{
		return;
	}

	if (bValid)
	{
		LODValidity |= (1 << LODLevel);
	}
	else
	{
		LODValidity &= ~(1 << LODLevel);
	}
}

/**
 * 모듈 직렬화
 * @param bIsLoading true면 로드, false면 저장
 * @param InOutHandle JSON 핸들
 */
void UParticleModule::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	if (bIsLoading)
	{
		if (InOutHandle.hasKey("bSpawnModule")) bSpawnModule = InOutHandle["bSpawnModule"].ToBool();
		if (InOutHandle.hasKey("bUpdateModule")) bUpdateModule = InOutHandle["bUpdateModule"].ToBool();
		if (InOutHandle.hasKey("bFinalUpdateModule")) bFinalUpdateModule = InOutHandle["bFinalUpdateModule"].ToBool();
		if (InOutHandle.hasKey("bSupported3DDrawMode")) bSupported3DDrawMode = InOutHandle["bSupported3DDrawMode"].ToBool();
		if (InOutHandle.hasKey("b3DDrawMode")) b3DDrawMode = InOutHandle["b3DDrawMode"].ToBool();
		if (InOutHandle.hasKey("LODValidity")) LODValidity = static_cast<uint8>(InOutHandle["LODValidity"].ToInt());
	}
	else
	{
		InOutHandle["bSpawnModule"] = bSpawnModule;
		InOutHandle["bUpdateModule"] = bUpdateModule;
		InOutHandle["bFinalUpdateModule"] = bFinalUpdateModule;
		InOutHandle["bSupported3DDrawMode"] = bSupported3DDrawMode;
		InOutHandle["b3DDrawMode"] = b3DDrawMode;
		InOutHandle["LODValidity"] = static_cast<int32>(LODValidity);
	}
}

/**
 * 모듈 복제
 * @param Source 원본 모듈
 */
void UParticleModule::DuplicateFrom(const UParticleModule* Source)
{
	if (!Source)
	{
		return;
	}

	bSpawnModule = Source->bSpawnModule;
	bUpdateModule = Source->bUpdateModule;
	bFinalUpdateModule = Source->bFinalUpdateModule;
	bSupported3DDrawMode = Source->bSupported3DDrawMode;
	b3DDrawMode = Source->b3DDrawMode;
	LODValidity = Source->LODValidity;
}
