#pragma once

#include "AnimInstance.h"

class UAnimSequence;
class UAnimationAsset;

/**
 * @brief 단일 애니메이션 재생 전용 AnimInstance
 *
 * 하나의 애니메이션만 재생하는 간단한 케이스에 사용.
 * State Machine이나 Blend Space 없이 단순 재생만 필요할 때 최적.
 *
 * 사용 예시:
 * - 단순 루프 애니메이션 (Idle, 프롭 회전 등)
 * - 테스트용 애니메이션 재생
 * - StaticMesh처럼 단순한 애니메이션 오브젝트
 */
UCLASS()
class UAnimSingleNodeInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

public:
	UAnimSingleNodeInstance();
	virtual ~UAnimSingleNodeInstance() override = default;

	// ===== Phase 2: 단일 노드 재생 메서드 =====

	/**
	 * @brief 재생할 애니메이션 에셋 설정
	 *
	 * Play()와 분리하여 애니메이션만 먼저 설정 가능.
	 *
	 * @param NewAnimToPlay 재생할 애니메이션 시퀀스
	 */
	void SetAnimationAsset(UAnimSequence* NewAnimToPlay);

	/**
	 * @brief 애니메이션 재생 시작
	 *
	 * @param bLooping 루프 재생 여부
	 */
	void Play(bool bLooping = true);

	/**
	 * @brief 애니메이션 정지
	 */
	void Stop();

	/**
	 * @brief 재생 속도 설정
	 *
	 * @param InPlayRate 재생 속도 (1.0 = 정상, 2.0 = 2배속)
	 */
	void SetPlayRate(float InPlayRate);

	/**
	 * @brief 루핑 여부 설정
	 *
	 * @param bInLooping 루프 재생 여부
	 */
	void SetLooping(bool bInLooping);

	/**
	 * @brief 재생 시간을 특정 위치로 설정
	 *
	 * @param InTimeSeconds 설정할 시간 (초 단위)
	 * @param bFireNotifies 해당 구간의 Notify를 발동시킬지 여부
	 */
	void SetPositionWithNotify(float InTimeSeconds, bool bFireNotifies);

	// ===== Getters =====

	/**
	 * @brief 현재 재생 중인 애니메이션 가져오기
	 */
	UAnimSequence* GetCurrentAnimationAsset() const { return CurrentSequence; }

	/**
	 * @brief 루핑 여부 확인
	 */
	bool IsLooping() const { return bLooping; }

	// ===== Override =====

	/**
	 * @brief 매 프레임 업데이트
	 *
	 * 단일 애니메이션 재생 로직 처리.
	 * 루핑, 재생 종료 등을 관리.
	 */
	virtual void UpdateAnimation(float DeltaSeconds) override;

protected:
	/** 현재 재생 중인 애니메이션 시퀀스 */
	UAnimSequence* CurrentSequence;

	/** 루프 재생 여부 */
	bool bLooping;

	/**
	 * @brief 애니메이션이 끝에 도달했을 때 처리
	 */
	void OnAnimationEnd();
};
