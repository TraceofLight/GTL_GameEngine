#pragma once

#include "PrimitiveComponent.h"

// Forward declarations
class UParticleSystem;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

/**
 * Component that manages and renders a particle system
 * Handles emitter instances and their rendering data
 */
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_REFLECTION_BODY();

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent();

	/** Array of emitter instances, one for each emitter in the template */
	TArray<FParticleEmitterInstance*> EmitterInstances;

	/** Particle system template that defines the emitters and their properties */
	UParticleSystem* Template;

	/** Array of render data for each emitter, used by the rendering system */
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// TODO: Add methods for component initialization
	// TODO: Add methods for tick and update
	// TODO: Add methods for spawning and activation
	// TODO: Add methods for rendering data preparation
	// TODO: Add collision event handling

protected:
	// TODO: Override virtual methods from UPrimitiveComponent as needed
	// TODO: Add internal helper methods
};
