#pragma once
#include "../Animation/SingleAnimationPlayData.h"
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
	void InitializeComponent() override;
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
	void SetAnimationMode(EAnimationMode NewMode);
	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);
	void SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform);

	// Bone Editing Mode
	void SetBoneEditingMode(bool bEnabled, int32 InEditingBoneIndex = -1)
	{
		bIsBoneEditingMode = bEnabled;
		EditingBoneIndex = bEnabled ? InEditingBoneIndex : -1;
	}
	bool IsBoneEditingMode() const { return bIsBoneEditingMode; }
	int32 GetEditingBoneIndex() const { return EditingBoneIndex; }

	// 편집된 본 델타 관리
	void SetBoneDelta(int32 BoneIndex, const FTransform& Delta);
	void ClearBoneDelta(int32 BoneIndex);
	void ClearAllBoneDeltas();
	bool HasBoneDelta(int32 BoneIndex) const;
	const FTransform* GetBoneDelta(int32 BoneIndex) const;
	const TMap<int32, FTransform>& GetAllBoneDeltas() const { return EditedBoneDeltas; }

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

	// Pose 전체 설정 (BlendSpace2D 등에서 사용)
	void SetPose(const TArray<FTransform>& InLocalSpacePose, const TArray<FTransform>& InComponentSpacePose);

	// Reset to Reference Pose (T-Pose)
	void ResetToReferencePose();

	// AnimNotify 델리게이트
	FOnAnimNotify OnAnimNotify;

	UPROPERTY(EditAnywhere, Category="Animation")
	EAnimationMode AnimationMode = EAnimationMode::AnimationSingleNode;

	UPROPERTY(EditAnywhere, Category="Animation")
	class UAnimStateMachine* AnimBlueprint = nullptr;

	UPROPERTY(EditAnywhere, Category="Animation")
	FSingleAnimationPlayData AnimationData;

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
	bool bIsBoneEditingMode = false;  // 기즈모로 본을 편집 중인지 여부
	int32 EditingBoneIndex = -1;      // 현재 편집 중인 본 인덱스 (-1이면 없음)
	TMap<int32, FTransform> EditedBoneDeltas;  // 편집된 본의 델타 (BoneIndex -> Delta Transform)
};
