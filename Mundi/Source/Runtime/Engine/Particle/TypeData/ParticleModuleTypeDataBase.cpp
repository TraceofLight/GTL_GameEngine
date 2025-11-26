#include "pch.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleSystem.h"
#include "StaticMesh.h"
#include "ResourceManager.h"

IMPLEMENT_CLASS(UParticleModuleTypeDataBase)
IMPLEMENT_CLASS(UParticleModuleTypeDataSprite)
IMPLEMENT_CLASS(UParticleModuleTypeDataMesh)
IMPLEMENT_CLASS(UParticleModuleTypeDataBeam)

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

// ========== UParticleModuleTypeDataBeam ==========

UParticleModuleTypeDataBeam::UParticleModuleTypeDataBeam()
	: SourceOffset(FVector::Zero())
	, TargetOffset(FVector(100.0f, 0.0f, 0.0f))  // 기본: X축 방향 100 유닛
	, BeamWidth(10.0f)
	, Segments(10)
	, bTaperBeam(false)
	, TaperFactor(0.0f)
	, Sheets(1)
	, UVTiling(1.0f)
{
}

EDynamicEmitterType UParticleModuleTypeDataBeam::GetEmitterType() const
{
	return EDynamicEmitterType::Beam2;
}

const char* UParticleModuleTypeDataBeam::GetVertexFactoryName() const
{
	return "FParticleBeamVertexFactory";
}

void UParticleModuleTypeDataBeam::Serialize(bool bIsLoading, JSON& InOutHandle)
{
	UParticleModuleTypeDataBase::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		// Source/Target Offset
		if (InOutHandle.hasKey("SourceOffset"))
		{
			SourceOffset.X = static_cast<float>(InOutHandle["SourceOffset"][0].ToFloat());
			SourceOffset.Y = static_cast<float>(InOutHandle["SourceOffset"][1].ToFloat());
			SourceOffset.Z = static_cast<float>(InOutHandle["SourceOffset"][2].ToFloat());
		}
		if (InOutHandle.hasKey("TargetOffset"))
		{
			TargetOffset.X = static_cast<float>(InOutHandle["TargetOffset"][0].ToFloat());
			TargetOffset.Y = static_cast<float>(InOutHandle["TargetOffset"][1].ToFloat());
			TargetOffset.Z = static_cast<float>(InOutHandle["TargetOffset"][2].ToFloat());
		}

		// Beam 형태
		if (InOutHandle.hasKey("BeamWidth")) BeamWidth = JsonToFloatDistribution(InOutHandle["BeamWidth"]);
		if (InOutHandle.hasKey("Segments")) Segments = static_cast<int32>(InOutHandle["Segments"].ToInt());
		if (InOutHandle.hasKey("bTaperBeam")) bTaperBeam = InOutHandle["bTaperBeam"].ToBool();
		if (InOutHandle.hasKey("TaperFactor")) TaperFactor = static_cast<float>(InOutHandle["TaperFactor"].ToFloat());

		// 렌더링
		if (InOutHandle.hasKey("Sheets")) Sheets = static_cast<int32>(InOutHandle["Sheets"].ToInt());
		if (InOutHandle.hasKey("UVTiling")) UVTiling = static_cast<float>(InOutHandle["UVTiling"].ToFloat());
	}
	else
	{
		// Source/Target Offset
		JSON srcArr = JSON::Make(JSON::Class::Array);
		srcArr.append(SourceOffset.X);
		srcArr.append(SourceOffset.Y);
		srcArr.append(SourceOffset.Z);
		InOutHandle["SourceOffset"] = srcArr;

		JSON tgtArr = JSON::Make(JSON::Class::Array);
		tgtArr.append(TargetOffset.X);
		tgtArr.append(TargetOffset.Y);
		tgtArr.append(TargetOffset.Z);
		InOutHandle["TargetOffset"] = tgtArr;

		// Beam 형태
		InOutHandle["BeamWidth"] = FloatDistributionToJson(BeamWidth);
		InOutHandle["Segments"] = Segments;
		InOutHandle["bTaperBeam"] = bTaperBeam;
		InOutHandle["TaperFactor"] = TaperFactor;

		// 렌더링
		InOutHandle["Sheets"] = Sheets;
		InOutHandle["UVTiling"] = UVTiling;
	}
}

void UParticleModuleTypeDataBeam::DuplicateFrom(const UParticleModule* Source)
{
	UParticleModuleTypeDataBase::DuplicateFrom(Source);

	const UParticleModuleTypeDataBeam* SrcBeam = static_cast<const UParticleModuleTypeDataBeam*>(Source);
	if (!SrcBeam)
	{
		return;
	}

	SourceOffset = SrcBeam->SourceOffset;
	TargetOffset = SrcBeam->TargetOffset;
	BeamWidth = SrcBeam->BeamWidth;
	Segments = SrcBeam->Segments;
	bTaperBeam = SrcBeam->bTaperBeam;
	TaperFactor = SrcBeam->TaperFactor;
	Sheets = SrcBeam->Sheets;
	UVTiling = SrcBeam->UVTiling;
}
