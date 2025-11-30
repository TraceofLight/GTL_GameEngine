// =============================================================================
// Sky Sphere Shader
// BP_Sky_Sphere 스타일의 절차적 스카이 렌더링
// =============================================================================

// b0: ModelBuffer (VS)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
    row_major float4x4 WorldInverseTranspose;
};

// b1: ViewProjBuffer (VS/PS)
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;
    row_major float4x4 InverseProjectionMatrix;
};

// b9: SkyConstantBuffer (PS)
cbuffer SkyConstantBuffer : register(b9)
{
    float4 ZenithColor;         // 천정(상단) 색상, RGBA
    float4 HorizonColor;        // 수평선 색상, RGBA
    float4 GroundColor;         // 지면(하단) 색상, RGBA (수평선 아래)

    float3 SunDirection;        // 태양 방향 (정규화됨, 월드 공간)
    float SunDiskSize;          // 태양 원반 크기 (0.0 ~ 1.0)

    float4 SunColor;            // 태양 색상 + 강도 (RGB + Intensity in A)

    float HorizonFalloff;       // 수평선 그라디언트 감쇠 (1.0 ~ 10.0)
    float SunHeight;            // 태양 높이 (0.0 = 수평선, 1.0 = 천정)
    float OverallBrightness;    // 전체 밝기 스케일
    float CloudOpacity;         // 구름 불투명도 (미래 확장용)
};

// b3: ColorId (VS/PS) - Object ID 렌더링용
cbuffer ColorId : register(b3)
{
    float4 Color;
    uint UUID;
    float3 Padding;
};

// 입력/출력 구조체
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position     : SV_POSITION;
    float3 WorldPos     : TEXCOORD0;
    float3 LocalDir     : TEXCOORD1;    // 스카이 샘플링용 방향 벡터
    float2 TexCoord     : TEXCOORD2;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    uint UUID    : SV_Target1;
};

// =============================================================================
// Vertex Shader
// =============================================================================
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    // 로컬 위치를 월드로 변환
    float4 WorldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    Output.WorldPos = WorldPos.xyz;

    // View-Projection 변환
    float4 ViewPos = mul(WorldPos, ViewMatrix);
    Output.Position = mul(ViewPos, ProjectionMatrix);

    // 스카이 샘플링용 방향 벡터 (정규화된 로컬 위치)
    // Sky Sphere는 원점 중심이므로 Position이 곧 방향 벡터
    Output.LocalDir = normalize(Input.Position);

    Output.TexCoord = Input.TexCoord;

    return Output;
}

// =============================================================================
// Pixel Shader Helper Functions
// =============================================================================

// 부드러운 그라디언트 보간
float SmoothGradient(float T, float Falloff)
{
    return pow(saturate(T), Falloff);
}

// 태양 원반 렌더링
float3 ComputeSunDisk(float3 ViewDir, float3 SunDir, float DiskSize, float3 InSunColor, float SunIntensity)
{
    // 시선 방향과 태양 방향의 내적
    float SunDot = dot(ViewDir, SunDir);

    // DiskSize를 각도 기반으로 변환 (0.01 = 약 0.5도, 0.1 = 약 5도)
    float DiskAngle = DiskSize * 0.5f;  // DiskSize가 직접적으로 크기에 영향

    // 태양 원반 마스크 (부드러운 가장자리)
    float SunMask = smoothstep(1.0f - DiskAngle, 1.0f - DiskAngle * 0.5f, SunDot);

    // 태양 글로우 (DiskSize에 비례하여 글로우 범위 조절)
    float GlowFalloff = 32.0f / max(DiskSize, 0.01f);  // DiskSize가 작을수록 글로우가 더 집중됨
    float SunGlow = pow(saturate(SunDot), GlowFalloff) * 0.3f * DiskSize;

    return InSunColor * (SunMask + SunGlow) * SunIntensity;
}

// =============================================================================
// Pixel Shader
// =============================================================================
PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;

    // 정규화된 방향 벡터
    float3 ViewDir = normalize(Input.LocalDir);

    // 수직 성분 (Z-up 좌표계)
    float VerticalGradient = ViewDir.z;

    // 수평선 위/아래 분리
    float AboveHorizon = saturate(VerticalGradient);           // 0 ~ 1 (위쪽)
    float BelowHorizon = saturate(-VerticalGradient);          // 0 ~ 1 (아래쪽)

    // 그라디언트 계산 (Falloff 적용)
    float SkyGradient = SmoothGradient(AboveHorizon, HorizonFalloff);
    float GroundGradient = SmoothGradient(BelowHorizon, HorizonFalloff * 0.5f);

    // 기본 스카이 색상 블렌딩
    // 수평선 위: Horizon → Zenith
    // 수평선 아래: Horizon → Ground
    float3 SkyColor;
    if (VerticalGradient >= 0.0f)
    {
        SkyColor = lerp(HorizonColor.rgb, ZenithColor.rgb, SkyGradient);
    }
    else
    {
        SkyColor = lerp(HorizonColor.rgb, GroundColor.rgb, GroundGradient);
    }

    // 태양 디스크 추가 (수평선 위에만)
    if (VerticalGradient >= -0.1f && SunDiskSize > 0.0f)
    {
        float3 SunContribution = ComputeSunDisk(
            ViewDir,
            normalize(SunDirection),
            SunDiskSize,
            SunColor.rgb,
            SunColor.a
        );
        SkyColor += SunContribution;
    }

    // 전체 밝기 적용
    SkyColor *= OverallBrightness;

    // 출력
    Output.Color = float4(SkyColor, 1.0f);
    Output.UUID = UUID;

    return Output;
}
