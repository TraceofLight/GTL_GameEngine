#include "pch.h"
#include "AnimStateMachine.h"
#include "AnimationRuntime.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "Source/Runtime/Engine/Animation/PoseContext.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/Character.h"
#include "Source/Runtime/Engine/Components/CharacterMovementComponent.h"

IMPLEMENT_CLASS(UAnimationStateMachine)

UAnimationStateMachine::UAnimationStateMachine()
	: CurrentState(EAnimState::Idle)
	, PreviousState(EAnimState::Idle)
	, bIsTransitioning(false)
	, TransitionTime(0.0f)
	, TransitionDuration(0.3f)
	, OwnerPawn(nullptr)
	, OwnerCharacter(nullptr)
	, MovementComponent(nullptr)
	, WalkSpeed(300.0f)
	, RunSpeed(600.0f)
{
	// 상태별 애니메이션 배열 초기화
	StateAnimations.SetNum(static_cast<int32>(EAnimState::MAX));
	for (int32 i = 0; i < StateAnimations.Num(); ++i)
	{
		StateAnimations[i] = nullptr;
	}
}

/**
 * @brief State Machine 초기화
 */
void UAnimationStateMachine::Initialize(APawn* InPawn)
{
	OwnerPawn = InPawn;

	// Character로 캐스팅 시도
	OwnerCharacter = Cast<ACharacter>(InPawn);
	if (OwnerCharacter)
	{
		// MovementComponent 캐싱
		MovementComponent = OwnerCharacter->GetCharacterMovement();
	}

	// 초기 상태 결정
	CurrentState = DetermineStateFromMovement();
	PreviousState = CurrentState;
}

/**
 * @brief 매 프레임 상태 업데이트
 */
void UAnimationStateMachine::UpdateState(float DeltaSeconds)
{
	if (!OwnerPawn)
	{
		return;
	}

	// 전환 중이면 블렌딩 진행
	if (bIsTransitioning)
	{
		TransitionTime += DeltaSeconds;

		// 전환 완료
		if (TransitionTime >= TransitionDuration)
		{
			bIsTransitioning = false;
			PreviousState = CurrentState;
			TransitionTime = 0.0f;
		}

		return;
	}

	// 전환 가능한 상태 체크
	CheckTransitions();
}

/**
 * @brief 상태별 애니메이션 등록
 */
void UAnimationStateMachine::RegisterStateAnimation(EAnimState State, UAnimSequence* Animation)
{
	int32 StateIndex = static_cast<int32>(State);
	if (StateIndex >= 0 && StateIndex < StateAnimations.Num())
	{
		StateAnimations[StateIndex] = Animation;
	}
}

/**
 * @brief 전환 규칙 추가
 */
void UAnimationStateMachine::AddTransition(const FAnimStateTransition& Transition)
{
	Transitions.Add(Transition);
}

/**
 * @brief 현재 재생 중인 애니메이션 가져오기
 */
UAnimSequence* UAnimationStateMachine::GetCurrentAnimation() const
{
	int32 StateIndex = static_cast<int32>(CurrentState);
	if (StateIndex >= 0 && StateIndex < StateAnimations.Num())
	{
		return StateAnimations[StateIndex];
	}
	return nullptr;
}

/**
 * @brief 전환 진행도 가져오기 (0~1)
 */
