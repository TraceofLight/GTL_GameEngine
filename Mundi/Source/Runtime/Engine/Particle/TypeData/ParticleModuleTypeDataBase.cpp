#include "pch.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleSystem.h"

IMPLEMENT_CLASS(UParticleModuleTypeDataBase)
IMPLEMENT_CLASS(UParticleModuleTypeDataSprite)
IMPLEMENT_CLASS(UParticleModuleTypeDataMesh)

// ========== UParticleModuleTypeDataBase ==========

UParticleModuleTypeDataBase::UParticleModuleTypeDataBase()
{
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

/**
 * 이미터 타입 반환
 * @return 동적 이미터 타입
 */
EDynamicEmitterType UParticleModuleTypeDataBase::GetEmitterType() const
{
	return EDynamicEmitterType::Sprite;
}

/**
 * 이 타입 데이터가 사용하는 버텍스 팩토리 타입 반환
 * @return 버텍스 팩토리 타입 (문자열)
 */
const char* UParticleModuleTypeDataBase::GetVertexFactoryName() const
{
	return "FParticleSpriteVertexFactory";
}

// ========== UParticleModuleTypeDataSprite ==========

UParticleModuleTypeDataSprite::UParticleModuleTypeDataSprite()
{
}

EDynamicEmitterType UParticleModuleTypeDataSprite::GetEmitterType() const
{
	return EDynamicEmitterType::Sprite;
}

const char* UParticleModuleTypeDataSprite::GetVertexFactoryName() const
{
	return "FParticleSpriteVertexFactory";
}

// ========== UParticleModuleTypeDataMesh ==========

UParticleModuleTypeDataMesh::UParticleModuleTypeDataMesh()
	: Mesh(nullptr)
	, bCastShadows(false)
	, DoCollisions(false)
	, MeshAlignment(EParticleAxisLock::None)
	, bOverrideMaterial(false)
	, Pitch(0.0f)
	, Roll(0.0f)
	, Yaw(0.0f)
{
}

EDynamicEmitterType UParticleModuleTypeDataMesh::GetEmitterType() const
{
	return EDynamicEmitterType::Mesh;
}

const char* UParticleModuleTypeDataMesh::GetVertexFactoryName() const
{
	return "FMeshParticleVertexFactory";
}

uint32 UParticleModuleTypeDataMesh::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	// 메시 파티클은 추가로 회전/스케일 데이터가 필요할 수 있음
	// 현재는 FBaseParticle에 이미 포함되어 있으므로 0
	return 0;
}

/**
 * Mesh 프로퍼티가 변경되었을 때 호출
 */
void UParticleModuleTypeDataMesh::OnMeshChanged()
{
	// 소유 ParticleSystem에 변경 전파
	if (OwnerSystem)
	{
		OwnerSystem->OnModuleChanged();
	}
}
