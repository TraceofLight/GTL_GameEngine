#pragma once
// b0 in VS
#include "Color.h"
#include "LightManager.h"

struct ModelBufferType // b0
{
    FMatrix Model;
    FMatrix ModelInverseTranspose;  // For correct normal transformation with non-uniform scale
};

struct ViewProjBufferType // b1 고유번호 고정
{
    FMatrix View;
    FMatrix Proj;
    FMatrix InvView;
    FMatrix InvProj;
};

struct DecalBufferType
{
    FMatrix DecalMatrix;
    float Opacity;
};

// Fireball material parameters (b6 in PS)
struct FireballBufferType
{
    float Time;
    FVector2D Resolution;
    float Padding0; // align to 16 bytes

    FVector CameraPosition;
    float Padding1; // align to 16 bytes

    FVector2D UVScrollSpeed;
    FVector2D UVRotateRad;

    uint32 LayerCount;
    float LayerDivBase;
    float GateK;
    float IntensityScale;
};

struct PostProcessBufferType // b0
{
    float Near;
    float Far;
    int IsOrthographic; // 0 = Perspective, 1 = Orthographic
    float Padding; // 16바이트 정렬을 위한 패딩
};

struct FogBufferType // b2
{
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;

    FVector4 FogInscatteringColor; // 16 bytes alignment 위해 중간에 넣음

    float FogMaxOpacity;
    float FogHeight; // fog base height
    float Padding[2]; // 16바이트 정렬을 위한 패딩
};

struct alignas(16) FFadeInOutBufferType // b2
{
    FLinearColor FadeColor = FLinearColor(0, 0, 0, 1);  //보통 (0, 0, 0, 1)

    float Opacity = 0.0f;   // 0~1
    float Weight  = 1.0f;
    float _Pad[2] = {0,0};
};
static_assert(sizeof(FFadeInOutBufferType) % 16 == 0, "CB must be 16-byte aligned");

struct alignas(16) FVinetteBufferType // b2
{
    FLinearColor Color;

    float     Radius    = 0.35f;                        // 효과 시작 반경(0~1)
    float     Softness  = 0.25f;                        // 페더 폭(0~1)
    float     Intensity = 1.0f;                       // 컬러 블렌드 강도 (0~1)
    float     Roundness = 2.0f;                       // 1=마름모, 2= 원형, >1 더 네모에 가깝게

    float     Weight    = 1.0f;
    float     _Pad0[3];
};
static_assert(sizeof(FVinetteBufferType) % 16 == 0, "CB must be 16-byte aligned");

struct alignas(16) FGammaCorrectionBufferType
{
    float Gamma;
    float Padding[3];
};
static_assert(sizeof(FGammaCorrectionBufferType) % 16 == 0, "CB must be 16-byte aligned");

// DoF 모드 열거형
enum class EDoFMode : int32
{
    Cinematic = 0,   // 시네마틱 DoF (선형 모델, 아티스트 친화적)
    Physical = 1,    // 물리 기반 DoF (실제 카메라 렌즈 시뮬레이션)
    TiltShift = 2,   // 틸트-시프트 (화면 Y 좌표 기반, 미니어처 효과)
    PointFocus = 3,  // 점 초점 DoF (특정 3D 좌표 중심 구형 초점)
};

// DoF 블러 방식 열거형
enum class EDoFBlurMethod : int32
{
    Disc12 = 0,      // 12 샘플 디스크 블러 (기본, 빠름)
    Disc24 = 1,      // 24 샘플 디스크 블러 (고품질)
    Gaussian = 2,    // 가우시안 블러 (자연스러움)
    Hexagonal = 3,   // 6각형 보케 (아나모픽 렌즈 스타일)
    CircularGather = 4, // 다중 링 원형 블러 (최고 품질, 느림)
};

struct alignas(16) FDepthOfFieldBufferType // b2
{
    // 공통 파라미터 (16 bytes)
    float FocusDistance;    // 초점 거리 (View Space 단위)
    float MaxBlurRadius;    // 최대 블러 반경 (픽셀 단위)
    float BokehSize;        // 보케 크기
    int32 DoFMode;          // 0: Cinematic, 1: Physical, 2: TiltShift, 3: PointFocus

