#include "pch.h"
#include "AnimNode_StateMachine.h"
#include "AnimationRuntime.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/Character.h"
#include "Source/Runtime/Engine/Components/CharacterMovementComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

FAnimNode_StateMachine::FAnimNode_StateMachine()
	: StateMachine(nullptr)
	, CurrentState(EAnimState::Idle)
	, PreviousState(EAnimState::Idle)
	, bIsTransitioning(false)
	, TransitionTime(0.0f)
	, TransitionDuration(0.3f)
	, CurrentAnimTime(0.0f)
	, PreviousAnimTime(0.0f)
	, OwnerPawn(nullptr)
	, OwnerCharacter(nullptr)
	, MovementComponent(nullptr)
	, WalkSpeed(30.0f)  // Walk/Run 전환 속도 (MaxWalkSpeed 60의 절반)
	, RunSpeed(60.0f)   // 최대 속도
{
}

/**
 * @brief 노드 초기화
 */
void FAnimNode_StateMachine::Initialize(APawn* InPawn)
{
	OwnerPawn = InPawn;

	// Character로 캐스팅 시도
	OwnerCharacter = Cast<ACharacter>(InPawn);
	if (OwnerCharacter)
	{
		// MovementComponent 캐싱
		MovementComponent = OwnerCharacter->GetCharacterMovement();
	}

	// 초기 상태 설정
	CurrentState = EAnimState::Idle;
	PreviousState = EAnimState::Idle;
	bIsTransitioning = false;
	TransitionTime = 0.0f;
	CurrentAnimTime = 0.0f;
	PreviousAnimTime = 0.0f;
}

/**
 * @brief State Machine 애셋 설정
 */
void FAnimNode_StateMachine::SetStateMachine(UAnimStateMachine* InStateMachine)
{
	StateMachine = InStateMachine;
}

/**
 * @brief 매 프레임 업데이트
 */
void FAnimNode_StateMachine::Update(float DeltaSeconds)
{
	if (!StateMachine)
	{
		return;
	}

	// 전환 중이면 전환 진행
	if (bIsTransitioning)
	{
		TransitionTime += DeltaSeconds;

		// 이전 애니메이션 시간 업데이트
		if (StateMachine->GetStateAnimation(PreviousState))
		{
			UAnimSequence* PrevAnim = StateMachine->GetStateAnimation(PreviousState);
			PreviousAnimTime += DeltaSeconds;

			// 루프 처리
			if (PrevAnim && PrevAnim->GetDataModel())
			{
				float PrevDuration = PrevAnim->GetDataModel()->GetPlayLength();
				if (PreviousAnimTime > PrevDuration)
				{
					PreviousAnimTime = fmod(PreviousAnimTime, PrevDuration);
				}
			}
		}

		// 전환 완료 체크
		if (TransitionTime >= TransitionDuration)
		{
			bIsTransitioning = false;
			TransitionTime = 0.0f;
			PreviousState = CurrentState;
		}
	}

	// 현재 애니메이션 시간 업데이트
	if (StateMachine->GetStateAnimation(CurrentState))
	{
		UAnimSequence* CurrentAnim = StateMachine->GetStateAnimation(CurrentState);
		CurrentAnimTime += DeltaSeconds;

		// 루프 처리
		if (CurrentAnim && CurrentAnim->GetDataModel())
		{
			float CurrentDuration = CurrentAnim->GetDataModel()->GetPlayLength();
			if (CurrentAnimTime > CurrentDuration)
			{
				CurrentAnimTime = fmod(CurrentAnimTime, CurrentDuration);
			}
		}
	}

	// 전환 가능한 상태 체크
	CheckTransitions();
}

/**
 * @brief 전환 진행도 가져오기
 */
