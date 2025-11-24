#include "pch.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleDataContainer.h"
#include "ParticleModuleRequired.h"

/**
 * Serialize FDynamicEmitterReplayDataBase
 * 파티클 리플레이 데이터 직렬화 (기본)
 */
void FDynamicEmitterReplayDataBase::Serialize(FArchive& Ar)
{
	// Emitter type
	Ar << eEmitterType;

	// Active particle count
	Ar << ActiveParticleCount;

	// Particle stride
	Ar << ParticleStride;

	// Particle data container
	// NOTE: FParticleDataContainer should have its own Serialize method
	// For now, we'll serialize the raw data
	if (Ar.IsSaving())
	{
		// Saving: write particle data
		int32 DataSize = ActiveParticleCount * ParticleStride;
		Ar << DataSize;
		if (DataSize > 0 && DataContainer.ParticleData)
		{
			Ar.Serialize(DataContainer.ParticleData, DataSize);
		}

		// Save indices
		int32 IndexCount = DataContainer.ParticleIndicesNumShorts;
		Ar << IndexCount;
		if (IndexCount > 0 && DataContainer.ParticleIndices)
		{
			Ar.Serialize(DataContainer.ParticleIndices, IndexCount * sizeof(uint16));
		}
	}
	else if (Ar.IsLoading())
	{
		// Loading: read particle data
		int32 DataSize = 0;
		Ar << DataSize;
		if (DataSize > 0)
		{
			DataContainer.Allocate(ActiveParticleCount, ParticleStride);
			Ar.Serialize(DataContainer.ParticleData, DataSize);
		}

		// Load indices
		int32 IndexCount = 0;
		Ar << IndexCount;
		if (IndexCount > 0)
		{
			// Allocate already created ParticleIndices, just load data
			if (DataContainer.ParticleIndices)
			{
				Ar.Serialize(DataContainer.ParticleIndices, IndexCount * sizeof(uint16));
			}
		}
	}

	// Scale
	Ar << Scale.X << Scale.Y << Scale.Z;

	// Sort mode
	Ar << SortMode;
}

/**
 * Serialize FDynamicSpriteEmitterReplayDataBase
 * 스프라이트 파티클 리플레이 데이터 직렬화
 */
void FDynamicSpriteEmitterReplayDataBase::Serialize(FArchive& Ar)
{
	// Call parent implementation
	FDynamicEmitterReplayDataBase::Serialize(Ar);

	// Material interface
	// NOTE: We can't serialize UObject* directly in a simple way
	// For now, we'll skip material serialization (render thread doesn't need to save/load)
	// If needed, serialize material path as string

	// Required module
	if (Ar.IsSaving())
	{
		// Save required module data
		bool bHasRequiredModule = (RequiredModule != nullptr);
		Ar << bHasRequiredModule;

		if (bHasRequiredModule)
		{
			Ar << RequiredModule->NumFrames;
			Ar << RequiredModule->AlphaThreshold;
		}
	}
	else if (Ar.IsLoading())
	{
		// Load required module data
		bool bHasRequiredModule = false;
		Ar << bHasRequiredModule;

		if (bHasRequiredModule)
		{
			if (!RequiredModule)
			{
				RequiredModule = new FParticleRequiredModule();
			}

			Ar << RequiredModule->NumFrames;
			Ar << RequiredModule->AlphaThreshold;
		}
	}
}
