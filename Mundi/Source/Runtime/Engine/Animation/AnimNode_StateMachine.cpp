#include "pch.h"
#include "AnimNode_StateMachine.h"
#include "AnimationRuntime.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "AnimInstance.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/Character.h"

FAnimNode_StateMachine::FAnimNode_StateMachine()
	: StateMachineAsset(nullptr), OwnerPawn(nullptr)
	  , OwnerAnimInstance(nullptr), ActiveNode(nullptr), PreviousNode(nullptr)
	  , bIsTransitioning(false), TransitionAlpha(0)
	  , CurrentTransitionDuration(0), CurrentAnimTime(0.0f), PreviousAnimTime(0.0f), PreviousFrameAnimTime(0.0f)
{
}

/**
 * @brief 노드 초기화
 */
void FAnimNode_StateMachine::Initialize(APawn* InPawn)
{
	OwnerPawn = InPawn;
	if (ACharacter* Character = Cast<ACharacter>(InPawn))
	{
		OwnerAnimInstance = Character->GetMesh()->GetAnimInstance();
	}

	if (StateMachineAsset)
	{
		FName EntryName = StateMachineAsset->GetEntryStateName();
		TransitionTo(EntryName, 0.0f);
	}
}

/**
 * @brief 매 프레임 업데이트
 */
void FAnimNode_StateMachine::Update(float DeltaSeconds)
{
	if (!ActiveNode) return;

	// 이전 프레임 시간 저장 (Notify 구간 판정용)
	PreviousFrameAnimTime = CurrentAnimTime;

	// 애니메이션 시간 갱신
	CurrentAnimTime += DeltaSeconds;
	if (ActiveNode->AnimationAsset && ActiveNode->bLoop)
	{
		// 루프 처리
		float Length = ActiveNode->AnimationAsset->GetPlayLength();
		if (Length > 0.f)
		{
			CurrentAnimTime = fmod(CurrentAnimTime, Length);
		}
	}

	// 블렌딩 중이라면 이전 애니메이션 시간도 갱신
	if (bIsTransitioning)
	{
		PreviousAnimTime += DeltaSeconds;
		TransitionAlpha += DeltaSeconds / CurrentTransitionDuration;

		if (TransitionAlpha >= 1.0f)
		{
			// 전환 완료
			bIsTransitioning = false;
			//  인터럽트 플래그 해제
			bIsInterruptedBlend = false;
			PreviousNode = nullptr;
			TransitionAlpha = 0.0f;
		}
	}

	// 트랜지션 체크
	CheckTransitions();
}

/**
 * @brief 현재 포즈 평가
 */
void FAnimNode_StateMachine::Evaluate(FPoseContext& OutPose)
{
	if (!ActiveNode) { return; }
	UAnimSequence* CurrentAnim = ActiveNode->AnimationAsset;
	CurrentAnim->GetDataModel()->Skeleton = const_cast<FSkeleton*>(OutPose.Skeleton); // 미친 테스트코드임 지금 Anim Skeleton이 댕글링이라 해놓음

	// ==========================================
	// 1. 타겟 포즈 계산
	// ==========================================
	FPoseContext TargetPose;
	if (OutPose.Skeleton) { TargetPose.Initialize(OutPose.Skeleton); }
	else { TargetPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num()); }

	if (CurrentAnim)
	{
		FAnimationRuntime::GetPoseFromAnimSequence(CurrentAnim, CurrentAnimTime, TargetPose);
	}

	// ==========================================
	// 2. 블렌딩 처리
	// ==========================================
	if (!bIsTransitioning)
	{
		// 전환 중 아니면 타겟 포즈 그대로 사용
		OutPose = TargetPose;
	}
	else
	{
		FPoseContext SourcePose;
		if (OutPose.Skeleton) { SourcePose.Initialize(OutPose.Skeleton); }
		else { SourcePose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num()); }

		// 인터럽트 상황인가?
		if (bIsInterruptedBlend && !FrozenSnapshotPose.empty())
		{
			if (SourcePose.LocalSpacePose.Num() != FrozenSnapshotPose.size())
			{
				// 크기가 다르면 강제로 맞춤 (T-Pose 방지)
				SourcePose.LocalSpacePose.SetNum(FrozenSnapshotPose.size());
			}

			SourcePose.LocalSpacePose = FrozenSnapshotPose;
		}
		else if (PreviousNode && PreviousNode->AnimationAsset)
		{
			// 일반 블렌딩
			FAnimationRuntime::GetPoseFromAnimSequence(PreviousNode->AnimationAsset, PreviousAnimTime, SourcePose);
		}

		// Source -> Target 블렌딩
		FAnimationRuntime::BlendTwoPosesTogether(SourcePose, TargetPose, TransitionAlpha, OutPose);
	}

	// ==========================================
	// 3. 다음 프레임을 위해 현재 포즈 백업 (스냅샷)
	// ==========================================
	LastFramePose = OutPose.LocalSpacePose;
}

