#pragma once

#include "Actor.h"
#include "ConstantBufferType.h"
#include "ASkySphereActor.generated.h"

class USkySphereComponent;

/**
 * @brief Sky Sphere 렌더링을 위한 액터
 * @details BP_Sky_Sphere 스타일의 절차적 스카이 렌더링을 제공하는 액터입니다.
 *          USkySphereComponent를 소유하고 에디터에서 Sky 파라미터를 조정할 수 있습니다.
 *
 * @param SkySphereComponent Sky Sphere 컴포넌트 (Root)
 */
UCLASS(DisplayName="스카이 스피어 액터", Description="절차적 스카이 렌더링 액터")
class ASkySphereActor : public AActor
{
    GENERATED_REFLECTION_BODY()

public:
    ASkySphereActor();

protected:
    ~ASkySphereActor() override = default;

public:
    // AActor Interface
    void BeginPlay() override;
    void Tick(float DeltaSeconds) override;

    // Sky Component 접근
    USkySphereComponent* GetSkySphereComponent() const { return SkySphereComponent; }

    // ===== Sky Parameters (에디터 노출) =====

    UPROPERTY(EditAnywhere, Category="Sky|Colors", Tooltip="천정(상단) 색상")
    FLinearColor ZenithColor = FLinearColor(0.0343f, 0.1236f, 0.4f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Colors", Tooltip="수평선 색상")
    FLinearColor HorizonColor = FLinearColor(0.6471f, 0.8235f, 0.9451f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Colors", Tooltip="지면(하단) 색상")
    FLinearColor GroundColor = FLinearColor(0.3f, 0.25f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Sun", Tooltip="태양 색상 (RGB: 색상, A: 강도)")
    FLinearColor SunColor = FLinearColor(1.0f, 0.95f, 0.8f, 5.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Sun", Range="0.0, 1.0", Tooltip="태양 원반 크기 (0.0 ~ 1.0)")
    float SunDiskSize = 0.001f;

    UPROPERTY(EditAnywhere, Category="Sky|Atmosphere", Range="1.0, 10.0", Tooltip="수평선 그라디언트 감쇠")
    float HorizonFalloff = 3.0f;

    UPROPERTY(EditAnywhere, Category="Sky|Atmosphere", Range="0.0, 2.0", Tooltip="전체 밝기 스케일")
    float OverallBrightness = 1.0f;

    UPROPERTY(EditAnywhere, Category="Sky|Sun", Tooltip="DirectionalLight에서 태양 방향 자동 업데이트")
    bool bAutoUpdateSunDirection = true;

    UPROPERTY(EditAnywhere, Category="Sky|Sun", Tooltip="수동 태양 방향 (자동 업데이트 비활성화 시 사용)")
    FVector ManualSunDirection = FVector(0.0f, 0.5f, 0.866f);

    // Sky 파라미터를 컴포넌트에 동기화
    void SyncParametersToComponent();

protected:
    USkySphereComponent* SkySphereComponent = nullptr;

private:
    // 이전 프레임 파라미터 (변경 감지용)
    FLinearColor PrevZenithColor;
    FLinearColor PrevHorizonColor;
    FLinearColor PrevGroundColor;
    FLinearColor PrevSunColor;
    float PrevSunDiskSize = 0.0f;
    float PrevHorizonFalloff = 0.0f;
    float PrevOverallBrightness = 0.0f;

    // 파라미터 변경 여부 확인
    bool HasParametersChanged() const;
};