    // Cinematic 모드 파라미터 (16 bytes)
    float FocusRange;       // 초점 영역 범위
    float NearBlurScale;    // 근거리 블러 강도
    float FarBlurScale;     // 원거리 블러 강도
    float _Pad0;

    // Physical 모드 파라미터 (16 bytes)
    float FocalLength;      // 렌즈 초점거리 (mm), 예: 35, 50, 85
    float FNumber;          // 조리개 값 (F-Number), 예: 1.4, 2.8, 5.6
    float SensorWidth;      // 센서 너비 (mm), 풀프레임: 36, APS-C: 23.6
    float _Pad1;

    // Tilt-Shift 모드 파라미터 (16 bytes)
    float TiltShiftCenterY;     // 선명한 띠의 중심 Y (0~1)
    float TiltShiftBandWidth;   // 선명한 띠의 너비 (0~1)
    float TiltShiftBlurScale;   // 블러 강도 스케일
    float Weight;               // 효과 가중치

    // PointFocus 모드 파라미터 (16 bytes)
    FVector FocusPoint;         // 초점 지점 (World Space 좌표)
    float FocusRadius;          // 초점 반경 (이 반경 내에서는 선명)

    // PointFocus 추가 파라미터 (16 bytes)
    float PointFocusBlurScale;  // 블러 강도 스케일
    float PointFocusFalloff;    // 블러 감쇠 (1=선형, 2=제곱 등)
    int32 BlurMethod;           // 블러 방식 (EDoFBlurMethod)
    float _Pad3;
};
static_assert(sizeof(FDepthOfFieldBufferType) % 16 == 0, "CB must be 16-byte aligned");

struct FXAABufferType // b2
{
    FVector2D InvResolution;    // 1.0f / 해상도 (e.g., 1/1920, 1/1080)
    float FXAASpanMax;          // 최대 탐색 범위 (픽셀 단위, 권장: 8.0)
    float FXAAReduceMul;        // 감쇠 배율 (권장: 1/8 = 0.125)
    float FXAAReduceMin;        // 최소 감쇠값 (권장: 1/128 = 0.0078125)
    float Padding[3];           // 16바이트 정렬
};

// b0 in PS
struct FMaterialInPs
{
    FVector DiffuseColor; // Kd
    float OpticalDensity; // Ni

    FVector AmbientColor; // Ka
    float Transparency; // Tr Or d

    FVector SpecularColor; // Ks
    float SpecularExponent; // Ns

    FVector EmissiveColor; // Ke
    uint32 IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials

    FVector TransmissionFilter; // Tf
    float dummy; // 4 bytes padding
    FMaterialInPs() = default;
    FMaterialInPs(const FMaterialInfo& MaterialInfo)
        :DiffuseColor(MaterialInfo.DiffuseColor),
        OpticalDensity(MaterialInfo.OpticalDensity),
        AmbientColor(MaterialInfo.AmbientColor),
        Transparency(MaterialInfo.Transparency),
        SpecularColor(MaterialInfo.SpecularColor),
        SpecularExponent(MaterialInfo.SpecularExponent),
        EmissiveColor(MaterialInfo.EmissiveColor),
        IlluminationModel(MaterialInfo.IlluminationModel),
        TransmissionFilter(MaterialInfo.TransmissionFilter),
        dummy(0)
    {

    }
};


struct FPixelConstBufferType
{
    FMaterialInPs Material;
    uint32 bHasMaterial;
    uint32 bHasDiffuseTexture;
    uint32 bHasNormalTexture;
	float Padding; // 16바이트 정렬을 위한 패딩
};

struct ColorBufferType // b3
{
    FLinearColor Color;
    uint32 UUID;
    FVector Padding;
};

struct FLightBufferType
{
    FAmbientLightInfo AmbientLight;
    FDirectionalLightInfo DirectionalLight;

    uint32 PointLightCount;
    uint32 SpotLightCount;
    FVector2D Padding;
};

