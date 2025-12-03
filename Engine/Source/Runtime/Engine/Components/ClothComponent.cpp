#include "pch.h"
#include "ClothComponent.h"

#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

// NvCloth core and PhysX foundation types
#include "NvCloth/Factory.h"
#include "NvCloth/Solver.h"
#include "NvCloth/Fabric.h"
#include "NvCloth/Cloth.h"
#include "NvClothExt/ClothFabricCooker.h"

#include "foundation/PxAssert.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
#include "PhysicsCooking.h"
#include "RenderManager.h"
#include "SceneView.h"
#include "MeshBatchElement.h"
#include "Material.h"
#include "Shader.h"
#include "ResourceManager.h"

#include "ClothManager.h"

using namespace nv::cloth;

UClothComponent::UClothComponent()
{
    bCanEverTick = true;
	bClothEnabled = true;
	bClothInitialized = false;
}

UClothComponent::~UClothComponent()
{
    ReleaseCloth();
}

void UClothComponent::InitializeComponent()
{
    Super::InitializeComponent();

    // VertexBuffer 생성
    if (SkeletalMesh && !VertexBuffer)
    {
        SkeletalMesh->CreateVertexBufferForComp(&VertexBuffer);
    }

    if (bClothEnabled && !bClothInitialized)
    {
        SetupClothFromMesh();
    }

    // Cloth 시뮬레이션은 스키닝을 대체하므로 스키닝 비활성화
    bSkinningMatricesDirty = false;

	bClothInitialized = true;
}

void UClothComponent::BeginPlay()
{
    Super::BeginPlay();

    // 초기화 직후 최초로 VertexBuffer에 반영
    if (bClothInitialized && VertexBuffer && SkeletalMesh)
    {
        UpdateVerticesFromCloth();
    }
}

void UClothComponent::EndPlay()
{
    Super::EndPlay();

    // PIE 종료 시 원본 상태 복구
    if (bHasSavedOriginalState)
    {
        RestoreOriginalState();
    }
}

void UClothComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

	if (IsPendingDestroy() || !IsRegistered())
	{
		return;
	}

	// 에디터 모드에서는 시뮬레이션 하지 않음 (PIE에서만 실행)
	if (GetWorld() && !GetWorld()->bPie)
	{
		return;
	}

    if (!bClothEnabled)
    {
		UE_LOG("[ClothComponent] Cloth is disabled\n");
        return;
    }

    if (!bClothInitialized)
    {
		UE_LOG("[ClothComponent] Cloth is not initialized\n");
		InitializeComponent();
    }

    // Paint된 weight가 있으면 시뮬레이션 전에 적용
    if (bClothWeightsDirty)
    {
        ApplyPaintedWeights();
    }

    UpdateClothSimulation(DeltaTime);
}

void UClothComponent::OnCreatePhysicsState()
{
    Super::OnCreatePhysicsState();

    if (!bClothInitialized && bClothEnabled)
    {
        SetupClothFromMesh();
    }
}

void UClothComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
    {
        return;
    }

    if (!bClothEnabled || !bClothInitialized)
    {
        // Cloth가 비활성화된 경우 아무것도 렌더링하지 않음
		InitializeComponent();
    }

    // Cloth Section만 렌더링
    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();

    auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
    {
        UMaterialInterface* Material = GetMaterial(SectionIndex);
        UShader* Shader = nullptr;

        if (Material && Material->GetShader())
        {
            Shader = Material->GetShader();
        }
        else
        {
            Material = UResourceManager::GetInstance().GetDefaultMaterial();
            if (Material)
            {
                Shader = Material->GetShader();
            }
            if (!Material || !Shader)
            {
                UE_LOG("UClothComponent: 기본 머티리얼이 없습니다.");
                return { nullptr, nullptr };
            }
        }
        return { Material, Shader };
    };

    for (uint32 SectionIndex = 0; SectionIndex < MeshGroupInfos.size(); ++SectionIndex)
    {
        const FGroupInfo& Group = MeshGroupInfos[SectionIndex];

        // Cloth 섹션만 렌더링
        if (!Group.bEnableCloth)
        {
            continue;
        }

        if (Group.IndexCount == 0)
        {
            continue;
        }

        auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
        if (!MaterialToUse || !ShaderToUse)
        {
            continue;
        }

        FMeshBatchElement BatchElement;
        TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
        if (0 < MaterialToUse->GetShaderMacros().Num())
        {
            ShaderMacros.Append(MaterialToUse->GetShaderMacros());
        }

        // Cloth는 CPU 변형만 사용 (GPU 스키닝 비활성화)
        FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

        if (ShaderVariant)
        {
            BatchElement.VertexShader = ShaderVariant->VertexShader;
            BatchElement.PixelShader = ShaderVariant->PixelShader;
            BatchElement.InputLayout = ShaderVariant->InputLayout;
        }

        BatchElement.Material = MaterialToUse;

        // Cloth 전용 VertexBuffer 사용 (시뮬레이션 결과)
        BatchElement.VertexBuffer = VertexBuffer;
        BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();
        BatchElement.VertexStride = sizeof(FVertexDynamic);

        BatchElement.IndexCount = Group.IndexCount;
        BatchElement.StartIndex = Group.StartIndex;
        BatchElement.BaseVertexIndex = 0;
        BatchElement.WorldMatrix = GetWorldMatrix();
        BatchElement.ObjectID = InternalIndex;
        BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        // Cloth 플래그 설정
        BatchElement.bClothEnabled = false;
        BatchElement.ClothMode = 0; // ABSOLUTE (disabled)
        BatchElement.ClothBaseVertexIndex = 0;
        BatchElement.ClothSRV = nullptr;

        OutMeshBatchElements.Add(BatchElement);
    }
}

void UClothComponent::SetupClothFromMesh()
{

	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
	{
		UE_LOG("[Cloth Error]: No SkeletalMesh or SkeletalMeshData");
		return;
	}

	// 이미 초기화된 경우 기존 리소스 release부터 수행
	if (bClothInitialized)
	{
		ReleaseCloth();
	}

	BuildClothMesh();           // Extract vertices + indices
	CreateClothFabric();        // Cook -> Fabric
	CreateClothInstance();      // Create nv::cloth::Cloth instance
	CreatePhaseConfig();

	//만든 cloth를 solver에 전달;
	FClothManager::GetInstance().AddClothToSolver(cloth);

	ApplyClothProperties();
	ApplyTetherConstraint();

	// PIE 모드에서 시뮬레이션 시작 전 원본 상태 저장
	if (GetWorld() && GetWorld()->bPie && !bHasSavedOriginalState)
	{
		SaveOriginalState();
	}

 }

