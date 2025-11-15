#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"

USkeletalMeshComponent::USkeletalMeshComponent()
    : AnimInstance(nullptr)
    , TestTime(0.0f)
    , bIsInitialized(false)
{
    // Enable component tick for animation updates
    bCanEverTick = true;

    // 테스트용 기본 메시 설정
    SetSkeletalMesh(GDataDir + "/Test.fbx");
}

/**
 * @brief Animation 인스턴스 설정
 */
void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InAnimInstance)
{
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

    // Animation 인스턴스 업데이트
    // UpdateAnimation 내부에서 시간 업데이트 -> 포즈 평가(Evaluate) -> 본 트랜스폼 설정 수행
    if (AnimInstance)
    {
        AnimInstance->UpdateAnimation(DeltaTime);
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

/**
 * @brief 컴포넌트 복제 시 AnimInstance 상태도 복제
 */
void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

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
