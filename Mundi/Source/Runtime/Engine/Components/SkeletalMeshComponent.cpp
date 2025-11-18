#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"

USkeletalMeshComponent::USkeletalMeshComponent()
    : AnimInstance(nullptr)
    , TestTime(0.0f)
    , bIsInitialized(false)
{
    // Enable component tick for animation updates
    bCanEverTick = true;

    // 테스트용 기본 메시 설정 제거 (메모리 누수 방지)
    // SetSkeletalMesh(GDataDir + "/Test.fbx");
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    // AnimInstance 정리 (메모리 누수 방지)
    if (AnimInstance)
    {
        ObjectFactory::DeleteObject(AnimInstance);
        AnimInstance = nullptr;
    }
}

/**
 * @brief Animation 인스턴스 설정
 */
void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InAnimInstance)
{
    // 기존 AnimInstance 삭제 (메모리 누수 방지)
    if (AnimInstance && AnimInstance != InAnimInstance)
    {
        ObjectFactory::DeleteObject(AnimInstance);
        AnimInstance = nullptr;
    }

    AnimInstance = InAnimInstance;
    if (AnimInstance)
    {
        AnimInstance->Initialize(this);
    }
}

/**
 * @brief Animation Notify 처리 (AnimInstance에서 호출)
 * @param Notify 트리거된 Notify 이벤트
 */
void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
    // 소유 액터에게 Notify 전달
    AActor* OwnerActor = GetOwner();
    if (OwnerActor)
    {
        OwnerActor->HandleAnimNotify(Notify);
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    // Animation 인스턴스 업데이트 (Unreal Engine 방식)
    if (AnimInstance)
    {
        // BlendSpace2D 노드 우선 체크 (더 우선순위가 높음)
        FAnimNode_BlendSpace2D* BlendSpace2DNode = AnimInstance->GetBlendSpace2DNode();
        if (BlendSpace2DNode && BlendSpace2DNode->GetBlendSpace())
        {
            // BlendSpace2D 노드 업데이트
            BlendSpace2DNode->Update(DeltaTime);

            // BlendSpace2D에서 계산된 포즈를 가져와 적용
            FPoseContext PoseContext;

            // Skeleton으로 PoseContext 초기화
            if (SkeletalMesh && SkeletalMesh->GetSkeleton())
            {
                PoseContext.Initialize(SkeletalMesh->GetSkeleton());
            }

            BlendSpace2DNode->Evaluate(PoseContext);

            // 계산된 포즈 적용
            if (PoseContext.IsValid())
            {
                PoseContext.EnsureComponentSpaceValid();
                SetPose(PoseContext.LocalSpacePose, PoseContext.ComponentSpacePose);
            }
        }
        // BlendSpace2D가 없으면 State Machine 체크
        else
        {
            FAnimNode_StateMachine* StateMachineNode = AnimInstance->GetStateMachineNode();
            if (StateMachineNode && StateMachineNode->GetStateMachine())
            {
                // State Machine 노드 업데이트
                StateMachineNode->Update(DeltaTime);

                // State Machine에서 계산된 포즈를 가져와 적용
                FPoseContext PoseContext;

                // Skeleton으로 PoseContext 초기화 (CRITICAL FIX)
                if (SkeletalMesh && SkeletalMesh->GetSkeleton())
                {
                    PoseContext.Initialize(SkeletalMesh->GetSkeleton());
                }

                StateMachineNode->Evaluate(PoseContext);

                // 계산된 포즈 적용
                if (PoseContext.IsValid())
                {
                    PoseContext.EnsureComponentSpaceValid();
                    SetPose(PoseContext.LocalSpacePose, PoseContext.ComponentSpacePose);
                }
            }
            else
            {
                // State Machine도 없으면 기본 AnimInstance 업데이트
                AnimInstance->UpdateAnimation(DeltaTime);
            }
        }
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);
        TempFinalSkinningNormalMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
        }

        ForceRecomputePose();
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
        TempFinalSkinningNormalMatrices.Empty();
    }
}

