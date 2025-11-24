#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

#include "UParticleModuleRequired.generated.h"

// Forward declarations
class UMaterial;

/**
 * 렌더 스레드용 Required Module 데이터 (최소 버전)
 */
struct FParticleRequiredModule
{
	/** Number of SubUV animation frames */
	uint32 NumFrames;

	/** Alpha cutout threshold */
	float AlphaThreshold;

	/** Constructor */
	FParticleRequiredModule()
		: NumFrames(1)
		, AlphaThreshold(0.0f)
	{
	}
};


/**
 * @brief 이미터에 필수적인 설정을 포함하는 모듈
 * @details 파티클의 기본 속성들을 정의 (Material, Duration, ScreenAlignment 등)
 *
 * @param Material 파티클에 사용할 머티리얼
 * @param EmitterDuration 이미터 지속 시간
 * @param EmitterDurationLow 이미터 지속 시간 최소값 (랜덤)
 * @param EmitterLoops 루프 횟수 (0 = 무한)
 * @param bDurationRecalcEachLoop 매 루프마다 지속 시간 재계산
 * @param ScreenAlignment 화면 정렬 방식
 * @param SubImages_Horizontal SubUV 가로 이미지 수
 * @param SubImages_Vertical SubUV 세로 이미지 수
 * @param SortMode 정렬 모드
 * @param bUseLocalSpace 로컬 공간 사용 여부
 * @param bKillOnDeactivate 비활성화 시 파티클 제거
 * @param bKillOnCompleted 완료 시 파티클 제거
 * @param MaxDrawCount 최대 그리기 개수
 * @param EmitterNormalsMode 노멀 모드
 */
UCLASS()
class UParticleModuleRequired :
	public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleRequired();
	~UParticleModuleRequired() override = default;

	// Functions
	float GetEmitterDuration() const;
	int32 GetTotalSubImages() const;

	/**
	 * Create renderer resource (render thread copy)
	 * 렌더러 리소스 생성 (렌더 스레드용 복사본)
	 *
	 * @return FParticleRequiredModule* - Allocated render thread data (caller must delete)
	 */
	FParticleRequiredModule* CreateRendererResource() const;

	// Getters
	UMaterial* GetMaterial() const { return Material; }
	float GetEmitterDurationValue() const { return EmitterDuration; }
	float GetEmitterDurationLow() const { return EmitterDurationLow; }
	int32 GetEmitterLoops() const { return EmitterLoops; }
	bool IsDurationRecalcEachLoop() const { return bDurationRecalcEachLoop; }
	EParticleScreenAlignment GetScreenAlignment() const { return ScreenAlignment; }
	int32 GetSubImagesHorizontal() const { return SubImages_Horizontal; }
	int32 GetSubImagesVertical() const { return SubImages_Vertical; }
	EParticleSortMode GetSortMode() const { return SortMode; }
	bool IsUseLocalSpace() const { return bUseLocalSpace; }
	bool IsKillOnDeactivate() const { return bKillOnDeactivate; }
	bool IsKillOnCompleted() const { return bKillOnCompleted; }
	int32 GetMaxDrawCount() const { return MaxDrawCount; }
	EEmitterNormalsMode GetEmitterNormalsMode() const { return EmitterNormalsMode; }
	EParticleSubUVInterpMethod GetInterpolationMethod() const { return InterpolationMethod; }
	EParticleAxisLock GetAxisLockOption() const { return AxisLockOption; }

	// Setters
	void SetMaterial(UMaterial* InMaterial) { Material = InMaterial; }
	void SetEmitterDuration(float InDuration) { EmitterDuration = InDuration; }
	void SetEmitterDurationLow(float InDurationLow) { EmitterDurationLow = InDurationLow; }
	void SetEmitterLoops(int32 InLoops) { EmitterLoops = InLoops; }
	void SetDurationRecalcEachLoop(bool bRecalc) { bDurationRecalcEachLoop = bRecalc; }
	void SetScreenAlignment(EParticleScreenAlignment InAlignment) { ScreenAlignment = InAlignment; }
	void SetSubImagesHorizontal(int32 InCount) { SubImages_Horizontal = InCount; }
	void SetSubImagesVertical(int32 InCount) { SubImages_Vertical = InCount; }
	void SetSortMode(EParticleSortMode InMode) { SortMode = InMode; }
	void SetUseLocalSpace(bool bInUseLocalSpace) { bUseLocalSpace = bInUseLocalSpace; }
	void SetKillOnDeactivate(bool bInKill) { bKillOnDeactivate = bInKill; }
	void SetKillOnCompleted(bool bInKill) { bKillOnCompleted = bInKill; }
	void SetMaxDrawCount(int32 InCount) { MaxDrawCount = InCount; }
	void SetEmitterNormalsMode(EEmitterNormalsMode InMode) { EmitterNormalsMode = InMode; }
	void SetInterpolationMethod(EParticleSubUVInterpMethod InMethod) { InterpolationMethod = InMethod; }
	void SetAxisLockOption(EParticleAxisLock InOption) { AxisLockOption = InOption; }

protected:
	UMaterial* Material;
	float EmitterDuration;
	float EmitterDurationLow;
	int32 EmitterLoops;
	bool bDurationRecalcEachLoop;
	EParticleScreenAlignment ScreenAlignment;
	int32 SubImages_Horizontal;
	int32 SubImages_Vertical;
	EParticleSortMode SortMode;
	bool bUseLocalSpace;
	bool bKillOnDeactivate;
	bool bKillOnCompleted;
	int32 MaxDrawCount;
	EEmitterNormalsMode EmitterNormalsMode;
	EParticleSubUVInterpMethod InterpolationMethod;
	EParticleAxisLock AxisLockOption;
};

