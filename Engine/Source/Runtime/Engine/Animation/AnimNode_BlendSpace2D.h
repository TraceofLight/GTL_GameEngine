#pragma once
#include "PoseContext.h"
#include "AnimationTypes.h"

class UBlendSpace2D;
class APawn;
class ACharacter;
class UCharacterMovementComponent;
class UAnimInstance;
class USkeletalMeshComponent;

/**
 * @brief Blend Space 2D 실행 노드
 *
 * UBlendSpace2D 애셋을 실행하는 인스턴스 클래스.
 * 캐릭터마다 별도의 실행 상태를 가집니다.
 */
class FAnimNode_BlendSpace2D
{
public:
	FAnimNode_BlendSpace2D();
	~FAnimNode_BlendSpace2D();

	// ===== 초기화 =====

	/**
	 * @brief 노드 초기화
	 * @param InPawn 소유 Pawn (Movement 정보 접근용)
	 * @param InAnimInstance 소유 AnimInstance (Notify 트리거용)
	 * @param InMeshComp 소유 SkeletalMeshComponent
	 */
	void Initialize(APawn* InPawn, UAnimInstance* InAnimInstance = nullptr, USkeletalMeshComponent* InMeshComp = nullptr);

	/**
	 * @brief BlendSpace 애셋 설정
	 */
	void SetBlendSpace(UBlendSpace2D* InBlendSpace);

	/**
	 * @brief BlendSpace 애셋 가져오기
	 */
	UBlendSpace2D* GetBlendSpace() const { return BlendSpace; }

	// ===== 업데이트 =====

	/**
	 * @brief 매 프레임 업데이트
	 * @param DeltaSeconds 델타 타임
	 */
	void Update(float DeltaSeconds);

	/**
	 * @brief 포즈 계산 (블렌딩 수행)
	 * @param OutPose 출력 포즈
	 */
	void Evaluate(FPoseContext& OutPose);

	// ===== 파라미터 설정 =====

	/**
	 * @brief 블렌드 파라미터 직접 설정
	 */
	void SetBlendParameter(FVector2D InParameter);

	/**
	 * @brief 현재 블렌드 파라미터 가져오기
	 */
	FVector2D GetBlendParameter() const { return BlendParameter; }

	/**
	 * @brief 자동 파라미터 계산 활성화 (Movement 기반)
	 */
	void SetAutoCalculateParameter(bool bEnable);

	// ===== 재생 제어 =====

	/**
	 * @brief 일시정지 설정
	 */
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	/**
	 * @brief 일시정지 상태 확인
	 */
	bool IsPaused() const { return bPaused; }

	/**
	 * @brief 정규화된 재생 시간 설정 (0~1)
	 * @param InNormalizedTime 0.0 = 시작, 1.0 = 끝
	 */
	void SetNormalizedTime(float InNormalizedTime);

	/**
	 * @brief 정규화된 재생 시간 가져오기 (0~1)
	 */
	float GetNormalizedTime() const { return NormalizedTime; }

	/**
	 * @brief 블렌딩된 애니메이션의 최대 길이 가져오기 (초)
	 */
	float GetMaxAnimationLength() const;

	/**
	 * @brief 루프 설정
	 */
	void SetLoop(bool bInLoop) { bLoop = bInLoop; }

	/**
	 * @brief 루프 상태 확인
	 */
	bool IsLooping() const { return bLoop; }

private:
	// ===== 애셋 참조 =====
	UBlendSpace2D* BlendSpace;

	// ===== 실행 상태 =====
	FVector2D BlendParameter;      // 현재 블렌드 파라미터 (예: 속도, 방향)
	bool bAutoCalculateParameter;  // Movement에서 자동으로 파라미터 계산할지
	bool bPaused;                  // 일시정지 상태
	bool bLoop;                    // 루프 재생 여부

	// 각 샘플 애니메이션의 재생 시간
	TArray<float> SampleAnimTimes;

	// 동기화된 재생 시간 (0~1 정규화)
	float NormalizedTime;

	// ===== Owner 참조 =====
	APawn* OwnerPawn;
	ACharacter* OwnerCharacter;
	UCharacterMovementComponent* MovementComponent;
	UAnimInstance* OwnerAnimInstance;
	USkeletalMeshComponent* OwnerMeshComp;

	// ===== Notify 상태 =====
	// 각 샘플의 이전 프레임 시간 (Notify 트리거용)
	TArray<float> PreviousSampleAnimTimes;
	// 현재 활성 NotifyState 목록
	TArray<FAnimNotifyEvent> ActiveAnimNotifyState;
	// 이전 Leader 인덱스 (Leader 변경 감지용)
	int32 PreviousLeaderIndex;

	// ===== 내부 헬퍼 =====

	/**
	 * @brief Movement 상태에서 블렌드 파라미터 자동 계산
	 */
	void CalculateBlendParameterFromMovement();

	/**
	 * @brief Sync Marker 기반으로 Follower 애니메이션 동기화
	 */
	void SyncFollowersWithMarkers(int32 LeaderIndex, float LeaderTime, float DeltaSeconds);
};
