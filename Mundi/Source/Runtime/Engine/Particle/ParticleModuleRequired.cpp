#include "pch.h"
#include "ParticleModuleRequired.h"

#include "ParticleTypes.h"
#include "Material.h"
#include "ResourceManager.h"

UParticleModuleRequired::UParticleModuleRequired()
	: Material(nullptr)
	, EmitterDuration(1.0f)
	, EmitterDurationLow(0.0f)
	, EmitterLoops(0)
	, bDurationRecalcEachLoop(false)
	, ScreenAlignment(EParticleScreenAlignment::Square)
	, SubImages_Horizontal(1)
	, SubImages_Vertical(1)
	, SortMode(EParticleSortMode::None)
	, bUseLocalSpace(false)
	, bKillOnDeactivate(false)
	, bKillOnCompleted(false)
	, MaxDrawCount(500)
	, EmitterNormalsMode(EEmitterNormalsMode::CameraFacing)
	, InterpolationMethod(EParticleSubUVInterpMethod::None)
	, AxisLockOption(EParticleAxisLock::None)
	, BlendMode(EParticleBlendMode::None)
{
	// Required 모듈은 Spawn/Update에 참여하지 않음 (설정만 제공)
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

/**
 * 실제 이미터 지속 시간 반환 (랜덤 범위 적용)
 * @return 이미터 지속 시간
 */
float UParticleModuleRequired::GetEmitterDuration() const
{
	if (EmitterDurationLow > 0.0f && EmitterDurationLow < EmitterDuration)
	{
		// 랜덤 범위
		float Alpha = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return EmitterDurationLow + Alpha * (EmitterDuration - EmitterDurationLow);
	}
	return EmitterDuration;
}

/**
 * 총 SubUV 이미지 수 반환
 * @return SubImages_Horizontal * SubImages_Vertical
 */
int32 UParticleModuleRequired::GetTotalSubImages() const
{
	return SubImages_Horizontal * SubImages_Vertical;
}

/**
 * Create renderer resource (render thread copy)
 * 렌더러 리소스 생성 (렌더 스레드용 복사본)
 *
 * @return FParticleRequiredModule* - Allocated render thread data (caller must delete)
 */
FParticleRequiredModule* UParticleModuleRequired::CreateRendererResource() const
{
	FParticleRequiredModule* FReqMod = new FParticleRequiredModule();

	// SubUV 프레임 개수
	FReqMod->NumFrames = GetTotalSubImages();

	// 알파 임계값 (현재 UParticleModuleRequired에 없으므로 기본값)
	// TODO: 필요하면 UParticleModuleRequired에 AlphaThreshold 멤버 추가
	FReqMod->AlphaThreshold = 0.0f;

	return FReqMod;
}

void UParticleModuleRequired::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		// Material 로드 (경로로 저장됨)
		if (InOutHandle.hasKey("MaterialPath"))
		{
			FString MaterialPath = InOutHandle["MaterialPath"].ToString();
			if (!MaterialPath.empty())
			{
				Material = UResourceManager::GetInstance().Load<UMaterial>(MaterialPath);
			}
		}

		if (InOutHandle.hasKey("EmitterDuration")) EmitterDuration = static_cast<float>(InOutHandle["EmitterDuration"].ToFloat());
		if (InOutHandle.hasKey("EmitterDurationLow")) EmitterDurationLow = static_cast<float>(InOutHandle["EmitterDurationLow"].ToFloat());
		if (InOutHandle.hasKey("EmitterLoops")) EmitterLoops = static_cast<int32>(InOutHandle["EmitterLoops"].ToInt());
		if (InOutHandle.hasKey("bDurationRecalcEachLoop")) bDurationRecalcEachLoop = InOutHandle["bDurationRecalcEachLoop"].ToBool();
		if (InOutHandle.hasKey("ScreenAlignment")) ScreenAlignment = static_cast<EParticleScreenAlignment>(InOutHandle["ScreenAlignment"].ToInt());
		if (InOutHandle.hasKey("SubImages_Horizontal")) SubImages_Horizontal = static_cast<int32>(InOutHandle["SubImages_Horizontal"].ToInt());
		if (InOutHandle.hasKey("SubImages_Vertical")) SubImages_Vertical = static_cast<int32>(InOutHandle["SubImages_Vertical"].ToInt());
		if (InOutHandle.hasKey("SortMode")) SortMode = static_cast<EParticleSortMode>(InOutHandle["SortMode"].ToInt());
		if (InOutHandle.hasKey("bUseLocalSpace")) bUseLocalSpace = InOutHandle["bUseLocalSpace"].ToBool();
		if (InOutHandle.hasKey("bKillOnDeactivate")) bKillOnDeactivate = InOutHandle["bKillOnDeactivate"].ToBool();
		if (InOutHandle.hasKey("bKillOnCompleted")) bKillOnCompleted = InOutHandle["bKillOnCompleted"].ToBool();
		if (InOutHandle.hasKey("MaxDrawCount")) MaxDrawCount = static_cast<int32>(InOutHandle["MaxDrawCount"].ToInt());
		if (InOutHandle.hasKey("EmitterNormalsMode")) EmitterNormalsMode = static_cast<EEmitterNormalsMode>(InOutHandle["EmitterNormalsMode"].ToInt());
		if (InOutHandle.hasKey("InterpolationMethod")) InterpolationMethod = static_cast<EParticleSubUVInterpMethod>(InOutHandle["InterpolationMethod"].ToInt());
		if (InOutHandle.hasKey("AxisLockOption")) AxisLockOption = static_cast<EParticleAxisLock>(InOutHandle["AxisLockOption"].ToInt());
	}
	else
	{
		// Material 저장 (경로)
		if (Material)
		{
			InOutHandle["MaterialPath"] = Material->GetFilePath();
		}
		else
		{
			InOutHandle["MaterialPath"] = FString("");
		}

		InOutHandle["EmitterDuration"] = EmitterDuration;
		InOutHandle["EmitterDurationLow"] = EmitterDurationLow;
		InOutHandle["EmitterLoops"] = EmitterLoops;
		InOutHandle["bDurationRecalcEachLoop"] = bDurationRecalcEachLoop;
		InOutHandle["ScreenAlignment"] = static_cast<int32>(ScreenAlignment);
		InOutHandle["SubImages_Horizontal"] = SubImages_Horizontal;
		InOutHandle["SubImages_Vertical"] = SubImages_Vertical;
		InOutHandle["SortMode"] = static_cast<int32>(SortMode);
		InOutHandle["bUseLocalSpace"] = bUseLocalSpace;
		InOutHandle["bKillOnDeactivate"] = bKillOnDeactivate;
		InOutHandle["bKillOnCompleted"] = bKillOnCompleted;
		InOutHandle["MaxDrawCount"] = MaxDrawCount;
		InOutHandle["EmitterNormalsMode"] = static_cast<int32>(EmitterNormalsMode);
		InOutHandle["InterpolationMethod"] = static_cast<int32>(InterpolationMethod);
		InOutHandle["AxisLockOption"] = static_cast<int32>(AxisLockOption);
	}
}

void UParticleModuleRequired::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleRequired* SrcReq = static_cast<const UParticleModuleRequired*>(Source);
	if (!SrcReq)
	{
		return;
	}

	Material = SrcReq->Material;
	EmitterDuration = SrcReq->EmitterDuration;
	EmitterDurationLow = SrcReq->EmitterDurationLow;
	EmitterLoops = SrcReq->EmitterLoops;
	bDurationRecalcEachLoop = SrcReq->bDurationRecalcEachLoop;
	ScreenAlignment = SrcReq->ScreenAlignment;
	SubImages_Horizontal = SrcReq->SubImages_Horizontal;
	SubImages_Vertical = SrcReq->SubImages_Vertical;
	SortMode = SrcReq->SortMode;
	bUseLocalSpace = SrcReq->bUseLocalSpace;
	bKillOnDeactivate = SrcReq->bKillOnDeactivate;
	bKillOnCompleted = SrcReq->bKillOnCompleted;
	MaxDrawCount = SrcReq->MaxDrawCount;
	EmitterNormalsMode = SrcReq->EmitterNormalsMode;
	InterpolationMethod = SrcReq->InterpolationMethod;
	AxisLockOption = SrcReq->AxisLockOption;
}