void UClothComponent::ReleaseCloth()
{
	// 1. Cloth를 Solver에서 제거 (삭제 전 필수)
	if (cloth)
	{
		FClothManager::GetInstance().GetSolver()->removeCloth(cloth);
	}

	// 2. Cloth 삭제
	if (cloth)
	{
		UE_LOG("[ClothComponent] Deleting cloth\n");
		NV_CLOTH_DELETE(cloth);
		cloth = nullptr;
	}
	// 3. Phases 삭제
	if (phases)
	{
		UE_LOG("[ClothComponent] Deleting phases\n");
		delete[] phases;
		phases = nullptr;
	}
	// 5. Fabric 해제
	if (fabric)
	{
		UE_LOG("[ClothComponent] Releasing fabric\n");
		fabric->decRefCount();
		fabric = nullptr;
	}
}

// 제약 조건 업데이트
void UClothComponent::UpdateMotionConstraints()
{
	if (!cloth)
		return;

	Range<physx::PxVec4> motionConstraints = cloth->getMotionConstraints();

	for (uint32 i = 0; i < static_cast<uint32>(MotionConstraints.Num()) && i < static_cast<uint32>(motionConstraints.size()); ++i)
	{
		motionConstraints[i] = MotionConstraints[i];
	}
}


void UClothComponent::ClearMotionConstraints()
{
	if (cloth)
	{
		cloth->clearMotionConstraints();
	}

	MotionConstraints.Empty();
}

void UClothComponent::CreateOrResizeClothGPUBuffer(uint32 Float3Count)
{
	if (ClothGPUBuffer && ClothGPUBufferSize == Float3Count) return;

	ReleaseClothGPUBuffer();

	D3D11_BUFFER_DESC desc{};
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(float) * 3;
	desc.ByteWidth = Float3Count * desc.StructureByteStride;

	ID3D11Device* Device = URenderManager::GetInstance().GetRenderer()->GetRHIDevice()->GetDevice();
	HRESULT hr = Device->CreateBuffer(&desc, nullptr, &ClothGPUBuffer);
	if (FAILED(hr)) { UE_LOG("[Cloth] CreateBuffer failed"); return; }

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // structured
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = Float3Count;

	hr = Device->CreateShaderResourceView(ClothGPUBuffer, &srvDesc, &ClothGPUSRV);
	if (FAILED(hr)) { UE_LOG("[Cloth] Create SRV failed"); ReleaseClothGPUBuffer(); return; }

	ClothGPUBufferSize = Float3Count;
}

void UClothComponent::UpdateClothGPUBufferFromParticles()
{
	if (PreviousParticles.Num() == 0) return;

	// Previous Particle로부터 Buffer 생성
	const uint32 Count = (uint32)PreviousParticles.Num();
	CreateOrResizeClothGPUBuffer(Count);
	if (!ClothGPUBuffer) return;

	ID3D11DeviceContext* DeviceContext = URenderManager::GetInstance().GetRenderer()->GetRHIDevice()->GetDeviceContext();

	// Shader로 전달할 위치 데이터 갱신
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(DeviceContext->Map(ClothGPUBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		struct float3 { float x, y, z; };
		auto* dst = reinterpret_cast<float3*>(mapped.pData);
		for (uint32 i = 0; i < Count; ++i)
		{
			const physx::PxVec4& p = PreviousParticles[i];
			dst[i] = { p.x, p.y, p.z };
		}
		DeviceContext->Unmap(ClothGPUBuffer, 0);
	}

}

void UClothComponent::ReleaseClothGPUBuffer()
{
	if (ClothGPUSRV) { ClothGPUSRV->Release(); ClothGPUSRV = nullptr; }
	if (ClothGPUBuffer) { ClothGPUBuffer->Release(); ClothGPUBuffer = nullptr; }
	ClothGPUBufferSize = 0;
}

void UClothComponent::SetWindVelocity(const FVector& Velocity)
{
	if (cloth)
	{
		cloth->setWindVelocity(PxVec3(Velocity.X, Velocity.Y, Velocity.Z));
	}
}

void UClothComponent::SetWindParams(float Drag, float Lift)
{
	if (cloth)
	{
		cloth->setDragCoefficient(Drag);
		cloth->setLiftCoefficient(Lift);
	}
}


//void UClothComponent::AddTriangleMeshCollision()
//{
//	//PxVec3* triangles = ...;
//	//Range<const PxVec3> triangleR(triangles, triangles + triangleCount * 3);
//	//cloth->setTriangles(triangleR, 0, cloth->getNumTriangles());
//}

// cloth 1개당 구 32개까지만 가능
void UClothComponent::AddCollisionSphere(const FVector& Center, float Radius)
{
	if (!cloth)
		return;

	physx::PxVec4 sphere(Center.X, Center.Y, Center.Z, Radius);
	CollisionSpheres.Add(sphere);

	Range<const physx::PxVec4> sphereRange(CollisionSpheres.GetData(), CollisionSpheres.GetData() + CollisionSpheres.Num());
	cloth->setSpheres(sphereRange, 0, cloth->getNumSpheres());
}

void UClothComponent::AddCollisionCapsule(const FVector& Start, const FVector& End, float Radius)
{
	if (!cloth)
		return;

	// 양쪽 끝에 구 가능
	physx::PxVec4 sphere1(Start.X, Start.Y, Start.Z, Radius);
	physx::PxVec4 sphere2(End.X, End.Y, End.Z, Radius);

	int32 startIdx = CollisionSpheres.Num();
	CollisionSpheres.Add(sphere1);
	CollisionSpheres.Add(sphere2);

	// 구 반영
	Range<const physx::PxVec4> sphereRange(CollisionSpheres.GetData(), CollisionSpheres.GetData() + CollisionSpheres.Num());
	cloth->setSpheres(sphereRange, 0, CollisionSpheres.Num());

	// 캡슐 인덱스 설정 (구 0과 1을 연결해야 함)
	CollisionCapsules.Add(startIdx);
	CollisionCapsules.Add(startIdx + 1);

	cloth->setCapsules(Range<const uint32_t>(CollisionCapsules.GetData(), CollisionCapsules.GetData() + CollisionCapsules.Num()),
		0, CollisionCapsules.Num() / 2);

}

void UClothComponent::AddCollisionPlane(const FVector& Point, const FVector& Normal)
{
	if (!cloth)
		return;

	float d = - FVector::Dot(Normal, Point);
	physx::PxVec4 plane(Normal.X, Normal.Y, Normal.Z, d);

	CollisionPlanes.Add(plane);

	nv::cloth::Range<const physx::PxVec4> planesR(CollisionPlanes.GetData(), CollisionPlanes.GetData() + CollisionPlanes.Num());
	cloth->setPlanes(planesR, 0, CollisionPlanes.Num());

	// solver에서 plane을 convex shape으로써 처리하기 때문에, 각 plane은 convex mask로 함.
	// indices[i] = 1 << i; plane i 하나만 포함된 bit mask
	uint32_t convexMask = 1 << (CollisionPlanes.Num() - 1);
	CollisionConvexes.Add(convexMask);

	cloth->setConvexes(Range<const uint32_t>(CollisionConvexes.GetData(), CollisionConvexes.GetData() + CollisionConvexes.Num()),
		0, CollisionConvexes.Num());
}

void UClothComponent::ClearCollisionShapes()
{
	CollisionSpheres.Empty();
	CollisionCapsules.Empty();
	CollisionPlanes.Empty();
	CollisionConvexes.Empty();
	CollisionTriangles.Empty();

	if (cloth)
	{
		cloth->setSpheres(Range<const physx::PxVec4>(), 0, 0);
		cloth->setCapsules(Range<const uint32_t>(), 0, 0);
		cloth->setPlanes(Range<const physx::PxVec4>(), 0, 0);
		cloth->setConvexes(Range<const uint32_t>(), 0, 0);
	}
}

void UClothComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// NvCloth 리소스들은 복사하지 않음 (InitializeComponent에서 재생성)
	// fabric, cloth, phases는 포인터이므로 nullptr로 초기화
	fabric = nullptr;
	cloth = nullptr;
	phases = nullptr;

	// 초기화 플래그를 false로 설정하여 InitializeComponent에서 재생성되도록
	bClothInitialized = false;

	// GPU 버퍼도 재생성 필요
	ClothGPUBuffer = nullptr;
	ClothGPUSRV = nullptr;
	ClothGPUBufferSize = 0;
}

