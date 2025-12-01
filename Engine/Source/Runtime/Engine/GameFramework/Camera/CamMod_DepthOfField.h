#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_DepthOfField : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_DepthOfField, UCameraModifierBase)

    UCamMod_DepthOfField() = default;
    virtual ~UCamMod_DepthOfField() = default;

    // DoF 모드: 0 = Cinematic, 1 = Physical, 2 = TiltShift, 3 = PointFocus, 4 = ScreenPointFocus
    int32 DoFMode = 0;

    // 공통 파라미터
    float FocusDistance = 500.0f;   // 초점 거리 (View Space 단위)
    float MaxBlurRadius = 8.0f;     // 최대 블러 반경 (픽셀 단위)
    float BokehSize = 1.0f;         // 보케 크기

    // Cinematic 모드 파라미터 (아티스트 친화적 선형 모델)
    float FocusRange = 100.0f;      // 초점 영역 범위 (이 범위 내에서는 선명)
    float NearBlurScale = 0.02f;    // 근거리 블러 강도
    float FarBlurScale = 0.01f;     // 원거리 블러 강도

    // Physical 모드 파라미터 (렌즈 물리학 기반, Hyperfocal 적용)
    float FocalLength = 50.0f;      // 렌즈 초점거리 (mm): 35, 50, 85 등
    float FNumber = 2.8f;           // 조리개 값 (F-Number): 1.4, 2.8, 5.6 등
    float SensorWidth = 36.0f;      // 센서 너비 (mm): Full Frame=36, APS-C=23.6

    // Tilt-Shift 모드 파라미터 (화면 Y 좌표 기반, 미니어처 효과)
    float TiltShiftCenterY = 0.5f;      // 선명한 띠의 중심 (0~1)
    float TiltShiftBandWidth = 0.3f;    // 선명한 띠의 너비 (0~1)
    float TiltShiftBlurScale = 5.0f;    // 블러 강도

    // PointFocus 모드 파라미터 (점 초점, 구형 초점 영역, World Space)
    FVector FocusPoint = FVector(0.0f, 0.0f, 0.0f);  // 초점 지점 (World Space)
    float FocusRadius = 2.0f;           // 초점 반경 (이 반경 내에서는 선명)
    float PointFocusBlurScale = 0.5f;   // 블러 강도 스케일
    float PointFocusFalloff = 1.0f;     // 블러 감쇠 (1=선형, 2=제곱)

    // ScreenPointFocus 모드 파라미터 (화면 좌표 기반 점 초점, Screen Space)
    FVector2D ScreenFocusPoint = FVector2D(0.5f, 0.5f);  // 화면상의 초점 위치 (0~1 UV)
    float ScreenFocusRadius = 0.1f;         // 화면상의 초점 반경 (0~1)
    float ScreenFocusDepthRange = 50.0f;    // 초점 깊이 허용 범위
    float ScreenFocusBlurScale = 2.0f;      // 블러 강도 스케일
    float ScreenFocusFalloff = 1.0f;        // 블러 감쇠 (1=선형, 2=제곱)
    float ScreenFocusAspectRatio = 1.777f;  // 화면 비율 (16:9 = 1.777)

    // 블러 방식: 0=Disc12, 1=Disc24, 2=Gaussian, 3=Hexagonal, 4=CircularGather
    int32 BlurMethod = 0;

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

        // Params0: FocusDistance, FocusRange, NearBlurScale, FarBlurScale (Cinematic용)
        M.Payload.Params0 = FVector4(FocusDistance, FocusRange, NearBlurScale, FarBlurScale);
        // Params1: MaxBlurRadius, BokehSize, DoFMode, PointFocusFalloff
        M.Payload.Params1 = FVector4(MaxBlurRadius, BokehSize, static_cast<float>(DoFMode), PointFocusFalloff);
        // Params2: FocalLength, FNumber, SensorWidth, BlurMethod (Physical용 + 블러방식)
        M.Payload.Params2 = FVector4(FocalLength, FNumber, SensorWidth, static_cast<float>(BlurMethod));
        // Params3: FocusPoint.x, FocusPoint.y, FocusPoint.z, FocusRadius (PointFocus용)
        M.Payload.Params3 = FVector4(FocusPoint.X, FocusPoint.Y, FocusPoint.Z, FocusRadius);
        // Params4: ScreenFocusPoint.x, ScreenFocusPoint.y, ScreenFocusRadius, ScreenFocusDepthRange (ScreenPointFocus용)
        M.Payload.Params4 = FVector4(ScreenFocusPoint.X, ScreenFocusPoint.Y, ScreenFocusRadius, ScreenFocusDepthRange);
        // Params5: ScreenFocusBlurScale, ScreenFocusFalloff, ScreenFocusAspectRatio (ScreenPointFocus용)
        M.Payload.Params5 = FVector4(ScreenFocusBlurScale, ScreenFocusFalloff, ScreenFocusAspectRatio, 0.0f);
        // Color: TiltShiftCenterY, TiltShiftBandWidth, TiltShiftBlurScale, PointFocusBlurScale
        M.Payload.Color = FLinearColor(TiltShiftCenterY, TiltShiftBandWidth, TiltShiftBlurScale, PointFocusBlurScale);

        Out.Add(M);
    }
};
