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
