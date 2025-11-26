#pragma once
#include "../ParticleModule.h"
#include "UParticleModuleSubUV.generated.h"

/**
 * SubUV 페이로드 데이터
 * FBaseParticle 뒤에 붙는 추가 데이터
 */
struct FSubUVPayloadData
{
	/** 현재 SubUV 프레임 인덱스 (float로 저장하여 보간 지원) */
	float ImageIndex;

	FSubUVPayloadData()
		: ImageIndex(0.0f)
	{
	}
};

/**
 * @brief SubUV 애니메이션 모듈
 * @details 파티클의 텍스처를 프레임 단위로 애니메이션화
 *
 * [SubUV란?]
 * - 여러 개의 작은 이미지가 타일 형태로 배치된 텍스처 시트
 * - 각 파티클이 수명에 따라 다른 타일을 표시하여 애니메이션 효과 생성
 * - 예: 폭발 효과 (8x8 타일, 64프레임)
 *
 * [동작 방식]
 * 1. Spawn: 시작 프레임 설정 (랜덤 또는 0)
 * 2. Update: RelativeTime에 따라 프레임 진행
 * 3. Shader: floor(ImageIndex)로 현재 프레임, frac(ImageIndex)로 다음 프레임과 보간
 *
 * @param InterpolationMethod 보간 방식 (None, Linear, Random, RandomBlend)
 * @param bUseRealTime true이면 수명 대신 실시간 기준으로 애니메이션
 */
UCLASS()
class UParticleModuleSubUV : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleSubUV();
	~UParticleModuleSubUV() override = default;

	// ========== ParticleModule Override ==========
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// ========== Getters ==========
	EParticleSubUVInterpMethod GetInterpolationMethod() const { return InterpolationMethod; }
	bool IsUseRealTime() const { return bUseRealTime; }
	float GetStartingFrame() const { return StartingFrame; }

	// ========== Setters ==========
	void SetInterpolationMethod(EParticleSubUVInterpMethod InMethod) { InterpolationMethod = InMethod; }
	void SetUseRealTime(bool bInUseRealTime) { bUseRealTime = bInUseRealTime; }
	void SetStartingFrame(float InFrame) { StartingFrame = InFrame; }

public:  // Make properties public for reflection system
	/** 보간 방식 (Required Module의 InterpolationMethod를 오버라이드) */
	UPROPERTY()
	EParticleSubUVInterpMethod InterpolationMethod;

	/** true이면 파티클 수명 대신 실시간 기준으로 애니메이션 */
	UPROPERTY()
	bool bUseRealTime;

	/** 시작 프레임 (0.0 = 첫 프레임, -1.0 = 랜덤) */
	UPROPERTY()
	float StartingFrame;

	/** 실시간 애니메이션 속도 (프레임/초) */
	UPROPERTY()
	float FrameRate;
};
