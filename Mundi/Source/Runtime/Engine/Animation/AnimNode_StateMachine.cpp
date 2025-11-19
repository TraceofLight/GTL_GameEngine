#include "pch.h"
#include "AnimNode_StateMachine.h"
#include "AnimationRuntime.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "AnimInstance.h"
#include "SkeletalMeshComponent.h"
#include "BlendSpace2D.h"
#include "AnimNode_BlendSpace2D.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/Character.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Engine/Scripting/LuaManager.h"

FAnimNode_StateMachine::FAnimNode_StateMachine()
	: StateMachineAsset(nullptr), OwnerPawn(nullptr)
	  , OwnerAnimInstance(nullptr), ActiveNode(nullptr), PreviousNode(nullptr)
	  , bIsInterruptedBlend(false), bIsTransitioning(false), TransitionAlpha(0)
	  , CurrentTransitionDuration(0), CurrentAnimTime(0.0f), PreviousAnimTime(0.0f), PreviousFrameAnimTime(0.0f)
	  , PreviousFrameTransitionAlpha(0.0f), bHasTriggeredFullyBlended(false)
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

	// 현재 노드가 BlendSpace2D인 경우 노드 업데이트
	if (ActiveNode->AnimAssetType == EAnimAssetType::BlendSpace2D && ActiveNode->BlendSpaceAsset)
	{
		// AnimInstance에서 BlendSpace2D 파라미터 가져오기
		if (OwnerAnimInstance)
		{
			UBlendSpace2D* BS = ActiveNode->BlendSpaceAsset;
			float X = OwnerAnimInstance->GetFloat(FName(BS->XAxisName));
			float Y = OwnerAnimInstance->GetFloat(FName(BS->YAxisName));

			static int32 LogCounter = 0;
			if (LogCounter++ % 30 == 0)
			{
				UE_LOG("[StateMachine] BlendSpace2D Parameters: %s=%.2f, %s=%.2f",
					BS->XAxisName.c_str(), X, BS->YAxisName.c_str(), Y);
			}

			CurrentBlendSpaceNode.SetBlendParameter(FVector2D(X, Y));
		}
		else
		{
			static int32 WarnCounter = 0;
			if (WarnCounter++ % 60 == 0)
			{
				UE_LOG("[StateMachine] Warning: OwnerAnimInstance is nullptr!");
			}
		}

		CurrentBlendSpaceNode.Update(DeltaSeconds);
	}
	else if (ActiveNode->AnimAssetType == EAnimAssetType::AnimSequence)
	{
		// 이전 프레임 시간 저장 (Notify 구간 판정용)
		PreviousFrameAnimTime = CurrentAnimTime;
		// 애니메이션 시간 갱신
		CurrentAnimTime += DeltaSeconds;

		float AnimLength = 0.0f;
		if (ActiveNode->AnimationAsset)
		{
			AnimLength = ActiveNode->AnimationAsset->GetPlayLength();
		}

		bool bLooped = false;
		if (ActiveNode->AnimationAsset && ActiveNode->bLoop && AnimLength > 0.f)
		{
			// 루프 처리
			if (CurrentAnimTime >= AnimLength)
			{
				bLooped = true;
				CurrentAnimTime = fmod(CurrentAnimTime, AnimLength);
			}
		}

		// AnimSequence 자체의 Notify 처리
		if (ActiveNode->AnimationAsset && OwnerAnimInstance)
		{
			USkeletalMeshComponent* MeshComp = nullptr;
			if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
			{
				MeshComp = Character->GetMesh();
			}

			if (MeshComp)
			{
				AActor* Owner = MeshComp->GetOwner();
				UWorld* World = Owner ? Owner->GetWorld() : nullptr;
				FLuaManager* LuaMgr = World ? World->GetLuaManager() : nullptr;

				if (LuaMgr)
				{
					// 1. 현재 프레임에서 활성화되어야 할 Notify 수집
					TArray<const FAnimNotifyEvent*> CurrentFrameNotifies;

					if (bLooped)
					{
						// 루프 시: 이전 시간 ~ 끝 + 0 ~ 현재 시간
						ActiveNode->AnimationAsset->GetAnimNotifiesFromDeltaPositions(PreviousFrameAnimTime, AnimLength, CurrentFrameNotifies);
						ActiveNode->AnimationAsset->GetAnimNotifiesFromDeltaPositions(0.0f, CurrentAnimTime, CurrentFrameNotifies);
					}
					else
					{
						ActiveNode->AnimationAsset->GetAnimNotifiesFromDeltaPositions(PreviousFrameAnimTime, CurrentAnimTime, CurrentFrameNotifies);
					}

					// 2. NewActiveAnimNotifyState 구축 (이번 프레임에 활성화될 NotifyState 목록)
					TArray<FAnimNotifyEvent> NewActiveAnimNotifyState;
					TArray<const FAnimNotifyEvent*> NotifyStateBeginEvents;

					for (const FAnimNotifyEvent* NotifyEvent : CurrentFrameNotifies)
					{
						if (!NotifyEvent)
						{
							continue;
						}

						// AnimNotifyState (Duration > 0)
						if (NotifyEvent->Duration > 0.0f)
						{
							// 이미 ActiveAnimNotifyState에 있는지 확인
							bool bAlreadyActive = false;
							for (int32 i = 0; i < ActiveAnimNotifyState.Num(); ++i)
							{
								if (ActiveAnimNotifyState[i] == *NotifyEvent)
								{
									// 이미 활성화된 NotifyState → ActiveAnimNotifyState에서 제거 (NewActive로 이동)
									ActiveAnimNotifyState.erase(ActiveAnimNotifyState.begin() + i);
									bAlreadyActive = true;
									break;
								}
							}

							if (!bAlreadyActive)
							{
								// 새로 시작하는 NotifyState → NotifyBegin 호출 대기열에 추가
								NotifyStateBeginEvents.Add(NotifyEvent);
							}

							// NewActiveAnimNotifyState에 추가 (이번 프레임에도 계속 활성)
							NewActiveAnimNotifyState.Add(*NotifyEvent);
						}
						else
						{
							// 일반 Notify (Duration == 0) → 즉시 실행
							OwnerAnimInstance->TriggerNotify(*NotifyEvent, MeshComp);
						}
					}

					// 3. ActiveAnimNotifyState에 남아있는 항목들 → 더 이상 활성화되지 않음 → NotifyEnd 호출
					for (int32 i = 0; i < ActiveAnimNotifyState.Num(); ++i)
					{
						const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[i];
						LuaMgr->ExecuteNotifyStateEnd(AnimNotifyEvent.NotifyName.ToString(), AnimNotifyEvent.PropertyData, MeshComp, CurrentAnimTime);
					}

					// 4. 새로 시작하는 NotifyState → NotifyBegin 호출
					for (const FAnimNotifyEvent* NotifyEvent : NotifyStateBeginEvents)
					{
						LuaMgr->ExecuteNotifyStateBegin(NotifyEvent->NotifyName.ToString(), NotifyEvent->PropertyData, MeshComp, CurrentAnimTime);
					}

					// 5. ActiveAnimNotifyState 교체
					ActiveAnimNotifyState = std::move(NewActiveAnimNotifyState);

					// 6. 현재 활성화된 모든 NotifyState → NotifyTick 호출
					for (int32 i = 0; i < ActiveAnimNotifyState.Num(); ++i)
					{
						const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[i];
						LuaMgr->ExecuteNotifyStateTick(AnimNotifyEvent.NotifyName.ToString(), AnimNotifyEvent.PropertyData, MeshComp, CurrentAnimTime, DeltaSeconds);
					}
				}
			}
		}
	}

	// 블렌딩 중이라면 이전 애니메이션/BlendSpace도 갱신
	if (bIsTransitioning)
	{
		if (PreviousNode && PreviousNode->AnimAssetType == EAnimAssetType::BlendSpace2D && PreviousNode->BlendSpaceAsset)
		{
			// AnimInstance에서 BlendSpace2D 파라미터 가져오기
			if (OwnerAnimInstance)
			{
				UBlendSpace2D* BS = PreviousNode->BlendSpaceAsset;
				float X = OwnerAnimInstance->GetFloat(FName(BS->XAxisName));
				float Y = OwnerAnimInstance->GetFloat(FName(BS->YAxisName));

				PreviousBlendSpaceNode.SetBlendParameter(FVector2D(X, Y));
			}

			PreviousBlendSpaceNode.Update(DeltaSeconds);
		}
		else
		{
			PreviousAnimTime += DeltaSeconds;
		}

		PreviousFrameTransitionAlpha = TransitionAlpha;
		TransitionAlpha += DeltaSeconds / CurrentTransitionDuration;

		// Transition Notify 트리거 (진행률 기반)
		const FAnimStateNode* PrevNode = StateMachineAsset->FindNode(PreviousStateName);
		if (PrevNode)
		{
			for (const FAnimStateTransition& Trans : PrevNode->Transitions)
			{
				if (Trans.TargetStateName == CurrentStateName)
				{
					TriggerTransitionNotifies(&Trans, PreviousFrameTransitionAlpha, TransitionAlpha);
					break;
				}
			}
		}

		if (TransitionAlpha >= 1.0f)
		{
			// 전환 완료 - State Fully Blended Notify 트리거
			if (!bHasTriggeredFullyBlended && ActiveNode)
			{
				TriggerStateFullyBlendedNotifies(ActiveNode);
				bHasTriggeredFullyBlended = true;
			}

			bIsTransitioning = false;
			//  인터럽트 플래그 해제
			bIsInterruptedBlend = false;
			PreviousNode = nullptr;
			TransitionAlpha = 0.0f;
			PreviousFrameTransitionAlpha = 0.0f;
		}
	}

	// 트랜지션 체크
	CheckTransitions();
}

