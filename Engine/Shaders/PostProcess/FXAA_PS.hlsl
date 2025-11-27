//--------------------------------------------------------------------------------------
// FXAA (Fast Approximate Anti-Aliasing) Pixel Shader
// Based on NVIDIA FXAA 3.11 algorithm
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer FXAAParams : register(b2)
{
    float2 InvResolution;   // 1.0 / resolution (e.g., 1/1920, 1/1080)
    float FXAASpanMax;      // Max search span in pixels (recommended: 8.0)
    float FXAAReduceMul;    // Reduce multiplier (recommended: 1/8)
    float FXAAReduceMin;    // Minimum reduce value (recommended: 1/128)
    float3 Padding;
};

//--------------------------------------------------------------------------------------
// Texture and Sampler
//--------------------------------------------------------------------------------------
Texture2D g_SceneTexture : register(t0);
SamplerState g_SamplerLinear : register(s0);

//--------------------------------------------------------------------------------------
// Luminance calculation (Gamma-space optimized for FXAA)
//--------------------------------------------------------------------------------------
float Luma(float3 Color)
{
    // BT.601 coefficients - better suited for gamma-space FXAA
    // Green is most perceptually significant, followed by red, then blue
    return dot(Color, float3(0.299f, 0.587f, 0.114f));
}

//--------------------------------------------------------------------------------------
// FXAA Pixel Shader (NVIDIA FXAA 3.11 style)
//--------------------------------------------------------------------------------------
float4 mainPS(float4 Pos : SV_Position, float2 TexCoord : TEXCOORD0) : SV_Target
{
    float2 Tex = TexCoord;
    float2 Inv = InvResolution;

    // 1. Sample 5 pixels: center + 4 diagonal corners
    // Diagonal sampling is key for detecting angled edges
    float3 RgbNW = g_SceneTexture.Sample(g_SamplerLinear, Tex + float2(-Inv.x, -Inv.y)).rgb;
    float3 RgbNE = g_SceneTexture.Sample(g_SamplerLinear, Tex + float2( Inv.x, -Inv.y)).rgb;
    float3 RgbSW = g_SceneTexture.Sample(g_SamplerLinear, Tex + float2(-Inv.x,  Inv.y)).rgb;
    float3 RgbSE = g_SceneTexture.Sample(g_SamplerLinear, Tex + float2( Inv.x,  Inv.y)).rgb;
    float3 RgbM  = g_SceneTexture.Sample(g_SamplerLinear, Tex).rgb;

    // 2. Convert to luminance
    float LumaNW = Luma(RgbNW);
    float LumaNE = Luma(RgbNE);
    float LumaSW = Luma(RgbSW);
    float LumaSE = Luma(RgbSE);
    float LumaM  = Luma(RgbM);

    // 3. Compute local contrast (luminance range in neighborhood)
    float LumaMin = min(LumaM, min(min(LumaNW, LumaNE), min(LumaSW, LumaSE)));
    float LumaMax = max(LumaM, max(max(LumaNW, LumaNE), max(LumaSW, LumaSE)));
    float LumaRange = LumaMax - LumaMin;

    // 4. Early exit for low-contrast regions (no edge detected)
    float Threshold = max(0.0625f, LumaMax * 0.125f);
    if (LumaRange < Threshold)
    {
        return float4(RgbM, 1.0f);
    }

    // 5. Calculate edge direction using gradient
    // This produces a continuous 2D direction vector (not just horizontal/vertical)
    float2 Dir;
    Dir.x = -((LumaNW + LumaNE) - (LumaSW + LumaSE));
    Dir.y =  ((LumaNW + LumaSW) - (LumaNE + LumaSE));

    // 6. Direction reduction to stabilize in low-contrast/noisy areas
    // This prevents over-sensitive direction vectors
    float DirReduce = max((LumaNW + LumaNE + LumaSW + LumaSE) * (0.25f * FXAAReduceMul), FXAAReduceMin);

    // 7. Normalize direction by the smaller component to reduce orientation bias
    // Adding DirReduce prevents division by zero and dampens over-reaction
    float RcpDirMin = 1.0f / (min(abs(Dir.x), abs(Dir.y)) + DirReduce);
    float2 DirScaled = Dir * RcpDirMin;

    // 8. Clamp direction to maximum search span (in pixels)
    DirScaled = clamp(DirScaled, -FXAASpanMax, FXAASpanMax);
    Dir = DirScaled * InvResolution;

    // 9. Two-tap blur along edge direction (inner samples)
    // Samples at 1/3 and 2/3 along the direction for better coverage
    float3 RgbA = 0.5f * (
        g_SceneTexture.Sample(g_SamplerLinear, Tex + Dir * (1.0f / 3.0f - 0.5f)).rgb +
        g_SceneTexture.Sample(g_SamplerLinear, Tex + Dir * (2.0f / 3.0f - 0.5f)).rgb
    );

    // 10. Four-tap blur (combines inner + outer samples for better quality)
    float3 RgbB = RgbA * 0.5f + 0.25f * (
        g_SceneTexture.Sample(g_SamplerLinear, Tex + Dir * -0.5f).rgb +
        g_SceneTexture.Sample(g_SamplerLinear, Tex + Dir *  0.5f).rgb
    );

    // 11. Guard band check to prevent over-blur / halo artifacts
    // If blended luminance falls outside local range, we've sampled across an edge boundary
    float LumaB = Luma(RgbB);
    if ((LumaB < LumaMin) || (LumaB > LumaMax))
    {
        // Fall back to inner samples only
        RgbB = RgbA;
    }

    // 12. Final subpixel blending
    // Higher values = smoother but potentially more blur
    const float SubpixBlend = 0.99f;
    float3 FinalColor = lerp(RgbM, RgbB, SubpixBlend);

    return float4(FinalColor, 1.0f);
}
