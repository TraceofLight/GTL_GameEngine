#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"

// Forward declarations
class UStaticMesh;

/**
 * @brief 파티클 타입 데이터의 기본 클래스
 * @details Sprite, Mesh 등 특화된 렌더링 타입의 기본 클래스
 */
UCLASS()
class UParticleModuleTypeDataBase :
	public UParticleModule
{
	DECLARE_CLASS(UParticleModuleTypeDataBase, UParticleModule)

public:
	UParticleModuleTypeDataBase();
	~UParticleModuleTypeDataBase() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	virtual EDynamicEmitterType GetEmitterType() const;
	virtual const char* GetVertexFactoryName() const;
	virtual bool IsGPUSprites() const { return false; }
};

/**
 * @brief 스프라이트 타입 데이터
 * @details 빌보드 스프라이트 파티클 렌더링
 */
UCLASS()
class UParticleModuleTypeDataSprite :
	public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataSprite, UParticleModuleTypeDataBase)

public:
	UParticleModuleTypeDataSprite();
	~UParticleModuleTypeDataSprite() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// UParticleModuleTypeDataBase 인터페이스
	EDynamicEmitterType GetEmitterType() const override;
	const char* GetVertexFactoryName() const override;
};

// Forward declarations for Editor callbacks
class UParticleSystem;

/**
 * @brief 메시 타입 데이터
 * @details 3D 메시 파티클 렌더링
 *
 * @param Mesh 사용할 스태틱 메시
 * @param bCastShadows 그림자 캐스팅 여부
 * @param DoCollisions 충돌 처리 여부
 * @param MeshAlignment 메시 정렬 방식
 * @param bOverrideMaterial 머티리얼 오버라이드 여부
 * @param Pitch 피치 오프셋
 * @param Roll 롤 오프셋
 * @param Yaw 요 오프셋
 */
UCLASS()
class UParticleModuleTypeDataMesh :
	public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataMesh, UParticleModuleTypeDataBase)

public:
	UStaticMesh* Mesh;
	bool bCastShadows;
	bool DoCollisions;
	EParticleAxisLock MeshAlignment;
	bool bOverrideMaterial;
	float Pitch;
	float Roll;
	float Yaw;

	UParticleModuleTypeDataMesh();
	~UParticleModuleTypeDataMesh() override = default;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	EDynamicEmitterType GetEmitterType() const override;
	const char* GetVertexFactoryName() const override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;
	void OnMeshChanged();

	void SetOwnerSystem(UParticleSystem* InOwner) { OwnerSystem = InOwner; }

private:
	/** 소유 ParticleSystem (변경 전파용) */
	UParticleSystem* OwnerSystem = nullptr;
};
