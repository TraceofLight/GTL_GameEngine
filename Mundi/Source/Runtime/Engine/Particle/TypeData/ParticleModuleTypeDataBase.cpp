#include "pch.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleSystem.h"
#include "StaticMesh.h"
#include "ResourceManager.h"

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

void UParticleModuleTypeDataBase::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bIsLoading, InOutHandle);
	// Base class has no additional data
}

void UParticleModuleTypeDataBase::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModule::DuplicateFrom(Source);
	// Base class has no additional data
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

void UParticleModuleTypeDataSprite::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModuleTypeDataBase::Serialize(bIsLoading, InOutHandle);
	// Sprite has no additional data
}

void UParticleModuleTypeDataSprite::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModuleTypeDataBase::DuplicateFrom(Source);
	// Sprite has no additional data
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

void UParticleModuleTypeDataMesh::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModuleTypeDataBase::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		// Mesh 로드
		if (InOutHandle.hasKey("MeshPath"))
		{
			FString MeshPath = InOutHandle["MeshPath"].ToString();
			if (!MeshPath.empty())
			{
				// 상대 경로를 현재 작업 디렉토리 기준으로 해석
				FString ResolvedPath = ResolveAssetPath(MeshPath);
				Mesh = UResourceManager::GetInstance().Load<UStaticMesh>(ResolvedPath);
			}
		}

		if (InOutHandle.hasKey("bCastShadows")) bCastShadows = InOutHandle["bCastShadows"].ToBool();
		if (InOutHandle.hasKey("DoCollisions")) DoCollisions = InOutHandle["DoCollisions"].ToBool();
		if (InOutHandle.hasKey("MeshAlignment")) MeshAlignment = static_cast<EParticleAxisLock>(InOutHandle["MeshAlignment"].ToInt());
		if (InOutHandle.hasKey("bOverrideMaterial")) bOverrideMaterial = InOutHandle["bOverrideMaterial"].ToBool();
		if (InOutHandle.hasKey("Pitch")) Pitch = static_cast<float>(InOutHandle["Pitch"].ToFloat());
		if (InOutHandle.hasKey("Roll")) Roll = static_cast<float>(InOutHandle["Roll"].ToFloat());
		if (InOutHandle.hasKey("Yaw")) Yaw = static_cast<float>(InOutHandle["Yaw"].ToFloat());
	}
	else
	{
		// Mesh 저장
		if (Mesh)
		{
			FString RelativePath = MakeAssetRelativePath(Mesh->GetFilePath());
			InOutHandle["MeshPath"] = RelativePath;
		}
		else
		{
			InOutHandle["MeshPath"] = FString("");
		}

		InOutHandle["bCastShadows"] = bCastShadows;
		InOutHandle["DoCollisions"] = DoCollisions;
		InOutHandle["MeshAlignment"] = static_cast<int32>(MeshAlignment);
		InOutHandle["bOverrideMaterial"] = bOverrideMaterial;
		InOutHandle["Pitch"] = Pitch;
		InOutHandle["Roll"] = Roll;
		InOutHandle["Yaw"] = Yaw;
	}
}

void UParticleModuleTypeDataMesh::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModuleTypeDataBase::DuplicateFrom(Source);

	const UParticleModuleTypeDataMesh* SrcMesh = static_cast<const UParticleModuleTypeDataMesh*>(Source);
	if (!SrcMesh)
	{
		return;
	}

	Mesh = SrcMesh->Mesh;
	bCastShadows = SrcMesh->bCastShadows;
	DoCollisions = SrcMesh->DoCollisions;
	MeshAlignment = SrcMesh->MeshAlignment;
	bOverrideMaterial = SrcMesh->bOverrideMaterial;
	Pitch = SrcMesh->Pitch;
	Roll = SrcMesh->Roll;
	Yaw = SrcMesh->Yaw;
}
