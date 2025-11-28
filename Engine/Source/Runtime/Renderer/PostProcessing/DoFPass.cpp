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
    FDepthOfFieldBufferType DoFBuffer;
    DoFBuffer.FocusDistance = M.Payload.Params0.X;  // 초점 거리
    DoFBuffer.FocusRange = M.Payload.Params0.Y;     // 초점 범위
    DoFBuffer.NearBlurScale = M.Payload.Params0.Z;  // 근거리 블러 스케일
    DoFBuffer.FarBlurScale = M.Payload.Params0.W;   // 원거리 블러 스케일
    DoFBuffer.MaxBlurRadius = M.Payload.Params1.X;  // 최대 블러 반경
    DoFBuffer.BokehSize = M.Payload.Params1.Y;      // 보케 크기
    DoFBuffer.Weight = M.Weight;                     // 효과 가중치
    DoFBuffer._Pad0 = 0.0f;

    RHIDevice->SetAndUpdateConstantBuffer(DoFBuffer);

    // 6) Draw
    RHIDevice->DrawFullScreenQuad();

    // 7) 확정
    Swap.Commit();
}
