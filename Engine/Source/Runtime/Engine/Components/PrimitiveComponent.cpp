#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent() : bGenerateOverlapEvents(true)
{
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();

	if (gPhysics && gScene)
	{
		// 예시: Movable이면 Dynamic, 아니면 Static
		//bool bIsDynamic = (Mobility == EComponentMobility::Movable);
		BodyInstance.InitBody(gPhysics, gScene, GetWorldTransform().ToMatrix(), bSimulatePhysics);
	}
}
void UPrimitiveComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);
	BodyInstance.SyncPhysicsToComponent();
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

    // UStaticMeshComponent라면 World Partition에 추가. (null 체크는 Register 내부에서 수행)
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
