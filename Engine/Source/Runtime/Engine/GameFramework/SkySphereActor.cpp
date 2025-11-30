#include "pch.h"
#include "SkySphereActor.h"
#include "SkySphereComponent.h"
#include "ObjectFactory.h"

IMPLEMENT_CLASS(ASkySphereActor)

ASkySphereActor::ASkySphereActor()
{
    // SkySphereComponent 생성 및 루트로 설정
    SkySphereComponent = CreateDefaultSubobject<USkySphereComponent>(FName("SkySphereComponent"));
    SetRootComponent(SkySphereComponent);

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
