#include "pch.h"
#include "ParticleModuleBeamNoise.h"

IMPLEMENT_CLASS(UParticleModuleBeamNoise)

UParticleModuleBeamNoise::UParticleModuleBeamNoise()
	: bEnabled(true)
	, Strength(10.0f)
	, Frequency(1.0f)
	, Speed(1.0f)
	, bLockEndpoints(true)
{
	// 빔 전용 모듈이므로 Spawn/Update 파이프라인 사용 안함
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleBeamNoise::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("bEnabled")) bEnabled = InOutHandle["bEnabled"].ToBool();
		if (InOutHandle.hasKey("Strength")) Strength = static_cast<float>(InOutHandle["Strength"].ToFloat());
		if (InOutHandle.hasKey("Frequency")) Frequency = static_cast<float>(InOutHandle["Frequency"].ToFloat());
		if (InOutHandle.hasKey("Speed")) Speed = static_cast<float>(InOutHandle["Speed"].ToFloat());
		if (InOutHandle.hasKey("bLockEndpoints")) bLockEndpoints = InOutHandle["bLockEndpoints"].ToBool();
	}
	else
	{
		InOutHandle["bEnabled"] = bEnabled;
		InOutHandle["Strength"] = Strength;
		InOutHandle["Frequency"] = Frequency;
		InOutHandle["Speed"] = Speed;
		InOutHandle["bLockEndpoints"] = bLockEndpoints;
	}
}

void UParticleModuleBeamNoise::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleBeamNoise* SrcModule = static_cast<const UParticleModuleBeamNoise*>(Source);
	if (!SrcModule)
	{
		return;
	}

	bEnabled = SrcModule->bEnabled;
	Strength = SrcModule->Strength;
	Frequency = SrcModule->Frequency;
	Speed = SrcModule->Speed;
	bLockEndpoints = SrcModule->bLockEndpoints;
}
