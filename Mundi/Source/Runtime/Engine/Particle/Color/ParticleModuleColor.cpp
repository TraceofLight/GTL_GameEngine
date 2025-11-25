#include "pch.h"
#include "ParticleModuleColor.h"

UParticleModuleColor::UParticleModuleColor()
	: StartColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	, StartAlpha(1.0f)
	, bClampAlpha(true)
{
	bSpawnModule = true;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase)
	{
		return;
	}

	// 색상 설정
	FLinearColor Color = StartColor.GetValue();

	// 알파 별도 설정
	float Alpha = StartAlpha.GetValue();
	if (bClampAlpha)
	{
		Alpha = std::max(0.0f, std::min(1.0f, Alpha));
	}
	Color.A = Alpha;

	ParticleBase->Color = Color;
}

void UParticleModuleColor::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("StartColor")) StartColor = JsonToColorDistribution(InOutHandle["StartColor"]);
		if (InOutHandle.hasKey("StartAlpha")) StartAlpha = JsonToFloatDistribution(InOutHandle["StartAlpha"]);
		if (InOutHandle.hasKey("bClampAlpha")) bClampAlpha = InOutHandle["bClampAlpha"].ToBool();
	}
	else
	{
		InOutHandle["StartColor"] = ColorDistributionToJson(StartColor);
		InOutHandle["StartAlpha"] = FloatDistributionToJson(StartAlpha);
		InOutHandle["bClampAlpha"] = bClampAlpha;
	}
}

void UParticleModuleColor::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleColor* SrcColor = static_cast<const UParticleModuleColor*>(Source);
	if (!SrcColor)
	{
		return;
	}

	StartColor = SrcColor->StartColor;
	StartAlpha = SrcColor->StartAlpha;
	bClampAlpha = SrcColor->bClampAlpha;
}
