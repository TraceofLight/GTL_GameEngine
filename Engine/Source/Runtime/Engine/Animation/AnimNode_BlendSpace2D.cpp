#include "pch.h"
#include "AnimNode_BlendSpace2D.h"
#include "BlendSpace2D.h"
#include "AnimSequence.h"
#include "AnimationRuntime.h"
#include "AnimInstance.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/Character.h"
#include "Source/Runtime/Engine/Components/CharacterMovementComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"

FAnimNode_BlendSpace2D::FAnimNode_BlendSpace2D()
	: BlendSpace(nullptr)
	, BlendParameter(FVector2D::Zero())
	, bAutoCalculateParameter(true)
	, bPaused(false)
	, bLoop(true)
	, NormalizedTime(0.0f)
	, OwnerPawn(nullptr)
	, OwnerCharacter(nullptr)
	, MovementComponent(nullptr)
	, OwnerAnimInstance(nullptr)
	, OwnerMeshComp(nullptr)
	, PreviousLeaderIndex(-1)
{
}

FAnimNode_BlendSpace2D::~FAnimNode_BlendSpace2D()
{
}

/**
 * @brief 노드 초기화
 */
void FAnimNode_BlendSpace2D::Initialize(APawn* InPawn, UAnimInstance* InAnimInstance, USkeletalMeshComponent* InMeshComp)
{
	OwnerPawn = InPawn;
	OwnerAnimInstance = InAnimInstance;
	OwnerMeshComp = InMeshComp;

	if (OwnerPawn)
	{
		// Character로 캐스팅 시도
		OwnerCharacter = Cast<ACharacter>(OwnerPawn);

		if (OwnerCharacter)
		{
			MovementComponent = OwnerCharacter->GetCharacterMovement();
		}
	}

	// 샘플 애니메이션 시간 초기화
	if (BlendSpace)
	{
		int32 NumSamples = BlendSpace->GetNumSamples();
		SampleAnimTimes.SetNum(NumSamples);
		PreviousSampleAnimTimes.SetNum(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			SampleAnimTimes[i] = 0.0f;
			PreviousSampleAnimTimes[i] = 0.0f;
		}
	}

	// Notify 상태 초기화
	ActiveAnimNotifyState.Empty();
	PreviousLeaderIndex = -1;
}

/**
 * @brief BlendSpace 애셋 설정
 */
void FAnimNode_BlendSpace2D::SetBlendSpace(UBlendSpace2D* InBlendSpace)
{
	BlendSpace = InBlendSpace;

	// 샘플 애니메이션 시간 초기화
	if (BlendSpace)
	{
		int32 NumSamples = BlendSpace->GetNumSamples();
		SampleAnimTimes.SetNum(NumSamples);
		PreviousSampleAnimTimes.SetNum(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			SampleAnimTimes[i] = 0.0f;
			PreviousSampleAnimTimes[i] = 0.0f;
		}
	}

	// Notify 상태 초기화
	ActiveAnimNotifyState.Empty();
	PreviousLeaderIndex = -1;
}

/**
 * @brief 블렌드 파라미터 직접 설정
 */
void FAnimNode_BlendSpace2D::SetBlendParameter(FVector2D InParameter)
{
	BlendParameter = InParameter;
	bAutoCalculateParameter = false;

	// 디버그: 설정된 파라미터 확인
	static int32 SetParamCounter = 0;
	if (SetParamCounter++ % 30 == 0)
	{
		UE_LOG("[BlendSpace2D] SetBlendParameter: (%.2f, %.2f)",
			InParameter.X, InParameter.Y);
	}
}

/**
 * @brief 자동 파라미터 계산 활성화
 */
void FAnimNode_BlendSpace2D::SetAutoCalculateParameter(bool bEnable)
{
	bAutoCalculateParameter = bEnable;
}

/**
 * @brief 정규화된 재생 시간 설정 (0~1)
 */
