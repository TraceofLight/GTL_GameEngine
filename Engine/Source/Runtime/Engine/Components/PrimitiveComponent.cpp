#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true), bSimulatePhysics(false)
{
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	RecreatePhysicsBody();
}

void UPrimitiveComponent::InitPhysX()
{
	RecreatePhysicsBody();
}

void UPrimitiveComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	if (bSimulatePhysics)
	{
		BodyInstance.SyncPhysicsToComponent();
	}
}

void UPrimitiveComponent::EndPlay()
{
	BodyInstance.TermBody();
	Super::EndPlay(); 
}
void UPrimitiveComponent::UpdateWorldMatrixFromPhysics(const FMatrix& NewWorldMatrix)
{
	SetWorldTransform(FTransform(NewWorldMatrix));
	
}
void UPrimitiveComponent::OnRegister(UWorld* InWorld)
{
    Super::OnRegister(InWorld);

    // UStaticMeshComponent??野껊갭??World Partition????⑤베堉?. (null 癲ル슪???띿물??Register ????????????얜Ŧ類?
    if (InWorld)
    {
        if (UWorldPartitionManager* Partition = InWorld->GetPartitionManager())
        {
            Partition->Register(this);
        }
    }
}

void UPrimitiveComponent::OnUnregister()
{
    if (UWorld* World = GetWorld())
    {
        if (UWorldPartitionManager* Partition = World->GetPartitionManager())
        {
            Partition->Unregister(this);
        }
    }

    Super::OnUnregister();
}

void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    if (InMaterialName.empty())
    {
        return;
    }

    UMaterial* Material = UResourceManager::GetInstance().Load<UMaterial>(InMaterialName);
    if (!Material)
    {
        UE_LOG("[warning] SetMaterialByName: Failed to load material '%s' for slot %u", InMaterialName.c_str(), InElementIndex);
    }
    SetMaterial(InElementIndex, Material);
} 
 
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    const char* Key = "bSimulatePhysics";
    if (bInIsLoading)
    {
        bool v = bSimulatePhysics;
        FJsonSerializer::ReadBool(InOutHandle, Key, v, v, false);
        SetSimulatePhysics(v);
    }
    else
    {
        InOutHandle[Key] = bSimulatePhysics;
    }
}

void UPrimitiveComponent::OnCreatePhysicsState()
{
	// ???筌????????怨좊군?????筌??癲ル슢?꾤땟怨⑹젂??癲ル슢???섎뼀?Physics ???源놁젳
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.Other)
        {
            if (AActor* Owner = Info.Other->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void UPrimitiveComponent::OnComponentHit(UPrimitiveComponent* Other)
{
	// ???ㅼ굡?곗㏓쎗??우┻?????誘⑦←뵳??釉먮윞??癲??????

}

void UPrimitiveComponent::OnComponentBeginOverlap(UPrimitiveComponent* Other)
{
}

void UPrimitiveComponent::OnComponentEndOverlap(UPrimitiveComponent* Other)
{
}
void UPrimitiveComponent::SetSimulatePhysics(bool bSimulate)
{
	bSimulatePhysics = bSimulate;
	RecreatePhysicsBody();
}

void UPrimitiveComponent::RecreatePhysicsBody()
{
	if (!PHYSICS.GetPhysics() || !PHYSICS.GetScene())
		return;

	if (BodyInstance.IsValid())
	{
		BodyInstance.TermBody();
	}

	const bool bIsDynamic = bSimulatePhysics;
	BodyInstance.CreateActor(PHYSICS.GetPhysics(), GetWorldTransform().ToMatrix(), bIsDynamic);

	if (BodyInstance.IsValid())
	{
		OnCreatePhysicsState();
		BodyInstance.AddToScene(PHYSICS.GetScene());
	}
}
