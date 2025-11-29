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
    int DoFMode;            // 0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus, 4: ScreenPointFocus

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
    float _Pad3;

    // ScreenPointFocus 모드 파라미터 (16 bytes)
    float2 ScreenFocusPoint;        // 화면상의 초점 위치 (0~1 UV 좌표)
    float ScreenFocusRadius;        // 화면상의 초점 반경 (0~1, 화면 비율)
    float ScreenFocusDepthRange;    // 초점 깊이 허용 범위

    // ScreenPointFocus 추가 파라미터 (16 bytes)
    float ScreenFocusBlurScale;     // 블러 강도 스케일
    float ScreenFocusFalloff;       // 블러 감쇠 (1=선형, 2=제곱 등)
    float ScreenFocusAspectRatio;   // 화면 비율 보정 (width/height)
    float _Pad4;
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

// Circle of Confusion (CoC) 계산 - ScreenPointFocus 모드
// 화면상의 특정 x, y 좌표를 중심으로 초점 형성 (깊이도 고려)
// 화면 좌표 기반 점 초점: 클릭한 위치 주변이 선명
float CalculateCoC_ScreenPointFocus(float2 screenUV, float viewDepth)
{
    // 화면상의 거리 계산 (aspect ratio 보정)
    float2 toFocus = screenUV - ScreenFocusPoint;
    toFocus.x *= ScreenFocusAspectRatio;  // 가로 세로 비율 보정
    float screenDist = length(toFocus);

    // 초점 위치의 깊이 가져오기 (ScreenFocusPoint에서 샘플링)
    float focusRawDepth = g_DepthTex.Sample(g_PointClampSample, ScreenFocusPoint).r;
    float focusViewDepth = GetLinearDepth(focusRawDepth);

    // 깊이 차이 계산
    float depthDiff = abs(viewDepth - focusViewDepth);

    // 화면상 초점 반경 내 + 깊이 범위 내에서는 블러 없음
    if (screenDist <= ScreenFocusRadius && depthDiff <= ScreenFocusDepthRange)
    {
        return 0.0;
    }

    // 화면 거리와 깊이 차이를 결합하여 블러 계산
    float screenBlur = 0.0;
    float depthBlur = 0.0;

    // 화면 거리 기반 블러
    if (screenDist > ScreenFocusRadius)
    {
        float screenDistBeyond = screenDist - ScreenFocusRadius;
        screenBlur = pow(screenDistBeyond, ScreenFocusFalloff);
    }

    // 깊이 차이 기반 블러
    if (depthDiff > ScreenFocusDepthRange)
    {
        float depthDistBeyond = depthDiff - ScreenFocusDepthRange;
        depthBlur = depthDistBeyond * 0.1;  // 깊이 블러 스케일
    }

    // 화면 거리와 깊이 블러를 결합 (둘 중 큰 값 사용)
    float combinedBlur = max(screenBlur, depthBlur);

    // 블러 강도 적용
    float coc = combinedBlur * ScreenFocusBlurScale;

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
    else if (DoFMode == 3)  // PointFocus (World Space)
    {
        float3 worldPos = ReconstructWorldPosition(screenUV, rawDepth);
        return CalculateCoC_PointFocus(worldPos);
    }
    else                    // ScreenPointFocus (Screen Space)
    {
        return CalculateCoC_ScreenPointFocus(screenUV, viewDepth);
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

// Hexagonal: 37 샘플 6각형 패턴 (테두리 강조, 3개 링)
// 외곽 테두리에 샘플을 집중시켜 6각형 보케 형태가 뚜렷하게 보이도록 함
static const float3 HexagonalSamples[37] =  // xy = offset, z = weight
{
    // 중심 (1개, 낮은 가중치)
    float3(0.0, 0.0, 0.3),

    // 링 1: 내부 6각형 (반경 0.33, 6개)
    float3(0.33, 0.0, 0.5),
    float3(0.165, 0.286, 0.5),
    float3(-0.165, 0.286, 0.5),
    float3(-0.33, 0.0, 0.5),
    float3(-0.165, -0.286, 0.5),
    float3(0.165, -0.286, 0.5),

    // 링 2: 중간 6각형 (반경 0.66, 12개 - 꼭지점 + 변 중간)
    float3(0.66, 0.0, 0.7),
    float3(0.495, 0.286, 0.7),
    float3(0.33, 0.572, 0.7),
    float3(0.0, 0.572, 0.7),
    float3(-0.33, 0.572, 0.7),
    float3(-0.495, 0.286, 0.7),
    float3(-0.66, 0.0, 0.7),
    float3(-0.495, -0.286, 0.7),
    float3(-0.33, -0.572, 0.7),
    float3(0.0, -0.572, 0.7),
    float3(0.33, -0.572, 0.7),
    float3(0.495, -0.286, 0.7),

    // 링 3: 외곽 6각형 테두리 (반경 1.0, 18개 - 높은 가중치로 테두리 강조)
    // 각 변에 3개씩 샘플 배치
    float3(1.0, 0.0, 1.0),              // 꼭지점 1
    float3(0.833, 0.289, 1.0),
    float3(0.667, 0.577, 1.0),
    float3(0.5, 0.866, 1.0),            // 꼭지점 2
    float3(0.167, 0.866, 1.0),
    float3(-0.167, 0.866, 1.0),
    float3(-0.5, 0.866, 1.0),           // 꼭지점 3
    float3(-0.667, 0.577, 1.0),
    float3(-0.833, 0.289, 1.0),
    float3(-1.0, 0.0, 1.0),             // 꼭지점 4
    float3(-0.833, -0.289, 1.0),
    float3(-0.667, -0.577, 1.0),
    float3(-0.5, -0.866, 1.0),          // 꼭지점 5
    float3(-0.167, -0.866, 1.0),
    float3(0.167, -0.866, 1.0),
    float3(0.5, -0.866, 1.0),           // 꼭지점 6
    float3(0.667, -0.577, 1.0),
    float3(0.833, -0.289, 1.0)
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

// 공통: 깊이 기반 가중치 계산 (블리딩 방지)
// - 전경 객체가 배경 위로 블리딩되는 것 방지
// - 배경 객체가 선명/전경 영역으로 블리딩되는 것 방지
float CalculateSampleWeight(float centerCoC, float sampleCoC)
{
    float weight = 1.0;

    // Case 1: 배경 픽셀(centerCoC > 0)에 전경 샘플(sampleCoC < centerCoC) 블리딩 방지
    // 전경 객체가 배경 블러 위로 번지는 것을 막음
    if (centerCoC > 0.0 && sampleCoC < centerCoC)
    {
        weight = saturate(1.0 - (centerCoC - sampleCoC) / MaxBlurRadius);
    }
    // Case 2: 선명/전경 픽셀(centerCoC <= 0)에 배경 샘플(sampleCoC > 0) 블리딩 방지
    // 배경 블러가 선명한 영역이나 전경으로 번지는 것을 막음
    else if (centerCoC <= 0.0 && sampleCoC > 0.0)
    {
        weight = saturate(1.0 - sampleCoC / MaxBlurRadius);
    }
    // Case 3: 전경 픽셀(centerCoC < 0)에 더 가까운 전경 샘플 블리딩 방지
    // 더 가까운 전경 블러가 덜 가까운 전경으로 번지는 것을 막음
    else if (centerCoC < 0.0 && sampleCoC < centerCoC)
    {
        weight = saturate(1.0 - (centerCoC - sampleCoC) / MaxBlurRadius);
    }

    return weight;
}

// Disc12 블러 (12 샘플, 빠름)
float4 ApplyDisc12Blur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    float4 accumulatedColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0;

    for (int i = 0; i < 12; i++)
    {
        float2 offset = Disc12Samples[i] * blurRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return (totalWeight > 0.0) ? (accumulatedColor / totalWeight) : float4(0, 0, 0, 1);
}

// Disc24 블러 (24 샘플, 고품질)
float4 ApplyDisc24Blur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    float4 accumulatedColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0;

    for (int i = 0; i < 24; i++)
    {
        float2 offset = Disc24Samples[i] * blurRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return (totalWeight > 0.0) ? (accumulatedColor / totalWeight) : float4(0, 0, 0, 1);
}

// Gaussian 블러 (13 샘플, 가중치 기반)
float4 ApplyGaussianBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    float4 accumulatedColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0;

    for (int i = 0; i < 13; i++)
    {
        float2 offset = GaussianSamples[i].xy * blurRadius * pixelSize;
        float gaussianWeight = GaussianSamples[i].z;  // 미리 계산된 가우시안 가중치
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float depthWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        float finalWeight = gaussianWeight * depthWeight;

        accumulatedColor += sampleColor * finalWeight;
        totalWeight += finalWeight;
    }

    return (totalWeight > 0.0) ? (accumulatedColor / totalWeight) : float4(0, 0, 0, 1);
}

// Hexagonal 블러 (37 샘플, 6각형 보케 - 테두리 강조)
float4 ApplyHexagonalBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    float4 accumulatedColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0;

    for (int i = 0; i < 37; i++)
    {
        float2 offset = HexagonalSamples[i].xy * blurRadius * pixelSize;
        float hexWeight = HexagonalSamples[i].z;  // 6각형 패턴 가중치 (외곽일수록 높음)
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float depthWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        float finalWeight = hexWeight * depthWeight;

        accumulatedColor += sampleColor * finalWeight;
        totalWeight += finalWeight;
    }

    return (totalWeight > 0.0) ? (accumulatedColor / totalWeight) : float4(0, 0, 0, 1);
}

// CircularGather 블러 (48 샘플, 최고 품질)
float4 ApplyCircularGatherBlur(float2 texCoord, float blurRadius, float2 pixelSize, float centerCoC)
{
    float4 accumulatedColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0;

    for (int i = 0; i < 48; i++)
    {
        float2 offset = CircularGatherSamples[i] * blurRadius * pixelSize;
        float2 sampleUV = texCoord + offset;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleColor = g_SceneColorTex.Sample(g_LinearClampSample, sampleUV);
        float sampleRawDepth = g_DepthTex.Sample(g_PointClampSample, sampleUV).r;
        float sampleViewDepth = GetLinearDepth(sampleRawDepth);
        float sampleCoC = CalculateCoC(sampleViewDepth, sampleUV, sampleRawDepth);

        float sampleWeight = CalculateSampleWeight(centerCoC, sampleCoC);
        accumulatedColor += sampleColor * sampleWeight;
        totalWeight += sampleWeight;
    }

    return (totalWeight > 0.0) ? (accumulatedColor / totalWeight) : float4(0, 0, 0, 1);
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

    // CoC가 0에 가까우면 원본 반환
    if (absCoC < 0.5)
    {
        return centerColor;
    }

    // 화면 비율 보정을 위한 픽셀 크기
    float2 pixelSize = ScreenSize.zw;

    // 블러 반경 (픽셀 단위)
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
