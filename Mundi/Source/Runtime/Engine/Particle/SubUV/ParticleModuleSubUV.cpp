#include "pch.h"
#include "ParticleModuleSubUV.h"
#include "../ParticleEmitterInstance.h"
#include "../ParticleModuleRequired.h"
#include "../ParticleLODLevel.h"
#include "../ParticleHelper.h"
#include <random>

UParticleModuleSubUV::UParticleModuleSubUV()
	: InterpolationMethod(EParticleSubUVInterpMethod::None)
	, bUseRealTime(false)
	, StartingFrame(0.0f)
	, FrameRate(30.0f)
	, RandomImageChanges(1)
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Payload 메모리 주소 계산
	uint8* ParticlePtr = reinterpret_cast<uint8*>(ParticleBase);
	FSubUVPayloadData* SubUVData = reinterpret_cast<FSubUVPayloadData*>(ParticlePtr + Offset);

	// Required Module에서 InterpolationMethod 가져오기
	EParticleSubUVInterpMethod CurrentMethod = InterpolationMethod;
	if (Owner->CurrentLODLevel && Owner->CurrentLODLevel->RequiredModule)
	{
		if (CurrentMethod == EParticleSubUVInterpMethod::None)
		{
			CurrentMethod = Owner->CurrentLODLevel->RequiredModule->GetInterpolationMethod();
		}
	}

	// 시작 프레임 설정
	if (StartingFrame < 0.0f)
	{
		// 랜덤 프레임
		if (Owner->CurrentLODLevel && Owner->CurrentLODLevel->RequiredModule)
		{
			int32 TotalFrames = Owner->CurrentLODLevel->RequiredModule->GetTotalSubImages();
			if (TotalFrames > 0)
			{
				// Random 또는 RandomBlend 모드: 랜덤 프레임 선택 (생성 시 한 번만)
				if (CurrentMethod == EParticleSubUVInterpMethod::Random || 
					CurrentMethod == EParticleSubUVInterpMethod::RandomBlend)
				{
					SubUVData->ImageIndex = static_cast<float>(rand() % TotalFrames);
				}
				else
				{
					// Linear 계열: 0부터 시작
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
			SubUVData->ImageIndex = fmodf(SubUVData->ImageIndex, static_cast<float>(TotalFrames));
		}
		else
		{
			// 수명 기준 애니메이션
			switch (CurrentMethod)
			{
			case EParticleSubUVInterpMethod::None:
				// 업데이트 없음
				break;

			case EParticleSubUVInterpMethod::Linear:
				// 블렌딩 없이 주어진 순서대로 전환, 마지막 프레임 후 첫 프레임으로 순환
				{
					float FrameProgress = Particle.RelativeTime * TotalFrames;
					SubUVData->ImageIndex = fmodf(floorf(FrameProgress), static_cast<float>(TotalFrames));
				}
				break;

			case EParticleSubUVInterpMethod::Linear_Blend:
				// 현재와 다음 서브 이미지를 블렌딩하여 전환
				// 소수부가 두 텍스처 간의 알파 블렌딩 가중치로 사용됨
				{
					float FrameProgress = Particle.RelativeTime * TotalFrames;
					// TotalFrames를 넘으면 순환
					SubUVData->ImageIndex = fmodf(FrameProgress, static_cast<float>(TotalFrames));
				}
				break;

			case EParticleSubUVInterpMethod::Random:
				// 일정 주기마다 완전히 새로운 랜덤 프레임으로 변경 (블렌딩 없음)
				{
					int32 Changes = RandomImageChanges > 0 ? RandomImageChanges : 1;
					// 현재 어느 변경 구간에 있는지 계산
					int32 CurrentChangeIndex = static_cast<int32>(Particle.RelativeTime * Changes);
					
					// 해당 구간의 시드로 랜덤 프레임 결정 (같은 구간에서는 같은 프레임)
					// CurrentIndex를 시드로 사용하여 각 파티클마다 다른 시퀀스 보장
					uint32 Seed = static_cast<uint32>(CurrentIndex + CurrentChangeIndex * 10000);
					std::default_random_engine generator(Seed);
					std::uniform_int_distribution<int32> distribution(0, TotalFrames - 1);
					int32 FrameIndex = distribution(generator);
					
					SubUVData->ImageIndex = static_cast<float>(FrameIndex);
				}
				break;

			case EParticleSubUVInterpMethod::RandomBlend:
				// 일정 주기마다 새로운 랜덤 프레임으로 변경 (블렌딩 있음)
				// 소수부가 이전 프레임에서 다음 프레임으로의 알파 블렌딩 가중치
				{
					int32 Changes = RandomImageChanges > 0 ? RandomImageChanges : 1;
					
					// 현재 변경 구간 인덱스
					int32 CurrentChangeIndex = static_cast<int32>(Particle.RelativeTime * Changes);
					
					// 현재 구간 내에서의 진행도 (0.0 ~ 1.0)
					float ProgressInChange = fmodf(Particle.RelativeTime * Changes, 1.0f);
					
					// 현재 프레임 결정 (CurrentIndex 기반 시드로 일관성 유지)
					uint32 SeedCurrent = static_cast<uint32>(CurrentIndex + CurrentChangeIndex * 10000);
					std::default_random_engine generator(SeedCurrent);
					std::uniform_int_distribution<int32> distribution(0, TotalFrames - 1);
					int32 CurrentFrame = distribution(generator);
					
					// 소수부로 블렌딩 가중치 전달
					SubUVData->ImageIndex = static_cast<float>(CurrentFrame) + ProgressInChange;
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
		if (InOutHandle.hasKey("InterpolationMethod"))
		{
			int32 MethodInt = InOutHandle["InterpolationMethod"].ToInt();
			InterpolationMethod = static_cast<EParticleSubUVInterpMethod>(MethodInt);
		}
		if (InOutHandle.hasKey("bUseRealTime"))
		{
			bUseRealTime = InOutHandle["bUseRealTime"].ToBool();
		}
		if (InOutHandle.hasKey("StartingFrame"))
		{
			StartingFrame = InOutHandle["StartingFrame"].ToFloat();
		}
		if (InOutHandle.hasKey("FrameRate"))
		{
			FrameRate = InOutHandle["FrameRate"].ToFloat();
		}
		if (InOutHandle.hasKey("RandomImageChanges"))
		{
			RandomImageChanges = InOutHandle["RandomImageChanges"].ToInt();
		}
	}
	else
	{
		InOutHandle["InterpolationMethod"] = static_cast<int32>(InterpolationMethod);
		InOutHandle["bUseRealTime"] = bUseRealTime;
		InOutHandle["StartingFrame"] = StartingFrame;
		InOutHandle["FrameRate"] = FrameRate;
		InOutHandle["RandomImageChanges"] = RandomImageChanges;
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
	RandomImageChanges = SourceSubUV->RandomImageChanges;
}