void FAnimNode_BlendSpace2D::SetNormalizedTime(float InNormalizedTime)
{
	NormalizedTime = std::max(0.0f, std::min(1.0f, InNormalizedTime));

	// 모든 샘플의 시간을 정규화된 시간에 맞게 업데이트
	if (BlendSpace)
	{
		const int32 NumSamples = BlendSpace->GetNumSamples();
		for (int32 i = 0; i < NumSamples && i < SampleAnimTimes.Num(); ++i)
		{
			const FBlendSample& Sample = BlendSpace->Samples[i];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				float Duration = Sample.Animation->GetDataModel()->GetPlayLength();
				SampleAnimTimes[i] = NormalizedTime * Duration;
				if (i < PreviousSampleAnimTimes.Num())
				{
					PreviousSampleAnimTimes[i] = SampleAnimTimes[i];
				}
			}
		}
	}
}

/**
 * @brief 블렌딩된 애니메이션의 최대 길이 가져오기 (초)
 */
float FAnimNode_BlendSpace2D::GetMaxAnimationLength() const
{
	float MaxLength = 0.0f;

	if (BlendSpace)
	{
		const int32 NumSamples = BlendSpace->GetNumSamples();
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const FBlendSample& Sample = BlendSpace->Samples[i];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				float Duration = Sample.Animation->GetDataModel()->GetPlayLength();
				MaxLength = std::max(MaxLength, Duration);
			}
		}
	}

	return MaxLength;
}

/**
 * @brief 매 프레임 업데이트
 */
