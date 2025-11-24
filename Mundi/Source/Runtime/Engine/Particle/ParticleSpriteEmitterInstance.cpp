#include "pch.h"
#include "ParticleSpriteEmitterInstance.h"
#include "DynamicEmitterDataBase.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleSystemComponent.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"

// ============== Lifecycle ==============

FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance()
	: FParticleEmitterInstance()
{
}

FParticleSpriteEmitterInstance::~FParticleSpriteEmitterInstance()
{
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
