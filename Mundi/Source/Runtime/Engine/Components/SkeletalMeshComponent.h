#pragma once
#include "SkinnedMeshComponent.h"
#include "USkeletalMeshComponent.generated.h"

class UAnimInstance;
struct FAnimNotifyEvent;

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
	~USkeletalMeshComponent() override = default;

	// Functions
	void TickComponent(float DeltaTime) override;
	void SetSkeletalMesh(const FString& PathFileName) override;
	void HandleAnimNotify(const FAnimNotifyEvent& Notify);

	// Serialization
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	void DuplicateSubObjects() override;

	// Getters
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }
	FTransform GetBoneLocalTransform(int32 BoneIndex) const;
	FTransform GetBoneWorldTransform(int32 BoneIndex);

	// Setters
	void SetAnimInstance(UAnimInstance* InAnimInstance);
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);
	void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);

	// Batch Pose Update (AnimInstance에서 사용)
	void SetBoneLocalTransformDirect(int32 BoneIndex, const FTransform& NewLocalTransform);
	void RefreshBoneTransforms();

protected:
	UAnimInstance* AnimInstance;
	TArray<FTransform> CurrentLocalSpacePose;
	TArray<FTransform> CurrentComponentSpacePose;
	TArray<FMatrix> TempFinalSkinningMatrices;
	TArray<FMatrix> TempFinalSkinningNormalMatrices;

	void ForceRecomputePose();
	void UpdateComponentSpaceTransforms();
	void UpdateFinalSkinningMatrices();

private:
	float TestTime;
	bool bIsInitialized;
	FTransform TestBoneBasePose;
};
