#include "pch.h"
#include "ParticleMeshEmitterInstance.h"
#include "DynamicEmitterDataBase.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleSystemComponent.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "TypeData/ParticleModuleTypeDataBase.h"

// ============== 생성자/소멸자 ==============

FParticleMeshEmitterInstance::FParticleMeshEmitterInstance()
	: FParticleEmitterInstance()
	, MeshTypeData(nullptr)
	, InstanceBuffer(nullptr)
{
}

FParticleMeshEmitterInstance::~FParticleMeshEmitterInstance()
{
	if (InstanceBuffer)
	{
		InstanceBuffer->Release();
		InstanceBuffer = nullptr;
	}
}

// ============== Init ==============

void FParticleMeshEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	// 1. 부모 Init 호출 (메모리 할당, Stride 계산, LOD 설정 등)
	FParticleEmitterInstance::Init(InTemplate, InComponent);

	// 2. TypeDataModule에서 MeshTypeData 가져오기 
	if (CurrentLODLevel && CurrentLODLevel->TypeDataModule)
	{
		MeshTypeData = Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule);
	}

}

// ============== GetDynamicData ==============

FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
	// 1. IsDynamicDataRequired() 체크
	if (!IsDynamicDataRequired())
	{
		return nullptr;
	}
	// 2. dynamic data 생성
	FDynamicMeshEmitterData* NewEmitterData = new FDynamicMeshEmitterData();
	// 3. source data 채우기
	if (!FillReplayData(NewEmitterData->Source))
	{
		delete NewEmitterData;
		return nullptr;
	}

	// 4. Setup dynamic render data (Init must be called AFTER filling source data)
	NewEmitterData->Init(bSelected);
	NewEmitterData->OwnerInstance = this;

	return NewEmitterData;
}

// ============== FillReplayData ==============

bool FParticleMeshEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// 1. 부모 FillReplayData 호출
	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return false;
	}

	// 2. FDynamicMeshEmitterReplayData로 캐스팅
	FDynamicMeshEmitterReplayData& MeshData = static_cast<FDynamicMeshEmitterReplayData&>(OutData);

	// 3. 에미터 타입 설정
	MeshData.eEmitterType = EDynamicEmitterType::Mesh;

	// 4. 머티리얼 설정 (RequiredModule에서)
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		MeshData.MaterialInterface = CurrentLODLevel->RequiredModule->GetMaterial();

		// 상속 구조상 RequiredModule 멤버가 있으므로 채워줌
		MeshData.RequiredModule = CurrentLODLevel->RequiredModule->CreateRendererResource();
	}

	// 5. 메시 전용 데이터 설정 (MeshTypeData에서)
	if (MeshTypeData)
	{
		MeshData.MeshAlignment = static_cast<uint8>(MeshTypeData->MeshAlignment);
		MeshData.MeshAsset = MeshTypeData->Mesh;
	}

	return true;
}

// ============== GetReplayData ==============

FDynamicEmitterReplayDataBase* FParticleMeshEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return nullptr;
	}
	FDynamicMeshEmitterReplayData* NewData = new FDynamicMeshEmitterReplayData();

	if (!FillReplayData(*NewData))
	{
		delete NewData;
		return nullptr;
	}
	return NewData;

}

// ============== GetAllocatedSize ==============

void FParticleMeshEmitterInstance::GetAllocatedSize(int32& OutNum, int32& OutMax)
{
	// TODO: 구현
	// 부모 GetAllocatedSize 호출하거나 직접 계산
	// sizeof(FParticleMeshEmitterInstance) + 파티클 데이터 크기
	int32 Size = sizeof(FParticleMeshEmitterInstance);
	int32 ActiveParticleDataSize = (ParticleData != nullptr) ? (ActiveParticles * ParticleStride) : 0;
	int32 MaxActiveParticleDataSize = (ParticleData != nullptr) ? (MaxActiveParticles * ParticleStride) : 0;
	int32 ActiveParticleIndexSize = (ParticleIndices != nullptr) ? (ActiveParticles * sizeof(uint16)) : 0;
	int32 MaxActiveParticleIndexSize = (ParticleIndices != nullptr) ? (MaxActiveParticles * sizeof(uint16)) : 0;

	OutNum = Size + ActiveParticleDataSize + ActiveParticleIndexSize;
	OutMax = Size + MaxActiveParticleDataSize + MaxActiveParticleIndexSize;
}
