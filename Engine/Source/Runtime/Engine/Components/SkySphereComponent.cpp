#include "pch.h"
#include "SkySphereComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "MeshBatchElement.h"
#include "SceneView.h"
#include "World.h"
#include "DirectionalLightComponent.h"
#include "LightManager.h"
#include "ObjectFactory.h"
#include "VertexData.h"

IMPLEMENT_CLASS(USkySphereComponent)

USkySphereComponent::USkySphereComponent()
{
	// Sky는 컬링되지 않아야 함
	bIsCulled = false;
}

USkySphereComponent::~USkySphereComponent()
{
	// SphereMesh는 ResourceManager가 관리하므로 여기서 해제하지 않음
	SphereMesh = nullptr;
	SkyShader = nullptr;
}

void USkySphereComponent::BeginPlay()
{
	Super::BeginPlay();

	// 셰이더 로드 및 메시 생성 (아직 안 됐으면)
	EnsureResourcesLoaded();
}

void USkySphereComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	// DirectionalLight에서 태양 방향 자동 업데이트
	if (bAutoUpdateSunDirection)
	{
		UpdateSunDirectionFromLight();
	}

	// UPROPERTY 값을 렌더링용 상수 버퍼로 동기화
	SyncToConstantBuffer();
}

FAABB USkySphereComponent::GetWorldAABB() const
{
	// Sky Sphere는 무한히 큰 경계를 가짐 (항상 보임)
	const float HugeSize = 1e10f;
	return FAABB(FVector(-HugeSize), FVector(HugeSize));
}

void USkySphereComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
	// 리소스 로드 확인
	EnsureResourcesLoaded();
	if (!SphereMesh || !SkyShader)
	{
		return;
	}

	// 셰이더 variant 가져오기
	FShaderVariant* ShaderVariant = SkyShader->GetOrCompileShaderVariant(View->ViewShaderMacros);
	if (!ShaderVariant)
	{
		return;
	}

	// 카메라 위치에 Sky Sphere 배치 (항상 카메라를 따라감)
	FVector CameraPos = View->ViewLocation;
	FMatrix SkyWorldMatrix = FMatrix::MakeScale(SphereRadius) * FMatrix::MakeTranslation(CameraPos);

	// FMeshBatchElement 생성
	FMeshBatchElement Batch;
	Batch.VertexShader = ShaderVariant->VertexShader;
	Batch.PixelShader = ShaderVariant->PixelShader;
	Batch.InputLayout = ShaderVariant->InputLayout;
	Batch.Material = nullptr;  // Sky는 별도 머티리얼 시스템 사용 안 함
	Batch.VertexBuffer = SphereMesh->GetVertexBuffer();
	Batch.IndexBuffer = SphereMesh->GetIndexBuffer();
	Batch.VertexStride = SphereMesh->GetVertexStride();
	Batch.IndexCount = SphereMesh->GetIndexCount();
	Batch.StartIndex = 0;
	Batch.BaseVertexIndex = 0;
	Batch.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Batch.WorldMatrix = SkyWorldMatrix;
	Batch.ObjectID = 0;  // Sky는 피킹 대상 아님
	Batch.InstanceColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// Sky 전용 데이터
	Batch.bIsSky = true;
	Batch.SkyParams = &SkyParams;

	OutMeshBatchElements.Add(Batch);
}

// UPROPERTY 값을 SkyParams로 동기화
void USkySphereComponent::SyncToConstantBuffer()
{
	SkyParams.ZenithColor = ZenithColor;
	SkyParams.HorizonColor = HorizonColor;
	SkyParams.GroundColor = GroundColor;
	SkyParams.SunDirection = SunDirection.GetNormalized();
	SkyParams.SunDiskSize = SunDiskSize;
	SkyParams.SunColor = SunColor;
	SkyParams.HorizonFalloff = HorizonFalloff;
	SkyParams.OverallBrightness = OverallBrightness;
	SkyParams.SunHeight = std::max(0.0f, SunDirection.Z);
}

// 리소스 로드 보장 (lazy loading)
void USkySphereComponent::EnsureResourcesLoaded()
{
	// 이미 로드된 경우 스킵
	if (SphereMesh && SkyShader)
	{
		return;
	}

	// 셰이더 로드
	if (!SkyShader)
	{
		LoadShader();
	}

	// Sphere 메시 생성/로드
	if (!SphereMesh)
	{
		CreateSphereMesh();
	}
}

