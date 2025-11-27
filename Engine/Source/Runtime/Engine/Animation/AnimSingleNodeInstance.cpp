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
 * @brief 애니메이션 재생 (부모 클래스 오버라이드)

 * @param AnimSequence 재생할 애니메이션
 * @param InPlayRate 재생 속도
 */
void UAnimSingleNodeInstance::PlayAnimation(UAnimSequenceBase* AnimSequence, float InPlayRate)
{
	if (!AnimSequence)
	{
		return;
	}

	// AnimSequence로 다운캐스트 (UAnimSequenceBase는 추상 클래스)
	UAnimSequence* Sequence = dynamic_cast<UAnimSequence*>(AnimSequence);
	if (Sequence)
	{
		SetAnimationAsset(Sequence);
		SetPlayRate(InPlayRate);
		Play(true);  // 기본적으로 루프 활성화
	}
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
		// 루프 애니메이션
		if (PlayRate >= 0.0f)
		{
			// 정방향 재생: 시간이 넘어가면 0으로 되돌림
			while (CurrentTime >= AnimLength)
			{
				CurrentTime -= AnimLength;
				PreviousTime -= AnimLength;
			}
		}
		else
		{
			// 역방향 재생: 시간이 0보다 작아지면 끝으로 되돌림
			while (CurrentTime < 0.0f)
			{
				CurrentTime += AnimLength;
				PreviousTime += AnimLength;
			}
		}
	}
	else
	{
		// 루프가 아닌 애니메이션: 끝에 도달하면 정지
		if (PlayRate >= 0.0f)
		{
			// 정방향 재생
			if (CurrentTime >= AnimLength)
			{
				CurrentTime = AnimLength;
				bIsPlaying = false;
				OnAnimationEnd();
			}
		}
		else
		{
			// 역방향 재생
			if (CurrentTime <= 0.0f)
			{
				CurrentTime = 0.0f;
				bIsPlaying = false;
				OnAnimationEnd();
			}
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
