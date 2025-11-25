#include "pch.h"
#include "ParticleModuleSize.h"

UParticleModuleSize::UParticleModuleSize()
	: StartSize(FVector(1.0f, 1.0f, 1.0f))
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 파티클 크기 설정
	ParticleBase->Size = StartSize.GetValue();
}

void UParticleModuleSize::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartSize")) StartSize = JsonToVectorDistribution(InOutHandle["StartSize"]);
	}
	else
	{
		InOutHandle["StartSize"] = VectorDistributionToJson(StartSize);
	}
}

void UParticleModuleSize::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleSize* SrcSize = static_cast<const UParticleModuleSize*>(Source);
	if (!SrcSize)
	{
		return;
	}

	StartSize = SrcSize->StartSize;
}
