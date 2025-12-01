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
    // 공통 파라미터 (16 bytes)
    float FocusDistance;    // 초점 거리 (View Space)
    float MaxBlurRadius;    // 최대 블러 반경 (픽셀)
    float BokehSize;        // 보케 크기
    int DoFMode;            // 0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus

    // Cinematic 모드 파라미터 (16 bytes)
    float FocusRange;       // 초점 영역 범위
    float NearBlurScale;    // 근거리 블러 강도
    float FarBlurScale;     // 원거리 블러 강도
    float _Pad0;

    // Physical 모드 파라미터 (16 bytes)
    float FocalLength;      // 렌즈 초점거리 (mm)
    float FNumber;          // 조리개 값 (F-Number)
    float SensorWidth;      // 센서 너비 (mm)
    float _Pad1;

    // Tilt-Shift 모드 파라미터 (16 bytes)
    float TiltShiftCenterY;     // 선명한 띠의 중심 Y (0~1)
    float TiltShiftBandWidth;   // 선명한 띠의 너비 (0~1)
    float TiltShiftBlurScale;   // 블러 강도 스케일
    float Weight;               // 효과 가중치

    // PointFocus 모드 파라미터 (16 bytes)
    float3 FocusPoint;          // 초점 지점 (World Space 좌표)
    float FocusPointRadius;     // 초점 반경 (이 반경 내에서는 선명)

    // PointFocus 추가 파라미터 (16 bytes)
    float PointFocusBlurScale;  // 블러 강도 스케일
    float PointFocusFalloff;    // 블러 감쇠 (1=선형, 2=제곱 등)
    int BlurMethod;             // 블러 방식 (0:Disc12, 1:Disc24, 2:Gaussian, 3:Hexagonal, 4:CircularGather)
    int BleedingMethod;         // 번짐 처리 (0:None, 1:ScatterAsGather)
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

// Screen UV + Depth에서 World Position 복원
// 뷰포트 오프셋을 고려하여 정확한 NDC 계산
float3 ReconstructWorldPosition(float2 screenUV, float rawDepth)
{
    // 1) 전체 렌더 타겟 기준 UV를 뷰포트 로컬 UV로 변환
    //    screenUV: 전체 렌더 타겟 기준 (0~1)
    //    viewportLocalUV: 뷰포트 영역 내 상대 좌표 (0~1)
    //
    //    ViewportRect: x=TopLeftX, y=TopLeftY, z=Width, w=Height
    //    ScreenSize: x=RenderTargetWidth, y=RenderTargetHeight
    float2 pixelPos = screenUV * ScreenSize.xy;  // 렌더 타겟 내 픽셀 좌표
    float2 viewportLocalUV;
    viewportLocalUV.x = (pixelPos.x - ViewportRect.x) / ViewportRect.z;
    viewportLocalUV.y = (pixelPos.y - ViewportRect.y) / ViewportRect.w;

    // 2) 뷰포트 로컬 UV를 NDC로 변환 (-1 to 1)
    //    NDC는 뷰포트 기준이므로 뷰포트 로컬 UV 사용
    float2 ndc;
    ndc.x = viewportLocalUV.x * 2.0 - 1.0;
    ndc.y = 1.0 - viewportLocalUV.y * 2.0;  // Y축 반전 (DirectX 스타일)

    // 3) NDC + Depth로 클립 공간 좌표 생성
    float4 clipPos = float4(ndc, rawDepth, 1.0);

    // 4) 클립 공간 -> 뷰 공간
    float4 viewPos = mul(clipPos, InverseProjectionMatrix);
    viewPos /= viewPos.w;

    // 5) 뷰 공간 -> 월드 공간
    float4 worldPos = mul(viewPos, InverseViewMatrix);

    return worldPos.xyz;
}

// Circle of Confusion (CoC) 계산 - Cinematic 모드 (선형 모델)
// 아티스트 친화적인 직관적 파라미터
float CalculateCoC_Cinematic(float viewDepth)
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

    return clamp(coc, -MaxBlurRadius, MaxBlurRadius);
}