float UAnimationStateMachine::GetTransitionAlpha() const
{
	if (!bIsTransitioning || TransitionDuration <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(TransitionTime / TransitionDuration, 0.0f, 1.0f);
}

/**
 * @brief 현재 포즈 평가 (블렌딩 적용)
 *
 * Phase 1의 BlendTwoPosesTogether를 사용하여 부드러운 전환.
 */
void UAnimationStateMachine::EvaluateCurrentPose(FPoseContext& OutPose)
{
	// 전환 중이 아니면 현재 상태 애니메이션만 반환
	if (!bIsTransitioning)
	{
		UAnimSequence* CurrentAnim = GetCurrentAnimation();
		if (CurrentAnim)
		{
			// TODO: CurrentAnim에서 현재 시간의 포즈 추출
			// 지금은 RefPose로 초기화
			OutPose.Initialize(CurrentAnim->GetDataModel()->GetSkeleton());
			OutPose.ResetToRefPose();
		}
		return;
	}

	// 전환 중이면 두 상태 블렌딩
	int32 PrevStateIndex = static_cast<int32>(PreviousState);
	int32 CurrStateIndex = static_cast<int32>(CurrentState);

	UAnimSequence* PrevAnim = (PrevStateIndex >= 0 && PrevStateIndex < StateAnimations.Num())
		? StateAnimations[PrevStateIndex]
		: nullptr;

	UAnimSequence* CurrAnim = (CurrStateIndex >= 0 && CurrStateIndex < StateAnimations.Num())
		? StateAnimations[CurrStateIndex]
		: nullptr;

	if (!PrevAnim || !CurrAnim)
	{
		return;
	}

	// 두 포즈 평가
	FPoseContext PrevPose, CurrPose;
	PrevPose.Initialize(PrevAnim->GetDataModel()->GetSkeleton());
	CurrPose.Initialize(CurrAnim->GetDataModel()->GetSkeleton());

	// TODO: 실제 애니메이션 시간에서 포즈 추출
	PrevPose.ResetToRefPose();
	CurrPose.ResetToRefPose();

	// Phase 1 블렌딩 사용!
	float BlendAlpha = GetTransitionAlpha();
	FAnimationRuntime::BlendTwoPosesTogether(PrevPose, CurrPose, BlendAlpha, OutPose);
}

/**
 * @brief 전환 가능한 상태 체크
 */
void UAnimationStateMachine::CheckTransitions()
{
	// Movement 기반 상태 결정
	EAnimState DesiredState = DetermineStateFromMovement();

	// 상태 변경 필요
	if (DesiredState != CurrentState)
	{
		TransitionToState(DesiredState);
	}
}

/**
 * @brief 특정 상태로 전환
 */
void UAnimationStateMachine::TransitionToState(EAnimState NewState)
{
	// 이미 같은 상태면 무시
	if (NewState == CurrentState && !bIsTransitioning)
	{
		return;
	}

	// 전환 시작
	PreviousState = CurrentState;
	CurrentState = NewState;
	bIsTransitioning = true;
	TransitionTime = 0.0f;

	// 기본 전환 시간 (0.2초)
	TransitionDuration = 0.2f;

	// 등록된 전환 규칙 체크하여 블렌딩 시간 조정
	for (const FAnimStateTransition& Trans : Transitions)
	{
		if (Trans.FromState == PreviousState && Trans.ToState == CurrentState)
		{
			TransitionDuration = Trans.BlendTime;
			break;
		}
	}
}

/**
 * @brief 현재 Movement 상태 기반으로 AnimState 결정
 */
EAnimState UAnimationStateMachine::DetermineStateFromMovement()
{
	if (!OwnerPawn)
	{
		return EAnimState::Idle;
	}

	// MovementComponent가 있는 경우 (Character)
	if (MovementComponent && OwnerCharacter)
	{
		EMovementMode MovementMode = MovementComponent->GetMovementMode();
		FVector Velocity = OwnerCharacter->GetVelocity();
		float Speed = Velocity.Size();

		// 이동 모드에 따른 상태 결정
		switch (MovementMode)
		{
		case EMovementMode::Walking:
		{
			// Crouch 체크 (향후 확장)
			// if (MovementComponent->IsCrouching())
			//     return EAnimState::Crouch;

			// 속도 기반 상태 결정
			if (Speed < 1.0f)
			{
				return EAnimState::Idle;
			}
			else if (Speed <= WalkSpeed)
			{
				return EAnimState::Walk;
			}
			else
			{
				return EAnimState::Run;
			}
		}

		case EMovementMode::Falling:
		{
			// 상승 중이면 Jump, 하강 중이면 Fall
			if (Velocity.Z > 0.0f)
			{
				return EAnimState::Jump;
			}
			else
			{
				return EAnimState::Fall;
			}
		}

		case EMovementMode::Flying:
		{
			return EAnimState::Fly;
		}

		case EMovementMode::None:
		default:
		{
			return EAnimState::Idle;
		}
		}
	}

	// MovementComponent 없으면 Idle (APawn은 GetVelocity 없음)
	if (!OwnerCharacter)
	{
		return EAnimState::Idle;
	}

	// Character의 속도 기반
	FVector Velocity = OwnerCharacter->GetVelocity();
	float Speed = Velocity.Size();

	if (Speed < 1.0f)
	{
		return EAnimState::Idle;
	}
	else if (Speed <= WalkSpeed)
	{
		return EAnimState::Walk;
	}
	else
	{
		return EAnimState::Run;
	}
}

/**
 * @brief 상태 이름 가져오기 (디버깅용)
 */
const char* UAnimationStateMachine::GetStateName(EAnimState State)
{
	switch (State)
	{
	case EAnimState::Idle:   return "Idle";
	case EAnimState::Walk:   return "Walk";
	case EAnimState::Run:    return "Run";
	case EAnimState::Jump:   return "Jump";
	case EAnimState::Fall:   return "Fall";
	case EAnimState::Fly:    return "Fly";
	case EAnimState::Crouch: return "Crouch";
	default:                 return "Unknown";
	}
}
