#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"
#include "UParticleModuleMeshRotationRate.generated.h"

/**
 * @brief 메시 파티클의 3D 회전 속도를 설정하는 모듈
 * @details Spawn 시 파티클의 MeshRotationRate Payload 값을 설정
 *          에디터에서는 Degrees/Second로 입력, 내부적으로 Radians/Second로 변환
 *          Payload 시스템을 사용하여 FBaseParticle 변경 없이 추가 데이터 저장
 *
 * @param StartRotationRate 파티클 3D 회전 속도 분포 (Degrees/sec: Pitch, Yaw, Roll)
 */
UCLASS()
class UParticleModuleMeshRotationRate : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleMeshRotationRate();
	~UParticleModuleMeshRotationRate() override = default;

	// Payload 크기 반환 (FMeshRotationPayloadData 크기)
	// MeshRotation 모듈과 동일한 Payload 사용
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 3D 회전 속도 분포 (Degrees/sec: Pitch, Yaw, Roll)
	FVectorDistribution StartRotationRate;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