// Circle of Confusion (CoC) 계산 - Physical 모드 (렌즈 물리학)
// 실제 카메라 렌즈의 CoC 공식 사용
// CoC = |A| * |f| * |d - d_s| / (d * (d_s - f))
// A = 조리개 직경 = f / N
float CalculateCoC_Physical(float viewDepth)
{
    // 단위 변환: mm -> m (View Space는 보통 m 단위)
    float f = FocalLength * 0.001;      // mm to m
    float sensor = SensorWidth * 0.001; // mm to m

    float d = max(viewDepth, 0.001);    // 물체 거리 (0 방지)
    float d_s = max(FocusDistance, 0.001); // 초점 거리

    // 조리개 직경
    float A = f / max(FNumber, 0.1);

    // CoC 계산 (m 단위)
    // 간소화된 공식: d >> f 일 때 (d_s - f) ≈ d_s
    float cocMeters = A * f * abs(d - d_s) / (d * d_s);

    // Hyperfocal Distance 계산
    // H = f^2 / (N * c) where c = 허용 CoC (센서 대각선 / 1500 정도)
    // 센서 대각선 ≈ sensor * 1.5 (4:3 비율 가정)
    float maxCoC = sensor * 1.5 / 1500.0;  // 허용 CoC
    float H = (f * f) / (FNumber * maxCoC);

    // d > H 이면 무한대까지 선명 (Far limit이 무한대)
    // 이 경우 배경 블러가 급격히 감소
    if (d_s > H && d > H)
    {
        cocMeters *= 0.5; // 과초점 거리 효과로 블러 감소
    }

    // m 단위 CoC를 픽셀 단위로 변환
    // 화면 높이(픽셀) * (CoC / 센서높이) 로 변환
    float sensorHeight = sensor * 0.667; // 3:2 비율 가정
    float screenHeight = ScreenSize.y;
    float cocPixels = cocMeters * (screenHeight / sensorHeight);

    // 부호 결정: 전경(-), 배경(+)
    if (d < d_s) cocPixels = -cocPixels;

    return clamp(cocPixels, -MaxBlurRadius, MaxBlurRadius);
}

// Circle of Confusion (CoC) 계산 - Tilt-Shift 모드
// 화면 Y 좌표 기반으로 블러 계산 (미니어처 효과)
float CalculateCoC_TiltShift(float2 screenUV)
{
    // 화면 Y 좌표와 중심의 거리
    float distFromCenter = abs(screenUV.y - TiltShiftCenterY);

    // 선명한 띠 영역 내에서는 블러 없음
    float halfBandWidth = TiltShiftBandWidth * 0.5;
    if (distFromCenter < halfBandWidth)
    {
        return 0.0;
    }

    // 띠 바깥쪽으로 갈수록 블러 증가
    float coc = (distFromCenter - halfBandWidth) * TiltShiftBlurScale;

    return clamp(coc, 0.0, MaxBlurRadius);
}

// Circle of Confusion (CoC) 계산 - PointFocus 모드
// 특정 3D 좌표를 중심으로 구형(Spherical)으로 초점 형성
// 점 초점: FocusPoint 주변의 구형 영역만 선명하게
float CalculateCoC_PointFocus(float3 worldPos)
{
    // 초점 지점으로부터의 3D 유클리드 거리 계산
    float3 toFocus = worldPos - FocusPoint;
    float dist = length(toFocus);

    // 초점 반경 내에서는 블러 없음 (선명 영역)
    if (dist <= FocusPointRadius)
    {
        return 0.0;
    }

    // 반경 바깥쪽으로 갈수록 블러 증가
    float distBeyondRadius = dist - FocusPointRadius;

    // Falloff 적용 (1=선형, 2=제곱 등)
    float falloffDist = pow(distBeyondRadius, PointFocusFalloff);

    // 블러 강도 적용
    float coc = falloffDist * PointFocusBlurScale;

    return clamp(coc, 0.0, MaxBlurRadius);
}

// 통합 CoC 계산 함수
// PointFocus 모드는 World Position이 필요하므로 rawDepth와 screenUV를 받음
float CalculateCoC(float viewDepth, float2 screenUV, float rawDepth)
{
    if (DoFMode == 0)       // Cinematic
    {
        return CalculateCoC_Cinematic(viewDepth);
    }
    else if (DoFMode == 1)  // Physical
    {
        return CalculateCoC_Physical(viewDepth);
    }
    else if (DoFMode == 2)  // TiltShift
    {
        return CalculateCoC_TiltShift(screenUV);
    }
    else                    // PointFocus
    {
        float3 worldPos = ReconstructWorldPosition(screenUV, rawDepth);
        return CalculateCoC_PointFocus(worldPos);
    }
}

//=============================================================================
// 블러 샘플 패턴 정의
//=============================================================================

// Disc12: 12 샘플 원형 패턴 (30도 간격, 반경 1.0)
static const float2 Disc12Samples[12] =
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