/**
 * @brief 헬퍼: 노드에서 포즈 가져오기 (AnimSequence 또는 BlendSpace2D)
 */
static void GetPoseFromNode(const FAnimStateNode* Node, float Time, FPoseContext& OutPose, FAnimNode_BlendSpace2D* BlendSpaceNode)
{
	if (!Node) { return; }

	if (Node->AnimAssetType == EAnimAssetType::AnimSequence && Node->AnimationAsset)
	{
		UAnimSequence* Anim = Node->AnimationAsset;
		if (Anim->GetDataModel())
		{
			Anim->GetDataModel()->SetSkeleton(*OutPose.Skeleton);
			FAnimationRuntime::GetPoseFromAnimSequence(Anim, Time, OutPose);
		}
	}
	else if (Node->AnimAssetType == EAnimAssetType::BlendSpace2D && Node->BlendSpaceAsset && BlendSpaceNode)
	{
		// BlendSpace2D 노드를 통해 실제 블렌딩된 포즈 가져오기
		BlendSpaceNode->Evaluate(OutPose);
	}
}

/**
 * @brief 현재 포즈 평가
 */
void FAnimNode_StateMachine::Evaluate(FPoseContext& OutPose)
{
    if (!ActiveNode) { return; }

	// ==========================================
	// 1. 타겟 포즈 계산
	// ==========================================
	FPoseContext TargetPose;
	if (OutPose.Skeleton) { TargetPose.Initialize(OutPose.Skeleton); }
	else { TargetPose.LocalSpacePose.SetNum(OutPose.LocalSpacePose.Num()); }

	if (ActiveNode->AnimAssetType == EAnimAssetType::None)
	{
		// None 상태라면: 직전 프레임의 포즈를 유지 (Freeze)
		// 단, LastFramePose가 유효해야 함 (첫 프레임 제외)
		if (LastFramePose.Num() > 0)
		{
			// 본 개수가 맞는지 안전장치 확인
			if (TargetPose.LocalSpacePose.Num() != LastFramePose.size())
			{
				TargetPose.LocalSpacePose.SetNum(static_cast<int32>(LastFramePose.size()));
			}

			// 스냅샷을 타겟 포즈로 복사 -> 멈춤 효과
			TargetPose.LocalSpacePose = LastFramePose;
		}
		// LastFramePose가 비어있다면(게임 시작 직후 등) 그냥 초기화된 T-Pose 사용
	}
	else
	{
		GetPoseFromNode(ActiveNode, CurrentAnimTime, TargetPose, &CurrentBlendSpaceNode);
	}

	// ==========================================
	// 2. 블렌딩 처리
	// ==========================================
	if (!bIsTransitioning)
	{
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
				SourcePose.LocalSpacePose.SetNum(static_cast<int32>(FrozenSnapshotPose.size()));
			}

			SourcePose.LocalSpacePose = FrozenSnapshotPose;
		}
		else if (PreviousNode)
		{
			GetPoseFromNode(PreviousNode, PreviousAnimTime, SourcePose, &PreviousBlendSpaceNode);
		}
		TargetPose.Skeleton = SourcePose.Skeleton;

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

	// State Exit Notify 트리거 (이전 상태 나가기)
	if (ActiveNode)
	{
		TriggerStateExitNotifies(ActiveNode);
	}

	// 이전 애니메이션의 활성 NotifyState 종료
	if (!ActiveAnimNotifyState.IsEmpty())
	{
		USkeletalMeshComponent* MeshComp = nullptr;
		if (ACharacter* Character = Cast<ACharacter>(OwnerPawn))
		{
			MeshComp = Character->GetMesh();
		}

		if (MeshComp)
		{
			AActor* Owner = MeshComp->GetOwner();
			UWorld* World = Owner ? Owner->GetWorld() : nullptr;
			FLuaManager* LuaMgr = World ? World->GetLuaManager() : nullptr;

			if (LuaMgr)
			{
				for (const FAnimNotifyEvent& AnimNotifyEvent : ActiveAnimNotifyState)
				{
					LuaMgr->ExecuteNotifyStateEnd(AnimNotifyEvent.NotifyName.ToString(), AnimNotifyEvent.PropertyData, MeshComp, CurrentAnimTime);
				}
			}
		}
		ActiveAnimNotifyState.Empty();
	}

	// 이전 상태 저장
	PreviousStateName = CurrentStateName;
	PreviousNode = ActiveNode;
	PreviousAnimTime = CurrentAnimTime;

	// 이전 BlendSpace2D 노드 백업 (블렌딩용)
	if (ActiveNode && ActiveNode->AnimAssetType == EAnimAssetType::BlendSpace2D)
	{
		PreviousBlendSpaceNode = CurrentBlendSpaceNode;
	}

	// 새 상태로 전환
	CurrentStateName = NewStateName;
	ActiveNode = NextNode;
	CurrentAnimTime = 0.0f;
	bHasTriggeredFullyBlended = false; // 새 상태 진입 시 초기화

	// State Entry Notify 트리거 (새 상태 진입)
	TriggerStateEntryNotifies(ActiveNode);

	// 새 상태가 BlendSpace2D라면 노드 초기화
	if (NextNode->AnimAssetType == EAnimAssetType::BlendSpace2D && NextNode->BlendSpaceAsset)
	{
		CurrentBlendSpaceNode.SetBlendSpace(NextNode->BlendSpaceAsset);
		CurrentBlendSpaceNode.Initialize(OwnerPawn);
	}

	// [Case 1: 인터럽트 발생] (이미 전환 중일 때)
	if (bIsTransitioning)
	{
		FrozenSnapshotPose = LastFramePose;
		bIsInterruptedBlend = true;

		// 알파를 0으로 초기화해도, Source가 '방금 전 모습'이라서 튀지 않음
		CurrentTransitionDuration = BlendTime;
		TransitionAlpha = 0.0f;
		PreviousFrameTransitionAlpha = 0.0f;
	}
	// [Case 2: 일반 전환]
	else
	{
		// 일반 전환이므로 스냅샷 안 씀 (PreviousNode 애니메이션 사용)
		bIsInterruptedBlend = false;

		if (BlendTime > 0.0f)
		{
			bIsTransitioning = true;
			CurrentTransitionDuration = BlendTime;
			TransitionAlpha = 0.0f;
			PreviousFrameTransitionAlpha = 0.0f;
		}
		else
		{
			bIsTransitioning = false;
			// 블렌딩 없으면 즉시 Fully Blended Notify 트리거
			TriggerStateFullyBlendedNotifies(ActiveNode);
			bHasTriggeredFullyBlended = true;
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
			float CurrentVal = 0.0f;

			// 타입에 따라 비교할 값(CurrentVal) 결정
			if (Condition.Type == EAnimConditionType::Parameter)
			{
				CurrentVal = OwnerAnimInstance->GetFloat(Condition.ParameterName);
			}
			else if (Condition.Type == EAnimConditionType::TimeRemainingRatio)
			{
				if (ActiveNode->AnimationAsset)
				{
					float Duration = ActiveNode->AnimationAsset->GetPlayLength();
					if (Duration > 0.0f)
					{
						float Ratio = 1.0f - (CurrentAnimTime / Duration);
						CurrentVal = Ratio;
					}
				}
			}

			// 2. 비교 수행
			if (!EvaluateCondition(CurrentVal, Condition.Op, Condition.Threshold))
			{
				bAllConditionsMet = false;
				break;
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

void FAnimNode_StateMachine::TriggerStateEntryNotifies(const FAnimStateNode* Node)
{
	if (!Node || !OwnerAnimInstance)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	UE_LOG("[StateMachine] TriggerStateEntryNotifies: State=%s, Count=%d", Node->StateName.ToString().c_str(), Node->StateEntryNotifies.Num());

	for (const FAnimNotifyEvent& Notify : Node->StateEntryNotifies)
	{
		UE_LOG("[StateMachine]   - Triggering Entry Notify: %s", Notify.NotifyName.ToString().c_str());
		OwnerAnimInstance->TriggerNotify(Notify, MeshComp);
	}
}

void FAnimNode_StateMachine::TriggerStateExitNotifies(const FAnimStateNode* Node)
{
	if (!Node || !OwnerAnimInstance)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	for (const FAnimNotifyEvent& Notify : Node->StateExitNotifies)
	{
		OwnerAnimInstance->TriggerNotify(Notify, MeshComp);
	}
}

void FAnimNode_StateMachine::TriggerStateFullyBlendedNotifies(const FAnimStateNode* Node)
{
	if (!Node || !OwnerAnimInstance)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	for (const FAnimNotifyEvent& Notify : Node->StateFullyBlendedNotifies)
	{
		OwnerAnimInstance->TriggerNotify(Notify, MeshComp);
	}
}

void FAnimNode_StateMachine::TriggerTransitionNotifies(const FAnimStateTransition* Transition, float PrevAlpha, float CurrentAlpha)
{
	if (!Transition || !OwnerAnimInstance)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Transition Notify는 TriggerTime (진행률 0.0~1.0)에 따라 트리거
	for (const FAnimNotifyEvent& Notify : Transition->TransitionNotifies)
	{
		float TriggerTime = Notify.TriggerTime;

		// 이전 프레임과 현재 프레임 사이에 TriggerTime이 있으면 트리거
		if (PrevAlpha < TriggerTime && CurrentAlpha >= TriggerTime)
		{
			OwnerAnimInstance->TriggerNotify(Notify, MeshComp);
		}
	}
}