void FAnimNode_StateMachine::TransitionTo(FName NewStateName, float BlendTime)
{
	if (CurrentStateName == NewStateName) { return; }

	const FAnimStateNode* NextNode = StateMachineAsset->FindNode(NewStateName);
	if (!NextNode) { return; }

	// [Case 1: 인터럽트 발생] (이미 전환 중일 때)
	if (bIsTransitioning)
	{
		// 목적지 변경
		CurrentStateName = NewStateName;
		ActiveNode = NextNode;
		CurrentAnimTime = 0.0f;

		FrozenSnapshotPose = LastFramePose;
		bIsInterruptedBlend = true;

		// 알파를 0으로 초기화해도, Source가 '방금 전 모습'이라서 튀지 않음
		CurrentTransitionDuration = BlendTime;
		TransitionAlpha = 0.0f;
	}
	// [Case 2: 일반 전환]
	else
	{
		PreviousNode = ActiveNode;
		PreviousAnimTime = CurrentAnimTime;

		CurrentStateName = NewStateName;
		ActiveNode = NextNode;
		CurrentAnimTime = 0.0f;

		// 일반 전환이므로 스냅샷 안 씀 (PreviousNode 애니메이션 사용)
		bIsInterruptedBlend = false;

		if (BlendTime > 0.0f)
		{
			bIsTransitioning = true;
			CurrentTransitionDuration = BlendTime;
			TransitionAlpha = 0.0f;
		}
		else
		{
			bIsTransitioning = false;
		}
	}
}

void FAnimNode_StateMachine::CheckTransitions()
{
	if (!ActiveNode) return;

	// AnimInstance가 없으면 변수를 못 읽으니 중단
	if (!OwnerAnimInstance) return;

	// 현재 노드의 모든 트랜지션 후보 검사
	for (const FAnimStateTransition& Transition : ActiveNode->Transitions)
	{
		bool bAllConditionsMet = true;

		// 트랜지션 내의 모든 조건(AND) 확인
		for (const FAnimCondition& Condition : Transition.Conditions)
		{
			float CurrentVal = OwnerAnimInstance->GetFloat(Condition.ParameterName);
			if (!EvaluateCondition(CurrentVal, Condition.Op, Condition.Threshold))
			{
				bAllConditionsMet = false;
				break; // 하나라도 틀리면 이 트랜지션은 실패
			}
		}

		if (bAllConditionsMet)
		{
			TransitionTo(Transition.TargetStateName, Transition.BlendTime);
			return;
		}
	}
}

bool FAnimNode_StateMachine::EvaluateCondition(float CurrentValue, EAnimConditionOp Op, float Threshold)
{
	switch (Op)
	{
		case EAnimConditionOp::Equals:          return FMath::IsNearlyEqual(CurrentValue, Threshold);
		case EAnimConditionOp::NotEquals:       return !FMath::IsNearlyEqual(CurrentValue, Threshold);
		case EAnimConditionOp::Greater:         return CurrentValue > Threshold;
		case EAnimConditionOp::Less:            return CurrentValue < Threshold;
		case EAnimConditionOp::GreaterOrEqual:  return CurrentValue >= Threshold;
		case EAnimConditionOp::LessOrEqual:     return CurrentValue <= Threshold;
		default: return false;
	}
}
