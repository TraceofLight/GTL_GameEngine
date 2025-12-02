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

// Cloth control (VS b6, SRV t8)
cbuffer ClothCB : register(b6)
{
    uint ClothEnabled;
    uint ClothMode;         // 0: ABSOLUTE, 1: DELTA
    uint BaseVertexIndex;
    uint _ClothPad;
};
StructuredBuffer<float3> gClothPosition : register(t8);

// --- 셰이더 입출력 구조체 ---
struct VS_INPUT
{
#if PARTICLE_MESH
	// Mesh particle uses standard vertex input + per-instance data
	float3 Position : POSITION;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent : TANGENT0;
	float4 Color : COLOR;
	
	// Per-instance data (slot 1) - 순서와 타입이 C++ Input Layout과 정확히 일치해야 함
	float4 InstanceColor : INSTANCE_COLOR; // offset 0:  DXGI_FORMAT_R32G32B32A32_FLOAT
	float4 InstanceTransform0 : INSTANCE_TRANSFORM; // offset 16: DXGI_FORMAT_R32G32B32A32_FLOAT (semantic index 0)
	float4 InstanceTransform1 : INSTANCE_TRANSFORM1; // offset 32: DXGI_FORMAT_R32G32B32A32_FLOAT (semantic index 1)
	float4 InstanceTransform2 : INSTANCE_TRANSFORM2; // offset 48: DXGI_FORMAT_R32G32B32A32_FLOAT (semantic index 2)
	float4 InstanceVelocity : INSTANCE_VELOCITY; // offset 64: DXGI_FORMAT_R32G32B32A32_FLOAT
	int4 InstanceSubUVParams : INSTANCE_SUBUVPARAMS; // offset 80: DXGI_FORMAT_R16G16B16A16_SINT
	float InstanceSubUVLerp : INSTANCE_SUBUVLERP; // offset 88: DXGI_FORMAT_R32_FLOAT
	float InstanceRelativeTime : INSTANCE_RELATIVETIME; // offset 92: DXGI_FORMAT_R32_FLOAT
#else
    float3 Position : POSITION;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent : TANGENT0;
	float4 Color : COLOR;
#if GPU_SKINNING
	uint4 BlendIndices : BLENDINDICES;
	float4 BlendWeights : BLENDWEIGHTS;
#endif
	
#endif
};

// 출력은 오직 클립 공간 위치만 필요
struct VS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
};

VS_OUT mainVS(VS_INPUT Input, uint VertexId : SV_VertexID)
{
	VS_OUT Output = (VS_OUT) 0;
	
#if PARTICLE_MESH
	// Mesh particle: Reconstruct world matrix from instance data
	row_major float4x4 InstanceWorldMatrix;
	InstanceWorldMatrix[0] = float4(Input.InstanceTransform0.xyz, 0);
	InstanceWorldMatrix[1] = float4(Input.InstanceTransform1.xyz, 0);
	InstanceWorldMatrix[2] = float4(Input.InstanceTransform2.xyz, 0);
	InstanceWorldMatrix[3] = float4(Input.InstanceTransform0.w, Input.InstanceTransform1.w, Input.InstanceTransform2.w, 1);
	
	// Transform position to world space using instance transform
	float4 worldPos = mul(float4(Input.Position, 1.0f), InstanceWorldMatrix);
	Output.WorldPosition = worldPos.xyz;
	
	// Transform to view and projection space
	float4 viewPos = mul(worldPos, ViewMatrix);
	Output.Position = mul(viewPos, ProjectionMatrix);
#else
	
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

    // Cloth absolute override (component space)
    if (ClothEnabled == 1 && ClothMode == 0)
    {
        uint vid = VertexId + BaseVertexIndex;
        float3 posComp = gClothPosition[vid];
        Input.Position = posComp;
    }

    // 모델 좌표 -> 월드 좌표 -> 뷰 좌표 -> 클립 좌표
    float4 WorldPos = mul(float4(Input.Position, 1.0f), WorldMatrix);
    float4 ViewPos = mul(WorldPos, ViewMatrix);
    Output.Position = mul(ViewPos, ProjectionMatrix);
    Output.WorldPosition = WorldPos.xyz;

#endif
    return Output;
}
