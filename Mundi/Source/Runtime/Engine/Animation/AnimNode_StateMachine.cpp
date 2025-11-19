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
			PreviousStateName = FName();
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
    // Case A: 전환 중이 아님 (단일 애니메이션 재생)
    // ==========================================
    if (!bIsTransitioning)
    {
       if (CurrentAnim && CurrentAnim->GetDataModel())
       {
          FAnimationRuntime::GetPoseFromAnimSequence(CurrentAnim, CurrentAnimTime, OutPose);
       }
       return;
    }

    // ==========================================
    // Case B: 전환 중 (블렌딩)
    // ==========================================
    if (!PreviousNode)
    {
        if (CurrentAnim && CurrentAnim->GetDataModel())
        {
            FAnimationRuntime::GetPoseFromAnimSequence(CurrentAnim, CurrentAnimTime, OutPose);
        }
        return;
    }

    UAnimSequence* PrevAnim = PreviousNode->AnimationAsset;
    if (!PrevAnim || !PrevAnim->GetDataModel())
    {
        if (CurrentAnim) FAnimationRuntime::GetPoseFromAnimSequence(CurrentAnim, CurrentAnimTime, OutPose);
        return;
    }
    if (!CurrentAnim || !CurrentAnim->GetDataModel())
    {
        FAnimationRuntime::GetPoseFromAnimSequence(PrevAnim, PreviousAnimTime, OutPose);
        return;
    }

    // 두 포즈 샘플링

    // [Target] 현재 상태 포즈
    FPoseContext CurrentPose;
    if (OutPose.Skeleton)
    {
        CurrentPose.Initialize(OutPose.Skeleton);
    }
    else
    {
        CurrentPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num());
    }

    FAnimationRuntime::GetPoseFromAnimSequence(CurrentAnim, CurrentAnimTime, CurrentPose);

    // [Source] 이전 상태 포즈
    FPoseContext PreviousPose;
    if (OutPose.Skeleton)
    {
        PreviousPose.Initialize(OutPose.Skeleton);
    }
    else
    {
        PreviousPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num());
    }

    FAnimationRuntime::GetPoseFromAnimSequence(PrevAnim, PreviousAnimTime, PreviousPose);

    // 최종 블렌딩 (Source -> Target)
    float BlendAlpha = TransitionAlpha;
    FAnimationRuntime::BlendTwoPosesTogether(PreviousPose, CurrentPose, BlendAlpha, OutPose);
}

void FAnimNode_StateMachine::TransitionTo(FName NewStateName, float BlendTime)
{
	if (CurrentStateName == NewStateName) { return; }

	const FAnimStateNode* NextNode = StateMachineAsset->FindNode(NewStateName);
	if (!NextNode) { return; }

	PreviousStateName = CurrentStateName;
	PreviousNode = ActiveNode;
	PreviousAnimTime = CurrentAnimTime;

	CurrentStateName = NewStateName;
	ActiveNode = NextNode;
	CurrentAnimTime = 0.0f;

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

void FAnimNode_StateMachine::CheckTransitions()
{
	if (!ActiveNode || bIsTransitioning) return;

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