void FAnimNode_BlendSpace2D::Update(float DeltaSeconds)
{
	if (!BlendSpace)
	{
		return;
	}

	// 일시정지 상태면 시간 진행 없이 리턴 (Evaluate만 호출됨)
	if (bPaused)
	{
		return;
	}

	// 자동 파라미터 계산
	if (bAutoCalculateParameter)
	{
		//CalculateBlendParameterFromMovement();
	}

	// 디버그: BlendParameter 확인
	static int32 ParamLogCounter = 0;
	if (ParamLogCounter++ % 30 == 0)
	{
		UE_LOG("[BlendSpace2D] Update: BlendParameter=(%.2f, %.2f)",
			BlendParameter.X, BlendParameter.Y);
	}

	// 샘플 개수 확인 및 배열 크기 조정
	const int32 NumSamples = BlendSpace->GetNumSamples();
	if (SampleAnimTimes.Num() != NumSamples)
	{
		SampleAnimTimes.SetNum(NumSamples);
		PreviousSampleAnimTimes.SetNum(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			SampleAnimTimes[i] = 0.0f;
			PreviousSampleAnimTimes[i] = 0.0f;
		}
	}

	// 이전 시간 저장 (Notify 트리거용)
	for (int32 i = 0; i < NumSamples; ++i)
	{
		PreviousSampleAnimTimes[i] = SampleAnimTimes[i];
	}

	// Step 1: 블렌드 가중치 계산하여 Leader(가장 높은 가중치) 찾기
	TArray<int32> SampleIndices;
	TArray<float> Weights;
	BlendSpace->GetBlendWeights(BlendParameter, SampleIndices, Weights);

	int32 ReferenceSampleIndex = -1;
	float ReferenceDuration = 0.0f;
	float MaxWeight = 0.0f;

	// 가장 높은 가중치를 가진 샘플을 Leader로 선택
	for (int32 i = 0; i < SampleIndices.Num(); ++i)
	{
		int32 SampleIndex = SampleIndices[i];
		float Weight = Weights[i];

		if (Weight > MaxWeight && SampleIndex >= 0 && SampleIndex < NumSamples)
		{
			const FBlendSample& Sample = BlendSpace->Samples[SampleIndex];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				ReferenceSampleIndex = SampleIndex;
				ReferenceDuration = Sample.Animation->GetDataModel()->GetPlayLength();
				MaxWeight = Weight;
			}
		}
	}

	// 유효한 샘플이 없으면 중단
	if (ReferenceSampleIndex == -1)
	{
		return;
	}

	// Step 2: 기준 샘플의 시간 누적 (RateScale 적용)
	const FBlendSample& RefSample = BlendSpace->Samples[ReferenceSampleIndex];
	float LeaderRateScale = RefSample.RateScale;
	SampleAnimTimes[ReferenceSampleIndex] += DeltaSeconds * LeaderRateScale;

	// 루프 처리
	if (SampleAnimTimes[ReferenceSampleIndex] >= ReferenceDuration)
	{
		if (bLoop)
		{
			SampleAnimTimes[ReferenceSampleIndex] = fmod(SampleAnimTimes[ReferenceSampleIndex], ReferenceDuration);
		}
		else
		{
			// 루프 안 함 - 끝에서 클램프
			SampleAnimTimes[ReferenceSampleIndex] = ReferenceDuration - 0.001f;
		}
	}

	// 정규화된 시간 계산 (0~1)
	NormalizedTime = (ReferenceDuration > 0.0f) ? (SampleAnimTimes[ReferenceSampleIndex] / ReferenceDuration) : 0.0f;

	// Step 3: 나머지 샘플들을 기준 샘플과 동기화
	if (BlendSpace->bUseSyncMarkers)
	{
		// Sync Marker 기반 동기화
		SyncFollowersWithMarkers(ReferenceSampleIndex, SampleAnimTimes[ReferenceSampleIndex], DeltaSeconds);
	}
	else
	{
		// 기본 정규화된 시간 동기화
		for (int32 i = 0; i < NumSamples; ++i)
		{
			if (i == ReferenceSampleIndex)
			{
				continue; // 이미 업데이트함
			}

			const FBlendSample& Sample = BlendSpace->Samples[i];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				float Duration = Sample.Animation->GetDataModel()->GetPlayLength();
				SampleAnimTimes[i] = NormalizedTime * Duration;
			}
		}
	}

	// ========================================
	// Notify 트리거링 (Leader 애니메이션에서만)
	// ========================================
	if (!OwnerAnimInstance || !OwnerMeshComp)
	{
		// 디버그: AnimInstance/MeshComp가 설정되지 않은 경우
		static int32 WarnCounter = 0;
		if (WarnCounter++ % 300 == 0)
		{
			UE_LOG("BlendSpace2D: Update: OwnerAnimInstance=%p, OwnerMeshComp=%p (Notify disabled)",
				OwnerAnimInstance, OwnerMeshComp);
		}
	}

	if (OwnerAnimInstance && OwnerMeshComp && ReferenceSampleIndex >= 0)
	{
		const FBlendSample& LeaderSample = BlendSpace->Samples[ReferenceSampleIndex];
		if (LeaderSample.Animation)
		{
			float PreviousAnimTime = PreviousSampleAnimTimes[ReferenceSampleIndex];
			float CurrentAnimTime = SampleAnimTimes[ReferenceSampleIndex];
			float AnimLength = ReferenceDuration;

			// 루프 체크
			bool bLooped = (CurrentAnimTime < PreviousAnimTime) && (AnimLength > 0.0f);

			// 현재 프레임에서 활성화되어야 할 Notify 수집
			TArray<const FAnimNotifyEvent*> CurrentFrameNotifies;

			if (bLooped)
			{
				// 루프 시: 이전 시간 ~ 끝 + 0 ~ 현재 시간
				LeaderSample.Animation->GetAnimNotifiesFromDeltaPositions(PreviousAnimTime, AnimLength, CurrentFrameNotifies);
				LeaderSample.Animation->GetAnimNotifiesFromDeltaPositions(0.0f, CurrentAnimTime, CurrentFrameNotifies);
			}
			else
			{
				LeaderSample.Animation->GetAnimNotifiesFromDeltaPositions(PreviousAnimTime, CurrentAnimTime, CurrentFrameNotifies);
			}

			// 디버그: Notify 수집 결과
			if (CurrentFrameNotifies.Num() > 0)
			{
				UE_LOG("BlendSpace2D: Update: Found %d notifies (PrevTime=%.3f, CurrTime=%.3f, Leader=%d)",
					CurrentFrameNotifies.Num(), PreviousAnimTime, CurrentAnimTime, ReferenceSampleIndex);
			}

			// NewActiveAnimNotifyState 구축 (이번 프레임에 활성화될 NotifyState 목록)
			TArray<FAnimNotifyEvent> NewActiveAnimNotifyState;
			TArray<const FAnimNotifyEvent*> NotifyStateBeginEvents;
			TArray<const FAnimNotifyEvent*> NotifyStateEndEvents;

			for (const FAnimNotifyEvent* NotifyEvent : CurrentFrameNotifies)
			{
				if (!NotifyEvent)
				{
					continue;
				}

				if (NotifyEvent->Duration > 0.0f)
				{
					// NotifyState인 경우
					float EndTime = NotifyEvent->TriggerTime + NotifyEvent->Duration;

					// 현재 시간이 NotifyState 범위 내인지 확인
					bool bIsActive = false;
					if (bLooped)
					{
						bIsActive = (CurrentAnimTime >= NotifyEvent->TriggerTime && CurrentAnimTime <= EndTime) ||
							(EndTime > AnimLength && CurrentAnimTime <= (EndTime - AnimLength));
					}
					else
					{
						bIsActive = (CurrentAnimTime >= NotifyEvent->TriggerTime && CurrentAnimTime <= EndTime);
					}

					if (bIsActive)
					{
						NewActiveAnimNotifyState.Add(*NotifyEvent);

						// 이전 프레임에 활성 상태가 아니었으면 Begin
						bool bWasActive = false;
						for (const FAnimNotifyEvent& PrevNotify : ActiveAnimNotifyState)
						{
							if (PrevNotify.NotifyName == NotifyEvent->NotifyName &&
								FMath::Abs(PrevNotify.TriggerTime - NotifyEvent->TriggerTime) < 0.001f)
							{
								bWasActive = true;
								break;
							}
						}

						if (!bWasActive)
						{
							NotifyStateBeginEvents.Add(NotifyEvent);
						}
					}
				}
				else
				{
					// 일반 Notify (Duration == 0) → 즉시 실행
					UE_LOG("BlendSpace2D: Update: Triggering notify '%s' at time %.3f",
						NotifyEvent->NotifyName.ToString().c_str(), NotifyEvent->TriggerTime);
					OwnerAnimInstance->TriggerNotify(*NotifyEvent, OwnerMeshComp);
				}
			}

			// NotifyState End 이벤트 감지
			for (const FAnimNotifyEvent& PrevNotify : ActiveAnimNotifyState)
			{
				bool bStillActive = false;
				for (const FAnimNotifyEvent& NewNotify : NewActiveAnimNotifyState)
				{
					if (NewNotify.NotifyName == PrevNotify.NotifyName &&
						FMath::Abs(NewNotify.TriggerTime - PrevNotify.TriggerTime) < 0.001f)
					{
						bStillActive = true;
						break;
					}
				}

				if (!bStillActive)
				{
					NotifyStateEndEvents.Add(&PrevNotify);
				}
			}

			// NotifyState Begin/Tick/End 트리거
			for (const FAnimNotifyEvent* BeginNotify : NotifyStateBeginEvents)
			{
				OwnerAnimInstance->TriggerNotify(*BeginNotify, OwnerMeshComp);
			}

			// Tick은 현재 활성 상태인 모든 NotifyState에 대해 (필요시 구현)

			for (const FAnimNotifyEvent* EndNotify : NotifyStateEndEvents)
			{
				// End 이벤트 트리거 (필요시 구현)
			}

			// 5. ActiveAnimNotifyState 업데이트
			ActiveAnimNotifyState = NewActiveAnimNotifyState;
		}
	}

	// 이전 Leader 인덱스 업데이트
	PreviousLeaderIndex = ReferenceSampleIndex;

	// 디버그: 시간 동기화 및 Leader 정보 확인
	static int32 TimeLogCounter = 0;
	if (TimeLogCounter++ % 60 == 0)
	{
		UE_LOG("[BlendSpace2D] Update: NormalizedTime=%.3f, DeltaSeconds=%.4f, Leader=%d (Weight=%.3f)",
			NormalizedTime, DeltaSeconds, ReferenceSampleIndex, MaxWeight);
		for (int32 i = 0; i < SampleIndices.Num() && i < 3; ++i)
		{
			int32 SampleIdx = SampleIndices[i];
			const FBlendSample& Sample = BlendSpace->Samples[SampleIdx];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				UE_LOG("  Sample[%d]%s: Time=%.3f / %.2f (Weight=%.3f)",
					SampleIdx, (SampleIdx == ReferenceSampleIndex) ? " [LEADER]" : "",
					SampleAnimTimes[SampleIdx],
					Sample.Animation->GetDataModel()->GetPlayLength(),
					Weights[i]);
			}
		}
	}
}

