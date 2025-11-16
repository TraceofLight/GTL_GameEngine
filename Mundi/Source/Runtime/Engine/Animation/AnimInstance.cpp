#include "pch.h"
#include "AnimInstance.h"
#include "SkeletalMeshComponent.h"
#include "AnimSequenceBase.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "BlendSpace2D.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/GameFramework/Pawn.h"
#include "Actor.h"

IMPLEMENT_CLASS(UAnimInstance)

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

/**
 * @brief 매 프레임 업데이트 (Notify Trigger)
 * @param DeltaSeconds 델타 타임
 */
void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
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

	// Notify Trigger
	TriggerAnimNotifies(DeltaSeconds);

	// Pose Evaluation (본 포즈 샘플링 및 적용)
	EvaluateAnimation();
}

/**
 * @brief Animation Notify Trigger 처리
 * @param DeltaSeconds 델타 타임
 */
void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (!CurrentAnimation || !OwnerComponent)
	{
		return;
	}

	// 현재 시간 구간에서 트리거되어야 할 Notify 수집
	TArray<const FAnimNotifyEvent*> TriggeredNotifies;
	CurrentAnimation->GetAnimNotifiesFromDeltaPositions(PreviousTime, CurrentTime, TriggeredNotifies);

	// 각 Notify 처리
	for (const FAnimNotifyEvent* NotifyEvent : TriggeredNotifies)
	{
		if (NotifyEvent)
		{
			HandleNotify(*NotifyEvent);
		}
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
 * @brief 개별 Notify 이벤트 처리 (오버라이드 가능)
 * @param NotifyEvent 트리거된 Notify 이벤트
 */
void UAnimInstance::HandleNotify(const FAnimNotifyEvent& NotifyEvent)
{
	// 기본 구현: SkeletalMeshComponent를 통해 Actor에게 전달
	if (OwnerComponent)
	{
		OwnerComponent->HandleAnimNotify(NotifyEvent);
	}
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
		UE_LOG("[AnimInstance] EvaluateAnimation: CurrentAnimation is not UAnimSequence!");
		return;
	}

	static int32 EvalLogCounter = 0;
	if (EvalLogCounter++ % 60 == 0)
	{
		UE_LOG("[AnimInstance] EvaluateAnimation: Time=%.2f", CurrentTime);
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

	// Owner Pawn 설정 (SkeletalMeshComponent의 Owner를 사용)
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
