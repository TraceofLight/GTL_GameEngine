#include "pch.h"
#include "ParticleModuleLocation.h"

IMPLEMENT_CLASS(UParticleModuleLocation)

UParticleModuleLocation::UParticleModuleLocation()
	: StartLocation(FVector::Zero())
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 파티클 초기 위치 설정 (기존 위치에 더함)
	ParticleBase->Location = ParticleBase->Location + StartLocation.GetValue();
}

void UParticleModuleLocation::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartLocation")) StartLocation = JsonToVectorDistribution(InOutHandle["StartLocation"]);
	}
	else
	{
		InOutHandle["StartLocation"] = VectorDistributionToJson(StartLocation);
	}
}

void UParticleModuleLocation::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleLocation* SrcLocation = static_cast<const UParticleModuleLocation*>(Source);
	if (!SrcLocation)
	{
		return;
	}

	StartLocation = SrcLocation->StartLocation;
}