/**
 * @brief 포즈 계산 (블렌딩 수행)
 */
void FAnimNode_BlendSpace2D::Evaluate(FPoseContext& OutPose)
{
	if (!BlendSpace || BlendSpace->GetNumSamples() == 0)
	{
		return;
	}

	// 블렌드 가중치 계산
	TArray<int32> SampleIndices;
	TArray<float> Weights;

	BlendSpace->GetBlendWeights(BlendParameter, SampleIndices, Weights);

	// 유효한 샘플이 없으면 중단
	if (SampleIndices.Num() == 0 || Weights.Num() == 0)
	{
		return;
	}

	// 포즈 배열 준비
	TArray<FPoseContext> SourcePoses;
	SourcePoses.SetNum(SampleIndices.Num());

	// 각 SourcePose 초기화 (Skeleton 설정)
	for (int32 i = 0; i < SourcePoses.Num(); ++i)
	{
		if (OutPose.Skeleton)
		{
			SourcePoses[i].Initialize(OutPose.Skeleton);
		}
		else
		{
			SourcePoses[i].LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num());
		}
	}

	// 각 샘플의 포즈 샘플링
	for (int32 i = 0; i < SampleIndices.Num(); ++i)
	{
		int32 SampleIndex = SampleIndices[i];
		if (SampleIndex < 0 || SampleIndex >= BlendSpace->Samples.Num())
		{
			continue;
		}

		const FBlendSample& Sample = BlendSpace->Samples[SampleIndex];
		if (!Sample.Animation)
		{
			continue;
		}

		// DataModel과 Skeleton 확인 및 설정
		if (!Sample.Animation->GetDataModel())
		{
			continue;
		}

		// Skeleton 설정 (OutPose에서 가져옴)
		if (OutPose.Skeleton)
		{
			Sample.Animation->GetDataModel()->SetSkeleton(*OutPose.Skeleton);
		}

		// 애니메이션 시간 가져오기
		float AnimTime = (SampleIndex < SampleAnimTimes.Num()) ? SampleAnimTimes[SampleIndex] : 0.0f;

		// 포즈 샘플링
		FAnimationRuntime::GetPoseFromAnimSequence(
			Sample.Animation,
			AnimTime,
			SourcePoses[i]
		);
	}

	// 여러 포즈를 가중치로 블렌딩
	FAnimationRuntime::BlendPosesTogetherPerBone(
		SourcePoses,
		Weights,
		OutPose
	);
}