// b10 고유번호 고정
struct FViewportConstants
{
    // x = Viewport TopLeftX
    // y = Viewport TopLeftY
    // z = Viewport Width
    // w = Viewport Height
    FVector4 ViewportRect;

    // x = Screen Width (전체 렌더 타겟 너비)
    // y = Screen Height (전체 렌더 타겟 높이)
    // z = 1.0f / Screen Width
    // w = 1.0f / Screen Height
    FVector4 ScreenSize;
};

struct CameraBufferType // b7
{
    FVector CameraPosition;
    float Padding;
};

// b11: 타일 기반 라이트 컬링 상수 버퍼
struct FTileCullingBufferType
{
    uint32 TileSize;          // 타일 크기 (픽셀, 기본 16)
    uint32 TileCountX;        // 가로 타일 개수
    uint32 TileCountY;        // 세로 타일 개수
    uint32 bUseTileCulling;   // 타일 컬링 활성화 여부 (0=비활성화, 1=활성화)
    uint32 ViewportStartX;    // 뷰포트 시작 X 좌표
    uint32 ViewportStartY;    // 뷰포트 시작 Y 좌표
    uint32 Padding[2];
};

struct FPointLightShadowBufferType
{
    FMatrix LightViewProjection[6]; // 각 큐브맵 면에 대한 뷰-프로젝션 행렬 (6개)
    FVector LightPosition;          // 라이트의 월드 공간 위치
    float FarPlane;                 // 섀도우 맵의 원거리 평면 (깊이 범위 계산용)
    uint32 LightIndex;              // 현재 렌더링 중인 라이트 인덱스
    FVector Padding;                // 16바이트 정렬
};

struct FSkinningBuffer
{
	FMatrix SkinningMatrices[256];
};

// b6: SubUV 파티클 애니메이션 파라미터 (Sprite Particle용)
struct FParticleSubUVBufferType
{
    int32 SubImages_Horizontal;   // 가로 타일 개수
    int32 SubImages_Vertical;     // 세로 타일 개수
    int32 bInterpolateUV;         // UV 보간 사용 여부 (RandomBlend)
    int32 Padding;                // 정렬을 위한 패딩
};

// b9: Sky Sphere 상수 버퍼
struct alignas(16) FSkyConstantBuffer
{
    FLinearColor ZenithColor;       // 천정(상단) 색상 (16 bytes)
    FLinearColor HorizonColor;      // 수평선 색상 (16 bytes)
    FLinearColor GroundColor;       // 지면(하단) 색상 (16 bytes)

    FVector SunDirection;           // 태양 방향 (정규화됨, 월드 공간)
    float SunDiskSize;              // 태양 원반 크기 (0.0 ~ 1.0) (16 bytes)

    FLinearColor SunColor;          // 태양 색상 + 강도 (RGB + Intensity in A) (16 bytes)

    float HorizonFalloff;           // 수평선 그라디언트 감쇠 (1.0 ~ 10.0)
    float SunHeight;                // 태양 높이 (0.0 = 수평선, 1.0 = 천정)
    float OverallBrightness;        // 전체 밝기 스케일
    float CloudOpacity;             // 구름 불투명도 (미래 확장용) (16 bytes)

    // 기본값 설정
    FSkyConstantBuffer()
        : ZenithColor(0.0343f, 0.1236f, 0.4f, 1.0f)
        , HorizonColor(0.6471f, 0.8235f, 0.9451f, 1.0f)
        , GroundColor(0.3f, 0.25f, 0.2f, 1.0f)
        , SunDirection(0.0f, 0.5f, 0.866f)
        , SunDiskSize(0.001f)
        , SunColor(1.0f, 0.95f, 0.8f, 5.0f)
        , HorizonFalloff(3.0f)
        , SunHeight(0.5f)
        , OverallBrightness(1.0f)
        , CloudOpacity(0.0f)
    {
    }
};

#define CONSTANT_BUFFER_INFO(TYPE, SLOT, VS, PS) \
constexpr uint32 TYPE##Slot = SLOT;\
constexpr bool TYPE##IsVS = VS;\
constexpr bool TYPE##IsPS = PS;