float FAnimNode_StateMachine::GetTransitionAlpha() const
{
	if (!bIsTransitioning || TransitionDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(TransitionTime / TransitionDuration, 0.0f, 1.0f);
}

/**
 * @brief 현재 재생 중인 애니메이션 가져오기
 */
UAnimSequence* FAnimNode_StateMachine::GetCurrentAnimation() const
{
	if (!StateMachine)
	{
		return nullptr;
	}

	return StateMachine->GetStateAnimation(CurrentState);
}

/**
 * @brief 현재 포즈 평가
 */
void FAnimNode_StateMachine::Evaluate(FPoseContext& OutPose)
{
	if (!StateMachine)
	{
		return;
	}

	// 전환 중이 아니면 현재 상태의 애니메이션만 재생
	if (!bIsTransitioning)
	{
		UAnimSequence* CurrentAnim = StateMachine->GetStateAnimation(CurrentState);
		if (!CurrentAnim || !CurrentAnim->GetDataModel())
		{
			return;
		}

		// 포즈 샘플링
		FAnimationRuntime::GetPoseFromAnimSequence(
			CurrentAnim,
			CurrentAnimTime,
			OutPose
		);

		return;
	}

	// 전환 중이면 두 포즈를 블렌딩
	UAnimSequence* PrevAnim = StateMachine->GetStateAnimation(PreviousState);
	UAnimSequence* CurrentAnim = StateMachine->GetStateAnimation(CurrentState);

	if (!PrevAnim || !CurrentAnim)
	{
		return;
	}

	if (!PrevAnim->GetDataModel() || !CurrentAnim->GetDataModel())
	{
		return;
	}

	// 이전 포즈 샘플링
	FPoseContext PreviousPose;
	PreviousPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num());
	FAnimationRuntime::GetPoseFromAnimSequence(
		PrevAnim,
		PreviousAnimTime,
		PreviousPose
	);

	// 현재 포즈 샘플링
	FPoseContext CurrentPose;
	CurrentPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num());
	FAnimationRuntime::GetPoseFromAnimSequence(
		CurrentAnim,
		CurrentAnimTime,
		CurrentPose
	);

	// 블렌딩
	float BlendAlpha = GetTransitionAlpha();
	FAnimationRuntime::BlendTwoPosesTogether(
		PreviousPose,
		CurrentPose,
		BlendAlpha,
		OutPose
	);
}

/**
 * @brief 전환 가능한 상태 체크
 */
void FAnimNode_StateMachine::CheckTransitions()
{
	if (!StateMachine || !MovementComponent)
	{
		return;
	}

	// Movement 상태 기반으로 목표 상태 결정
	EAnimState TargetState = DetermineStateFromMovement();

	// 현재 상태와 다르면 전환 시도
	if (TargetState != CurrentState && !bIsTransitioning)
	{
		TransitionToState(TargetState);
	}
}

/**
 * @brief 특정 상태로 전환
 */
void FAnimNode_StateMachine::TransitionToState(EAnimState NewState)
{
	if (!StateMachine)
	{
		return;
	}

	// 전환 규칙에서 블렌드 시간 찾기
	float BlendTime = StateMachine->FindTransitionBlendTime(CurrentState, NewState);

	// 전환 시작
	PreviousState = CurrentState;
	CurrentState = NewState;
	bIsTransitioning = true;
	TransitionTime = 0.0f;
	TransitionDuration = BlendTime;
	PreviousAnimTime = CurrentAnimTime;
	CurrentAnimTime = 0.0f;
}

/**
 * @brief Movement 상태 기반으로 AnimState 결정
 */
EAnimState FAnimNode_StateMachine::DetermineStateFromMovement()
{
	if (!MovementComponent)
	{
		return EAnimState::Idle;
	}

	// 속도 가져오기
	FVector Velocity = MovementComponent->GetVelocity();
	float Speed = Velocity.Size();

	// 공중에 있으면
	if (MovementComponent->IsFalling())
	{
		return EAnimState::Fall;
	}

	// 속도에 따라 상태 결정
	if (Speed < 1.0f)
	{
		return EAnimState::Idle;
	}
	else if (Speed < WalkSpeed)
	{
		return EAnimState::Walk;
	}
	else
	{
		return EAnimState::Run;
	}
}
