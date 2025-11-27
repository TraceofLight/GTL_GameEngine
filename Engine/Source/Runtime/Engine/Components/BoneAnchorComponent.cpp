#include "pch.h"
#include "BoneAnchorComponent.h"
#include "SelectionManager.h"
#include "Source/Runtime/Engine/SkeletalViewer/ViewerState.h"

IMPLEMENT_CLASS(UBoneAnchorComponent)

void UBoneAnchorComponent::SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex)
{
    Target = InTarget;
    BoneIndex = InBoneIndex;
    UpdateAnchorFromBone();
}

void UBoneAnchorComponent::UpdateAnchorFromBone()
{
    if (!Target || BoneIndex < 0)
        return;

    // 플래그 설정: 이 호출에서는 캐시에 저장하지 않음
    bUpdatingFromBone = true;

    const FTransform BoneWorld = Target->GetBoneWorldTransform(BoneIndex);
    SetWorldTransform(BoneWorld);

    // 피킹 시점의 애니메이션 트랜스폼 저장
    // 이미 EditedBoneTransforms에 델타가 있으면 저장하지 않음 (편집 중인 본)
    if (State)
    {
        auto It = State->EditedBoneTransforms.find(BoneIndex);
        if (It == State->EditedBoneTransforms.end())
        {
            // 편집되지 않은 본: 현재 로컬 트랜스폼을 기준으로 저장
            State->AnimationBoneTransforms[BoneIndex] = Target->GetBoneLocalTransform(BoneIndex);
        }
    }

    bUpdatingFromBone = false;
}

void UBoneAnchorComponent::OnTransformUpdated()
{
    Super::OnTransformUpdated();

    if (!Target || BoneIndex < 0)
        return;

    // UpdateAnchorFromBone에서 호출된 경우 (피킹 시) 캐시에 저장하지 않음
    if (bUpdatingFromBone)
    {
        return;
    }

    const FTransform AnchorWorld = GetWorldTransform();
    Target->SetBoneWorldTransform(BoneIndex, AnchorWorld);

    // 편집된 bone transform 델타를 캐시에 저장 (애니메이션 재생 중에도 유지)
    if (State)
    {
        // 현재 편집된 로컬 트랜스폼
        const FTransform EditedTransform = Target->GetBoneLocalTransform(BoneIndex);

        // 원본 애니메이션 트랜스폼 가져오기
        auto It = State->AnimationBoneTransforms.find(BoneIndex);
        if (It != State->AnimationBoneTransforms.end())
        {
            const FTransform& AnimTransform = It->second;

            // 델타 계산: Position=차이, Rotation=상대회전, Scale=비율
            FTransform Delta;
            Delta.Translation = EditedTransform.Translation - AnimTransform.Translation;
            Delta.Rotation = AnimTransform.Rotation.Inverse() * EditedTransform.Rotation;

            // Scale 나눗셈 (0 방지)
            Delta.Scale3D = FVector(
                AnimTransform.Scale3D.X != 0.0f ? EditedTransform.Scale3D.X / AnimTransform.Scale3D.X : 1.0f,
                AnimTransform.Scale3D.Y != 0.0f ? EditedTransform.Scale3D.Y / AnimTransform.Scale3D.Y : 1.0f,
                AnimTransform.Scale3D.Z != 0.0f ? EditedTransform.Scale3D.Z / AnimTransform.Scale3D.Z : 1.0f
            );

            State->EditedBoneTransforms[BoneIndex] = Delta;
        }
        else
        {
            // 애니메이션 트랜스폼이 없으면 Identity 델타 저장 (Skeletal 모드 등)
            FTransform Delta;
            Delta.Translation = FVector::Zero();
            Delta.Rotation = FQuat::Identity();
            Delta.Scale3D = FVector::One();
            State->EditedBoneTransforms[BoneIndex] = Delta;

            // 현재 트랜스폼을 기준으로 저장
            State->AnimationBoneTransforms[BoneIndex] = EditedTransform;
        }

        // Bone line 실시간 업데이트를 위해 dirty 플래그 설정
        State->bBoneLinesDirty = true;
    }
}
