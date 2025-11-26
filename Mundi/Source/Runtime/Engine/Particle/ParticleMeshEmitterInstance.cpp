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
	// 부모 Init 호출 (메모리 할당, Stride 계산, LOD 설정 등)
	FParticleEmitterInstance::Init(InTemplate, InComponent);

	// TypeDataModule에서 MeshTypeData 가져오기
	if (CurrentLODLevel && CurrentLODLevel->TypeDataModule)
	{
		MeshTypeData = Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule);
	}

	// 3. InstanceBuffer 생성 (GPU 인스턴싱용)
	if (Component && MaxActiveParticles > 0)
	{
		ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
		if (Device)
		{
			// 동적 인스턴스 버퍼 생성 (매 프레임 업데이트)
			const int32 MaxInstanceCount = MaxActiveParticles;

			// 버퍼 디스크립션 설정
			D3D11_BUFFER_DESC BufferDesc = {};
			BufferDesc.ByteWidth = sizeof(FMeshParticleInstanceVertex) * MaxInstanceCount;
			BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			BufferDesc.MiscFlags = 0;
			BufferDesc.StructureByteStride = 0;

			// 버퍼 생성
			HRESULT hr = Device->CreateBuffer(&BufferDesc, nullptr, &InstanceBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to create particle mesh instance buffer");
			}
		}
	}
}

// ============== IsDynamicDataRequired ==============

bool FParticleMeshEmitterInstance::IsDynamicDataRequired() const
{
	// MeshTypeData 유효성 체크
	if (!MeshTypeData)
	{
		return false;
	}

	// Mesh 에셋 유효성 체크
	UStaticMesh* Mesh = MeshTypeData->Mesh;
	if (!Mesh)
	{
		return false;
	}

	// 렌더 데이터 유효성 체크 (버퍼가 생성되어 있어야 함)
	if (!Mesh->GetVertexBuffer() || Mesh->GetIndexCount() == 0)
	{
		return false;
	}

	// 부모 체크 (ActiveParticles > 0 && CurrentLODLevel != nullptr)
	return FParticleEmitterInstance::IsDynamicDataRequired();
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

		// BlendMode (렌더러에서 블렌드 스테이트 분기용)
		MeshData.BlendMode = CurrentLODLevel->RequiredModule->GetBlendMode();
	}

	// 5. 메시 전용 데이터 설정 (MeshTypeData에서)
	if (MeshTypeData)
	{
		MeshData.MeshAlignment = static_cast<uint8>(MeshTypeData->MeshAlignment);
		MeshData.MeshAsset = MeshTypeData->Mesh;
	}

	// 6. MeshRotation Payload 오프셋 설정
	// bMeshRotationActive 플래그가 true인 경우에만 활성화
	MeshData.MeshRotationOffset = bMeshRotationActive ? PayloadOffset : 0;
	MeshData.bMeshRotationActive = bMeshRotationActive;

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

// ============== Resize ==============

bool FParticleMeshEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	// 1. 부모 Resize 호출 (파티클 데이터/인덱스 재할당)
	// 부모에서는 Sprite 버퍼만 처리하므로 Mesh는 여기서 추가 처리 필요
	if (!FParticleEmitterInstance::Resize(NewMaxActiveParticles, bSetMaxActiveCount))
	{
		return false;
	}

	// 2. InstanceBuffer 재생성
	if (Component)
	{
		ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
		if (Device)
		{
			// 기존 InstanceBuffer 해제
			if (InstanceBuffer)
			{
				InstanceBuffer->Release();
				InstanceBuffer = nullptr;
			}

			// 새로운 InstanceBuffer 생성
			const int32 MaxInstanceCount = NewMaxActiveParticles;

			D3D11_BUFFER_DESC BufferDesc = {};
			BufferDesc.ByteWidth = sizeof(FMeshParticleInstanceVertex) * MaxInstanceCount;
			BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
			BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			BufferDesc.MiscFlags = 0;
			BufferDesc.StructureByteStride = 0;

			HRESULT hr = Device->CreateBuffer(&BufferDesc, nullptr, &InstanceBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to resize particle mesh instance buffer");
				return false;
			}
		}
	}

	return true;
}
