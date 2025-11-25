#pragma once

#include "Actor.h"
#include "AParticleSystemActor.generated.h"

class UParticleSystemComponent;
class UParticleSystem;

/**
 * @brief ParticleSystem을 Scene에 배치하는 Actor
 * @details ParticleSystemComponent를 소유하고 파티클 효과를 재생
 *
 * @param ParticleSystemComponent 파티클 시스템 컴포넌트
 */
UCLASS(DisplayName="파티클 시스템", Description="파티클 효과를 배치하는 액터입니다")
class AParticleSystemActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AParticleSystemActor();

	virtual void Tick(float DeltaTime) override;

	// Getters
	UParticleSystemComponent* GetParticleSystemComponent() const { return ParticleSystemComponent; }

	// ParticleSystem Template 설정
	void SetParticleSystem(UParticleSystem* InTemplate);

	// 복사 관련
	void DuplicateSubObjects() override;

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	~AParticleSystemActor() override;

	UParticleSystemComponent* ParticleSystemComponent;
};
