#include "pch.h"
#include "ParticleSystemComponent.h"

UParticleSystemComponent::UParticleSystemComponent()
	: Template(nullptr)
{
	// Enable component tick for particle updates
	bCanEverTick = true;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	// Clean up emitter instances
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			delete Instance;
		}
	}
	EmitterInstances.Empty();

	// Clean up render data
	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		if (RenderData)
		{
			delete RenderData;
		}
	}
	EmitterRenderData.Empty();
}