//매크로를 인자로 받고 그 매크로 함수에 버퍼 전달
#define CONSTANT_BUFFER_LIST_SMALL(MACRO) \
MACRO(ModelBufferType)              \
MACRO(DecalBufferType)              \
MACRO(FireballBufferType)           \
MACRO(PostProcessBufferType)        \
MACRO(FogBufferType)                \
MACRO(FFadeInOutBufferType)         \
MACRO(FGammaCorrectionBufferType)   \
MACRO(FVinetteBufferType)           \
MACRO(FXAABufferType)               \
MACRO(FDepthOfFieldBufferType)      \
MACRO(FPixelConstBufferType)        \
MACRO(ViewProjBufferType)           \
MACRO(ColorBufferType)              \
MACRO(CameraBufferType)             \
MACRO(FLightBufferType)             \
MACRO(FViewportConstants)           \
MACRO(FTileCullingBufferType)       \
MACRO(FPointLightShadowBufferType)  \
MACRO(FParticleSubUVBufferType)     \
MACRO(FSkyConstantBuffer)

// 2. void*로만 전달해야 하는 큰 버퍼들
#define CONSTANT_BUFFER_LIST_LARGE(MACRO) \
MACRO(FSkinningBuffer)

// 3. (생성/해제용) 전체 리스트
#define CONSTANT_BUFFER_LIST(MACRO) \
CONSTANT_BUFFER_LIST_SMALL(MACRO) \
CONSTANT_BUFFER_LIST_LARGE(MACRO)

// 16 바이트 패딩 어썰트
#define STATIC_ASSERT_CBUFFER_ALIGNMENT(Type) \
    static_assert(sizeof(Type) % 16 == 0, "[ " #Type " ] Bad Size. Needs 16-Byte Padding.");
CONSTANT_BUFFER_LIST(STATIC_ASSERT_CBUFFER_ALIGNMENT)

//VS, PS 세팅은 함수 파라미터로 결정하게 하는게 훨씬 나을듯 나중에 수정 필요
//그리고 UV Scroll 상수버퍼도 처리해줘야함
CONSTANT_BUFFER_INFO(ModelBufferType, 0, true, false)
CONSTANT_BUFFER_INFO(PostProcessBufferType, 0, false, true)
CONSTANT_BUFFER_INFO(ViewProjBufferType, 1, true, true) // b1 카메라 행렬 고정
CONSTANT_BUFFER_INFO(FogBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FFadeInOutBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FGammaCorrectionBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FVinetteBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FXAABufferType, 2, false, true)
CONSTANT_BUFFER_INFO(FDepthOfFieldBufferType, 2, false, true)
CONSTANT_BUFFER_INFO(ColorBufferType, 3, true, true)   // b3 color
CONSTANT_BUFFER_INFO(FPixelConstBufferType, 4, true, true) // GOURAUD에도 사용되므로 VS도 true
CONSTANT_BUFFER_INFO(FSkinningBuffer, 5, true, false) // b5, VS Only (GPU Skinning)
CONSTANT_BUFFER_INFO(DecalBufferType, 6, true, true)
CONSTANT_BUFFER_INFO(FireballBufferType, 6, false, true)
CONSTANT_BUFFER_INFO(CameraBufferType, 7, true, true)  // b7, VS+PS (UberLit.hlsl과 일치)
CONSTANT_BUFFER_INFO(FLightBufferType, 8, true, true)
CONSTANT_BUFFER_INFO(FViewportConstants, 10, true, true)   // 뷰 포트 크기에 따라 전체 화면 복사를 보정하기 위해 설정 (10번 고유번호로 사용)
CONSTANT_BUFFER_INFO(FTileCullingBufferType, 11, false, true)  // b11, PS only (UberLit.hlsl과 일치)
CONSTANT_BUFFER_INFO(FPointLightShadowBufferType, 12, true, true)  // b12, VS+PS
CONSTANT_BUFFER_INFO(FParticleSubUVBufferType, 6, false, true)  // b6, PS only (Sprite Particle SubUV 파라미터)
CONSTANT_BUFFER_INFO(FSkyConstantBuffer, 9, false, true)  // b9, PS only (Sky Sphere 파라미터)