void UClothComponent::InitializeNvCloth()
{
	// NvCloth 라이브러리 전역 초기화 (한 번만 수행)
	//if (!g_bNvClothInitialized)
	//{
	//	nv::cloth::InitializeNvCloth(&g_ClothAllocator, &g_ClothErrorCallback, &g_ClothAssertHandler, nullptr);
	//	g_bNvClothInitialized = true;
	//	UE_LOG("[NvCloth] Initialized successfully\n");
	//}

}

void UClothComponent::CreateClothFabric()
{
	ClothMeshDesc meshDesc;
	meshDesc.setToDefault();

	meshDesc.points.data = ClothParticles.GetData();
	meshDesc.points.stride = sizeof(physx::PxVec4);
	meshDesc.points.count = ClothParticles.Num();

	meshDesc.triangles.data = ClothIndices.GetData();
	meshDesc.triangles.stride = sizeof(uint32) * 3;
	meshDesc.triangles.count = ClothIndices.Num() / 3;

	physx::PxVec3 gravity(0, 0, -981.0f); // cm/s^2

	fabric = NvClothCookFabricFromMesh(FClothManager::GetInstance().GetFactory(), meshDesc, gravity, &phaseTypeInfo, false);

	if (!fabric)
	{
		UE_LOG("Failed to cook cloth fabric!");
	}
}

void UClothComponent::CreateClothInstance()
{
	if (!FClothManager::GetInstance().GetFactory() || !fabric)
		return;

	cloth = FClothManager::GetInstance().GetFactory()->createCloth(
		nv::cloth::Range<physx::PxVec4>(ClothParticles.GetData(), ClothParticles.GetData() + ClothParticles.Num()),
		*fabric
	);

	if (!cloth)
	{
		UE_LOG("Failed to create cloth instance!");
	}
}

void UClothComponent::CreatePhaseConfig()
{
	if (!fabric || !cloth)
		return;

	int32 numPhases = fabric->getNumPhases();
	phases = new PhaseConfig[numPhases];

	for (int i = 0; i < numPhases; ++i)
	{
		phases[i].mPhaseIndex = i;

		// Phase 타입에 따라 제약 조건 반영
		switch (phaseTypeInfo[i])
		{
		case nv::cloth::ClothFabricPhaseType::eINVALID:
			//UE_LOG(LogTemp, Error, TEXT("Invalid phase type!"));
			break;
		case nv::cloth::ClothFabricPhaseType::eVERTICAL:
		case nv::cloth::ClothFabricPhaseType::eHORIZONTAL:
			phases[i].mStiffness = ClothSettings.StretchStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.StretchStiffnessMultiplier;
			phases[i].mCompressionLimit = ClothSettings.CompressionLimit;
			phases[i].mStretchLimit = ClothSettings.StretchLimit;
			break;
		case nv::cloth::ClothFabricPhaseType::eBENDING:
			phases[i].mStiffness = ClothSettings.BendStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.BendStiffnessMultiplier;
			phases[i].mCompressionLimit = 1.0f;
			phases[i].mStretchLimit = 1.0f;
			break;
		case nv::cloth::ClothFabricPhaseType::eSHEARING:
			phases[i].mStiffness = ClothSettings.ShearStiffness;
			phases[i].mStiffnessMultiplier = ClothSettings.ShearStiffnessMultiplier;
			phases[i].mCompressionLimit = 1.0f;
			phases[i].mStretchLimit = 1.0f;
			break;
		}
	}

	cloth->setPhaseConfig(nv::cloth::Range<nv::cloth::PhaseConfig>(phases, phases + numPhases));

}

