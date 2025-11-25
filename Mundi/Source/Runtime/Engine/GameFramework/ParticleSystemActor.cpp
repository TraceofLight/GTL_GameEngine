#include "pch.h"
#include "ParticleSystemActor.h"
#include "Source/Runtime/Engine/Particle/ParticleSystemComponent.h"

AParticleSystemActor::AParticleSystemActor()
{
	ObjectName = "Particle System Actor";
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>("ParticleSystemComponent");

	RootComponent = ParticleSystemComponent;
}

AParticleSystemActor::~AParticleSystemActor()
{
}

void AParticleSystemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AParticleSystemActor::SetParticleSystem(UParticleSystem* InTemplate)
{
	if (ParticleSystemComponent)
	{
		ParticleSystemComponent->SetTemplate(InTemplate);
		ParticleSystemComponent->ActivateSystem(true);
	}
}

void AParticleSystemActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Component))
		{
			ParticleSystemComponent = PSC;
			break;
		}
	}
}

void AParticleSystemActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		ParticleSystemComponent = Cast<UParticleSystemComponent>(RootComponent);
	}
}