// Disc24: 24 샘플 원형 패턴 (2개 링: 내부 8개 + 외부 16개)
static const float2 Disc24Samples[24] =
{
    // 내부 링 (반경 0.4, 45도 간격)
    float2(0.4, 0.0),
    float2(0.283, 0.283),
    float2(0.0, 0.4),
    float2(-0.283, 0.283),
    float2(-0.4, 0.0),
    float2(-0.283, -0.283),
    float2(0.0, -0.4),
    float2(0.283, -0.283),
    // 외부 링 (반경 1.0, 22.5도 간격)
    float2(1.0, 0.0),
    float2(0.924, 0.383),
    float2(0.707, 0.707),
    float2(0.383, 0.924),
    float2(0.0, 1.0),
    float2(-0.383, 0.924),
    float2(-0.707, 0.707),
    float2(-0.924, 0.383),
    float2(-1.0, 0.0),
    float2(-0.924, -0.383),
    float2(-0.707, -0.707),
    float2(-0.383, -0.924),
    float2(0.0, -1.0),
    float2(0.383, -0.924),
    float2(0.707, -0.707),
    float2(0.924, -0.383)
};

// Gaussian: 13 샘플 가우시안 패턴 (중심 + 4방향 x 3거리)
static const float3 GaussianSamples[13] =  // xy = offset, z = weight
{
    float3(0.0, 0.0, 0.2270270270),           // 중심
    float3(1.0, 0.0, 0.1945945946),           // 오른쪽
    float3(-1.0, 0.0, 0.1945945946),          // 왼쪽
    float3(0.0, 1.0, 0.1945945946),           // 위
    float3(0.0, -1.0, 0.1945945946),          // 아래
    float3(0.707, 0.707, 0.1216216216),       // 대각선
    float3(-0.707, 0.707, 0.1216216216),
    float3(0.707, -0.707, 0.1216216216),
    float3(-0.707, -0.707, 0.1216216216),
    float3(0.5, 0.0, 0.0540540541),           // 중간거리
    float3(-0.5, 0.0, 0.0540540541),
    float3(0.0, 0.5, 0.0540540541),
    float3(0.0, -0.5, 0.0540540541)
};

// Hexagonal: 19 샘플 6각형 패턴 (중심 + 6개 꼭지점 + 12개 중간점)
static const float2 HexagonalSamples[19] =
{
    float2(0.0, 0.0),         // 중심
    // 외부 꼭지점 (60도 간격)
    float2(1.0, 0.0),
    float2(0.5, 0.866),
    float2(-0.5, 0.866),
    float2(-1.0, 0.0),
    float2(-0.5, -0.866),
    float2(0.5, -0.866),
    // 중간 링 (30도 오프셋, 반경 0.577)
    float2(0.577, 0.0),
    float2(0.289, 0.5),
    float2(-0.289, 0.5),
    float2(-0.577, 0.0),
    float2(-0.289, -0.5),
    float2(0.289, -0.5),
    // 가장자리 중간점
    float2(0.75, 0.433),
    float2(0.0, 0.866),
    float2(-0.75, 0.433),
    float2(-0.75, -0.433),
    float2(0.0, -0.866),
    float2(0.75, -0.433)
};

// CircularGather: 48 샘플 다중 링 (3개 링: 8 + 16 + 24)
static const float2 CircularGatherSamples[48] =
{
    // 링 1 (반경 0.25, 8 샘플)
    float2(0.25, 0.0), float2(0.177, 0.177), float2(0.0, 0.25), float2(-0.177, 0.177),
    float2(-0.25, 0.0), float2(-0.177, -0.177), float2(0.0, -0.25), float2(0.177, -0.177),
    // 링 2 (반경 0.55, 16 샘플)
    float2(0.55, 0.0), float2(0.508, 0.21), float2(0.389, 0.389), float2(0.21, 0.508),
    float2(0.0, 0.55), float2(-0.21, 0.508), float2(-0.389, 0.389), float2(-0.508, 0.21),
    float2(-0.55, 0.0), float2(-0.508, -0.21), float2(-0.389, -0.389), float2(-0.21, -0.508),
    float2(0.0, -0.55), float2(0.21, -0.508), float2(0.389, -0.389), float2(0.508, -0.21),
    // 링 3 (반경 1.0, 24 샘플)
    float2(1.0, 0.0), float2(0.966, 0.259), float2(0.866, 0.5), float2(0.707, 0.707),
    float2(0.5, 0.866), float2(0.259, 0.966), float2(0.0, 1.0), float2(-0.259, 0.966),
    float2(-0.5, 0.866), float2(-0.707, 0.707), float2(-0.866, 0.5), float2(-0.966, 0.259),
    float2(-1.0, 0.0), float2(-0.966, -0.259), float2(-0.866, -0.5), float2(-0.707, -0.707),
    float2(-0.5, -0.866), float2(-0.259, -0.966), float2(0.0, -1.0), float2(0.259, -0.966),
    float2(0.5, -0.866), float2(0.707, -0.707), float2(0.866, -0.5), float2(0.966, -0.259)
};

