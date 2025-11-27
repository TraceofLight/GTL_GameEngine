#include "pch.h"
#include "ParticleModuleLifetime.h"

UParticleModuleLifetime::UParticleModuleLifetime()
	: Lifetime(1.0f)
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 파티클 수명 설정
	ParticleBase->Lifetime = Lifetime.GetValue();
	ParticleBase->RelativeTime = 0.0f;
}

void UParticleModuleLifetime::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("Lifetime")) Lifetime = JsonToFloatDistribution(InOutHandle["Lifetime"]);
	}
	else
	{
		InOutHandle["Lifetime"] = FloatDistributionToJson(Lifetime);
	}
}

void UParticleModuleLifetime::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleLifetime* SrcLifetime = static_cast<const UParticleModuleLifetime*>(Source);
	if (!SrcLifetime)
	{
		return;
	}

	Lifetime = SrcLifetime->Lifetime;
}
