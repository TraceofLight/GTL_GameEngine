#pragma once
#include "PrimitiveComponent.h"
#include "ConstantBufferType.h"
#include "USkySphereComponent.generated.h"

class UShader;
class UStaticMesh;
struct FMeshBatchElement;

/**
 * @brief Sky Sphere 렌더링을 위한 컴포넌트
 *
 * @param ZenithColor 천정(상단) 색상
 * @param HorizonColor 수평선 색상
 * @param GroundColor 지면(하단) 색상
 * @param SunDirection 태양 방향 벡터
 * @param SunDiskSize 태양 원반 크기
 * @param SunColor 태양 색상 및 강도
 * @param HorizonFalloff 수평선 그라디언트 감쇠
 * @param OverallBrightness 전체 밝기 스케일
 * @param bAutoUpdateSunDirection DirectionalLight에서 태양 방향 자동 업데이트 여부
 * @param SkyParams 렌더링용 상수 버퍼
 * @param SphereMesh Sky Sphere 메시 (런타임 생성 또는 로드)
 * @param SkyShader Sky 셰이더
 * @param SphereRadius Sphere 반지름
 */
UCLASS(DisplayName="스카이 스피어", Description="스카이 렌더링 컴포넌트입니다")
class USkySphereComponent : public UPrimitiveComponent
{
    GENERATED_REFLECTION_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Sky|Colors", DisplayName="천정 색상")
    FLinearColor ZenithColor = FLinearColor(0.0343f, 0.1236f, 0.4f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Colors", DisplayName="수평선 색상")
    FLinearColor HorizonColor = FLinearColor(0.6471f, 0.8235f, 0.9451f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Colors", DisplayName="지면 색상")
    FLinearColor GroundColor = FLinearColor(0.3f, 0.25f, 0.2f, 1.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Sun", DisplayName="태양 방향")
    FVector SunDirection = FVector(0.0f, 0.5f, 0.866f);

    UPROPERTY(EditAnywhere, Category="Sky|Sun", DisplayName="태양 크기")
    float SunDiskSize = 0.001f;

    UPROPERTY(EditAnywhere, Category="Sky|Sun", DisplayName="태양 색상")
    FLinearColor SunColor = FLinearColor(1.0f, 0.95f, 0.8f, 5.0f);

    UPROPERTY(EditAnywhere, Category="Sky|Atmosphere", DisplayName="수평선 감쇠")
    float HorizonFalloff = 3.0f;

    UPROPERTY(EditAnywhere, Category="Sky|Atmosphere", DisplayName="전체 밝기")
    float OverallBrightness = 1.0f;

    UPROPERTY(EditAnywhere, Category="Sky|Sun", DisplayName="태양 방향 자동 업데이트")
    bool bAutoUpdateSunDirection = true;

    USkySphereComponent();
    ~USkySphereComponent() override;

    // UActorComponent Interface
    void BeginPlay() override;
    void TickComponent(float DeltaSeconds) override;

    // UPrimitiveComponent Interface
    FAABB GetWorldAABB() const override;
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	// Getter & Setter
    const FSkyConstantBuffer& GetSkyConstantBuffer() const { return SkyParams; }
    FSkyConstantBuffer& GetSkyConstantBufferMutable() { return SkyParams; }
    UStaticMesh* GetSphereMesh() { EnsureResourcesLoaded(); return SphereMesh; }
    UShader* GetSkyShader() { EnsureResourcesLoaded(); return SkyShader; }
    void SetSphereRadius(float InRadius) { SphereRadius = InRadius; }
    float GetSphereRadius() const { return SphereRadius; }

protected:
    FSkyConstantBuffer SkyParams;
    UStaticMesh* SphereMesh = nullptr;
    UShader* SkyShader = nullptr;
    float SphereRadius = 10000.0f;

    void SyncToConstantBuffer();

private:
    void EnsureResourcesLoaded();
    void CreateSphereMesh();
    void LoadShader();
    void UpdateSunDirectionFromLight();
};
