#pragma once

#include "AnimStateMachine.h"
#include "PoseContext.h"

class APawn;
class ACharacter;
class UAnimInstance;

/**
 * @brief State Machine 실행 노드
 *
 * UAnimStateMachine 애셋을 실행하는 인스턴스.
 * 현재 상태, 전환 진행도, 재생 시간 등 실행 데이터를 관리합니다.
 *
 * 언리얼의 FAnimNode_StateMachine과 유사한 구조.
 */
class FAnimNode_StateMachine
{
public:
	FAnimNode_StateMachine();
	~FAnimNode_StateMachine() = default;

	// ===== 초기화 및 업데이트 =====

	/**
	 * @brief 노드 초기화
	 * @param InPawn 소유 Pawn
	 */
	void Initialize(APawn* InPawn);

	/**
	 * @brief 매 프레임 업데이트
	 * ActiveNode의 상태를 체크하여 상태 전환.
	 * @param DeltaSeconds 델타 타임
	 */
	void Update(float DeltaSeconds);

	/**
	 * @brief 현재 포즈 평가 (블렌딩 적용)
	 * @param OutPose 출력 포즈
	 */
	void Evaluate(FPoseContext& OutPose);

	/**
	 * @brief State Machine 애셋 설정
	 * @param InStateMachine 사용할 State Machine 애셋
	 */
	void SetStateMachine(UAnimStateMachine* InStateMachine) { StateMachineAsset = InStateMachine; }

	/**
	 * @brief State Machine 애셋 가져오기
	 */
	UAnimStateMachine* GetStateMachine() const { return StateMachineAsset; }

	// ===== 상태 제어 =====

	/**
	 * @brief 강제 상태 변경
	 */
	void TransitionTo(FName NewStateName, float BlendTime = -1.0f);

	/**
	 * @brief 현재 상태 이름
	 */
	FName GetCurrentStateName() const { return CurrentStateName; }

	/**
	 * @brief 현재 재생 중인 애니메이션 가져오기
	 */
	UAnimSequence* GetCurrentAnimation() const
	{
		return ActiveNode ? ActiveNode->AnimationAsset : nullptr;
	}

	/**
	 * @brief 현재 애니메이션 재생 시간 가져오기
	 */
	float GetCurrentAnimTime() const { return CurrentAnimTime; }

	/**
	 * @brief 이전 프레임 애니메이션 시간 가져오기 (Notify 구간 판정용)
	 */
	float GetPreviousFrameAnimTime() const
	{
		return PreviousFrameAnimTime;
	}

// ===== 데이터 참조 =====
protected:
	/** 실행할 State Machine 에셋 데이터 */
	UAnimStateMachine* StateMachineAsset;

	/** 소유자 (데이터 접근용) */
	APawn* OwnerPawn;

	/** AnimInstance (데이터 접근용) */
	UAnimInstance* OwnerAnimInstance;

	// ===== 런타임 상태 =====

	/** 현재 상태 이름 */
	FName CurrentStateName;

	/** 현재 활성화된 노드 포인터 (매번 Map 검색 안 하려고 캐싱) */
	const FAnimStateNode* ActiveNode;

// ===== 트랜지션/블렌딩 관련 =====
protected:
	/** 이전 상태 이름 (블렌딩 소스) */
	FName PreviousStateName;

	/** 이전 노드 포인터 */
	const FAnimStateNode* PreviousNode;

	bool bIsTransitioning;
	float TransitionAlpha;    // 0.0 ~ 1.0
	float CurrentTransitionDuration; // 블렌딩 시간

	// ===== 애니메이션 재생 시간 =====
	float CurrentAnimTime;  // 현재 상태 재생 시간
	float PreviousAnimTime; // 이전 상태 재생 시간 (블렌딩용)
	float PreviousFrameAnimTime; // 이전 프레임의 CurrentAnimTime (Notify 구간 판정용)

// ===== 내부 로직 =====
protected:
	/**
	 * @brief 트랜지션 조건 검사
	 * * 현재 노드의 Transitions 배열을 순회하며 조건 만족 시 이동
	 */
	void CheckTransitions();

	// 내부 헬퍼 함수: 비교 연산 수행
	bool EvaluateCondition(float CurrentValue, EAnimConditionOp Op, float Threshold);
};