void UClothComponent::RetrievingSimulateResult()
{
	if (!cloth)
		return;

	nv::cloth::MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	// 결과를 복사해옴
	PreviousParticles.SetNum(particles.size());
	for (uint32 i = 0; i < static_cast<uint32>(particles.size()); ++i)
	{
		//do something with particles[i]
		//the xyz components are the current positions
		//the w component is the invMass.
		PreviousParticles[i] = particles[i];
	}

    // Upload to GPU buffer for absolute cloth positions
    UpdateClothGPUBufferFromParticles();
    //destructor of particles should be called before mCloth is destroyed.
}

void UClothComponent::ApplyClothProperties()
{
	if (!cloth)
		return;

	// 중력 설정 반영
	if (ClothSettings.bUseGravity)
	{
		cloth->setGravity(physx::PxVec3(
			ClothSettings.GravityOverride.X,
			ClothSettings.GravityOverride.Y,
			ClothSettings.GravityOverride.Z
		));
	}

	// 감쇠 설정 반영
	cloth->setDamping(physx::PxVec3(
		ClothSettings.Damping,
		ClothSettings.Damping,
		ClothSettings.Damping
	));

	cloth->setFriction(0.5f);

	// 선형/각속도 저항
	cloth->setLinearDrag(physx::PxVec3(
		ClothSettings.LinearDrag,
		ClothSettings.LinearDrag,
		ClothSettings.LinearDrag
	));

	cloth->setAngularDrag(physx::PxVec3(
		ClothSettings.AngularDrag,
		ClothSettings.AngularDrag,
		ClothSettings.AngularDrag
	));

	// Solver 주파수 설정 반영
	//cloth->setSolverFrequency(120.0f);
	cloth->setSolverFrequency(60.0f); //default is 300  게임 fps 보다 낮게 설정하면 시각적으로 어색함


	// 바람 설정 반영
	cloth->setWindVelocity(physx::PxVec3(
		ClothSettings.WindVelocity.X,
		ClothSettings.WindVelocity.Y,
		ClothSettings.WindVelocity.Z
	));
	cloth->setDragCoefficient(ClothSettings.WindDrag);
	cloth->setLiftCoefficient(ClothSettings.WindLift);
}

void UClothComponent::ApplyTetherConstraint()
{
	if (!cloth || !ClothSettings.bUseTethers)
		return;

	cloth->setTetherConstraintScale(ClothSettings.TetherScale);
	cloth->setTetherConstraintStiffness(ClothSettings.TetherStiffness);
	//cloth->setTetherConstraintStiffness(0.0f); // 비활성화
	//cloth->setTetherConstraintStiffness(1.0f); // 완전 고정
}

void UClothComponent::AttachingClothToCharacter()
{
	if (!cloth || AttachmentVertices.Num() == 0)
		return;

	// MappedRange 를 통해서 현재 정점 데이터를 직접 갱신
	MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	for (int32 i = 0; i < AttachmentVertices.Num(); ++i)
	{
		int32 vertexIndex = AttachmentVertices[i];
		if (vertexIndex >= 0 && static_cast<uint32>(vertexIndex) < static_cast<uint32>(particles.size()))
		{
			FVector attachPos = GetAttachmentPosition(i);

			// 고정점으로 매 프레임마다 세팅, w는 0으로 설정
			particles[vertexIndex] = physx::PxVec4(attachPos.X, attachPos.Y, attachPos.Z, 0.0f);
		}
	}
}
FVector UClothComponent::GetAttachmentPosition(int32 AttachmentIndex)
{
	if (AttachmentIndex < 0 || AttachmentIndex >= AttachmentBoneNames.Num())
		return FVector::Zero();

	FName boneName = AttachmentBoneNames[AttachmentIndex];
	int32 boneIndex = GetBoneIndex(boneName);

	if (boneIndex != INDEX_NONE)
	{
		FTransform boneTransform = GetBoneTransform(boneIndex);
		FVector offset = AttachmentOffsets.IsValidIndex(AttachmentIndex) ? AttachmentOffsets[AttachmentIndex] : FVector::Zero();
		return boneTransform.TransformPosition(offset);
	}

	return FVector::Zero();
}


void UClothComponent::SetupCapeAttachment()
{
	// 망토 상단 정점들을 캐릭터의 어깨/등 본에 부착
	AttachmentVertices.Empty();
	AttachmentBoneNames.Empty();
	AttachmentOffsets.Empty();

	// 망토 상단 위치를 정렬된 어깨/등 본에 부착
	// 첫 번째 줄 정점들을 위치를 정렬된 어깨/등 본에
	int32 verticesPerRow = 10; // 망토 가로 정점 수

	for (int32 i = 0; i < verticesPerRow; ++i)
	{
		AttachmentVertices.Add(i); // 첫 번째 줄 정점 인덱스들
		AttachmentBoneNames.Add(FName("Spine3")); // 동일 본

		// 오프셋을 좌우로 퍼뜨리기
		float offsetX = (i - verticesPerRow / 2.0f) * 5.0f; // 5cm 간격
		AttachmentOffsets.Add(FVector(0, offsetX, 10.0f)); // 약간 위
	}

	// 해당 정점 0으로 설정해야 고정된 정점으로
	for (int32 vertIdx : AttachmentVertices)
	{
		if (vertIdx >= 0 && vertIdx < ClothParticles.Num())
		{
			ClothParticles[vertIdx].w = 0.0f; // inverse mass = 0 (고정점)
		}
	}
}

