#pragma once
#include "ParticleModule.h"
#include "ParticleTypes.h"
#include "UParticleModuleMeshRotation.generated.h"

/**
 * @brief 메시 파티클의 초기 3D 회전값을 설정하는 모듈
 * @details Spawn 시 파티클의 MeshRotation Payload 값을 설정
 *          에디터에서는 Degrees로 입력, 내부적으로 Radians로 변환
 *          Payload 시스템을 사용하여 FBaseParticle 변경 없이 추가 데이터 저장
 *
 * @param StartRotation 파티클 초기 3D 회전 분포 (Degrees: Pitch, Yaw, Roll)
 */
UCLASS()
class UParticleModuleMeshRotation : public UParticleModule
{
	GENERATED_REFLECTION_BODY()

public:
	UParticleModuleMeshRotation();
	~UParticleModuleMeshRotation() override = default;

	// Payload 크기 반환 (FMeshRotationPayloadData 크기)
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	// Serialize/Duplicate
	void Serialize(bool bIsLoading, JSON& InOutHandle) override;
	void DuplicateFrom(const UParticleModule* Source) override;

	// 파티클 초기 3D 회전 분포 (Degrees: Pitch, Yaw, Roll)
	FVectorDistribution StartRotation;

	// UParticleModule 인터페이스
	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 커브 지원 (Distribution 기반)
	bool ModuleHasCurves() const override { return true; }
};
