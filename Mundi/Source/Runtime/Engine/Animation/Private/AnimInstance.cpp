#include "pch.h"
#include "Source/Runtime/Engine/Animation/Public/AnimInstance.h"

#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/Public/AnimSequenceBase.h"

IMPLEMENT_CLASS(UAnimInstance)

UAnimInstance::UAnimInstance()
	: OwnerComponent(nullptr)
	, CurrentAnimation(nullptr)
	, CurrentTime(0.0f)
	, PreviousTime(0.0f)
	, PlayRate(1.0f)
	, bIsPlaying(false)
{
}

/**
 * @brief Animation 인스턴스 초기화
 * @param InOwner 소유한 스켈레탈 메쉬 컴포넌트
 */
void UAnimInstance::Initialize(USkeletalMeshComponent* InOwner)
{
	OwnerComponent = InOwner;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	bIsPlaying = false;
}

/**
 * @brief 매 프레임 업데이트 (Notify Trigger)
 * @param DeltaSeconds 델타 타임
 */
void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bIsPlaying || !CurrentAnimation)
	{
		return;
	}

	// 이전 시간 저장
	PreviousTime = CurrentTime;

	// 현재 시간 업데이트
	CurrentTime += DeltaSeconds * PlayRate;

	// Animation 길이 체크
	float AnimLength = CurrentAnimation->GetPlayLength();
	if (CurrentAnimation->IsLooping())
	{
		// 루프 Animation: 시간이 넘어가면 0으로 되돌림
		while (CurrentTime >= AnimLength)
		{
			CurrentTime -= AnimLength;
			PreviousTime -= AnimLength;
		}
	}
	else
	{
		// 루프가 아닌 Animation: 끝에 도달하면 정지
		if (CurrentTime >= AnimLength)
		{
			CurrentTime = AnimLength;
			bIsPlaying = false;
		}
	}

	// Notify Trigger
	TriggerAnimNotifies(DeltaSeconds);
}

/**
 * @brief Animation Notify Trigger 처리
 * @param DeltaSeconds 델타 타임
 */
void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (!CurrentAnimation || !OwnerComponent)
	{
		return;
	}

	// 현재 시간 구간에서 트리거되어야 할 Notify 수집
	TArray<const FAnimNotifyEvent*> TriggeredNotifies;
	CurrentAnimation->GetAnimNotifiesFromDeltaPositions(PreviousTime, CurrentTime, TriggeredNotifies);

	// 각 Notify 처리
	for (const FAnimNotifyEvent* NotifyEvent : TriggeredNotifies)
	{
		if (NotifyEvent)
		{
			HandleNotify(*NotifyEvent);
		}
	}
}

/**
 * @brief Animation 시퀀스 재생
 * @param AnimSequence 재생할 Animation 시퀀스
 * @param InPlayRate 재생 속도
 */
void UAnimInstance::PlayAnimation(UAnimSequenceBase* AnimSequence, float InPlayRate)
{
	if (!AnimSequence)
	{
		return;
	}

	CurrentAnimation = AnimSequence;
	PlayRate = InPlayRate;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	bIsPlaying = true;
}

void UAnimInstance::StopAnimation()
{
	bIsPlaying = false;
}

void UAnimInstance::SetPosition(float NewTime)
{
	PreviousTime = CurrentTime;
	CurrentTime = NewTime;

	if (CurrentAnimation)
	{
		float AnimLength = CurrentAnimation->GetPlayLength();
		if (CurrentTime < 0.0f)
		{
			CurrentTime = 0.0f;
		}
		else if (CurrentTime > AnimLength)
		{
			CurrentTime = AnimLength;
		}
	}
}

/**
 * @brief 개별 Notify 이벤트 처리 (오버라이드 가능)
 * @param NotifyEvent 트리거된 Notify 이벤트
 */
void UAnimInstance::HandleNotify(const FAnimNotifyEvent& NotifyEvent)
{
	// 기본 구현: SkeletalMeshComponent를 통해 Actor에게 전달
	if (OwnerComponent)
	{
		OwnerComponent->HandleAnimNotify(NotifyEvent);
	}
}
