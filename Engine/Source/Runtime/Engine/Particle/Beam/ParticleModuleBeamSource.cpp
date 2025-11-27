#include "pch.h"
#include "ParticleModuleBeamSource.h"

IMPLEMENT_CLASS(UParticleModuleBeamSource)

UParticleModuleBeamSource::UParticleModuleBeamSource()
	: SourceOffset(FVector::Zero())
{
	// 빔 전용 모듈이므로 Spawn/Update 파이프라인 사용 안함
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleBeamSource::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("SourceOffset"))
		{
			SourceOffset.X = static_cast<float>(InOutHandle["SourceOffset"][0].ToFloat());
			SourceOffset.Y = static_cast<float>(InOutHandle["SourceOffset"][1].ToFloat());
			SourceOffset.Z = static_cast<float>(InOutHandle["SourceOffset"][2].ToFloat());
		}
	}
	else
	{
		JSON srcArr = JSON::Make(JSON::Class::Array);
		srcArr.append(SourceOffset.X);
		srcArr.append(SourceOffset.Y);
		srcArr.append(SourceOffset.Z);
		InOutHandle["SourceOffset"] = srcArr;
	}
}

void UParticleModuleBeamSource::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleBeamSource* SrcModule = static_cast<const UParticleModuleBeamSource*>(Source);
	if (!SrcModule)
	{
		return;
	}

	SourceOffset = SrcModule->SourceOffset;
}
