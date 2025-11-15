#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "AnimSequenceBase.h"

IMPLEMENT_CLASS(UAnimSingleNodeInstance)

UAnimSingleNodeInstance::UAnimSingleNodeInstance()
	: CurrentSequence(nullptr)
	, bLooping(true)
{
}

/**
 * @brief 재생할 애니메이션 에셋 설정
 */
void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequence* NewAnimToPlay)
{
	CurrentSequence = NewAnimToPlay;
	CurrentAnimation = NewAnimToPlay;  // 부모 클래스 필드도 동기화

	// 새 애니메이션 설정 시 시간 리셋
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
}

/**
 * @brief 애니메이션 재생 시작
 */
void UAnimSingleNodeInstance::Play(bool bInLooping)
{
	if (!CurrentSequence)
	{
		// 애니메이션이 설정되지 않았으면 경고
		return;
	}

	bLooping = bInLooping;
	bIsPlaying = true;

	// 처음부터 시작
	if (CurrentTime <= 0.0f)
	{
		CurrentTime = 0.0f;
		PreviousTime = 0.0f;
	}
}

/**
 * @brief 애니메이션 정지
 */
void UAnimSingleNodeInstance::Stop()
{
	bIsPlaying = false;
}

/**
 * @brief 재생 속도 설정
 */
void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
	PlayRate = InPlayRate;
}

/**
 * @brief 루핑 여부 설정
 */
void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
	bLooping = bInLooping;
}

/**
 * @brief 재생 시간을 특정 위치로 설정
 */
void UAnimSingleNodeInstance::SetPositionWithNotify(float InTimeSeconds, bool bFireNotifies)
{
	if (!CurrentSequence)
	{
		return;
	}

	PreviousTime = CurrentTime;
	CurrentTime = InTimeSeconds;

	// 애니메이션 길이 범위 내로 클램핑
	float AnimLength = CurrentSequence->GetPlayLength();
	if (CurrentTime < 0.0f)
	{
		CurrentTime = 0.0f;
	}
	else if (CurrentTime > AnimLength)
	{
		CurrentTime = AnimLength;
	}

	// Notify 발동
	if (bFireNotifies)
	{
		TriggerAnimNotifies(CurrentTime - PreviousTime);
	}
}

/**
 * @brief 매 프레임 업데이트 (Override)
 */
void UAnimSingleNodeInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bIsPlaying || !CurrentSequence)
	{
		return;
	}

	// 이전 시간 저장
	PreviousTime = CurrentTime;

	// 현재 시간 업데이트
	CurrentTime += DeltaSeconds * PlayRate;

	// 애니메이션 길이 체크
	float AnimLength = CurrentSequence->GetPlayLength();

	if (bLooping)
	{
		// 루프 애니메이션: 시간이 넘어가면 0으로 되돌림
		while (CurrentTime >= AnimLength)
		{
			CurrentTime -= AnimLength;
			PreviousTime -= AnimLength;

			// 루프 시점에 이벤트 발생 (필요시)
			// OnAnimationLooped();
		}
	}
	else
	{
		// 루프가 아닌 애니메이션: 끝에 도달하면 정지
		if (CurrentTime >= AnimLength)
		{
			CurrentTime = AnimLength;
			bIsPlaying = false;

			// 애니메이션 종료 이벤트
			OnAnimationEnd();
		}
	}

	// Notify 트리거
	TriggerAnimNotifies(DeltaSeconds);
}

/**
 * @brief 애니메이션이 끝에 도달했을 때 처리
 */
void UAnimSingleNodeInstance::OnAnimationEnd()
{
	// 서브클래스에서 오버라이드하여 종료 로직 구현 가능
	// 예: 다음 애니메이션 재생, 이벤트 발생 등
}
