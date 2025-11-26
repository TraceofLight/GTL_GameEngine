#include "pch.h"
#include "ParticleSpriteEmitterInstance.h"
#include "DynamicEmitterDataBase.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleSystemComponent.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "D3D11RHI.h"
#include "VertexData.h"

// ============== Lifecycle ==============

FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance()
	: FParticleEmitterInstance()
{
}

FParticleSpriteEmitterInstance::~FParticleSpriteEmitterInstance()
{
}

void FParticleSpriteEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance::Init(InTemplate, InComponent);

	// GPU 버퍼 생성 (파티클 스프라이트용)
	// 각 파티클은 4개의 정점으로 구성된 쿼드(Quad)로 렌더링됨
	if (Component && MaxActiveParticles > 0)
	{
		ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
		if (Device)
		{
			// 1. Vertex Buffer 생성 (동적 버퍼)
			// 각 파티클당 4개의 정점 필요 (쿼드의 4개 코너)
			const int32 MaxVertexCount = MaxActiveParticles * 4;
			
			// 초기 정점 데이터 생성 (빈 버퍼로 시작)
			std::vector<FParticleSpriteVertex> InitialVertices;
			InitialVertices.resize(MaxVertexCount);
			
			// 동적 버퍼로 생성 (매 프레임 업데이트 가능)
			HRESULT hr = D3D11RHI::CreateVertexBuffer<FParticleSpriteVertex>(Device, InitialVertices, &VertexBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to create particle sprite vertex buffer");
			}

			// 2. Index Buffer 생성 (정적 버퍼)
			// 각 파티클당 2개의 삼각형 = 6개의 인덱스 필요
			const int32 MaxIndexCount = MaxActiveParticles * 6;
			
			TArray<uint32> Indices;
			Indices.Reserve(MaxIndexCount);
			
			// 모든 쿼드에 대한 인덱스 사전 생성
			for (int32 QuadIndex = 0; QuadIndex < MaxActiveParticles; ++QuadIndex)
			{
				const uint32 BaseVertexIndex = QuadIndex * 4;
				
				// 첫 번째 삼각형 (좌상단 -> 우상단 -> 좌하단)
				Indices.Add(BaseVertexIndex + 0);
				Indices.Add(BaseVertexIndex + 1);
				Indices.Add(BaseVertexIndex + 2);
				
				// 두 번째 삼각형 (좌하단 -> 우상단 -> 우하단)
				Indices.Add(BaseVertexIndex + 2);
				Indices.Add(BaseVertexIndex + 1);
				Indices.Add(BaseVertexIndex + 3);
			}
			
			// 인덱스 버퍼 생성
			D3D11_BUFFER_DESC IndexBufferDesc = {};
			IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			IndexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.Num());
			IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			IndexBufferDesc.CPUAccessFlags = 0;
			
			D3D11_SUBRESOURCE_DATA IndexInitData = {};
			IndexInitData.pSysMem = Indices.GetData();
			
			hr = Device->CreateBuffer(&IndexBufferDesc, &IndexInitData, &IndexBuffer);
			if (FAILED(hr))
			{
				UE_LOG("Failed to create particle sprite index buffer");
			}
		}
	}
}

// ============== Dynamic Data ==============

/**
 * Retrieves the dynamic data for the emitter
 *
 * @param bSelected - Whether the emitter is selected in the editor
 * @return FDynamicEmitterDataBase* - The dynamic data, or nullptr if not required
 */
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData(bool bSelected)
{
	// Check if we have data to render
	if (!IsDynamicDataRequired())
	{
		return nullptr;
	}

	// Allocate the dynamic data
	FDynamicSpriteEmitterData* NewEmitterData = new FDynamicSpriteEmitterData();

	// Fill in the source data (calls FillReplayData)
	if (!FillReplayData(NewEmitterData->Source))
	{
		delete NewEmitterData;
		return nullptr;
	}

	// Setup dynamic render data (Init must be called AFTER filling source data)
	NewEmitterData->Init(bSelected);

	NewEmitterData->OwnerInstance = this;

	return NewEmitterData;
}

