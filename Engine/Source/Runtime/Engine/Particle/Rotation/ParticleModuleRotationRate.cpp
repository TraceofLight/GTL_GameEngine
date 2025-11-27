#include "pch.h"
#include "ParticleModuleRotationRate.h"

UParticleModuleRotationRate::UParticleModuleRotationRate()
	: StartRotationRate(0.0f)
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// Degrees/sec → Radians/sec 변환 후 적용
	float RateDegrees = StartRotationRate.GetValue();
	ParticleBase->RotationRate = DegreesToRadians(RateDegrees);
}

void UParticleModuleRotationRate::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartRotationRate")) StartRotationRate = JsonToFloatDistribution(InOutHandle["StartRotationRate"]);
	}
	else
	{
		InOutHandle["StartRotationRate"] = FloatDistributionToJson(StartRotationRate);
	}
}

void UParticleModuleRotationRate::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleRotationRate* SrcRotationRate = static_cast<const UParticleModuleRotationRate*>(Source);
	if (!SrcRotationRate)
	{
		return;
	}

	StartRotationRate = SrcRotationRate->StartRotationRate;
}