/**
 * @brief 특정 뼈의 부모 기준 로컬 트랜스폼을 설정
 * @param BoneIndex 수정할 뼈의 인덱스
 * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
 */
void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

/**
 * @brief 본 로컬 트랜스폼을 설정 (ForceRecomputePose 호출 안 함 - 배치 업데이트용)
 * @param BoneIndex 수정할 뼈의 인덱스
 * @param NewLocalTransform 새로운 부모 기준 로컬 FTransform
 */
void USkeletalMeshComponent::SetBoneLocalTransformDirect(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
    }
}

/**
 * @brief 본 트랜스폼 갱신 (AnimInstance에서 배치 업데이트 후 호출)
 */
void USkeletalMeshComponent::RefreshBoneTransforms()
{
    ForceRecomputePose();
}

/**
 * @brief Pose 전체 설정 (BlendSpace2D 등에서 사용)
 */
void USkeletalMeshComponent::SetPose(const TArray<FTransform>& InLocalSpacePose, const TArray<FTransform>& InComponentSpacePose)
{
	CurrentLocalSpacePose = InLocalSpacePose;
	CurrentComponentSpacePose = InComponentSpacePose;
	RefreshBoneTransforms();
}

/**
 * @brief Reference Pose(T-Pose)로 리셋
 */
void USkeletalMeshComponent::ResetToReferencePose()
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
        return;

    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    if (CurrentLocalSpacePose.Num() != NumBones)
        return;

    for (int32 i = 0; i < NumBones; ++i)
    {
        const FBone& ThisBone = Skeleton.Bones[i];
        const int32 ParentIndex = ThisBone.ParentIndex;
        FMatrix LocalBindMatrix;

        if (ParentIndex == -1) // 루트 본
        {
            LocalBindMatrix = ThisBone.BindPose;
        }
        else // 자식 본
        {
            const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
            LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
        }
        // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
        CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix);
    }

    ForceRecomputePose();
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}

/**
 * @brief 특정 뼈의 현재 로컬 트랜스폼을 반환
 */
FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

/**
 * @brief 기즈모를 렌더링하기 위해 특정 뼈의 월드 트랜스폼을 계산
 */
FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

/**
 * @brief CurrentLocalSpacePose의 변경사항을 ComponentSpace -> FinalMatrices 계산까지 모두 수행
 */
void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; }

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();
    // ComponentSpace -> Final Skinning Matrices 계산
    UpdateFinalSkinningMatrices();
    UpdateSkinningMatrices(TempFinalSkinningMatrices, TempFinalSkinningNormalMatrices);
}

/**
 * @brief CurrentLocalSpacePose를 기반으로 CurrentComponentSpacePose 채우기
 */
void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

/**
 * @brief CurrentComponentSpacePose를 기반으로 TempFinalSkinningMatrices 채우기
 */
void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
        TempFinalSkinningNormalMatrices[BoneIndex] = TempFinalSkinningMatrices[BoneIndex].Inverse().Transpose();
    }
}

// ===== Phase 4: 애니메이션 편의 메서드 구현 =====

/**
 * @brief 애니메이션 재생 (간편 메서드)
 */
void USkeletalMeshComponent::PlayAnimation(UAnimSequence* AnimToPlay, bool bLooping)
{
    if (!AnimToPlay)
    {
        return;
    }

    // AnimSingleNodeInstance 생성 (없으면)
    UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(AnimInstance);
    if (!SingleNodeInstance)
    {
        SingleNodeInstance = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(SingleNodeInstance);
    }

    // 애니메이션 설정 및 재생
    SingleNodeInstance->SetAnimationAsset(AnimToPlay);
    SingleNodeInstance->Play(bLooping);
}

/**
 * @brief 애니메이션 정지
 */
void USkeletalMeshComponent::StopAnimation()
{
    if (AnimInstance)
    {
        AnimInstance->StopAnimation();
    }
}

/**
 * @brief State Machine 설정 (AnimInstance를 통해)
 */