/**
 * @brief Movement 상태에서 블렌드 파라미터 자동 계산
 */
void FAnimNode_BlendSpace2D::CalculateBlendParameterFromMovement()
{
	if (!MovementComponent || !OwnerCharacter)
	{
		BlendParameter = FVector2D::Zero();
		return;
	}

	// 속도 계산
	FVector Velocity = MovementComponent->GetVelocity();
	float Speed = Velocity.Size();

	// 방향 계산 (캐릭터 기준 로컬 방향)
	FVector Forward = OwnerCharacter->GetActorForward();
	FVector Right = OwnerCharacter->GetActorRight();

	// 속도 벡터를 로컬 좌표로 변환
	FVector LocalVelocity = Velocity;
	LocalVelocity.Normalize();

	float ForwardDot = FVector::Dot(LocalVelocity, Forward);
	float RightDot = FVector::Dot(LocalVelocity, Right);

	// 각도 계산 (라디안 → 도)
	float Angle = std::atan2(RightDot, ForwardDot) * (180.0f / 3.14159265f);

	// BlendParameter 설정
	// X = 속도, Y = 방향
	BlendParameter.X = Speed;
	BlendParameter.Y = Angle;

	static int32 LogCounter = 0;
	if (LogCounter++ % 60 == 0)
	{
		UE_LOG("[BlendSpace2D] Speed=%.1f, Angle=%.1f", Speed, Angle);
	}
}