void UClothComponent::AddBodyCollision()
{
	// 캐릭터 몸체 구조에 따라 충돌 구 또는 캡슐 추가 (망토가 몸을 뚫지 않게)

	// 골반 구 추가
	AddCollisionSphere(GetBoneLocation(FName("Pelvis")), 25.0f);

	// 척추 캡슐
	AddCollisionCapsule(
		GetBoneLocation(FName("Spine1")),
		GetBoneLocation(FName("Spine2")),
		20.0f
	);

	AddCollisionCapsule(
		GetBoneLocation(FName("Spine2")),
		GetBoneLocation(FName("Spine3")),
		20.0f
	);

	// 어깨 구 추가들
	AddCollisionSphere(GetBoneLocation(FName("LeftShoulder")), 15.0f);
	AddCollisionSphere(GetBoneLocation(FName("RightShoulder")), 15.0f);

	// 팔 캡슐들
	AddCollisionCapsule(
		GetBoneLocation(FName("LeftArm")),
		GetBoneLocation(FName("LeftForeArm")),
		10.0f
	);

	AddCollisionCapsule(
		GetBoneLocation(FName("RightArm")),
		GetBoneLocation(FName("RightForeArm")),
		10.0f
	);

	// 다리 캡슐들
	AddCollisionCapsule(
		GetBoneLocation(FName("LeftUpLeg")),
		GetBoneLocation(FName("LeftLeg")),
		12.0f
	);

	AddCollisionCapsule(
		GetBoneLocation(FName("RightUpLeg")),
		GetBoneLocation(FName("RightLeg")),
		12.0f
	);
}

int32 UClothComponent::GetBoneIndex(const FName& BoneName) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
		return INDEX_NONE;

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	auto It = Skeleton->BoneNameToIndex.find(BoneName.ToString());
	if (It != Skeleton->BoneNameToIndex.end())
		return It->second;

	return INDEX_NONE;
}

FTransform UClothComponent::GetBoneTransform(int32 BoneIndex) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
		return FTransform();

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	if (BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
		return FTransform();

	// BindPose로부터 Transform 생성
	const FBone& Bone = Skeleton->Bones[BoneIndex];
	FTransform BoneTransform(Bone.BindPose);

	// 컴포넌트의 월드 공간 변환과 결합
	FTransform ComponentTransform = GetWorldTransform();
	return ComponentTransform.GetWorldTransform(BoneTransform);
}

FVector UClothComponent::GetBoneLocation(const FName& BoneName)
{
	int32 boneIndex = GetBoneIndex(BoneName);
	if (boneIndex != INDEX_NONE)
	{
		return GetBoneTransform(boneIndex).GetLocation();
	}
	return FVector::Zero();
}

void UClothComponent::UpdateClothSimulation(float DeltaTime)
{
	if (!cloth)
	{
		UE_LOG("[ClothComponent] UpdateClothSimulation: cloth is null\n");
		return;
	}

	static int frameCount = 0;
	if (frameCount++ % 60 == 0)  // 1초마다 로그
	{
		UE_LOG("[ClothComponent] UpdateClothSimulation running (frame %d)\n", frameCount);
	}

	// 캐릭터와 부착된 정점 갱신
	AttachingClothToCharacter();

	//// 시뮬레이션 실행 (ClothManager를 통해)
	//FClothManager::GetInstance().ClothSimulation(DeltaTime);

	// 결과 가져오기
	RetrievingSimulateResult();

	// 렌더링 정점 갱신
	UpdateVerticesFromCloth();

	// 디버그용 particle 위치 시각화
	// Debug draw cloth particles as spheres (PVD doesn't show NvCloth)
	URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
	if (!Renderer)
	{
		static bool warned = false;
		if (!warned)
		{
			UE_LOG("[ClothComponent] WARNING: Renderer is null, cannot draw debug spheres\n");
			warned = true;
		}
		return;
	}

	static int debugFrameCount = 0;
	if (debugFrameCount++ % 60 == 0)
	{
		UE_LOG("[ClothComponent] Drawing debug spheres for %d particles\n", PreviousParticles.Num());
	}

	if (Renderer)
	{
		const int NumSegments = 8;
		const float Radius = 2.0f;  // 2cm 크기 구
		TArray<FVector> StartPoints;
		TArray<FVector> EndPoints;
		TArray<FVector4> Colors;

		// 모든 데이터를 사전에 예약
		StartPoints.Reserve(PreviousParticles.Num() * NumSegments * 3);
		EndPoints.Reserve(PreviousParticles.Num() * NumSegments * 3);
		Colors.Reserve(PreviousParticles.Num() * NumSegments * 3);

		for (int32 i = 0; i < PreviousParticles.Num(); ++i)
		{
			const physx::PxVec4& p = PreviousParticles[i];
			const FVector Center(p.x, p.y, p.z);
			const bool bFixed = (p.w == 0.0f);

			// 고정된 정점은 빨강, 일반 정점은 초록
			const FVector4 Col = bFixed ? FVector4(1.0f, 0.0f, 0.0f, 1.0f) : FVector4(0.0f, 1.0f, 0.0f, 1.0f);

			// 3개의 원으로 구를 근사
			// XY 평면 원 (Z 고정)
			for (int s = 0; s < NumSegments; ++s)
			{
				const float a0 = (static_cast<float>(s) / NumSegments) * TWO_PI;
				const float a1 = (static_cast<float>((s + 1) % NumSegments) / NumSegments) * TWO_PI;
				const FVector p0 = Center + FVector(Radius * std::cos(a0), Radius * std::sin(a0), 0.0f);
				const FVector p1 = Center + FVector(Radius * std::cos(a1), Radius * std::sin(a1), 0.0f);
				StartPoints.Add(p0);
				EndPoints.Add(p1);
				Colors.Add(Col);
			}

			// XZ 평면 원 (Y 고정)
			for (int s = 0; s < NumSegments; ++s)
			{
				const float a0 = (static_cast<float>(s) / NumSegments) * TWO_PI;
				const float a1 = (static_cast<float>((s + 1) % NumSegments) / NumSegments) * TWO_PI;
				const FVector p0 = Center + FVector(Radius * std::cos(a0), 0.0f, Radius * std::sin(a0));
				const FVector p1 = Center + FVector(Radius * std::cos(a1), 0.0f, Radius * std::sin(a1));
				StartPoints.Add(p0);
				EndPoints.Add(p1);
				Colors.Add(Col);
			}

			// YZ 평면 원 (X 고정)
			for (int s = 0; s < NumSegments; ++s)
			{
				const float a0 = (static_cast<float>(s) / NumSegments) * TWO_PI;
				const float a1 = (static_cast<float>((s + 1) % NumSegments) / NumSegments) * TWO_PI;
				const FVector p0 = Center + FVector(0.0f, Radius * std::cos(a0), Radius * std::sin(a0));
				const FVector p1 = Center + FVector(0.0f, Radius * std::cos(a1), Radius * std::sin(a1));
				StartPoints.Add(p0);
				EndPoints.Add(p1);
				Colors.Add(Col);
			}
		}

		// 한 번에 모든 라인 그리기
		if (StartPoints.Num() > 0)
		{
			Renderer->AddLines(StartPoints, EndPoints, Colors);
		}
	}
}

