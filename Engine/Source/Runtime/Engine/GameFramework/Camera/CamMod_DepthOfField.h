#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_DepthOfField : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_DepthOfField, UCameraModifierBase)

    UCamMod_DepthOfField() = default;
    virtual ~UCamMod_DepthOfField() = default;

    // DoF Parameters
    float FocusDistance = 500.0f;   // 초점 거리 (View Space 단위)
    float FocusRange = 100.0f;      // 초점 영역 범위 (이 범위 내에서는 선명)
    float NearBlurScale = 0.02f;    // 근거리 블러 강도
    float FarBlurScale = 0.01f;     // 원거리 블러 강도
    float MaxBlurRadius = 8.0f;     // 최대 블러 반경 (픽셀 단위)
    float BokehSize = 1.0f;         // 보케 크기

    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override {}

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override
    {
        if (!bEnabled) return;

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::DepthOfField;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        // Params0: FocusDistance, FocusRange, NearBlurScale, FarBlurScale
        M.Payload.Params0 = FVector4(FocusDistance, FocusRange, NearBlurScale, FarBlurScale);
        // Params1: MaxBlurRadius, BokehSize, (unused), (unused)
        M.Payload.Params1 = FVector4(MaxBlurRadius, BokehSize, 0.0f, 0.0f);

        Out.Add(M);
    }
};
