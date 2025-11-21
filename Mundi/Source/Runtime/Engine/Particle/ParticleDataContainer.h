#pragma once
#include "pch.h"
/**
 * Container for particle data arrays
 * Manages memory allocation for particle data and indices
 */
struct FParticleDataContainer
{
	/** Total size of allocated memory block in bytes */
	int32 MemBlockSize;

	/** Size of particle data array in bytes */
	int32 ParticleDataNumBytes;

	/** Size of particle indices array in shorts (uint16) */
	int32 ParticleIndicesNumShorts;

	/** Pointer to particle data array (also the base of the allocated memory block) */
	uint8* ParticleData;

	/** Pointer to particle indices array (located at the end of the memory block, not separately allocated) */
	uint16* ParticleIndices;

	FParticleDataContainer()
		: MemBlockSize(0)
		, ParticleDataNumBytes(0)
		, ParticleIndicesNumShorts(0)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
	{
	}

	~FParticleDataContainer()
	{
		// TODO: Implement cleanup logic
	}

	// TODO: Add methods for memory allocation and deallocation
	// TODO: Add methods for resizing and managing particle data
};
