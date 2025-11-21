#pragma once

#include "pch.h"

// Forward declarations
class UParticleEmitter;
class UParticleSystemComponent;
class UParticleLODLevel;
struct FParticleEventInstancePayload;

/**
 * Runtime instance of a particle emitter
 * Manages particle spawning, updating, and lifetime for a single emitter
 */
struct FParticleEmitterInstance
{
	/** Template emitter that this instance is based on */
	UParticleEmitter* SpriteTemplate;

	/** Owner particle system component */
	UParticleSystemComponent* Component;

	/** Current LOD level index being used */
	int32 CurrentLODLevelIndex;

	/** Current LOD level being used */
	UParticleLODLevel* CurrentLODLevel;

	/** Pointer to the particle data array */
	uint8* ParticleData;

	/** Pointer to the particle index array */
	uint16* ParticleIndices;

	/** Pointer to the instance data array */
	uint8* InstanceData;

	/** The size of the Instance data array in bytes */
	int32 InstancePayloadSize;

	/** The offset to the particle data in bytes */
	int32 PayloadOffset;

	/** The total size of a single particle in bytes */
	int32 ParticleSize;

	/** The stride between particles in the ParticleData array in bytes */
	int32 ParticleStride;

	/** The number of particles currently active in the emitter */
	int32 ActiveParticles;

	/** Monotonically increasing counter for particle IDs */
	uint32 ParticleCounter;

	/** The maximum number of active particles that can be held in the particle data array */
	int32 MaxActiveParticles;

	/** The fraction of time left over from spawning (for sub-frame spawning accuracy) */
	float SpawnFraction;

	FParticleEmitterInstance()
		: SpriteTemplate(nullptr)
		, Component(nullptr)
		, CurrentLODLevelIndex(0)
		, CurrentLODLevel(nullptr)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
		, InstanceData(nullptr)
		, InstancePayloadSize(0)
		, PayloadOffset(0)
		, ParticleSize(0)
		, ParticleStride(0)
		, ActiveParticles(0)
		, ParticleCounter(0)
		, MaxActiveParticles(0)
		, SpawnFraction(0.0f)
	{
	}

	~FParticleEmitterInstance()
	{
		// TODO: Implement cleanup logic
	}

	/**
	 * Spawns particles in the emitter
	 * @param Count - Number of particles to spawn
	 * @param StartTime - Starting time for the first particle
	 * @param Increment - Time increment between each particle spawn
	 * @param InitialLocation - Initial location for spawned particles
	 * @param InitialVelocity - Initial velocity for spawned particles
	 * @param EventPayload - Event payload data (optional)
	 */
	void SpawnParticles(
		int32 Count,
		float StartTime,
		float Increment,
		const FVector& InitialLocation,
		const FVector& InitialVelocity,
		FParticleEventInstancePayload* EventPayload = nullptr
	);

	/**
	 * Kills a particle at the specified index
	 * @param Index - Index of the particle to kill
	 */
	void KillParticle(int32 Index);

	// TODO: Add methods for updating particles
	// TODO: Add methods for PreSpawn and PostSpawn
	// TODO: Add methods for tick and rendering preparation
};
