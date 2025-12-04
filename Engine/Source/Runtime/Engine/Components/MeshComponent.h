#pragma once
#include "PrimitiveComponent.h"
#include "UMeshComponent.generated.h"

class UShader;

UCLASS(DisplayName="메시 컴포넌트", Description="지오메트리 데이터를 렌더링하는 컴포넌트입니다")
class UMeshComponent : public UPrimitiveComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Materials", Tooltip="Material slots for the mesh")
    TArray<UMaterialInterface*> MaterialSlots;

    // 사용자가 SetMaterial로 오버라이드한 슬롯 추적 (비동기 로드 후에도 유지)
    TArray<bool> MaterialSlotOverrides;

    UPROPERTY(EditAnywhere, Category="Rendering", Tooltip="그림자를 드리울지 여부입니다")
    bool bCastShadows = true;
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    void DuplicateSubObjects() override;

protected:
    void MarkWorldPartitionDirty();

// Material Section
public:
    UMaterialInterface* GetMaterial(uint32 InSectionIndex) const override;
    void SetMaterial(uint32 InElementIndex, UMaterialInterface* InNewMaterial) override;

    // bIsInitialSetup: true이면 초기 머티리얼 설정으로 간주하여 MaterialSlotOverrides 플래그를 설정하지 않음
    // 비동기 로드 콜백에서 호출 시 true로 전달하여 사용자 오버라이드와 구분
    void SetMaterialInternal(uint32 InElementIndex, UMaterialInterface* InNewMaterial, bool bIsInitialSetup);

    UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(uint32 ElementIndex);

    const TArray<UMaterialInterface*>& GetMaterialSlots() const { return MaterialSlots; }

    void SetMaterialTextureByUser(const uint32 InMaterialSlotIndex, EMaterialTextureSlot Slot, UTexture* Texture);
   	UFUNCTION(LuaBind, DisplayName="SetColor", Tooltip="Set material color parameter")
    void SetMaterialColorByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, const FLinearColor& Value);
    UFUNCTION(LuaBind, DisplayName="SetScalar", Tooltip="Set material scalar parameter")
    void SetMaterialScalarByUser(const uint32 InMaterialSlotIndex, const FString& ParameterName, float Value);

protected:
    void ClearDynamicMaterials();

    TArray<UMaterialInstanceDynamic*> DynamicMaterialInstances;

// Shadow Section
public:
    bool IsCastShadows() const { return bCastShadows; }

private:
};
