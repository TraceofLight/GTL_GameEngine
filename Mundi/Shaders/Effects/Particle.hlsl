//================================================================================================
// Filename:      Decal.hlsl
// Description:   Decal projection shader with lighting support
//                Supports GOURAUD, LAMBERT, PHONG lighting models
//================================================================================================

// --- 조명 모델 선택 ---
// ViewMode에서 동적으로 설정됨 (SceneRenderer::RenderDecalPass)
// 가능한 매크로:
// - LIGHTING_MODEL_GOURAUD
// - LIGHTING_MODEL_LAMBERT
// - LIGHTING_MODEL_PHONG
// - (매크로 없음 = Unlit)

// --- 공통 조명 시스템 include ---
#include "../Common/LightStructures.hlsl"
#include "../Common/LightingBuffers.hlsl"
#include "../Common/LightingCommon.hlsl"

// --- Decal 전용 상수 버퍼 ---
cbuffer ModelBuffer : register(b0)
{
	row_major float4x4 WorldMatrix;
	row_major float4x4 WorldInverseTranspose;
}

cbuffer ViewProjBuffer : register(b1)
{
	row_major float4x4 ViewMatrix;
	row_major float4x4 ProjectionMatrix;
	row_major float4x4 InverseViewMatrix;
	row_major float4x4 InverseProjectionMatrix;
}

//cbuffer DecalBuffer : register(b6)
//{
//	row_major float4x4 DecalMatrix;
//	float DecalOpacity;
//}

// --- 텍스처 리소스 ---
//Texture2D g_DecalTexColor : register(t0);
Texture2D g_SpriteTexture : register(t0);
TextureCubeArray g_ShadowAtlasCube : register(t8);
Texture2D g_ShadowAtlas2D : register(t9);
Texture2D<float2> g_VSMShadowAtlas : register(t10);
TextureCubeArray<float2> g_VSMShadowCube : register(t11);

SamplerState g_Sample : register(s0);
SamplerComparisonState g_ShadowSample : register(s2);
SamplerState g_VSMSampler : register(s3);

// --- 입출력 구조체 ---
struct VS_INPUT
{
	/** The position of the particle. */
	float3 Position : POSITION;
	/** The relative time of the particle. */
	float RelativeTime : RELATIVE_TIME;
	/** The previous position of the particle. */
	float3 OldPosition : OLD_POSITION;
	/** Value that remains constant over the lifetime of a particle. */
	float ParticleId : PARTICLE_ID;
	/** The size of the particle. */
	float2 Size : SIZE;
	/** The rotation of the particle. */
	float Rotation : ROTATION;
	/** The sub-image index for the particle. */
	float SubImageIndex : SUB_IMAGE_INDEX;
	/** The color of the particle. */
	float4 Color : COLOR;

	float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float2 Texcoord : TEXCOORD0;

#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    float3 WorldPos : POSITION1;    // 조명 계산용
    float3 Normal : NORMAL0;        // 조명 계산용

	#ifdef LIGHTING_MODEL_GOURAUD
			float4 LitColor : COLOR0;   // Pre-calculated lighting (Gouraud)
	#endif
#endif
};

void GetTangents(float3 TranslatedWorldPosition, float3 OldTranslatedWorldPosition, float SpriteRotation, out float3 OutRight, out float3 OutUp)
{
	// Select camera up/right vectors.
	float3 CameraRight = InverseViewMatrix[1].xyz;
	float3 CameraUp = InverseViewMatrix[2].xyz;

	// Determine the vector from the particle to the camera and the particle's movement direction.
	float3 CameraDirection = normalize(InverseViewMatrix[3].xyz - TranslatedWorldPosition);
	float3 RightVector = CameraRight.xyz;
	float3 UpVector = CameraUp.xyz;

	//float4 LocalTangentSelector = SpriteVF.TangentSelector;

	//BRANCH

	//if (SpriteVF.CameraFacingBlend.x > 0.f)
	//{
	//	// Blend between PSA_FacingCamera and PSA_Square over distance
	//	float CameraDistanceSq = GetDistanceToCameraFromViewVectorSqr(ResolvedView.TranslatedWorldCameraOrigin - TranslatedWorldPosition);
	//	float AlignmentMode = saturate(CameraDistanceSq * SpriteVF.CameraFacingBlend.y - SpriteVF.CameraFacingBlend.z);

	//	float3 CameraFacingRight = SafeNormalize(cross(CameraDirection, float3(0, 0, 1)));
	//	float3 CameraFacingUp = cross(CameraDirection, CameraFacingRight);

	//	RightVector = normalize(lerp(RightVector, CameraFacingRight, AlignmentMode));
	//	UpVector = normalize(lerp(UpVector, CameraFacingUp, AlignmentMode));
	//}
	//else
	//{
	//	FLATTEN

	//	if (LocalTangentSelector.y > 0)
	//	{
	//		// Tangent vectors for PSA_Velocity.
	//		float3 ParticleDirection = SafeNormalize(TranslatedWorldPosition - OldTranslatedWorldPosition);
	//		RightVector = SafeNormalize(cross(CameraDirection, ParticleDirection));
	//		UpVector = -ParticleDirection;
	//	}
	//	else if (LocalTangentSelector.z > 0)
	//	{
	//		// Tangent vectors for rotation locked about an axis.
	//		RightVector = SpriteVF.AxisLockRight.xyz;
	//		UpVector = -SafeNormalize(cross(RightVector, CameraDirection));
	//	}
	//	else if (LocalTangentSelector.w > 0)
	//	{
	//		// Tangent vectors for camera facing position.
	//		RightVector = SafeNormalize(cross(CameraDirection, float3(0, 0, 1)));
	//		UpVector = cross(CameraDirection, RightVector);
	//	}
	//}

	// Determine the angle of rotation.
	float SinRotation; // = 0
	float CosRotation; // = 1
	sincos(SpriteRotation, SinRotation, CosRotation);

	// Rotate the sprite to determine final tangents.
	OutRight = SinRotation * UpVector + CosRotation * RightVector;
	OutUp = CosRotation * UpVector - SinRotation * RightVector;
}

