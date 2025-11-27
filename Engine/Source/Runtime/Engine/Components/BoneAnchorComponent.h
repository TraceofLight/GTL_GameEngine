#pragma once

#include "SceneComponent.h"
#include "SkeletalMeshComponent.h"

class ViewerState;  // Forward declaration

// A single anchor component that represents the transform of a selected bone.
// The viewer selects this component so the editor gizmo latches onto it.
class UBoneAnchorComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UBoneAnchorComponent, USceneComponent)

    void SetTarget(USkeletalMeshComponent* InTarget, int32 InBoneIndex);
    void SetBoneIndex(int32 InBoneIndex) { BoneIndex = InBoneIndex; }
    int32 GetBoneIndex() const { return BoneIndex; }
    USkeletalMeshComponent* GetTarget() const { return Target; }

    // ViewerState 설정 (편집된 bone transform 캐시 접근용)
    void SetViewerState(ViewerState* InState) { State = InState; }

    // Updates this anchor's world transform from the target bone's current transform
    void UpdateAnchorFromBone();

    // When user moves gizmo, write back to the bone
    void OnTransformUpdated() override;

private:
    USkeletalMeshComponent* Target = nullptr;
    int32 BoneIndex = -1;
    ViewerState* State = nullptr;

    // UpdateAnchorFromBone 호출 중인지 구분하는 플래그
    bool bUpdatingFromBone = false;
};
