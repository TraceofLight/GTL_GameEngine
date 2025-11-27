#include "pch.h"
#include "ParticleModuleRotation.h"

UParticleModuleRotation::UParticleModuleRotation()
	: StartRotation(0.0f)
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// Degrees → Radians 변환 후 적용
	float RotationDegrees = StartRotation.GetValue();
	ParticleBase->Rotation = DegreesToRadians(RotationDegrees);
}

void UParticleModuleRotation::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartRotation")) StartRotation = JsonToFloatDistribution(InOutHandle["StartRotation"]);
	}
	else
	{
		InOutHandle["StartRotation"] = FloatDistributionToJson(StartRotation);
	}
}

void UParticleModuleRotation::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleRotation* SrcRotation = static_cast<const UParticleModuleRotation*>(Source);
	if (!SrcRotation)
	{
		return;
	}

	StartRotation = SrcRotation->StartRotation;
}
