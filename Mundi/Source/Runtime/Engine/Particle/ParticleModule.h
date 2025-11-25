#pragma once
#include "Object.h"
#include "Particle.h"

// Forward declarations
struct FParticleEmitterInstance;
class UParticleModuleTypeDataBase;

/**
 * @brief 파티클 모듈의 기본 클래스
 * @details 모든 파티클 모듈의 부모 클래스로, Spawn / Update / FinalUpdate 인터페이스 제공
 *
 * @param bSpawnModule Spawn 시점에 호출되는지 여부
 * @param bUpdateModule Update 시점에 호출되는지 여부
 * @param bFinalUpdateModule FinalUpdate 시점에 호출되는지 여부
 * @param bSupported3DDrawMode 3D 드로우 모드 지원 여부
 * @param b3DDrawMode 3D 드로우 모드 활성화 여부
 * @param LODValidity LOD 유효성 비트마스크
 */
UCLASS()
class UParticleModule :
	public UObject
{
	DECLARE_CLASS(UParticleModule, UObject)

public:
	UParticleModule();
	~UParticleModule() override = default;

	// Functions
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase);
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData);
	virtual uint32 RequiredBytesPerInstance();
	bool IsValidForLODLevel(int32 LODLevel) const;
	void SetLODValidity(int32 LODLevel, bool bValid);

	// Serialize/Duplicate
	virtual void Serialize(bool bIsLoading, JSON& InOutHandle);
	virtual void DuplicateFrom(const UParticleModule* Source);

	// Getters
	bool IsSpawnModule() const { return bSpawnModule; }
	bool IsUpdateModule() const { return bUpdateModule; }
	bool IsFinalUpdateModule() const { return bFinalUpdateModule; }
	bool IsSupported3DDrawMode() const { return bSupported3DDrawMode; }
	bool Is3DDrawMode() const { return b3DDrawMode; }
	uint8 GetLODValidity() const { return LODValidity; }

protected:
	bool bSpawnModule;
	bool bUpdateModule;
	bool bFinalUpdateModule;
	bool bSupported3DDrawMode;
	bool b3DDrawMode;
	uint8 LODValidity;
};
