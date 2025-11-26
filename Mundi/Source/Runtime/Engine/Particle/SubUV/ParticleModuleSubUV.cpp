#include "pch.h"
#include "ParticleModuleSubUV.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticleModuleRequired.h"
#include "../ParticleLODLevel.h"
#include "../ParticleHelper.h"

UParticleModuleSubUV::UParticleModuleSubUV()
	: InterpolationMethod(EParticleSubUVInterpMethod::None)
	, bUseRealTime(false)
	, StartingFrame(0.0f)
	, FrameRate(30.0f)
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Payload 메모리 주소 계산
	uint8* ParticlePtr = reinterpret_cast<uint8*>(ParticleBase);
	FSubUVPayloadData* SubUVData = reinterpret_cast<FSubUVPayloadData*>(ParticlePtr + Offset);

	// 시작 프레임 설정
	if (StartingFrame < 0.0f)
	{
		// 랜덤 프레임
		if (Owner->CurrentLODLevel && Owner->CurrentLODLevel->RequiredModule)
		{
			int32 TotalFrames = Owner->CurrentLODLevel->RequiredModule->GetTotalSubImages();
			if (TotalFrames > 0)
			{
				SubUVData->ImageIndex = static_cast<float>(rand() % TotalFrames);
			}
			else
			{
				SubUVData->ImageIndex = 0.0f;
			}
		}
		else
		{
			SubUVData->ImageIndex = 0.0f;
		}
	}
	else
	{
		// 지정된 프레임
		SubUVData->ImageIndex = StartingFrame;
	}
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	if (!Owner || !Owner->CurrentLODLevel || !Owner->CurrentLODLevel->RequiredModule)
	{
		return;
	}

	// Required Module에서 SubUV 설정 가져오기
	UParticleModuleRequired* RequiredModule = Owner->CurrentLODLevel->RequiredModule;
	const int32 SubImages_Horizontal = RequiredModule->GetSubImagesHorizontal();
	const int32 SubImages_Vertical = RequiredModule->GetSubImagesVertical();
	const int32 TotalFrames = RequiredModule->GetTotalSubImages();

	if (TotalFrames <= 1)
	{
		// SubUV가 비활성화되어 있으면 업데이트 필요 없음
		return;
	}

	// InterpolationMethod 결정 (모듈 설정 > Required 설정)
	EParticleSubUVInterpMethod CurrentMethod = InterpolationMethod;
	if (CurrentMethod == EParticleSubUVInterpMethod::None)
	{
		CurrentMethod = RequiredModule->GetInterpolationMethod();
	}

	// 각 파티클 업데이트
	// BEGIN_UPDATE_LOOP 매크로 확장 - Owner를 통해 인스턴스 데이터 접근
	for (int32 i = Owner->ActiveParticles - 1; i >= 0; i--)
	{
		const int32 CurrentIndex = Owner->ParticleIndices[i];
		uint8* ParticlePtr = Owner->ParticleData + (CurrentIndex * Owner->ParticleStride);
		FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticlePtr);
		
		FSubUVPayloadData* SubUVData = reinterpret_cast<FSubUVPayloadData*>(ParticlePtr + Offset);

		if (bUseRealTime)
		{
			// 실시간 기준 애니메이션
			SubUVData->ImageIndex += FrameRate * DeltaTime;
			SubUVData->ImageIndex = fmod(SubUVData->ImageIndex, static_cast<float>(TotalFrames));
		}
		else
		{
			// 수명 기준 애니메이션
			switch (CurrentMethod)
			{
			case EParticleSubUVInterpMethod::None:
				// 고정 프레임 (변화 없음)
				break;

			case EParticleSubUVInterpMethod::Linear:
				// 선형 보간: RelativeTime에 따라 0 ~ TotalFrames-1
				SubUVData->ImageIndex = Particle.RelativeTime * (TotalFrames - 1);
				break;

			case EParticleSubUVInterpMethod::Random:
				// 랜덤 프레임 (Spawn에서 설정한 값 유지)
				break;

			case EParticleSubUVInterpMethod::RandomBlend:
				// 랜덤 프레임 + 다음 프레임과 블렌드
				// RelativeTime에 따라 소수부를 증가시켜 블렌드
				{
					int32 CurrentFrame = static_cast<int32>(SubUVData->ImageIndex);
					float BlendFactor = Particle.RelativeTime;
					SubUVData->ImageIndex = static_cast<float>(CurrentFrame) + BlendFactor;
				}
				break;
			}
		}
	}
}

uint32 UParticleModuleSubUV::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FSubUVPayloadData);
}

void UParticleModuleSubUV::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		JSON& SubUV = InOutHandle;
		if (SubUV.hasKey("InterpolationMethod"))
		{
			int32 MethodInt = SubUV["InterpolationMethod"].ToInt();
			InterpolationMethod = static_cast<EParticleSubUVInterpMethod>(MethodInt);
		}
		if (SubUV.hasKey("bUseRealTime"))
		{
			bUseRealTime = SubUV["bUseRealTime"].ToBool();
		}
		if (SubUV.hasKey("StartingFrame"))
		{
			StartingFrame = SubUV["StartingFrame"].ToFloat();
		}
		if (SubUV.hasKey("FrameRate"))
		{
			FrameRate = SubUV["FrameRate"].ToFloat();
		}
	}
	else
	{
		InOutHandle["InterpolationMethod"] = static_cast<int32>(InterpolationMethod);
		InOutHandle["bUseRealTime"] = bUseRealTime;
		InOutHandle["StartingFrame"] = StartingFrame;
		InOutHandle["FrameRate"] = FrameRate;
	}
}

void UParticleModuleSubUV::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);

	const UParticleModuleSubUV* SourceSubUV = static_cast<const UParticleModuleSubUV*>(Source);
	InterpolationMethod = SourceSubUV->InterpolationMethod;
	bUseRealTime = SourceSubUV->bUseRealTime;
	StartingFrame = SourceSubUV->StartingFrame;
	FrameRate = SourceSubUV->FrameRate;
}