void UClothComponent::UpdateVerticesFromCloth()
{
	// 시뮬레이션 결과를 렌더링 정점 버퍼로 복사
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData() || PreviousParticles.Num() == 0)
		return;

	const auto& MeshData = SkeletalMesh->GetSkeletalMeshData();
	const auto& GroupInfos = MeshData->GroupInfos;

	// Cloth Section만 업데이트
	int32 ParticleIdx = 0;

	for (const auto& Group : GroupInfos)
	{
		if (!Group.bEnableCloth)
			continue;

		// 이 Section의 정점들만 업데이트
		UpdateSectionVertices(Group, ParticleIdx);
	}

	// 노멀 재계산
	RecalculateNormals();

	// VertexBuffer 갱신
	if (VertexBuffer)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
	}

	//// SkinnedVertices 크기 초기화
	//SkinnedVertices.SetNum(PreviousParticles.Num());

	//// 1. Cloth 시뮬레이션 결과를 SkinnedVertices에 복사
	//for (int32 i = 0; i < PreviousParticles.Num(); ++i)
	//{
	//	const physx::PxVec4& particle = PreviousParticles[i];
	//	const auto& originalVertex = originalVertices[i];

	//	SkinnedVertices[i].pos = FVector(particle.x, particle.y, particle.z);
	//	SkinnedVertices[i].tex = originalVertex.UV;
	//	SkinnedVertices[i].color = FVector4(1, 1, 1, 1);
	//	SkinnedVertices[i].Tangent = FVector4(originalVertex.Tangent.X, originalVertex.Tangent.Y, originalVertex.Tangent.Z, originalVertex.Tangent.W);

	//	// 노멀은 재계산할것이므로
	//	SkinnedVertices[i].normal = FVector(0, 0, 1);
	//}

	//// 2. 노멀 재계산 (면의 기준으로)
	//RecalculateNormals();

	//// 3. VertexBuffer 갱신
	//if (VertexBuffer)
	//{
	//	SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
	//}
}

void UClothComponent::UpdateSectionVertices(const FGroupInfo& Group, int32& ParticleIdx)
{
	// Section의 인덱스로부터 정점 추출
	const auto& AllIndices = SkeletalMesh->GetSkeletalMeshData()->Indices;
	const auto& AllVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
	const auto& OriginalVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;

	TSet<uint32> UsedVertices;
	SkinnedVertices.resize(AllVertices.size());
	for (uint32 i = 0; i < Group.IndexCount; ++i)
	{
		uint32 GlobalVertexIdx = AllIndices[Group.StartIndex + i];

		if (!UsedVertices.Contains(GlobalVertexIdx))
		{
			UsedVertices.Add(GlobalVertexIdx);

			// Cloth 시뮬레이션 결과를 SkinnedVertices에 반영
			const physx::PxVec4& Particle = PreviousParticles[ParticleIdx];
			const auto& OriginalVertex = OriginalVertices[GlobalVertexIdx];

			SkinnedVertices[GlobalVertexIdx].pos = FVector(Particle.x, Particle.y, Particle.z);
			SkinnedVertices[GlobalVertexIdx].tex = OriginalVertex.UV;
			SkinnedVertices[GlobalVertexIdx].Tangent = FVector4(OriginalVertex.Tangent.X, OriginalVertex.Tangent.Y, OriginalVertex.Tangent.Z, OriginalVertex.Tangent.W);
			SkinnedVertices[GlobalVertexIdx].color = FVector4(1, 1, 1, 1);
			SkinnedVertices[GlobalVertexIdx].normal = FVector(0, 0, 1);  // RecalculateNormals에서 재계산됨

			ParticleIdx++;
		}
	}
}



/*
void UClothComponent::UpdateVerticesFromCloth()
{
	// 시뮬레이션 결과를 렌더링 정점 버퍼로 복사
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData() || PreviousParticles.Num() == 0)
		return;

	const auto& originalVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
	if (PreviousParticles.Num() != originalVertices.Num())
		return;

	// SkinnedVertices 크기 초기화
	SkinnedVertices.SetNum(PreviousParticles.Num());

	// 1. Cloth 시뮬레이션 결과를 SkinnedVertices에 복사
	for (int32 i = 0; i < PreviousParticles.Num(); ++i)
	{
		const physx::PxVec4& particle = PreviousParticles[i];
		const auto& originalVertex = originalVertices[i];

		SkinnedVertices[i].pos = FVector(particle.x, particle.y, particle.z);
		SkinnedVertices[i].tex = originalVertex.UV;
		SkinnedVertices[i].color = FVector4(1, 1, 1, 1);
		SkinnedVertices[i].Tangent = FVector4(originalVertex.Tangent.X, originalVertex.Tangent.Y, originalVertex.Tangent.Z, originalVertex.Tangent.W);

		// 노멀은 재계산할것이므로
		SkinnedVertices[i].normal = FVector(0, 0, 1);
	}

	// 2. 노멀 재계산 (면의 기준으로)
	RecalculateNormals();

	// 3. VertexBuffer 갱신
	if (VertexBuffer)
	{
		SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
	}
}
*/
void UClothComponent::RecalculateNormals()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	const auto& indices = SkeletalMesh->GetSkeletalMeshData()->Indices;
	if (indices.Num() == 0 || indices.Num() % 3 != 0)
		return;

	// 모든 노멀을 0으로 초기화
	for (auto& vertex : SkinnedVertices)
	{
		vertex.normal = FVector::Zero();
	}

	// 각각의 면위치 노멀을 계산해서 정점에 누적
	for (int32 i = 0; i < indices.Num(); i += 3)
	{
		uint32 idx0 = indices[i];
		uint32 idx1 = indices[i + 1];
		uint32 idx2 = indices[i + 2];

		const uint32 SkinnedVerticesNum = static_cast<uint32>(SkinnedVertices.Num());
		if (idx0 >= SkinnedVerticesNum || idx1 >= SkinnedVerticesNum || idx2 >= SkinnedVerticesNum)
		{
			continue;
		}

		const FVector& v0 = SkinnedVertices[idx0].pos;
		const FVector& v1 = SkinnedVertices[idx1].pos;
		const FVector& v2 = SkinnedVertices[idx2].pos;

		// 면위치의 벡터
		FVector edge1 = v1 - v0;
		FVector edge2 = v2 - v0;

		// 외적으로 노멀 계산 (시계방향 전제)
		FVector faceNormal = FVector::Cross(edge1, edge2);

		// 각 정점에 노멀 누적
		SkinnedVertices[idx0].normal += faceNormal;
		SkinnedVertices[idx1].normal += faceNormal;
		SkinnedVertices[idx2].normal += faceNormal;
	}

	// 누적된 노멀을 정규화
	for (auto& vertex : SkinnedVertices)
	{
		vertex.normal.Normalize();
	}
}

