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