void USkeletalMeshComponent::SetAnimationStateMachine(UAnimStateMachine* InStateMachine)
{
    // AnimInstance가 없으면 생성
    if (!AnimInstance)
    {
        AnimInstance = NewObject<UAnimInstance>();
        AnimInstance->Initialize(this);
        UE_LOG("[SkeletalMeshComponent] AnimInstance created for StateMachine");
    }

    if (AnimInstance)
    {
        AnimInstance->SetStateMachine(InStateMachine);
    }
}

/**
 * @brief BlendSpace2D 설정 (AnimInstance를 통해)
 */
void USkeletalMeshComponent::SetBlendSpace2D(UBlendSpace2D* InBlendSpace)
{
    // AnimInstance가 없으면 생성
    if (!AnimInstance)
    {
        AnimInstance = NewObject<UAnimInstance>();
        AnimInstance->Initialize(this);
        UE_LOG("[SkeletalMeshComponent] AnimInstance created for BlendSpace2D");
    }

    if (AnimInstance)
    {
        AnimInstance->SetBlendSpace2D(InBlendSpace);
    }
}

/**
 * @brief 컴포넌트 복제 시 AnimInstance 상태도 복제
 */
void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // Tick 활성화 (복제 시 유지되지 않으므로 명시적으로 설정)
    bCanEverTick = true;

    // AnimInstance가 있으면 복제
    if (AnimInstance)
    {
        UAnimInstance* OldAnimInstance = AnimInstance;

        // 새로운 AnimInstance 생성
        AnimInstance = NewObject<UAnimInstance>();
        AnimInstance->Initialize(this);

        // 애니메이션 상태 복사
        if (OldAnimInstance->GetCurrentAnimation())
        {
            AnimInstance->PlayAnimation(OldAnimInstance->GetCurrentAnimation(), OldAnimInstance->GetPlayRate());
            AnimInstance->SetPosition(OldAnimInstance->GetCurrentTime());

            if (!OldAnimInstance->IsPlaying())
            {
                AnimInstance->StopAnimation();
            }
        }

        // 이전 AnimInstance 삭제 (메모리 누수 방지)
        ObjectFactory::DeleteObject(OldAnimInstance);
    }
}

/**
 * @brief 컴포넌트 직렬화 시 AnimInstance 상태도 저장/로드
 */
void USkeletalMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // AnimInstance 로드
        bool bHasAnimInstance = false;
        FJsonSerializer::ReadBool(InOutHandle, "HasAnimInstance", bHasAnimInstance, false);

        if (bHasAnimInstance)
        {
            // AnimInstance 생성
            if (!AnimInstance)
            {
                AnimInstance = NewObject<UAnimInstance>();
                AnimInstance->Initialize(this);
            }

            // 애니메이션 상태 로드
            FString AnimPath;
            float SavedTime = 0.0f;
            float SavedPlayRate = 1.0f;
            bool bWasPlaying = false;

            FJsonSerializer::ReadString(InOutHandle, "AnimationPath", AnimPath, "");
            FJsonSerializer::ReadFloat(InOutHandle, "CurrentTime", SavedTime, 0.0f);
            FJsonSerializer::ReadFloat(InOutHandle, "PlayRate", SavedPlayRate, 1.0f);
            FJsonSerializer::ReadBool(InOutHandle, "IsPlaying", bWasPlaying, false);

            // TODO: AnimPath를 통해 AnimSequence를 로드하고 재생
            // 현재는 AnimSequence 로드 시스템이 완성되지 않아 스킵
        }
    }
    else
    {
        // AnimInstance 저장
        bool bHasAnimInstance = (AnimInstance != nullptr);
        InOutHandle["HasAnimInstance"] = bHasAnimInstance;

        if (bHasAnimInstance && AnimInstance->GetCurrentAnimation())
        {
            // 애니메이션 상태 저장
            // TODO: CurrentAnimation의 경로를 저장 (현재는 Asset 경로 시스템 미완성)
            InOutHandle["AnimationPath"] = FString("");
            InOutHandle["CurrentTime"] = AnimInstance->GetCurrentTime();
            InOutHandle["PlayRate"] = AnimInstance->GetPlayRate();
            InOutHandle["IsPlaying"] = AnimInstance->IsPlaying();
        }
    }
}
