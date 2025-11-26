#pragma once
#include "ParticleTypes.h"
#include "Source/Runtime/AssetManagement/ResourceBase.h"

#include "UParticleSystem.generated.h"

class UParticleEmitter;
struct ID3D11Device;

/**
 * @brief 파티클 시스템 클래스
 * @details 여러 이미터를 포함하는 완전한 파티클 효과를 정의
 *
 * @param Emitters 이미터 배열
 * @param UpdateTime_FPS 업데이트 FPS (0 = 무제한)
 * @param UpdateTime_Delta 고정 델타 타임
 * @param WarmupTime 웜업 시간 (초)
 * @param WarmupTickRate 웜업 틱 레이트
 * @param bUseFixedRelativeBoundingBox 고정 바운딩 박스 사용
 * @param FixedRelativeBoundingBox 고정 바운딩 박스 크기
 * @param LODDistanceCheckTime LOD 거리 체크 시간
 * @param LODMethod LOD 선택 방법
 * @param LODDistances LOD 전환 거리 배열
 * @param bRegenerateLODDuplicate LOD 재생성 여부
 * @param SystemUpdateMode 시스템 업데이트 모드
 * @param bOrientZAxisTowardCamera Z축을 카메라 방향으로 정렬
 * @param SecondsBeforeInactive 비활성화까지 대기 시간
 * @param Delay 시작 지연 시간
 * @param bAutoDeactivate 자동 비활성화
 */
UCLASS()
class UParticleSystem :
	public UResourceBase
{
	GENERATED_REFLECTION_BODY()

public:
	// 이미터 배열
	TArray<UParticleEmitter*> Emitters;

	// 업데이트 설정
	float UpdateTime_FPS;
	float UpdateTime_Delta;
	float WarmupTime;
	int32 WarmupTickRate;

	// 바운딩 박스
	bool bUseFixedRelativeBoundingBox;
	FVector FixedRelativeBoundingBox;

	// LOD 설정
	float LODDistanceCheckTime;
	EParticleSystemLODMethod LODMethod;
	TArray<float> LODDistances;
	bool bRegenerateLODDuplicate;

	// 시스템 설정
	EParticleSystemUpdateMode SystemUpdateMode;
	bool bOrientZAxisTowardCamera;
	float SecondsBeforeInactive;
	float Delay;
	bool bAutoDeactivate;

	// 썸네일 (Base64 인코딩된 DDS 데이터)
	FString ThumbnailData;

	// 생성자/소멸자
	UParticleSystem();
	~UParticleSystem() override = default;

	// ResourceManager 패턴 Load (기존 패턴과 호환)
	void Load(const FString& InFilePath, ID3D11Device* InDevice);

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle);
	void DuplicateFrom(const UParticleSystem* Source);

	// 파일 저장/로드 (직접 사용)
	bool SaveToFile(const FString& InFilePath);
	bool LoadFromFileInternal(const FString& InFilePath);

	// Functions
	void UpdateAllModuleLists();
	bool ContainsEmitterType(EDynamicEmitterType EmitterType);
	void BuildEmitters();
	void SetupSoloing();
	int32 GetNumLODs() const;
	float GetLODDistance(int32 LODLevelIndex) const;

	// Getters
	int32 GetNumEmitters() const { return static_cast<int32>(Emitters.size()); }
	UParticleEmitter* GetEmitter(int32 Index);
	bool HasGPUEmitter() const { return bHasGPUEmitter; }
	float GetMaxDuration() const { return MaxDuration; }

	void OnModuleChanged();

protected:
	// 캐시된 정보
	bool bHasGPUEmitter;
	float MaxDuration;
};
