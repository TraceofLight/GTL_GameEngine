// Depth of Field Post Process Shader
// Implements bokeh-style blur based on pixel depth relative to focus distance

Texture2D g_DepthTex : register(t0);
Texture2D g_SceneColorTex : register(t1);

SamplerState g_LinearClampSample : register(s0);
SamplerState g_PointClampSample : register(s1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

cbuffer PostProcessCB : register(b0)
{
    float Near;
    float Far;
    int IsOrthographic;
    float Padding;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
}

cbuffer DepthOfFieldCB : register(b2)
{
    float FocusDistance;    // 초점 거리 (View Space)
    float FocusRange;       // 초점 영역 범위
    float NearBlurScale;    // 근거리 블러 강도
    float FarBlurScale;     // 원거리 블러 강도

    float MaxBlurRadius;    // 최대 블러 반경 (픽셀)
    float BokehSize;        // 보케 크기
    float Weight;           // 효과 가중치
    float _Pad0;
}

cbuffer ViewportConstants : register(b10)
{
    float4 ViewportRect;    // x: TopLeftX, y: TopLeftY, z: Width, w: Height
    float4 ScreenSize;      // x: Width, y: Height, z: 1/Width, w: 1/Height
}

// 깊이 버퍼에서 View Space Z 계산
float GetLinearDepth(float rawDepth)
{
    if (IsOrthographic)
    {
        return Near + rawDepth * (Far - Near);
    }
    else
    {
        // Perspective projection reverse-Z 처리
        return Near * Far / (Far - rawDepth * (Far - Near));
    }
}

// Circle of Confusion (CoC) 계산
// 반환값: 양수 = 원거리 블러, 음수 = 근거리 블러
float CalculateCoC(float viewDepth)
{
    float diff = viewDepth - FocusDistance;

    // 초점 영역 내에서는 블러 없음
    if (abs(diff) < FocusRange)
    {
        return 0.0;
    }

    float coc;
    if (diff > 0.0)
    {
        // 원거리 (배경) 블러
        coc = (diff - FocusRange) * FarBlurScale;
    }
    else
    {
        // 근거리 (전경) 블러
        coc = (diff + FocusRange) * NearBlurScale;
    }

    // 최대 블러 반경으로 클램프
    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// 디스크 형태의 보케 샘플 패턴 (12 샘플)
static const float2 BokehSamples[12] =
{
    float2(1.0, 0.0),
    float2(0.866, 0.5),
    float2(0.5, 0.866),
    float2(0.0, 1.0),
    float2(-0.5, 0.866),
    float2(-0.866, 0.5),
    float2(-1.0, 0.0),
    float2(-0.866, -0.5),
    float2(-0.5, -0.866),
    float2(0.0, -1.0),
    float2(0.5, -0.866),
    float2(0.866, -0.5)
};

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 원본 색상 및 깊이 샘플링
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);
    float centerDepth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    // View Space 깊이 계산
    float centerViewDepth = GetLinearDepth(centerDepth);

    // CoC 계산
    float centerCoC = CalculateCoC(centerViewDepth);
    float absCoC = abs(centerCoC);

    // CoC가 0에 가까우면 원본 반환
    if (absCoC < 0.5)
    {
        return centerColor;
    }

    // 화면 비율 보정을 위한 픽셀 크기
    float2 pixelSize = ScreenSize.zw;

    // 블러 반경 (픽셀 단위)
    float blurRadius = absCoC * BokehSize;

    // 블러 샘플링
    float4 accumulatedColor = centerColor;
    float totalWeight = 1.0;

    for (int i = 0; i < 12; i++)
    {
        float2 offset = BokehSamples[i] * blurRadius * pixelSize;
        float2 sampleUV = input.texCoord + offset;

        // UV 범위 체크
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        // 샘플 색상 및 깊이
        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth);

        // 근거리 객체가 원거리 객체 위에 블리딩되는 것 방지
        float sampleWeight = 1.0;

        // 샘플이 중심보다 더 가까우면 (전경) 가중치 증가
        if (centerCoC > 0.0 && sampleCoC < centerCoC)
        {
            sampleWeight = saturate(1.0 - (centerCoC - sampleCoC) / MaxBlurRadius);
        }

        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    float4 blurredColor = accumulatedColor / totalWeight;

    // Weight를 사용하여 원본과 블러된 색상 블렌딩
    float4 finalColor = lerp(centerColor, blurredColor, Weight);

    return finalColor;
}