//=============================================================================
// 블러 적용 함수들
//=============================================================================

// 공통: 깊이 기반 가중치 계산 (전경 객체가 배경 위로 블리딩되는 것 방지)
float CalculateSampleWeight(float centerCoC, float sampleCoC)
{
    float weight = 1.0;
    if (centerCoC > 0.0 && sampleCoC < centerCoC)
    {
        weight = saturate(1.0 - (centerCoC - sampleCoC) / MaxBlurRadius);
    }
    return weight;
}

// Scatter-as-Gather용 가중치 계산
// 샘플의 CoC가 중심까지 도달할 수 있는지 체크하여 번짐 효과 구현
//
// 광학적 원리:
// - 배경 블러는 전경 "뒤로" 번짐 (전경이 배경을 가림)
// - 전경 블러는 배경 "위로" 번짐 (전경이 배경 위에 있음)
// - 따라서 배경이 선명한 전경 "위로" 번지면 안 됨
float CalculateSampleWeight_SaG(float centerCoC, float sampleCoC, float distPixels)
{
    float absSampleCoC = abs(sampleCoC);
    float absCenterCoC = abs(centerCoC);

    // 샘플이 중심까지 도달할 수 있는가? (Scatter 조건)
    bool sampleReachesCenter = (distPixels <= absSampleCoC);
    // 중심이 샘플까지 도달할 수 있는가? (기존 Gather 조건)
    bool centerReachesSample = (distPixels <= absCenterCoC);

    if (!sampleReachesCenter && !centerReachesSample)
    {
        return 0.0;
    }

    float scatterWeight = 0.0;
    float gatherWeight = 0.0;

    if (sampleReachesCenter)
    {
        // 샘플이 중심으로 번지는 경우: 거리에 따라 감쇠
        scatterWeight = 1.0 - (distPixels / max(absSampleCoC, 0.001));
        scatterWeight = saturate(scatterWeight);

        // 깊이 순서 기반 감쇠: 배경이 선명한 전경 위로 번지는 걸 방지
        // sampleCoC > centerCoC: 샘플이 중심보다 뒤에 있음 (배경 → 전경 번짐)
        // 이 경우, 중심이 선명할수록 번짐을 줄여야 함 (전경이 배경을 가리므로)
        if (sampleCoC > centerCoC)
        {
            // 중심의 선명도 (0에 가까울수록 선명)
            float centerSharpness = 1.0 - saturate(absCenterCoC / max(MaxBlurRadius, 0.001));
            // 깊이 차이 (클수록 더 많이 감쇠)
            float depthDiff = saturate((sampleCoC - centerCoC) / max(MaxBlurRadius, 0.001));
            // 감쇠 적용: 중심이 선명하고 깊이 차이가 클수록 감쇠
            // centerSharpness=1, depthDiff=1이면 scatterWeight가 5%로 감소
            scatterWeight *= (1.0 - centerSharpness * depthDiff * 0.95);
        }
        // sampleCoC < centerCoC: 샘플이 중심보다 앞에 있음 (전경 → 배경 번짐)
        // 이 경우는 번짐이 자연스러움 (전경이 배경 위에 있으니까) - 감쇠 없음
    }

    if (centerReachesSample)
    {
        // 기존 Gather 로직
        gatherWeight = 1.0;
        if (centerCoC > 0.0 && sampleCoC < centerCoC)
        {
            gatherWeight = saturate(1.0 - (centerCoC - sampleCoC) / max(MaxBlurRadius, 0.001));
        }
    }

    // 두 가중치 중 큰 값 사용
    return max(scatterWeight, gatherWeight);
}

