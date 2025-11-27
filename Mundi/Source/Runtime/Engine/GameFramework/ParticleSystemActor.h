#pragma once

#include "Actor.h"
#include "AParticleSystemActor.generated.h"

class UParticleSystemComponent;
class UParticleSystem;
class ULineComponent;

/**
 * @brief ParticleSystem을 Scene에 배치하는 Actor
 * @details ParticleSystemComponent를 소유하고 파티클 효과를 재생
 *
 * @param ParticleSystemComponent 파티클 시스템 컴포넌트
 * @param BoundsLineComponent 바운딩 박스/스피어 시각화용 라인 컴포넌트
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

	// 바운딩 박스/스피어 표시
	void UpdateBoundsVisualization(bool bShowBounds);

	// 복사 관련
	void DuplicateSubObjects() override;

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	~AParticleSystemActor() override;

	UParticleSystemComponent* ParticleSystemComponent;
	ULineComponent* BoundsLineComponent;

private:
	void CreateBoundsLines();
};
