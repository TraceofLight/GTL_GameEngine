#include "pch.h"
#include "ParticleModuleBeamTarget.h"

IMPLEMENT_CLASS(UParticleModuleBeamTarget)

UParticleModuleBeamTarget::UParticleModuleBeamTarget()
	: TargetOffset(FVector(100.0f, 0.0f, 0.0f))  // 기본: X축 방향 100 유닛
{
	// 빔 전용 모듈이므로 Spawn/Update 파이프라인 사용 안함
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

void UParticleModuleBeamTarget::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		if (InOutHandle.hasKey("TargetOffset"))
		{
			TargetOffset.X = static_cast<float>(InOutHandle["TargetOffset"][0].ToFloat());
			TargetOffset.Y = static_cast<float>(InOutHandle["TargetOffset"][1].ToFloat());
			TargetOffset.Z = static_cast<float>(InOutHandle["TargetOffset"][2].ToFloat());
		}
	}
	else
	{
		JSON tgtArr = JSON::Make(JSON::Class::Array);
		tgtArr.append(TargetOffset.X);
		tgtArr.append(TargetOffset.Y);
		tgtArr.append(TargetOffset.Z);
		InOutHandle["TargetOffset"] = tgtArr;
	}
}

void UParticleModuleBeamTarget::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleBeamTarget* SrcModule = static_cast<const UParticleModuleBeamTarget*>(Source);
	if (!SrcModule)
	{
		return;
	}

	TargetOffset = SrcModule->TargetOffset;
}
