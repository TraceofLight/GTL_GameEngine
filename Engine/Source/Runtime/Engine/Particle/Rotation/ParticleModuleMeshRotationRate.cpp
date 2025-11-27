#include "pch.h"
#include "ParticleModuleMeshRotationRate.h"
#include "Particle.h"
#include "ParticleEmitterInstance.h"

UParticleModuleMeshRotationRate::UParticleModuleMeshRotationRate()
	: StartRotationRate(FVector::Zero())
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

uint32 UParticleModuleMeshRotationRate::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	// FMeshRotationPayloadData 크기만큼 Payload 공간 요청
	// MeshRotation 모듈과 동일한 Payload 공유
	return sizeof(FMeshRotationPayloadData);
}

void UParticleModuleMeshRotationRate::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !Owner)
	{
		return;
	}

	// MeshRotation 모듈이 활성화됨을 표시
	Owner->bMeshRotationActive = true;

	// Payload 위치 계산 (FBaseParticle 뒤의 추가 데이터 영역)
	FMeshRotationPayloadData* PayloadData =
		reinterpret_cast<FMeshRotationPayloadData*>(reinterpret_cast<uint8*>(ParticleBase) + Offset);

	// 3D 회전 속도 설정 (Degrees/sec → Radians/sec)
	FVector RateDegrees = StartRotationRate.GetValue();
	PayloadData->RotationRate = FVector(
		DegreesToRadians(RateDegrees.X),
		DegreesToRadians(RateDegrees.Y),
		DegreesToRadians(RateDegrees.Z)
	);
}

void UParticleModuleMeshRotationRate::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartRotationRate")) StartRotationRate = JsonToVectorDistribution(InOutHandle["StartRotationRate"]);
	}
	else
	{
		InOutHandle["StartRotationRate"] = VectorDistributionToJson(StartRotationRate);
	}
}

void UParticleModuleMeshRotationRate::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleMeshRotationRate* SrcMeshRotationRate = static_cast<const UParticleModuleMeshRotationRate*>(Source);
	if (!SrcMeshRotationRate)
	{
		return;
	}

	StartRotationRate = SrcMeshRotationRate->StartRotationRate;
}
