#include "pch.h"
#include "DoFPass.h"
#include "../SceneView.h"
#include "../../RHI/SwapGuard.h"
#include "../../RHI/ConstantBufferType.h"

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
    if (!IsApplicable(M)) return;

    // 1) 스왑 + SRV 언바인드 관리 (Depth + SceneColorSource 2개 사용)
    FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);

    // 2) 타깃 RTV 설정
    RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 3) 셰이더
    UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
    UShader* DoFPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_PS.hlsl");
    if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !DoFPS || !DoFPS->GetPixelShader())
    {
        UE_LOG("DoF용 셰이더 없음!\n");
        return;
    }

    RHIDevice->PrepareShader(FullScreenTriangleVS, DoFPS);

    // 4) SRV / Sampler (깊이 + 현재 SceneColorSource)
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
    ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
    ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);

    if (!DepthSRV || !SceneSRV || !PointClampSamplerState || !LinearClampSamplerState)
    {
        UE_LOG("DoF: Depth SRV / Scene SRV / Sampler is null!\n");
        return;
    }

    ID3D11ShaderResourceView* Srvs[2] = { DepthSRV, SceneSRV };
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

    ID3D11SamplerState* Smps[2] = { LinearClampSamplerState, PointClampSamplerState };
    RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);

    // 5) 상수 버퍼 업데이트
    ECameraProjectionMode ProjectionMode = View->ProjectionMode;
    RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

    // DoF 파라미터 설정 (Payload에서 읽음)
    // Params0: FocusDistance, FocusRange, NearBlurScale, FarBlurScale (Cinematic용)
    // Params1: MaxBlurRadius, BokehSize, DoFMode, PointFocusFalloff
    // Params2: FocalLength, FNumber, SensorWidth, (unused) (Physical용)
    // Params3: FocusPoint.x, FocusPoint.y, FocusPoint.z, FocusRadius (PointFocus용)
    // Color: TiltShiftCenterY, TiltShiftBandWidth, TiltShiftBlurScale, PointFocusBlurScale
    FDepthOfFieldBufferType DoFBuffer;

    // 공통 파라미터
    DoFBuffer.FocusDistance = M.Payload.Params0.X;
    DoFBuffer.MaxBlurRadius = M.Payload.Params1.X;
    DoFBuffer.BokehSize = M.Payload.Params1.Y;
    DoFBuffer.DoFMode = static_cast<int32>(M.Payload.Params1.Z);  // 0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus
    DoFBuffer.Weight = M.Weight;

    // Cinematic 모드 파라미터
    DoFBuffer.FocusRange = M.Payload.Params0.Y;
    DoFBuffer.NearBlurScale = M.Payload.Params0.Z;
    DoFBuffer.FarBlurScale = M.Payload.Params0.W;
    DoFBuffer._Pad0 = 0.0f;

    // Physical 모드 파라미터
    DoFBuffer.FocalLength = M.Payload.Params2.X;
    DoFBuffer.FNumber = M.Payload.Params2.Y;
    DoFBuffer.SensorWidth = M.Payload.Params2.Z;
    DoFBuffer._Pad1 = 0.0f;

    // Tilt-Shift 모드 파라미터
    DoFBuffer.TiltShiftCenterY = M.Payload.Color.R;
    DoFBuffer.TiltShiftBandWidth = M.Payload.Color.G;
    DoFBuffer.TiltShiftBlurScale = M.Payload.Color.B;

    // PointFocus 모드 파라미터
    DoFBuffer.FocusPoint = FVector(M.Payload.Params3.X, M.Payload.Params3.Y, M.Payload.Params3.Z);
    DoFBuffer.FocusRadius = M.Payload.Params3.W;
    DoFBuffer.PointFocusBlurScale = M.Payload.Color.A;
    DoFBuffer.PointFocusFalloff = M.Payload.Params1.W;

    // 블러 방식
    DoFBuffer.BlurMethod = static_cast<int32>(M.Payload.Params2.W);

    // 번짐 처리 방식 (Params4에서 읽음)
    DoFBuffer.BleedingMethod = static_cast<int32>(M.Payload.Params4.X);

    RHIDevice->SetAndUpdateConstantBuffer(DoFBuffer);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