void UClothComponent::SaveOriginalState()
{
	if (!cloth || ClothParticles.Num() == 0)
	{
		UE_LOG("[ClothComponent] SaveOriginalState: Cannot save, cloth not initialized\n");
		return;
	}

	// 현재 ClothParticles의 초기 상태를 저장
	CacheOriginalParticles = ClothParticles;
	bHasSavedOriginalState = true;

}

void UClothComponent::RestoreOriginalState()
{
	if (!bHasSavedOriginalState || CacheOriginalParticles.Num() == 0)
	{
		UE_LOG("[ClothComponent] RestoreOriginalState: No saved state to restore\n");
		return;
	}

	if (!cloth)
	{
		UE_LOG("[ClothComponent] RestoreOriginalState: Cloth not initialized\n");
		return;
	}

	// 저장된 원본 위치로 복구
	ClothParticles = CacheOriginalParticles;
	PreviousParticles = CacheOriginalParticles;

	// Cloth 인스턴스에도 반영
	nv::cloth::MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();
	for (uint32 i = 0; i < static_cast<uint32>(CacheOriginalParticles.Num()) && i < static_cast<uint32>(particles.size()); ++i)
	{
		particles[i] = CacheOriginalParticles[i];
	}

	// VertexBuffer 업데이트
	UpdateVerticesFromCloth();


	// 복구 후 플래그 리셋
	bHasSavedOriginalState = false;
	CacheOriginalParticles.Empty();
}

void UClothComponent::BuildClothMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;


	// 원본 메시의 모든 정점 가져옴
	const auto& meshData = SkeletalMesh->GetSkeletalMeshData();
	const auto& allVertices = meshData->Vertices;
	const auto& allIndices = meshData->Indices;
	const auto& groupInfos = meshData->GroupInfos;

	ClothParticles.Empty();
	ClothIndices.Empty();
	ClothVertexToMeshVertex.Empty();
	ClothVertexWeights.Empty();

	// Cloth Section만 추출
	for (const auto& group : groupInfos)
	{
		//이미 fbx 파서에서 cloth는 true로 처리해줌
		if (!group.bEnableCloth)
			continue;
		ExtractClothSectionOrdered(group, allVertices, allIndices);
	}

	// 결과를 이전 데이터로 초기화
	PreviousParticles = ClothParticles;
}

void UClothComponent::ExtractClothSection(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices)
{
	// Section에서 인덱스 추출
	TArray<uint32> SectionIndices;
	for (uint32 i = 0; i < Group.IndexCount; ++i)
	{
		uint32 GlobalIndex = AllIndices[Group.StartIndex + i];
		SectionIndices.Add(GlobalIndex);
	}

	// 사용된 정점들만 추출 (중복제거)
	TSet<uint32> UsedVertices;
	for (uint32 idx : SectionIndices)
	{
		UsedVertices.Add(idx);
	}

	// 정점 인덱스 재매핑(global-> local)
	TMap<uint32, uint32> GlobalToLocal;
	int32 LocalIndex = 0;

	for (uint32 GlobalIdx : UsedVertices)
	{
		GlobalToLocal.Add(GlobalIdx, LocalIndex);

		const auto& Vertex = AllVertices[GlobalIdx];

		// 고정 정점 판별 (상단 정점)
		float invMass = 1.0f; //ShouldFixVertex(Vertex) ? 0.0f : 1.0f;

		ClothParticles.Add(physx::PxVec4(
			Vertex.Position.X,
			Vertex.Position.Y,
			Vertex.Position.Z,
			invMass
		));

		LocalIndex++;
	}

	// 인덱스를 local 인덱스로 변환
	for (uint32 GlobalIdx : SectionIndices)
	{
		uint32 LocalIdx = GlobalToLocal[GlobalIdx];
		ClothIndices.Add(LocalIdx);
	}
}

bool UClothComponent::ShouldFixVertex(const FSkinnedVertex& Vertex)
{
    return false;
}

