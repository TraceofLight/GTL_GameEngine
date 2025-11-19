// b0: ModelBuffer (VS) - ModelBufferType과 정확히 일치 (128 bytes)
cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix; // 64 bytes
    row_major float4x4 WorldInverseTranspose; // 64 bytes - 올바른 노멀 변환을 위함
};

// b1: ViewProjBuffer (VS) - ViewProjBufferType과 일치
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseViewMatrix;   // 0행 광원의 월드 좌표 + 스포트라이트 반경
    row_major float4x4 InverseProjectionMatrix;
};

cbuffer SkinningBuffer : register(b5)
{
	row_major float4x4 SkinningMatrices[256];
};

// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
	float4 Color : COLOR;
#if GPU_SKINNING
	uint4 BlendIndices : BLENDINDICES;
	float4 BlendWeights : BLENDWEIGHTS;
#endif
};

// 출력은 오직 클립 공간 위치만 필요
struct VS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
};

VS_OUT mainVS(VS_INPUT Input)
{
#if GPU_SKINNING
	float3 BlendPosition = float3(0, 0, 0);
	float3 BlendNormal = float3(0, 0, 0);
	float3 BlendTangent = float3(0, 0, 0);

	for (int i = 0; i < 4; ++i)
	{
		uint Idx = Input.BlendIndices[i];
		float Weight = Input.BlendWeights[i];

		BlendPosition += mul(float4(Input.Position, 1.0f), SkinningMatrices[Idx]) * Weight;
		// @TODO - InverseTranspose
		BlendNormal += mul(Input.Normal, (float3x3) SkinningMatrices[Idx]) * Weight;
		BlendTangent += mul(Input.Tangent.xyz, (float3x3) SkinningMatrices[Idx]) * Weight;
	}

	Input.Position = BlendPosition;
	Input.Normal = normalize(BlendNormal);
	Input.Tangent.xyz = normalize(BlendTangent);
#endif
    VS_OUT Output = (VS_OUT) 0;

    // 모델 좌표 -> 월드 좌표 -> 뷰 좌표 -> 클립 좌표
    float4 WorldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    float4 ViewPos = mul(WorldPos, ViewMatrix);
    Output.Position = mul(ViewPos, ProjectionMatrix);
    Output.WorldPosition = WorldPos.xyz;

    return Output;
}
