#pragma once
#include "SkinnedMeshComponent.h"
#include "Source/Runtime/Core/Misc/Delegates.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimStateMachine;
class UAnimSequence;
struct FAnimNotifyEvent;

DECLARE_DELEGATE_TYPE(FOnAnimNotify, const FAnimNotifyEvent&);

/**
 * @brief 스켈레탈 메시 컴포넌트
 * @details Animation 가능한 스켈레탈 메시를 렌더링하는 컴포넌트
 *
 * @param AnimInstance 현재 Animation 인스턴스
 * @param CurrentLocalSpacePose 각 뼈의 부모 기준 로컬 트랜스폼 배열
 * @param CurrentComponentSpacePose 컴포넌트 기준 월드 트랜스폼 배열
 * @param TempFinalSkinningMatrices 최종 스키닝 행렬 (임시 계산용)
 * @param TempFinalSkinningNormalMatrices 최종 노말 스키닝 행렬 (CPU 스키닝용)
 * @param TestTime 테스트용 시간 누적
 * @param bIsInitialized 테스트용 초기화 플래그
 * @param TestBoneBasePose 테스트용 기본 본 포즈
 */
UCLASS(DisplayName="스켈레탈 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_REFLECTION_BODY()

public:
	USkeletalMeshComponent();
	~USkeletalMeshComponent() override;

	// Functions
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;
	void SetSkeletalMesh(const FString& PathFileName) override;
	void HandleAnimNotify(const FAnimNotifyEvent& Notify);

	UFUNCTION(LuaBind)
	void TriggerAnimNotify(const FString& NotifyName, float TriggerTime, float Duration);

	// Serialization
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	void DuplicateSubObjects() override;

	// Getters
	UFUNCTION(LuaBind, DisplayName = "GetAnimInstance")
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }
	FTransform GetBoneLocalTransform(int32 BoneIndex) const;
	FTransform GetBoneWorldTransform(int32 BoneIndex);

	// Setters
	void SetAnimInstance(UAnimInstance* InAnimInstance);
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);
	void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);

	// ===== Phase 4: 애니메이션 편의 메서드 =====

	/**
	 * @brief 애니메이션 재생 (간편 메서드)
	 *
	 * AnimInstance를 자동 생성하고 애니메이션을 재생.
	 *
	 * @param AnimToPlay 재생할 애니메이션
	 * @param bLooping 루프 재생 여부
	 */
	void PlayAnimation(UAnimSequence* AnimToPlay, bool bLooping = true);

	/**
	 * @brief 애니메이션 정지
	 */
	void StopAnimation();

	void SetBlendSpace2D(class UBlendSpace2D* InBlendSpace);

	// Batch Pose Update (AnimInstance에서 사용)
	void SetBoneLocalTransformDirect(int32 BoneIndex, const FTransform& NewLocalTransform);
	void RefreshBoneTransforms();

	// Reset to Reference Pose (T-Pose)
	void ResetToReferencePose();

	// AnimNotify 델리게이트
	FOnAnimNotify OnAnimNotify;

protected:
	TArray<FTransform> CurrentLocalSpacePose;
	TArray<FTransform> CurrentComponentSpacePose;
	TArray<FMatrix> TempFinalSkinningMatrices;
	TArray<FMatrix> TempFinalSkinningNormalMatrices;

	void ForceRecomputePose();
	void UpdateComponentSpaceTransforms();
	void UpdateFinalSkinningMatrices();

private:
	UAnimInstance* AnimInstance;
	float TestTime;
	bool bIsInitialized;
	FTransform TestBoneBasePose;
};
