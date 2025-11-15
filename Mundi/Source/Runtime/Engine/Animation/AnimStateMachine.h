#pragma once

#include "Object.h"
#include "PoseContext.h"

class APawn;
class ACharacter;
class UCharacterMovementComponent;
class UAnimSequence;

/**
 * @brief 애니메이션 상태 열거형
 *
 * Character의 이동/액션 상태를 나타냄.
 */
enum class EAnimState : uint8
{
	/** 정지 상태 */
	Idle,

	/** 걷기 */
	Walk,

	/** 달리기 */
	Run,

	/** 점프 (상승 중) */
	Jump,

	/** 낙하 (하강 중) */
	Fall,

	/** 비행 */
	Fly,

	/** 앉기 */
	Crouch,

	/** 상태 개수 (마지막) */
	MAX
};

/**
 * @brief 상태 전환 규칙
 *
 * 한 상태에서 다른 상태로 전환할 때의 조건과 블렌딩 시간.
 */
struct FAnimStateTransition
{
	/** 시작 상태 */
	EAnimState FromState;

	/** 목표 상태 */
	EAnimState ToState;

	/** 전환 블렌딩 시간 (초) */
	float BlendTime;

	FAnimStateTransition()
		: FromState(EAnimState::Idle)
		, ToState(EAnimState::Idle)
		, BlendTime(0.2f)
	{
	}

	FAnimStateTransition(EAnimState From, EAnimState To, float InBlendTime)
		: FromState(From)
		, ToState(To)
		, BlendTime(InBlendTime)
	{
	}
};

/**
 * @brief 애니메이션 State Machine
 *
 * Character의 이동 상태에 따라 자동으로 애니메이션을 전환.
 * Phase 1의 블렌딩 시스템을 활용하여 부드러운 전환 제공.
 *
 * 사용 예시:
 * ```cpp
 * UAnimationStateMachine* SM = NewObject<UAnimationStateMachine>();
 * SM->Initialize(MyCharacter);
 * SM->RegisterStateAnimation(EAnimState::Idle, IdleAnim);
 * SM->RegisterStateAnimation(EAnimState::Walk, WalkAnim);
 * SM->RegisterStateAnimation(EAnimState::Run, RunAnim);
 * ```
 */
class UAnimationStateMachine : public UObject
{
	DECLARE_CLASS(UAnimationStateMachine, UObject)

public:
	UAnimationStateMachine();
	virtual ~UAnimationStateMachine() override = default;

	// ===== Phase 3: State Machine 메서드 =====

	/**
	 * @brief State Machine 초기화
	 *
	 * @param InPawn 소유 Pawn (Character)
	 */
	void Initialize(APawn* InPawn);

	/**
	 * @brief 매 프레임 상태 업데이트
	 *
	 * Character의 Movement 상태를 체크하여 애니메이션 상태 전환.
	 *
	 * @param DeltaSeconds 델타 타임
	 */
	void UpdateState(float DeltaSeconds);

	/**
	 * @brief 상태별 애니메이션 등록
	 *
	 * @param State 애니메이션 상태
	 * @param Animation 재생할 애니메이션 시퀀스
	 */
	void RegisterStateAnimation(EAnimState State, UAnimSequence* Animation);

	/**
	 * @brief 전환 규칙 추가
	 *
	 * @param Transition 전환 규칙 구조체
	 */
	void AddTransition(const FAnimStateTransition& Transition);

	/**
	 * @brief 현재 상태 가져오기
	 */
	EAnimState GetCurrentState() const { return CurrentState; }

	/**
	 * @brief 현재 재생 중인 애니메이션 가져오기
	 */
	UAnimSequence* GetCurrentAnimation() const;

	/**
	 * @brief 전환 중인지 확인
	 */
	bool IsTransitioning() const { return bIsTransitioning; }

	/**
	 * @brief 전환 진행도 가져오기 (0~1)
	 */
	float GetTransitionAlpha() const;

	/**
	 * @brief 현재 포즈 평가 (블렌딩 적용)
	 *
	 * Phase 1의 블렌딩 시스템을 사용하여 전환 중 포즈 계산.
	 *
	 * @param OutPose 출력 포즈
	 */
	void EvaluateCurrentPose(FPoseContext& OutPose);

protected:
	/** 현재 상태 */
	EAnimState CurrentState;

	/** 이전 상태 (전환용) */
	EAnimState PreviousState;

	/** 전환 중인지 여부 */
	bool bIsTransitioning;

	/** 전환 경과 시간 */
	float TransitionTime;

	/** 전환 총 시간 */
	float TransitionDuration;

	/** 상태 → 애니메이션 매핑 */
	TArray<UAnimSequence*> StateAnimations;  // EAnimState를 인덱스로 사용

	/** 전환 규칙들 */
	TArray<FAnimStateTransition> Transitions;

	/** 소유 Pawn */
	APawn* OwnerPawn;

	/** 소유 Character (캐싱) */
	ACharacter* OwnerCharacter;

	/** Character Movement Component (캐싱) */
	UCharacterMovementComponent* MovementComponent;

	// ===== 상태 판별 임계값 =====

	/** 걷기 최대 속도 */
	float WalkSpeed;

	/** 달리기 최소 속도 */
	float RunSpeed;

	// ===== 내부 메서드 =====

	/**
	 * @brief 전환 가능한 상태 체크
	 */
	void CheckTransitions();

	/**
	 * @brief 특정 상태로 전환
	 *
	 * @param NewState 새로운 상태
	 */
	void TransitionToState(EAnimState NewState);

	/**
	 * @brief 현재 Movement 상태 기반으로 AnimState 결정
	 */
	EAnimState DetermineStateFromMovement();

	/**
	 * @brief 상태 이름 가져오기 (디버깅용)
	 */
	static const char* GetStateName(EAnimState State);
};