// Disc12 블러 (12 샘플, 빠름)
float4 ApplyDisc12Blur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    // 중심 픽셀을 먼저 추가
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, texCoord);

    // SaG 모드에서 선명한 픽셀 보호: 중심이 선명할수록 가중치 부스트
    // 이렇게 하면 선명한 영역에서 배경 번짐의 상대적 영향이 줄어듦
    float centerWeight = 1.0;
    if (BleedingMethod == 1)
    {
        float centerSharpness = 1.0 - saturate(abs(centerCoC) / max(MaxBlurRadius, 0.001));
        centerWeight = 1.0 + centerSharpness * 4.0;  // 선명하면 최대 5.0
    }
    float4 accumulatedColor = centerColor * centerWeight;
    float totalWeight = centerWeight;

    // SaG 모드일 때는 최대 블러 반경으로 검색
    float searchRadius = (BleedingMethod == 1) ? MaxBlurRadius * BokehSize : blurRadius;

    for (int i = 0; i < 12; i++)
    {
        float2 offset = Disc12Samples[i] * searchRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight;
        if (BleedingMethod == 1)  // ScatterAsGather
        {
            float distPixels = length(Disc12Samples[i] * searchRadius);
            sampleWeight = CalculateSampleWeight_SaG(centerCoC, sampleCoC, distPixels);
        }
        else  // None (기존 방식)
        {
            sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        }

        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return accumulatedColor / totalWeight;
}

// Disc24 블러 (24 샘플, 고품질)
float4 ApplyDisc24Blur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    // 중심 픽셀을 먼저 추가
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, texCoord);

    // SaG 모드에서 선명한 픽셀 보호
    float centerWeight = 1.0;
    if (BleedingMethod == 1)
    {
        float centerSharpness = 1.0 - saturate(abs(centerCoC) / max(MaxBlurRadius, 0.001));
        centerWeight = 1.0 + centerSharpness * 4.0;
    }
    float4 accumulatedColor = centerColor * centerWeight;
    float totalWeight = centerWeight;

    // SaG 모드일 때는 최대 블러 반경으로 검색
    float searchRadius = (BleedingMethod == 1) ? MaxBlurRadius * BokehSize : blurRadius;

    for (int i = 0; i < 24; i++)
    {
        float2 offset = Disc24Samples[i] * searchRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight;
        if (BleedingMethod == 1)  // ScatterAsGather
        {
            float distPixels = length(Disc24Samples[i] * searchRadius);
            sampleWeight = CalculateSampleWeight_SaG(centerCoC, sampleCoC, distPixels);
        }
        else  // None (기존 방식)
        {
            sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        }

        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return accumulatedColor / totalWeight;
}

// Gaussian 블러 (13 샘플, 가중치 기반)
float4 ApplyGaussianBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    // 중심 픽셀을 먼저 추가
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, texCoord);

    // SaG 모드에서 선명한 픽셀 보호
    float centerWeight = 1.0;
    if (BleedingMethod == 1)
    {
        float centerSharpness = 1.0 - saturate(abs(centerCoC) / max(MaxBlurRadius, 0.001));
        centerWeight = 1.0 + centerSharpness * 4.0;
    }
    float4 accumulatedColor = centerColor * centerWeight;
    float totalWeight = centerWeight;

    // SaG 모드일 때는 최대 블러 반경으로 검색
    float searchRadius = (BleedingMethod == 1) ? MaxBlurRadius * BokehSize : blurRadius;

    for (int i = 0; i < 13; i++)
    {
        float2 offset = GaussianSamples[i].xy * searchRadius * pixelSize;
        float gaussianWeight = GaussianSamples[i].z;  // 미리 계산된 가우시안 가중치
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float depthWeight;
        if (BleedingMethod == 1)  // ScatterAsGather
        {
            float distPixels = length(GaussianSamples[i].xy * searchRadius);
            depthWeight = CalculateSampleWeight_SaG(centerCoC, sampleCoC, distPixels);
        }
        else  // None (기존 방식)
        {
            depthWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        }
        float finalWeight = gaussianWeight * depthWeight;

        accumulatedColor += sampleColor * finalWeight;
        totalWeight += finalWeight;
    }

    return accumulatedColor / totalWeight;
}

