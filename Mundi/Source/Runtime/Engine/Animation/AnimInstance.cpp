#include "pch.h"
#include "AnimInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequenceBase.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "BlendSpace2D.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Actor.h"
#include "Source/Runtime/Engine/Scripting/LuaManager.h"

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

void UAnimInstance::NativeBeginPlay()
{
	if (OwnerComponent)
	{
		AActor* Owner = OwnerComponent->GetOwner();
		APawn* OwnerPawn = Cast<APawn>(Owner);
		if (OwnerPawn)
		{
			StateMachineNode.Initialize(OwnerPawn);
		}
	}
}

/**
 * @brief 매 프레임 업데이트 (Notify Trigger)
 * @param DeltaSeconds 델타 타임
 */
void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// StateMachine 사용 여부 확인
	UAnimStateMachine* StateMachineAsset = StateMachineNode.GetStateMachine();
	if (StateMachineAsset)
	{
		// StateMachine 모드: StateMachine이 애니메이션 시간 관리
		// Notify는 StateMachine의 현재 상태 애니메이션으로 처리
		// (StateMachine은 자체적으로 Update/Evaluate 호출됨)
		TriggerAnimNotifies(DeltaSeconds);
		return;
	}

	// 일반 애니메이션 재생 모드
	if (!bIsPlaying || !CurrentAnimation)
	{
		return;
	}

	static int32 LogCounter = 0;
	if (LogCounter++ % 60 == 0) // Log every 60 frames
	{
		UE_LOG("[AnimInstance] UpdateAnimation: DeltaTime=%.3f, CurrentTime=%.2f, PlayRate=%.2f",
			DeltaSeconds, CurrentTime, PlayRate);
	}

	// 이전 시간 저장
	PreviousTime = CurrentTime;

	// 현재 시간 업데이트
	CurrentTime += DeltaSeconds * PlayRate;

	// Animation 길이 체크
	float AnimLength = CurrentAnimation->GetPlayLength();
	if (CurrentAnimation->IsLooping())
	{
		// 루프 Animation: 시간이 범위를 벗어나면 순환
		while (CurrentTime >= AnimLength)
		{
			CurrentTime -= AnimLength;
			PreviousTime -= AnimLength;
		}
		while (CurrentTime < 0.0f)
		{
			CurrentTime += AnimLength;
			PreviousTime += AnimLength;
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
		else if (CurrentTime < 0.0f)
		{
			CurrentTime = 0.0f;
			bIsPlaying = false;
		}
	}

	// Notify Trigger (UE 표준 방식: Begin/Tick/End 모두 처리)
	TriggerAnimNotifies(DeltaSeconds);

	// Pose Evaluation (본 포즈 샘플링 및 적용)
	EvaluateAnimation();
}

/**
 * @brief Animation Notify Trigger 처리 (UE 표준 방식)
 * @param DeltaSeconds 델타 타임
 * @details UE AnimInstance.cpp:1579-1692 참조
 */
void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (!OwnerComponent)
	{
		return;
	}

	// StateMachine 사용 여부에 따라 애니메이션/시간 정보 가져오기
	UAnimSequenceBase* ActiveAnimation = nullptr;
	float ActiveCurrentTime = 0.0f;
	float ActivePreviousTime = 0.0f;

	UAnimStateMachine* StateMachineAsset = StateMachineNode.GetStateMachine();
	if (StateMachineAsset)
	{
		// StateMachine 모드: 현재 상태의 애니메이션 정보 사용
		ActiveAnimation = StateMachineNode.GetCurrentAnimation();
		ActiveCurrentTime = StateMachineNode.GetCurrentAnimTime();
		ActivePreviousTime = StateMachineNode.GetPreviousFrameAnimTime();
	}
	else
	{
		// 일반 재생 모드: AnimInstance의 CurrentAnimation 사용
		ActiveAnimation = CurrentAnimation;
		ActiveCurrentTime = CurrentTime;
		ActivePreviousTime = PreviousTime;
	}

	if (!ActiveAnimation)
	{
		return;
	}

	AActor* Owner = OwnerComponent->GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	FLuaManager* LuaMgr = World->GetLuaManager();
	if (!LuaMgr)
	{
		return;
	}

	// 1. 현재 프레임에서 활성화되어야 할 Notify 수집 (NotifyQueue 역할)
	TArray<const FAnimNotifyEvent*> CurrentFrameNotifies;
	ActiveAnimation->GetAnimNotifiesFromDeltaPositions(ActivePreviousTime, ActiveCurrentTime, CurrentFrameNotifies);

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
			HandleNotify(*NotifyEvent);
		}
	}

	// 3. ActiveAnimNotifyState에 남아있는 항목들 → 더 이상 활성화되지 않음 → NotifyEnd 호출
	for (int32 i = 0; i < ActiveAnimNotifyState.Num(); ++i)
	{
		const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[i];
		LuaMgr->ExecuteNotifyStateEnd(AnimNotifyEvent.NotifyName.ToString(), AnimNotifyEvent.PropertyData, OwnerComponent, ActiveCurrentTime);
	}

	// 4. 새로 시작하는 NotifyState → NotifyBegin 호출
	for (const FAnimNotifyEvent* NotifyEvent : NotifyStateBeginEvents)
	{
		LuaMgr->ExecuteNotifyStateBegin(NotifyEvent->NotifyName.ToString(), NotifyEvent->PropertyData, OwnerComponent, ActiveCurrentTime);
	}

	// 5. ActiveAnimNotifyState 교체
	ActiveAnimNotifyState = std::move(NewActiveAnimNotifyState);

	// 6. 현재 활성화된 모든 NotifyState → NotifyTick 호출
	for (int32 i = 0; i < ActiveAnimNotifyState.Num(); ++i)
	{
		const FAnimNotifyEvent& AnimNotifyEvent = ActiveAnimNotifyState[i];
		LuaMgr->ExecuteNotifyStateTick(AnimNotifyEvent.NotifyName.ToString(), AnimNotifyEvent.PropertyData, OwnerComponent, ActiveCurrentTime, DeltaSeconds);
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

void UAnimInstance::ResumeAnimation()
{
	bIsPlaying = true;
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

		// 시간이 변경되었으므로 포즈도 갱신
		EvaluateAnimation();
	}
}

