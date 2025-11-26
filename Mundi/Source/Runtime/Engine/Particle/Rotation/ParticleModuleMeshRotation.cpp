#include "pch.h"
#include "ParticleModuleMeshRotation.h"
#include "Particle.h"
#include "ParticleEmitterInstance.h"

UParticleModuleMeshRotation::UParticleModuleMeshRotation()
	: StartRotation(FVector::Zero())
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

uint32 UParticleModuleMeshRotation::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	// FMeshRotationPayloadData 크기만큼 Payload 공간 요청
	return sizeof(FMeshRotationPayloadData);
}

void UParticleModuleMeshRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
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

	// 3D 회전 설정 (Degrees → Radians)
	FVector RotationDegrees = StartRotation.GetValue();
	PayloadData->Rotation = FVector(
		DegreesToRadians(RotationDegrees.X),
		DegreesToRadians(RotationDegrees.Y),
		DegreesToRadians(RotationDegrees.Z)
	);
}

void UParticleModuleMeshRotation::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartRotation")) StartRotation = JsonToVectorDistribution(InOutHandle["StartRotation"]);
	}
	else
	{
		InOutHandle["StartRotation"] = VectorDistributionToJson(StartRotation);
	}
}

void UParticleModuleMeshRotation::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleMeshRotation* SrcMeshRotation = static_cast<const UParticleModuleMeshRotation*>(Source);
	if (!SrcMeshRotation)
	{
		return;
	}

	StartRotation = SrcMeshRotation->StartRotation;
}
