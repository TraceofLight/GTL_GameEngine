#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "WorldPartitionManager.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
UPrimitiveComponent::UPrimitiveComponent()
	: bGenerateOverlapEvents(true)
	, bSimulatePhysics(false)
	, bSimulatePhysics_Cached(false)
{
	// 물리 시뮬레이션 변경 감지를 위해 Tick 활성화
	bCanEverTick = true;
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();

	// 캐시 동기화 후 물리 바디 생성
	bSimulatePhysics_Cached = bSimulatePhysics;
	RecreatePhysicsBody();
}

void UPrimitiveComponent::InitPhysX()
{
	RecreatePhysicsBody();
}

void UPrimitiveComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	// bSimulatePhysics 값이 외부에서 직접 변경되었는지 감지 (ImGui 등)
	if (bSimulatePhysics != bSimulatePhysics_Cached)
	{
		bSimulatePhysics_Cached = bSimulatePhysics;
		RecreatePhysicsBody();
	}

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

    // 먼저 캐시에서 동기적으로 확인 (이미 로드된 경우 즉시 설정)
    UMaterial* CachedMaterial = UResourceManager::GetInstance().Get<UMaterial>(InMaterialName);
    if (CachedMaterial)
    {
        SetMaterial(InElementIndex, CachedMaterial);
        return;
    }

    // 캐시에 없으면 비동기 로드
    UResourceManager::GetInstance().AsyncLoad<UMaterial>(
        InMaterialName,
        [this, InElementIndex, InMaterialName](UMaterial* LoadedMaterial)
        {
            if (LoadedMaterial)
            {
                this->SetMaterial(InElementIndex, LoadedMaterial);
            }
            else
            {
                UE_LOG("[warning] SetMaterialByName: Failed to load material '%s' for slot %u",
                    InMaterialName.c_str(), InElementIndex);
            }
        },
        EAssetLoadPriority::Normal
    );
}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    // FBodyInstance의 OwnerComponent를 복제된 컴포넌트(this)로 재설정
    BodyInstance = FBodyInstance(this);
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
	bSimulatePhysics_Cached = bSimulate;  // 캐시도 함께 업데이트
	RecreatePhysicsBody();
}

void UPrimitiveComponent::RecreatePhysicsBody()
{
	// PIE World에서만 물리 body 생성 (Editor World에서는 생성하지 않음)
	UWorld* World = GetWorld();
	if (!World || !World->bPie)
	{
		return;
	}

	// World별 Physics Scene 사용
	PxScene* WorldScene = World->GetPhysicsScene();
	if (!PHYSICS.GetPhysics() || !WorldScene)
	{
		return;
	}

	if (BodyInstance.IsValid())
	{
		BodyInstance.TermBody();
	}

	const bool bIsDynamic = bSimulatePhysics;
	BodyInstance.CreateActor(PHYSICS.GetPhysics(), GetWorldTransform().ToMatrix(), bIsDynamic);

	if (BodyInstance.IsValid())
	{
		OnCreatePhysicsState();
		BodyInstance.AddToScene(WorldScene);
	}
}