/**
 * @brief 개별 Notify 이벤트 처리
 * @param NotifyEvent 트리거된 Notify 이벤트
 */
void UAnimInstance::HandleNotify(const FAnimNotifyEvent& NotifyEvent)
{
	if (!OwnerComponent)
	{
		return;
	}

	AActor* Owner = OwnerComponent->GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	FLuaManager* LuaMgr = World->GetLuaManager();
	if (!LuaMgr)
	{
		return;
	}

	FString NotifyClassName = NotifyEvent.NotifyName.ToString();
	LuaMgr->ExecuteNotify(NotifyClassName, NotifyEvent.PropertyData, OwnerComponent, NotifyEvent.TriggerTime, NotifyEvent.Duration);
}

/**
 * @brief 수동으로 Notify 트리거 (State Machine에서 호출)
 * @param NotifyEvent 트리거할 Notify 이벤트
 * @param MeshComp Skeletal Mesh Component
 */
void UAnimInstance::TriggerNotify(const FAnimNotifyEvent& NotifyEvent, USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	FLuaManager* LuaMgr = World->GetLuaManager();
	if (!LuaMgr)
	{
		return;
	}

	FString NotifyClassName = NotifyEvent.NotifyName.ToString();
	LuaMgr->ExecuteNotify(NotifyClassName, NotifyEvent.PropertyData, MeshComp, NotifyEvent.TriggerTime, NotifyEvent.Duration);
}

/**
 * @brief 현재 애니메이션 시간에서 본 포즈를 샘플링하여 SkeletalMeshComponent에 적용
 * @details 언리얼 표준 흐름: UpdateAnimation (시간 업데이트) -> EvaluateAnimation (포즈 샘플링)
 */
void UAnimInstance::EvaluateAnimation()
{
	if (!CurrentAnimation || !OwnerComponent)
	{
		return;
	}

	// UAnimSequence로 캐스팅 (실제 포즈 데이터를 가진 클래스)
	UAnimSequence* AnimSequence = Cast<UAnimSequence>(CurrentAnimation);
	if (!AnimSequence)
	{
		return;
	}

	// SkeletalMesh에서 Skeleton 정보 가져오기
	USkeletalMesh* SkeletalMesh = OwnerComponent->GetSkeletalMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		return;
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const int32 NumBones = Skeleton->Bones.Num();

	// 각 본의 현재 시간에서의 Transform 샘플링 (배치 업데이트)
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FBone& Bone = Skeleton->Bones[BoneIndex];

		FVector Position;
		FQuat Rotation;
		FVector Scale;

		// AnimSequence에서 본 Transform 샘플링
		if (AnimSequence->GetBoneTransformAtTime(Bone.Name, CurrentTime, Position, Rotation, Scale))
		{
			// SkeletalMeshComponent에 직접 설정 (ForceRecomputePose 호출 안 함)
			FTransform BoneTransform(Position, Rotation, Scale);
			OwnerComponent->SetBoneLocalTransformDirect(BoneIndex, BoneTransform);
		}
	}

	// 모든 본 업데이트 완료 후 한 번만 갱신
	OwnerComponent->RefreshBoneTransforms();
}

/**
 * @brief State Machine 애셋 설정
 */
void UAnimInstance::SetStateMachine(UAnimStateMachine* InStateMachine)
{
	StateMachineNode.SetStateMachine(InStateMachine);
}

/**
 * @brief Blend Space 2D 애셋 설정
 */
void UAnimInstance::SetBlendSpace2D(UBlendSpace2D* InBlendSpace)
{
	BlendSpace2DNode.SetBlendSpace(InBlendSpace);

	// Owner Pawn 설정 (SkeletalMeshComponent의 Owner를 사용)
	if (OwnerComponent)
	{
		AActor* Owner = OwnerComponent->GetOwner();
		APawn* OwnerPawn = Cast<APawn>(Owner);
		if (OwnerPawn)
		{
			BlendSpace2DNode.Initialize(OwnerPawn);
		}
	}
}

/**
 * @brief BlendSpace2D 파라미터 설정 (Lua에서 호출)
 */
void UAnimInstance::SetBlendSpace2DParameter(float X, float Y)
{
	BlendSpace2DNode.SetBlendParameter(FVector2D(X, Y));
}

/**
 * @brief BlendSpace2D 자동 파라미터 계산 활성화/비활성화 (Lua에서 호출)
 */
void UAnimInstance::SetBlendSpace2DAutoCalculate(bool bEnable)
{
	BlendSpace2DNode.SetAutoCalculateParameter(bEnable);
}