void UClothComponent::ExtractClothSectionOrdered(const FGroupInfo& Group, const TArray<FSkinnedVertex>& AllVertices, const TArray<uint32>& AllIndices)
{
    // 1) Collect section indices (keep original order)
    TArray<uint32> SectionIndices;
    SectionIndices.Reserve(Group.IndexCount);
    for (uint32 i = 0; i < Group.IndexCount; ++i)
    {
        const uint32 GlobalIndex = AllIndices[Group.StartIndex + i];
        SectionIndices.Add(GlobalIndex);
    }

    // 2) Build ordered-unique vertex list by first appearance and a Global->Local map
    TArray<uint32> OrderedUniqueGlobals;
    OrderedUniqueGlobals.Reserve(SectionIndices.Num());
    TMap<uint32, uint32> GlobalToLocal;

    for (uint32 GlobalIdx : SectionIndices)
    {
        if (!GlobalToLocal.Contains(GlobalIdx))
        {
            const uint32 NewLocal = (uint32)OrderedUniqueGlobals.Num();
            GlobalToLocal.Add(GlobalIdx, NewLocal);
            OrderedUniqueGlobals.Add(GlobalIdx);
        }
    }

    // 3) Append particles in the same ordered-unique order for stable indexing
    for (uint32 GlobalIdx : OrderedUniqueGlobals)
    {
        const auto& Vertex = AllVertices[GlobalIdx];
        const float invMass = 1.0f; //ShouldFixVertex(Vertex) ? 0.0f : 1.0f;
        ClothParticles.Add(physx::PxVec4(
            Vertex.Position.X,
            Vertex.Position.Y,
            Vertex.Position.Z,
            invMass
        ));
    }

    // 4) Remap section indices to local (preserve triangle winding)
    for (uint32 GlobalIdx : SectionIndices)
    {
        const uint32 LocalIdx = GlobalToLocal[GlobalIdx];
        ClothIndices.Add(LocalIdx);
    }

    // 5) Build ClothVertexToMeshVertex mapping for Paint feature
    for (uint32 GlobalIdx : OrderedUniqueGlobals)
    {
        ClothVertexToMeshVertex.Add(GlobalIdx);
    }
}
//void UClothComponent::BuildClothMesh()
//{
//	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
//		return;
//
//	// 원본 메시의 모든 정점 가져옴
//	const auto& vertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
//	const auto& indices = SkeletalMesh->GetSkeletalMeshData()->Indices;
//
//	ClothParticles.Empty();
//	ClothIndices.Empty();
//
//	// 1. 메시의 Y 오프셋을 찾아서 (상단 정점 고정을 위함)
//	float maxY = -FLT_MAX;
//	float minY = FLT_MAX;
//	for (const auto& vertex : vertices)
//	{
//		maxY = std::max(maxY, vertex.Position.Y);
//		minY = std::min(minY, vertex.Position.Y);
//	}
//
//	// 상단 일정 비율을 고정시킬 Threshold로 설정 (설정값에 따라)
//	float fixedThreshold = maxY - (maxY - minY) * ClothSettings.FixedVertexRatio;
//
//	// 2. 정점 가져와서 복사 (PxVec4: xyz = position, w = inverse mass)
//	for (const auto& vertex : vertices)
//	{
//		// 상단 정점들은 고정 (invMass = 0)
//		float invMass = (vertex.Position.Y >= fixedThreshold) ? 0.0f : 1.0f;
//		ClothParticles.Add(physx::PxVec4(vertex.Position.X, vertex.Position.Y, vertex.Position.Z, invMass));
//	}
//
//	// 인덱스 복사
//	for (uint32 idx : indices)
//	{
//		ClothIndices.Add(idx);
//	}
//
//	// 결과를 이전 데이터로 초기화
//	PreviousParticles = ClothParticles;
//}

// ========== Cloth Paint API Implementation ==========

FVector UClothComponent::GetClothVertexPosition(int32 ClothVertexIndex) const
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothParticles.Num())
	{
		return FVector::Zero();
	}
	const physx::PxVec4& P = ClothParticles[ClothVertexIndex];
	return FVector(P.x, P.y, P.z);
}

float UClothComponent::GetVertexWeight(int32 ClothVertexIndex) const
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothVertexWeights.Num())
	{
		// Weight 배열이 초기화되지 않은 경우, ClothParticles의 invMass에서 가져옴
		if (ClothVertexIndex >= 0 && ClothVertexIndex < ClothParticles.Num())
		{
			return ClothParticles[ClothVertexIndex].w;  // invMass (0=fixed, 1=free)
		}
		return 1.0f;  // 기본값: 자유롭게 움직임
	}
	return ClothVertexWeights[ClothVertexIndex];
}

void UClothComponent::SetVertexWeight(int32 ClothVertexIndex, float Weight)
{
	if (ClothVertexIndex < 0 || ClothVertexIndex >= ClothParticles.Num())
	{
		return;
	}

	// Weight 배열이 초기화되지 않았으면 초기화
	if (ClothVertexWeights.Num() != ClothParticles.Num())
	{
		InitializeVertexWeights();
	}

	// Clamp weight to [0, 1]
	Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
	ClothVertexWeights[ClothVertexIndex] = Weight;
	bClothWeightsDirty = true;
}

void UClothComponent::SetVertexWeightByMeshVertex(uint32 MeshVertexIndex, float Weight)
{
	int32 ClothIdx = FindClothVertexByMeshVertex(MeshVertexIndex);
	if (ClothIdx != INDEX_NONE)
	{
		SetVertexWeight(ClothIdx, Weight);
	}
}

int32 UClothComponent::FindClothVertexByMeshVertex(uint32 MeshVertexIndex) const
{
	for (int32 i = 0; i < ClothVertexToMeshVertex.Num(); ++i)
	{
		if (ClothVertexToMeshVertex[i] == MeshVertexIndex)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UClothComponent::InitializeVertexWeights()
{
	const int32 NumParticles = ClothParticles.Num();
	ClothVertexWeights.SetNum(NumParticles);

	// ClothParticles의 현재 invMass 값으로 초기화
	for (int32 i = 0; i < NumParticles; ++i)
	{
		ClothVertexWeights[i] = ClothParticles[i].w;  // invMass (0=fixed, 1=free)
	}

	bClothWeightsDirty = false;
}

void UClothComponent::ApplyPaintedWeights()
{
	if (!bClothWeightsDirty || ClothVertexWeights.Num() == 0)
	{
		return;
	}

	// ClothParticles의 invMass(w) 값을 Paint된 weight로 업데이트
	for (int32 i = 0; i < ClothParticles.Num() && i < ClothVertexWeights.Num(); ++i)
	{
		ClothParticles[i].w = ClothVertexWeights[i];
	}

	// Cloth 인스턴스가 있으면 그곳에도 반영
	if (cloth)
	{
		nv::cloth::MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();
		for (int32 i = 0; i < ClothVertexWeights.Num() && i < (int32)particles.size(); ++i)
		{
			particles[i].w = ClothVertexWeights[i];
		}
	}

	// PreviousParticles에도 반영
	for (int32 i = 0; i < PreviousParticles.Num() && i < ClothVertexWeights.Num(); ++i)
	{
		PreviousParticles[i].w = ClothVertexWeights[i];
	}

	bClothWeightsDirty = false;
	UE_LOG("[ClothComponent] Applied painted weights to %d vertices\n", ClothVertexWeights.Num());
}
