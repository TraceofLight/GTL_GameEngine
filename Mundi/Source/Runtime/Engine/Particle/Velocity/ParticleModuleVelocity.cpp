#include "pch.h"
#include "ParticleModuleVelocity.h"

UParticleModuleVelocity::UParticleModuleVelocity()
	: StartVelocity(FVector::Zero())
	, StartVelocityRadial(0.0f)
	, bInWorldSpace(false)
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 기본 속도 설정
	FVector Velocity = StartVelocity.GetValue();

	// 방사형 속도 추가
	float RadialVelocity = StartVelocityRadial.GetValue();
	if (RadialVelocity != 0.0f)
	{
		// 파티클 위치를 정규화하여 방향으로 사용
		FVector Direction = ParticleBase->Location;
		float Len = Direction.Size();
		if (Len > 0.0001f)
		{
			Direction = Direction / Len;
			Velocity = Velocity + Direction * RadialVelocity;
		}
	}

	// 속도 설정
	ParticleBase->BaseVelocity = Velocity;
	ParticleBase->Velocity = Velocity;
}