// Sphere 메시 생성
void USkySphereComponent::CreateSphereMesh()
{
	auto& RM = UResourceManager::GetInstance();

	// 먼저 기존 sphere 메시가 있는지 확인
	SphereMesh = RM.Get<UStaticMesh>("SkySphere");
	if (SphereMesh)
	{
		return;
	}

	// Inside-Facing Sphere 메시를 프로시저럴하게 생성
	const int32 NumSegments = 32;  // 가로 분할
	const int32 NumRings = 16;     // 세로 분할
	const float Radius = 1.0f;     // 단위 구체 (스케일로 조정)

	FMeshData MeshData;

	// 버텍스 생성
	for (int32 Ring = 0; Ring <= NumRings; ++Ring)
	{
		const float Phi = PI * static_cast<float>(Ring) / static_cast<float>(NumRings);
		const float SinPhi = sinf(Phi);
		const float CosPhi = cosf(Phi);

		for (int32 Segment = 0; Segment <= NumSegments; ++Segment)
		{
			const float Theta = 2.0f * PI * static_cast<float>(Segment) / static_cast<float>(NumSegments);
			const float SinTheta = sinf(Theta);
			const float CosTheta = cosf(Theta);

			// 위치 (단위 구체)
			FVector Position;
			Position.X = Radius * SinPhi * CosTheta;
			Position.Y = Radius * SinPhi * SinTheta;
			Position.Z = Radius * CosPhi;

			// 노멀 (Inside-Facing이므로 안쪽을 향함)
			FVector Normal = -Position.GetNormalized();

			// UV
			FVector2D UV;
			UV.X = static_cast<float>(Segment) / static_cast<float>(NumSegments);
			UV.Y = static_cast<float>(Ring) / static_cast<float>(NumRings);

			// 색상 (기본 흰색)
			FVector4 Color(1.0f, 1.0f, 1.0f, 1.0f);

			MeshData.Vertices.push_back(Position);
			MeshData.Normal.push_back(Normal);
			MeshData.UV.push_back(UV);
			MeshData.Color.push_back(Color);
		}
	}

	// 인덱스 생성 (Inside-Facing이므로 역순으로 감기)
	for (int32 Ring = 0; Ring < NumRings; ++Ring)
	{
		for (int32 Segment = 0; Segment < NumSegments; ++Segment)
		{
			const uint32 Current = Ring * (NumSegments + 1) + Segment;
			const uint32 Next = Current + NumSegments + 1;

			// 첫 번째 삼각형 (CW → CCW로 역순)
			MeshData.Indices.push_back(Current);
			MeshData.Indices.push_back(Current + 1);
			MeshData.Indices.push_back(Next);

			// 두 번째 삼각형 (CW → CCW로 역순)
			MeshData.Indices.push_back(Next);
			MeshData.Indices.push_back(Current + 1);
			MeshData.Indices.push_back(Next + 1);
		}
	}

	// 메시 생성 및 등록
	SphereMesh = ObjectFactory::NewObject<UStaticMesh>();
	SphereMesh->Load(&MeshData, RM.GetDevice(), EVertexLayoutType::PositionColorTexturNormal);

	// ResourceManager에 등록 (캐싱)
	RM.Add<UStaticMesh>("SkySphere", SphereMesh);

	UE_LOG("SkySphereComponent: Created procedural sky sphere mesh (Vertices: %d, Indices: %d)",
	       static_cast<int32>(MeshData.Vertices.size()),
	       static_cast<int32>(MeshData.Indices.size()));
}

// 셰이더 로드
void USkySphereComponent::LoadShader()
{
	auto& RM = UResourceManager::GetInstance();

	SkyShader = RM.Load<UShader>("Shaders/Sky/Sky.hlsl");
	if (!SkyShader)
	{
		UE_LOG("SkySphereComponent: Failed to load Sky shader!");
	}
}

// DirectionalLight에서 태양 방향 업데이트
void USkySphereComponent::UpdateSunDirectionFromLight()
{
	UWorld* WorldPtr = GetWorld();
	if (!WorldPtr)
	{
		return;
	}

	// LightManager에서 DirectionalLight 찾기
	FLightManager* LightManager = WorldPtr->GetLightManager();
	if (!LightManager)
	{
		return;
	}

	TArray<UDirectionalLightComponent*> DirLights = LightManager->GetDirectionalLightList();
	if (DirLights.IsEmpty())
	{
		return;
	}

	// 첫 번째 DirectionalLight의 방향 사용
	UDirectionalLightComponent* MainLight = DirLights[0];
	if (MainLight)
	{
		// 태양 방향은 빛이 오는 방향의 반대 (빛이 비추는 방향)
		FVector LightDir = MainLight->GetLightDirection();
		SunDirection = -LightDir;
	}
}
