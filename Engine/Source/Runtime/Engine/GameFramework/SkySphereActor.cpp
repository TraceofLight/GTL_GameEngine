#include "pch.h"
#include "SkySphereActor.h"
#include "SkySphereComponent.h"
#include "BillboardComponent.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(ASkySphereActor)

ASkySphereActor::ASkySphereActor()
{
    // SkySphereComponent 생성 및 루트로 설정
    SkySphereComponent = CreateDefaultSubobject<USkySphereComponent>(FName("SkySphereComponent"));
    SetRootComponent(SkySphereComponent);

    // BillboardComponent 생성 (에디터 아이콘)
    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
    BillboardComponent->SetEditability(false);  // PIE에서 숨김
    BillboardComponent->SetTexture("Data/Default/Icon/SkyAtmosphere_64x.png");
    BillboardComponent->SetupAttachment(RootComponent);

    // 에디터에서도 Tick 허용 (실시간 파라미터 업데이트)
    bTickInEditor = true;
    bCanEverTick = true;

    // 초기 파라미터 저장
    PrevZenithColor = ZenithColor;
    PrevHorizonColor = HorizonColor;
    PrevGroundColor = GroundColor;
    PrevSunColor = SunColor;
    PrevSunDiskSize = SunDiskSize;
    PrevHorizonFalloff = HorizonFalloff;
    PrevOverallBrightness = OverallBrightness;
}

void ASkySphereActor::BeginPlay()
{
    Super::BeginPlay();

    // 컴포넌트에 파라미터 동기화
    SyncParametersToComponent();
}

void ASkySphereActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 파라미터가 변경되었으면 컴포넌트에 동기화
    if (HasParametersChanged())
    {
        SyncParametersToComponent();

        // 이전 값 업데이트
        PrevZenithColor = ZenithColor;
        PrevHorizonColor = HorizonColor;
        PrevGroundColor = GroundColor;
        PrevSunColor = SunColor;
        PrevSunDiskSize = SunDiskSize;
        PrevHorizonFalloff = HorizonFalloff;
        PrevOverallBrightness = OverallBrightness;
    }
}

void ASkySphereActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // 로드 시 컴포넌트 포인터 복원
        SkySphereComponent = Cast<USkySphereComponent>(RootComponent);

        for (UActorComponent* Component : OwnedComponents)
        {
            if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
            {
                BillboardComponent = Billboard;
                break;
            }
        }
    }
}

void ASkySphereActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    for (UActorComponent* Component : OwnedComponents)
    {
        if (USkySphereComponent* Sky = Cast<USkySphereComponent>(Component))
        {
            SkySphereComponent = Sky;
        }
        else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
        {
            BillboardComponent = Billboard;
        }
    }
}

void ASkySphereActor::SyncParametersToComponent()
{
    if (!SkySphereComponent)
    {
        return;
    }

    // UPROPERTY 멤버에 직접 접근
    SkySphereComponent->ZenithColor = ZenithColor;
    SkySphereComponent->HorizonColor = HorizonColor;
    SkySphereComponent->GroundColor = GroundColor;
    SkySphereComponent->SunColor = SunColor;
    SkySphereComponent->SunDiskSize = SunDiskSize;
    SkySphereComponent->HorizonFalloff = HorizonFalloff;
    SkySphereComponent->OverallBrightness = OverallBrightness;
    SkySphereComponent->bAutoUpdateSunDirection = bAutoUpdateSunDirection;

    // 수동 태양 방향 설정 (자동 업데이트가 비활성화된 경우)
    if (!bAutoUpdateSunDirection)
    {
        SkySphereComponent->SunDirection = ManualSunDirection.GetNormalized();
    }
}

bool ASkySphereActor::HasParametersChanged() const
{
    if (ZenithColor != PrevZenithColor) return true;
    if (HorizonColor != PrevHorizonColor) return true;
    if (GroundColor != PrevGroundColor) return true;
    if (SunColor != PrevSunColor) return true;
    if (SunDiskSize != PrevSunDiskSize) return true;
    if (HorizonFalloff != PrevHorizonFalloff) return true;
    if (OverallBrightness != PrevOverallBrightness) return true;
    return false;
}