/**
 * @brief Sync Marker 기반으로 Follower 애니메이션 동기화
 *
 * Leader 애니메이션의 Sync Marker를 기준으로 Follower들을 동기화합니다.
 * 예: Leader가 "LeftFoot" 마커를 지나면, Follower도 자기의 "LeftFoot" 마커로 맞춤
 */
void FAnimNode_BlendSpace2D::SyncFollowersWithMarkers(int32 LeaderIndex, float LeaderTime, float DeltaSeconds)
{
	if (!BlendSpace || LeaderIndex < 0 || LeaderIndex >= BlendSpace->Samples.Num())
	{
		return;
	}

	const FBlendSample& LeaderSample = BlendSpace->Samples[LeaderIndex];
	if (!LeaderSample.Animation)
	{
		return;
	}

	// Leader의 현재 시간 범위에서 Sync Marker 찾기
	float PrevTime = LeaderTime - DeltaSeconds;
	if (PrevTime < 0.0f) PrevTime = 0.0f;

	TArray<FAnimSyncMarker> PassedMarkers;
	LeaderSample.Animation->FindSyncMarkersInRange(PrevTime, LeaderTime, PassedMarkers);

	// 이번 프레임에 지나간 Sync Marker가 있으면 Follower들 동기화
	if (PassedMarkers.Num() > 0)
	{
		// 가장 최근에 지나간 마커 사용
		const FAnimSyncMarker& RecentMarker = PassedMarkers[PassedMarkers.Num() - 1];

		// 모든 Follower를 이 마커로 동기화
		for (int32 i = 0; i < BlendSpace->Samples.Num(); ++i)
		{
			if (i == LeaderIndex)
			{
				continue; // Leader는 스킵
			}

			const FBlendSample& FollowerSample = BlendSpace->Samples[i];
			if (!FollowerSample.Animation)
			{
				continue;
			}

			// Follower에서 같은 이름의 Sync Marker 찾기
			const FAnimSyncMarker* FollowerMarker = FollowerSample.Animation->FindSyncMarker(RecentMarker.MarkerName);
			if (FollowerMarker)
			{
				// Follower의 시간을 해당 Marker로 설정
				SampleAnimTimes[i] = FollowerMarker->Time;

				UE_LOG("[BlendSpace2D] Synced Sample[%d] to Marker '%s' at Time=%.3f",
					i, RecentMarker.MarkerName.c_str(), FollowerMarker->Time);
			}
			else
			{
				// Marker가 없으면 기본 정규화 시간 사용
				if (FollowerSample.Animation->GetDataModel())
				{
					float Duration = FollowerSample.Animation->GetDataModel()->GetPlayLength();
					SampleAnimTimes[i] = NormalizedTime * Duration;
				}
			}
		}
	}
	else
	{
		// Marker를 지나지 않았으면 기본 정규화 시간으로 동기화
		for (int32 i = 0; i < BlendSpace->Samples.Num(); ++i)
		{
			if (i == LeaderIndex)
			{
				continue;
			}

			const FBlendSample& Sample = BlendSpace->Samples[i];
			if (Sample.Animation && Sample.Animation->GetDataModel())
			{
				float Duration = Sample.Animation->GetDataModel()->GetPlayLength();
				SampleAnimTimes[i] = NormalizedTime * Duration;
			}
		}
	}
}