/**
 * Fill replay data with sprite-specific particle information
 *
 * @param OutData - Output replay data to fill
 * @return true if successful, false if no data to fill
 */
bool FParticleSpriteEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// CRITICAL: Call parent implementation first to fill common particle data
	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return false;
	}

	// Cast to sprite replay data type
	FDynamicSpriteEmitterReplayDataBase& SpriteData = static_cast<FDynamicSpriteEmitterReplayDataBase&>(OutData);

	// Set emitter type
	SpriteData.eEmitterType = EDynamicEmitterType::Sprite;

	// Fill sprite-specific data from RequiredModule
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		UParticleModuleRequired* RequiredModule = CurrentLODLevel->RequiredModule;

		// Material (직접 할당)
		SpriteData.MaterialInterface = RequiredModule->GetMaterial();

		// RequiredModule (깊은 복사 - CreateRendererResource 사용)
		SpriteData.RequiredModule = RequiredModule->CreateRendererResource();

		// BlendMode (렌더러에서 블렌드 스테이트 분기용)
		SpriteData.BlendMode = RequiredModule->GetBlendMode();
	}

	return true;
}

/**
 * Retrieves replay data for the emitter
 *
 * @return FDynamicEmitterReplayDataBase* - The replay data, or nullptr if not available
 */
FDynamicEmitterReplayDataBase* FParticleSpriteEmitterInstance::GetReplayData()
{
	if (ActiveParticles <= 0)
	{
		return nullptr;
	}

	// Allocate sprite replay data
	FDynamicSpriteEmitterReplayDataBase* NewEmitterReplayData = new FDynamicSpriteEmitterReplayDataBase();

	// Fill the replay data
	if (!FillReplayData(*NewEmitterReplayData))
	{
		delete NewEmitterReplayData;
		return nullptr;
	}

	return NewEmitterReplayData;
}

/**
 * Retrieve the allocated size of this instance
 *
 * @param OutNum - The size of this instance (currently used)
 * @param OutMax - The maximum size of this instance (allocated)
 */
void FParticleSpriteEmitterInstance::GetAllocatedSize(int32& OutNum, int32& OutMax)
{
	int32 Size = sizeof(FParticleSpriteEmitterInstance);
	int32 ActiveParticleDataSize = (ParticleData != nullptr) ? (ActiveParticles * ParticleStride) : 0;
	int32 MaxActiveParticleDataSize = (ParticleData != nullptr) ? (MaxActiveParticles * ParticleStride) : 0;
	int32 ActiveParticleIndexSize = (ParticleIndices != nullptr) ? (ActiveParticles * sizeof(uint16)) : 0;
	int32 MaxActiveParticleIndexSize = (ParticleIndices != nullptr) ? (MaxActiveParticles * sizeof(uint16)) : 0;

	OutNum = ActiveParticleDataSize + ActiveParticleIndexSize + Size;
	OutMax = MaxActiveParticleDataSize + MaxActiveParticleIndexSize + Size;
}

/**
 * Resize particle memory and GPU buffers (VertexBuffer, IndexBuffer)
 * 파티클 메모리 및 GPU 버퍼 리사이징
 *
 * @param NewMaxActiveParticles - New maximum particle count
 * @param bSetMaxActiveCount - If true, update peak active particles
 * @return true if successful
 */
bool FParticleSpriteEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	// 1. 부모 Resize 호출 (파티클 데이터/인덱스 재할당, 그리고 Sprite 버퍼 처리)
	// 부모에서 이미 Sprite 버퍼를 처리하지만, 명시적으로 다시 처리하지 않도록 주의
	// 부모의 Resize는 이미 타입을 판별해서 Sprite 버퍼를 재생성함
	return FParticleEmitterInstance::Resize(NewMaxActiveParticles, bSetMaxActiveCount);
}