void ComputeBillboardUVs(VS_INPUT Input, out float2 UVForPosition, out float2 UVForTexturing, out float2 UVForTexturingUnflipped)
{
	// Encoding the UV flip in the sign of the size data.
	float2 UVFlip = Input.Size;

	UVForTexturing.x = UVFlip.x < 0.0 ? 1.0 - Input.TexCoord.x : Input.TexCoord.x;
	UVForTexturing.y = UVFlip.y < 0.0 ? 1.0 - Input.TexCoord.y : Input.TexCoord.y;

	// Note: not inverting positions, as that would change the winding order
	UVForPosition = UVForTexturingUnflipped = Input.TexCoord.xy;
}


//================================================================================================
// 버텍스 셰이더
//================================================================================================
PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

    // World position
	float4 WorldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);

	float3 ParticleTranslatedWorldPosition = mul(float4(Input.Position, 1.0f), WorldMatrix).xyz;
	float3 ParticleOldTranslatedWorldPosition = mul(float4(Input.OldPosition, 1.0f), WorldMatrix).xyz;
	
	const float SpriteRotation = Input.Rotation;
	
	// Tangents.
	float3 Right, Up;
	GetTangents(ParticleTranslatedWorldPosition, ParticleOldTranslatedWorldPosition, SpriteRotation, Right, Up);


	float2 UVForPosition;
	float2 UVForTexturing;
	float2 UVForTexturingUnflipped;
	ComputeBillboardUVs(Input, UVForPosition, UVForTexturing, UVForTexturingUnflipped);

	// Vertex position.
	float4 VertexWorldPosition = float4(ParticleTranslatedWorldPosition, 1);
	float2 Size = abs(Input.Size);
	float2 PivotOffset = float2(-0.5f, -0.5f); // Center pivot
	VertexWorldPosition += Size.x * (UVForPosition.x + PivotOffset.x) * float4(Right, 0);
	VertexWorldPosition += Size.y * (UVForPosition.y + PivotOffset.y) * float4(Up, 0) * -1.0f; // Invert Y. dx11에서는 좌측 상단이 (0,0)이므로.
	
	//Intermediates.TangentToLocal = CalcTangentBasis(Intermediates);

	float4 ViewPos = mul(VertexWorldPosition, ViewMatrix);
	// Screen position
	float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
	Output.Position = mul(VertexWorldPosition, VP);
	Output.Texcoord = UVForTexturing;
	Output.Color = Input.Color;
	
	// 조명 관련
#if defined(LIGHTING_MODEL_GOURAUD) || defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // 조명 계산을 위한 데이터
    Output.WorldPos = VertexWorldPosition.xyz;
	float3 CameraDirection = normalize(InverseViewMatrix[3].xyz - TranslatedWorldPosition);
    Output.Normal = CameraDirection;

#ifdef LIGHTING_MODEL_GOURAUD
        // Gouraud: Vertex shader에서 조명 계산
        // Note: 여기서는 texture를 샘플링할 수 없으므로 white를 base color로 사용
        // Pixel shader에서 texture를 곱함
        float4 BaseColor = float4(1, 1, 1, 1);
        float3 ViewDir = CameraDirection;
        float SpecPower = 32.0f;

        float3 LitColor = CalculateAllLights(
            Output.WorldPos,
            ViewPos.xyz,
            Output.Normal,
            ViewDir,
            BaseColor,
            SpecPower,
            Output.Position,
            g_ShadowSample,
            g_ShadowAtlas2D,
            g_ShadowAtlasCube,
            g_VSMSampler,
            g_VSMShadowAtlas,
            g_VSMShadowCube
        );

        Output.LitColor = float4(LitColor, 1.0f);
#endif
#endif

	return Output;
}

//================================================================================================
// 픽셀 셰이더
//================================================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float4 SpriteTextureColor = g_SpriteTexture.Sample(g_Sample, Uv);

    // 3. 조명 계산 (매크로에 따라)
#ifdef LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 계산한 조명 결과 사용
    float4 FinalColor = Input.LitColor;
    FinalColor *= SpriteTextureColor;  // Texture modulation
    return FinalColor;

#elif defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_PHONG)
    // Lambert/Phong: PS에서 조명 계산
    float3 Normal = normalize(Input.Normal);
    float4 BaseColor = SpriteTextureColor;
    float SpecPower = 32.0f;
    float4 ViewPos = mul(float4(Input.WorldPos, 1.0f), ViewMatrix);

#ifdef LIGHTING_MODEL_PHONG
        float3 ViewDir = normalize(CameraPosition - Input.WorldPos);
#else
        float3 ViewDir = float3(0, 0, 0);  // Lambert는 사용 안 함
#endif

    float3 LitColor = CalculateAllLights(
        Input.WorldPos,
        ViewPos.xyz,
        Normal,
        ViewDir,
        BaseColor,
        SpecPower,
        Input.Position,
        g_ShadowSample,
        g_ShadowAtlas2D,
        g_ShadowAtlasCube,
        g_VSMSampler,
        g_VSMShadowAtlas,
        g_VSMShadowCube
    );

    float4 FinalColor = LitColor;
    return FinalColor;

#else
    // No lighting model - 기존 방식 (단순 텍스처)
	float4 FinalColor = SpriteTextureColor;
	return FinalColor;
#endif
}
