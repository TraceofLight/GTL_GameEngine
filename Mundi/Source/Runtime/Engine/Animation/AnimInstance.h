#pragma once
#include "Object.h"
#include "AnimNode_StateMachine.h"
#include "AnimNode_BlendSpace2D.h"
#include "UAnimInstance.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;
class UAnimStateMachine;
class UBlendSpace2D;
struct FAnimNotifyEvent;

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
class UAnimInstance : public UObject
{
public:
	GENERATED_REFLECTION_BODY()
	UAnimInstance();
	~UAnimInstance() override = default;

	// Functions
	void Initialize(USkeletalMeshComponent* InOwner);
	void NativeBeginPlay();
	virtual void UpdateAnimation(float DeltaSeconds);
	void TriggerAnimNotifies(float DeltaSeconds);
	void PlayAnimation(UAnimSequenceBase* AnimSequence, float InPlayRate = 1.0f);
	void StopAnimation();
	void ResumeAnimation();  // Resume from current position
	void SetPosition(float NewTime);

	// Getters
	float GetCurrentTime() const { return CurrentTime; }
	bool IsPlaying() const { return bIsPlaying; }
	float GetPlayRate() const { return PlayRate; }
	UAnimSequenceBase* GetCurrentAnimation() const { return CurrentAnimation; }
	USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

	// Setters
	void SetPlayRate(float InPlayRate) { PlayRate = InPlayRate; }

	// ===== State Machine =====

	/**
	 * @brief State Machine 애셋 설정
	 *
	 * @param InStateMachine State Machine 애셋
	 */
	void SetStateMachine(UAnimStateMachine* InStateMachine);

	/**
	 * @brief State Machine 노드 가져오기
	 */
	FAnimNode_StateMachine* GetStateMachineNode() { return &StateMachineNode; }

	// ===== Blend Space 2D =====

	/**
	 * @brief Blend Space 2D 애셋 설정
	 *
	 * @param InBlendSpace Blend Space 2D 애셋
	 */
	void SetBlendSpace2D(UBlendSpace2D* InBlendSpace);

	/**
	 * @brief Blend Space 2D 노드 가져오기
	 */
	FAnimNode_BlendSpace2D* GetBlendSpace2DNode() { return &BlendSpace2DNode; }

	// Pose Evaluation
	void EvaluateAnimation();

protected:
	USkeletalMeshComponent* OwnerComponent;
	UAnimSequenceBase* CurrentAnimation;
	float CurrentTime;
	float PreviousTime;
	float PlayRate;
	bool bIsPlaying;

	// ===== Animation Nodes =====
	FAnimNode_StateMachine StateMachineNode;
	FAnimNode_BlendSpace2D BlendSpace2DNode;

	virtual void HandleNotify(const FAnimNotifyEvent& NotifyEvent);

// ===== 파라미터(Blackboard) 시스템 =====
public:
	UFUNCTION(LuaBind, DisplayName = "SetFloat")
	void SetFloat(const FString& Key, float Value) { Parameters.Add(Key, Value); }
	float GetFloat(FName Key) const
	{
		if (const float* Val = Parameters.Find(Key))
		{
			return *Val;
		}
		return 0.0f;
	}

	UFUNCTION(LuaBind, DisplayName = "SetBool")
	void SetBool(const FString& Key, bool bValue) { SetFloat(Key, bValue ? 1.0f : 0.0f); }
	bool GetBool(FName Key) const { return GetFloat(Key) > 0.5f; }

protected:
	/** 변수 저장소 (이름 -> 값) */
	TMap<FName, float> Parameters;
};