// Hexagonal 블러 (19 샘플, 6각형 보케)
float4 ApplyHexagonalBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    // 중심 픽셀을 먼저 추가
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, texCoord);

    // SaG 모드에서 선명한 픽셀 보호
    float centerWeight = 1.0;
    if (BleedingMethod == 1)
    {
        float centerSharpness = 1.0 - saturate(abs(centerCoC) / max(MaxBlurRadius, 0.001));
        centerWeight = 1.0 + centerSharpness * 4.0;
    }
    float4 accumulatedColor = centerColor * centerWeight;
    float totalWeight = centerWeight;

    // SaG 모드일 때는 최대 블러 반경으로 검색
    float searchRadius = (BleedingMethod == 1) ? MaxBlurRadius * BokehSize : blurRadius;

    for (int i = 0; i < 19; i++)
    {
        float2 offset = HexagonalSamples[i] * searchRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight;
        if (BleedingMethod == 1)  // ScatterAsGather
        {
            float distPixels = length(HexagonalSamples[i] * searchRadius);
            sampleWeight = CalculateSampleWeight_SaG(centerCoC, sampleCoC, distPixels);
        }
        else  // None (기존 방식)
        {
            sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        }

        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return accumulatedColor / totalWeight;
}

// CircularGather 블러 (48 샘플, 최고 품질)
float4 ApplyCircularGatherBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    // 중심 픽셀을 먼저 추가
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, texCoord);

    // SaG 모드에서 선명한 픽셀 보호
    float centerWeight = 1.0;
    if (BleedingMethod == 1)
    {
        float centerSharpness = 1.0 - saturate(abs(centerCoC) / max(MaxBlurRadius, 0.001));
        centerWeight = 1.0 + centerSharpness * 4.0;
    }
    float4 accumulatedColor = centerColor * centerWeight;
    float totalWeight = centerWeight;

    // SaG 모드일 때는 최대 블러 반경으로 검색
    float searchRadius = (BleedingMethod == 1) ? MaxBlurRadius * BokehSize : blurRadius;

    for (int i = 0; i < 48; i++)
    {
        float2 offset = CircularGatherSamples[i] * searchRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight;
        if (BleedingMethod == 1)  // ScatterAsGather
        {
            float distPixels = length(CircularGatherSamples[i] * searchRadius);
            sampleWeight = CalculateSampleWeight_SaG(centerCoC, sampleCoC, distPixels);
        }
        else  // None (기존 방식)
        {
            sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        }

        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return accumulatedColor / totalWeight;
}

//=============================================================================
// 메인 픽셀 셰이더
//=============================================================================

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 원본 색상 및 깊이 샘플링
    float4 centerColor = g_SceneColorTex.Sample(g_LinearClampSample, input.texCoord);
    float centerRawDepth = g_DepthTex.Sample(g_PointClampSample, input.texCoord).r;

    // View Space 깊이 계산
    float centerViewDepth = GetLinearDepth(centerRawDepth);

    // CoC 계산 (모드에 따라 다르게 계산, PointFocus는 rawDepth 필요)
    float centerCoC = CalculateCoC(centerViewDepth, input.texCoord, centerRawDepth);
    float absCoC = abs(centerCoC);

    // CoC가 0에 가까우면 원본 반환 (단, SaG 모드에서는 주변 번짐을 위해 블러 처리 필요)
    if (BleedingMethod == 0 && absCoC < 0.5)
    {
        return centerColor;
    }

    // 화면 비율 보정을 위한 픽셀 크기
    float2 pixelSize = ScreenSize.zw;

    // 블러 반경 (픽셀 단위)
    // SaG 모드에서 centerCoC가 0이면 블러 반경도 0이지만, 블러 함수 내에서 searchRadius가 확장됨
    float blurRadius = absCoC * BokehSize;

    // 블러 방식에 따라 다른 함수 호출
    float4 blurredColor;
    if (BlurMethod == 0)        // Disc12 (기본, 빠름)
    {
        blurredColor = ApplyDisc12Blur(input.texCoord, blurRadius, pixelSize, centerCoC);
    }
    else if (BlurMethod == 1)   // Disc24 (고품질)
    {
        blurredColor = ApplyDisc24Blur(input.texCoord, blurRadius, pixelSize, centerCoC);
    }
    else if (BlurMethod == 2)   // Gaussian (자연스러움)
    {
        blurredColor = ApplyGaussianBlur(input.texCoord, blurRadius, pixelSize, centerCoC);
    }
    else if (BlurMethod == 3)   // Hexagonal (6각형 보케)
    {
        blurredColor = ApplyHexagonalBlur(input.texCoord, blurRadius, pixelSize, centerCoC);
    }
    else                        // CircularGather (최고 품질, 느림)
    {
        blurredColor = ApplyCircularGatherBlur(input.texCoord, blurRadius, pixelSize, centerCoC);
    }

    // Weight를 사용하여 원본과 블러된 색상 블렌딩
    float4 finalColor = lerp(centerColor, blurredColor, Weight);

    return finalColor;
}
