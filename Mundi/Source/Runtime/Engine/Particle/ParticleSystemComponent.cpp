#include "pch.h"
#include "ParticleSystemComponent.h"
#include "ParticleEmitterInstance.h"
#include "ParticleDataContainer.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"

UParticleSystemComponent::UParticleSystemComponent()
	: Template(nullptr)
	, bIsActive(false)
	, AccumulatedTime(0.0f)
{
	// Enable component tick for particle updates
	bCanEverTick = true;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearEmitterInstances();

	// Clean up render data
	for (FDynamicEmitterDataBase* RenderData : EmitterRenderData)
	{
		if (RenderData)
		{
			delete RenderData;
		}
	}
	EmitterRenderData.clear();
}

// ============== Lifecycle ==============

void UParticleSystemComponent::InitializeComponent()
{
	// Call parent implementation
	UPrimitiveComponent::InitializeComponent();

	// Initialize emitters from template
	if (Template)
	{
		InitializeEmitters();
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	// Call parent implementation
	UPrimitiveComponent::TickComponent(DeltaTime);

	// Skip if not active or no template
	if (!bIsActive || !Template)
	{
		return;
	}

	// Update accumulated time
	AccumulatedTime += DeltaTime;

	// Update all emitters
	UpdateEmitters(DeltaTime);
}

// ============== System Control ==============

void UParticleSystemComponent::ActivateSystem(bool bReset)
{
	if (!Template)
	{
		return;
	}

	// Reset if requested
	if (bReset)
	{
		ResetParticles();
	}

	// Create emitter instances if not exist
	if (EmitterInstances.empty())
	{
		CreateEmitterInstances();
	}

	bIsActive = true;
	AccumulatedTime = 0.0f;
}

void UParticleSystemComponent::DeactivateSystem()
{
	bIsActive = false;
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	// Same template, do nothing
	if (Template == NewTemplate)
	{
		return;
	}

	// Deactivate current system
	DeactivateSystem();

	// Clear existing instances
	ClearEmitterInstances();

	// Set new template
	Template = NewTemplate;

	// Re-initialize if we have a valid template
	if (Template)
	{
		InitializeEmitters();
	}
}

void UParticleSystemComponent::ResetParticles()
{
	// Reset all emitter instances
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->ActiveParticles = 0;
			Instance->ParticleCounter = 0;
			Instance->SpawnFraction = 0.0f;
		}
	}

	AccumulatedTime = 0.0f;
}

// ============== Emitter Management ==============

void UParticleSystemComponent::InitializeEmitters()
{
	if (!Template)
	{
		return;
	}

	// Clear existing instances first
	ClearEmitterInstances();

	// Create new instances
	CreateEmitterInstances();
}

void UParticleSystemComponent::UpdateEmitters(float DeltaTime)
{
	// Update each emitter instance
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			// Tick the emitter (updates particles)
			Instance->Tick(DeltaTime);
		}
	}
}

// ============== Protected Helpers ==============

void UParticleSystemComponent::CreateEmitterInstances()
{
	if (!Template)
	{
		return;
	}

	// Reserve space for emitter instances
	int32 NumEmitters = Template->GetNumEmitters();
	EmitterInstances.reserve(NumEmitters);

	// Create an instance for each emitter in the template
	for (int32 i = 0; i < NumEmitters; i++)
	{
		UParticleEmitter* Emitter = Template->GetEmitter(i);
		if (!Emitter)
		{
			continue;
		}

		// Create new emitter instance
		FParticleEmitterInstance* NewInstance = new FParticleEmitterInstance();

		// Initialize the instance with the emitter template and this component
		NewInstance->Init(Emitter, this);

		// Add to instances array
		EmitterInstances.Add(NewInstance);
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	// Clean up emitter instances
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			delete Instance;
		}
	}
	EmitterInstances.clear();
}
