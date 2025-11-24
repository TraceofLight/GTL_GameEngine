#pragma once
#include "ParticleTypes.h"
#include "UParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModuleTypeDataBase;

/**
 * @brief 파티클 이미터 클래스
 * @details 파티클 시스템 내의 단일 이미터로, 여러 LOD 레벨을 관리하고 모듈 정보를 캐싱
 *
 * @param EmitterName 이미터 이름
 * @param LODLevels LOD 레벨 배열
 * @param bDisabledLODsKeepEmitterAlive 비활성 LOD에서도 이미터 유지
 * @param bCollapsed 에디터에서 축소 상태
 * @param DetailMode 디테일 모드
 * @param EmitterRenderMode 렌더링 모드 (2D/3D)
 * @param EmitterEditorColor 에디터 표시 색상
 * @param InitialAllocationCount 초기 파티클 할당 수
 * @param bIsSoloing 솔로 모드 여부
 * @param PeakActiveParticles 최대 활성 파티클 수 (캐시)
 * @param RequiredBytes 파티클당 필요 바이트 (캐시)
 * @param ReqInstanceBytes 인스턴스당 필요 바이트 (캐시)
 * @param bRequiresLoopNotification 루프 알림 필요 여부 (캐시)
 * @param bAxisLockEnabled 축 고정 활성화 여부 (캐시)
 * @param LockAxisFlags 축 고정 플래그 (캐시)
 */
UCLASS()
class UParticleEmitter :
	public UObject
{
	GENERATED_REFLECTION_BODY()

public:
	// 기본 속성
	FString EmitterName;
	TArray<UParticleLODLevel*> LODLevels;
	bool bDisabledLODsKeepEmitterAlive;
	bool bCollapsed;
	int32 DetailMode;
	EEmitterRenderMode EmitterRenderMode;
	FLinearColor EmitterEditorColor;
	int32 InitialAllocationCount;
	bool bIsSoloing;

	// 생성자/소멸자
	UParticleEmitter();
	~UParticleEmitter() override = default;

	// Functions
	void CacheEmitterModuleInfo();
	UParticleLODLevel* GetLODLevel(int32 LODIndex);
	void SetEmitterName(const FString& InName);
	bool AutogenerateLowestLODLevel();

	// Getters
	const FString& GetEmitterName() const { return EmitterName; }
	int32 GetNumLODs() const { return static_cast<int32>(LODLevels.size()); }
	int32 GetPeakActiveParticles() const { return PeakActiveParticles; }
	uint32 GetRequiredBytes() const { return RequiredBytes; }
	uint32 GetReqInstanceBytes() const { return ReqInstanceBytes; }
	bool RequiresLoopNotification() const { return bRequiresLoopNotification; }
	bool IsAxisLockEnabled() const { return bAxisLockEnabled; }
	EParticleAxisLock GetLockAxisFlags() const { return LockAxisFlags; }
	UParticleModuleTypeDataBase* GetTypeDataModule() const { return TypeDataModule; }

protected:
	// 캐시된 모듈 정보
	int32 PeakActiveParticles;
	uint32 RequiredBytes;
	uint32 ReqInstanceBytes;
	bool bRequiresLoopNotification;
	bool bAxisLockEnabled;
	EParticleAxisLock LockAxisFlags;
	UParticleModuleTypeDataBase* TypeDataModule;
};
