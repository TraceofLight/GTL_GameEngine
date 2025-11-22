#include "pch.h"
#include "ParticleModuleLocation.h"

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
