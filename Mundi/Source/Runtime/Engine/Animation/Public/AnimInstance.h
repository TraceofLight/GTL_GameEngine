#pragma once

class UAnimSequenceBase;
class USkeletalMeshComponent;

/**
 * @brief Animation 인스턴스
 * @details Skeletal Mesh의 Animation 상태를 관리하고 Notify 처리
 *
 * @param OwnerComponent 소유한 Skeletal Mesh Component
 * @param CurrentAnimation 현재 재생 중인 Animation
 * @param CurrentTime 현재 재생 시간 (초 단위)
 * @param PreviousTime 이전 프레임의 재생 시간
 * @param PlayRate 재생 속도
 * @param bIsPlaying 재생 중인지 여부
 */
UCLASS()
class UAnimInstance :
	public UObject
{
	DECLARE_CLASS(UAnimInstance, UObject)

public:
	UAnimInstance();
	~UAnimInstance() override = default;

	// Functions
	void Initialize(USkeletalMeshComponent* InOwner);
	virtual void UpdateAnimation(float DeltaSeconds);
	void TriggerAnimNotifies(float DeltaSeconds);
	void PlayAnimation(UAnimSequenceBase* AnimSequence, float InPlayRate = 1.0f);
	void StopAnimation();
	void SetPosition(float NewTime);

	// Getters
	float GetCurrentTime() const { return CurrentTime; }
	bool IsPlaying() const { return bIsPlaying; }
	UAnimSequenceBase* GetCurrentAnimation() const { return CurrentAnimation; }
	USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
	USkeletalMeshComponent* OwnerComponent;
	UAnimSequenceBase* CurrentAnimation;
	float CurrentTime;
	float PreviousTime;
	float PlayRate;
	bool bIsPlaying;

	virtual void HandleNotify(const FAnimNotifyEvent& NotifyEvent);
};
