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

using namespace physx;
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

    if (bClothEnabled && !bClothInitialized)
    {
        SetupClothFromMesh();
    }
}

void UClothComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UClothComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!bClothEnabled || !bClothInitialized)
    {
        return;
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

void UClothComponent::SetupClothFromMesh()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	if (bClothInitialized)
		ReleaseCloth();

	InitializeNvCloth();
	BuildClothMesh();           // Extract vertices + indices
	CreateClothFabric();        // Cook -> Fabric
	CreateClothInstance();      // Create nv::cloth::Cloth instance
	CreatePhaseConfig();
	CreateSolver();
	ApplyClothProperties();
	ApplyTetherConstraint();

	bClothInitialized = (factory != nullptr && solver != nullptr && fabric != nullptr && cloth != nullptr);   
}

void UClothComponent::ReleaseCloth()
{
	if (!bClothInitialized)
		return;

	if (fabric)
	{
		fabric->decRefCount();
		fabric = nullptr;
	}

	if (cloth)
	{
		NV_CLOTH_DELETE(cloth);
		cloth = nullptr;
	}

	if (phases)
	{
		delete[] phases;
		phases = nullptr;
	}

	if (solver)
	{
		NV_CLOTH_DELETE(solver);
		solver = nullptr;
	} 

	if (factory)
	{
		NvClothDestroyFactory(factory);
		factory = nullptr;
	}
	 
	bClothInitialized = false; 
}

//?? 
void UClothComponent::UpdateMotionConstraints()
{
	if (!cloth)
		return;

	Range<physx::PxVec4> motionConstraints = cloth->getMotionConstraints();

	for (int i = 0; i < MotionConstraints.Num() && i < motionConstraints.size(); ++i)
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

// cloth 1개 당 최대 32개 까지 추가 가능
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

	// 두 개의 구를 추가
	physx::PxVec4 sphere1(Start.X, Start.Y, Start.Z, Radius);
	physx::PxVec4 sphere2(End.X, End.Y, End.Z, Radius);

	int32 startIdx = CollisionSpheres.Num();
	CollisionSpheres.Add(sphere1);
	CollisionSpheres.Add(sphere2);

	// 구 설정
	Range<const physx::PxVec4> sphereRange(CollisionSpheres.GetData(), CollisionSpheres.GetData() + CollisionSpheres.Num());
	cloth->setSpheres(sphereRange, 0, CollisionSpheres.Num());

	// 캡슐 인덱스 추가 (구 0과 1을 연결한다)
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

	// solver에서 plane이 convex shape의 일부라고 알려야지 충돌이 발생한다.
	// indices[i] = 1 << i; plane i 를 의미하는 bit mask
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

//void UClothComponent::AddTriangleMeshCollision()
//{
//	//PxVec3* triangles = ...;
//	//Range<const PxVec3> triangleR(triangles, triangles + triangleCount * 3);
//	//cloth->setTriangles(triangleR, 0, cloth->getNumTriangles());
//}

void UClothComponent::InitializeNvCloth()
{
	factory = NvClothCreateFactoryCPU();
	if (factory == nullptr)
	{
		//error
		//UE_LOG(LogTemp, Error, TEXT("Failed to create NvCloth factory!"));
	}
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

	fabric = NvClothCookFabricFromMesh(factory, meshDesc, gravity, &phaseTypeInfo, false);

	if (!fabric)
	{
		//UE_LOG(LogTemp, Error, TEXT("Failed to cook cloth fabric!"));
	}
} 

void UClothComponent::CreateClothInstance()
{
	if (!factory || !fabric)
		return;

	cloth = factory->createCloth(
		nv::cloth::Range<physx::PxVec4>(ClothParticles.GetData(), ClothParticles.GetData() + ClothParticles.Num()),
		*fabric
	);

	if (!cloth)
	{
		//UE_LOG(LogTemp, Error, TEXT("Failed to create cloth instance!"));
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

		// Phase 타입에 따라 다른 설정 적용
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

void UClothComponent::CreateSolver()
{
	if (!factory || !cloth)
		return;

	solver = factory->createSolver();

	if (solver)
	{
		solver->addCloth(cloth);
	}
	else
	{
		//UE_LOG(LogTemp, Error, TEXT("Failed to create cloth solver!"));
	}
}

void UClothComponent::SimulateCloth(float DeltaSeconds)
{
	if (!solver)
		return;

	solver->beginSimulation(DeltaSeconds);

	for (int i = 0; i < solver->getSimulationChunkCount(); ++i)
	{
		// multi thread로 호출가능
		solver->simulateChunk(i);
	}

	solver->endSimulation();
}

void UClothComponent::RetrievingSimulateResult()
{
	if (!cloth)
		return;

	nv::cloth::MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	// 이전 파티클 저장
	PreviousParticles.SetNum(particles.size());
	for (int i = 0; i < particles.size(); i++)
	{
		//do something with particles[i]
		//the xyz components are the current positions
		//the w component is the invMass.
		PreviousParticles[i] = particles[i];
	} 

	//destructor of particles should be called before mCloth is destroyed. 
}

void UClothComponent::ApplyClothProperties()
{
	if (!cloth)
		return;

	// 중력 설정
	if (ClothSettings.bUseGravity)
	{
		cloth->setGravity(physx::PxVec3(
			ClothSettings.GravityOverride.X,
			ClothSettings.GravityOverride.Y,
			ClothSettings.GravityOverride.Z
		));
	}

	// 감쇠 설정
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

	// Solver 주파수 설정
	//cloth->setSolverFrequency(120.0f); 
	cloth->setSolverFrequency(60.0f); //default is 300  목표 fps 배수로 설정하는게 일반적이다.

	// 바람 설정
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
	//cloth->setTetherConstraintStiffness(0.0f); // 비활성
	//cloth->setTetherConstraintStiffness(1.0f); // 기본값
}

void UClothComponent::AttachingClothToCharacter()
{
	if (!cloth || AttachmentVertices.Num() == 0)
		return;

	// MappedRange 스코프 내에서 파티클 업데이트
	MappedRange<physx::PxVec4> particles = cloth->getCurrentParticles();

	for (int i = 0; i < AttachmentVertices.Num(); ++i)
	{
		int32 vertexIndex = AttachmentVertices[i];
		if (vertexIndex >= 0 && vertexIndex < particles.size())
		{
			FVector attachPos = GetAttachmentPosition(i);

			// 고정할 부분이기 때문에 w를 0으로 설정
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
	// 망토 상단을 어깨/등 본에 부착하는 예제

	AttachmentVertices.Empty();
	AttachmentBoneNames.Empty();
	AttachmentOffsets.Empty();

	// 망토 상단 정점들을 찾아서 부착 설정
	// 예: 첫 번째 줄의 정점들을 등 본에 부착
	int32 verticesPerRow = 10; // 망토 너비 정점 수

	for (int32 i = 0; i < verticesPerRow; ++i)
	{
		AttachmentVertices.Add(i); // 첫 번째 줄 정점 인덱스
		AttachmentBoneNames.Add(FName("Spine3")); // 상체 본

		// 좌우 오프셋 계산
		float offsetX = (i - verticesPerRow / 2.0f) * 5.0f; // 5cm 간격
		AttachmentOffsets.Add(FVector(0, offsetX, 10.0f)); // 약간 위쪽
	}

	// 역질량을 0으로 설정하여 완전 고정
	for (int32 vertIdx : AttachmentVertices)
	{
		if (vertIdx >= 0 && vertIdx < ClothParticles.Num())
		{
			ClothParticles[vertIdx].w = 0.0f; // inverse mass = 0 (고정)
		}
	}
}

void UClothComponent::AddBodyCollision()
{
	// 캐릭터 몸체에 충돌 추가 (망토가 몸을 뚫지 않도록)

	// 골반 구체
	AddCollisionSphere(GetBoneLocation(FName("Pelvis")), 25.0f);

	// 척추 캡슐들
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

	// 어깨 구체들
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

	// BindPose를 사용하여 Transform 생성
	const FBone& Bone = Skeleton->Bones[BoneIndex];
	FTransform BoneTransform(Bone.BindPose);

	// 컴포넌트의 월드 트랜스폼을 적용
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
	if (!solver || !cloth)
		return;

	// 캐릭터에 부착된 정점 업데이트
	AttachingClothToCharacter();

	// 시뮬레이션 실행
	SimulateCloth(DeltaTime);

	// 결과 가져오기
	RetrievingSimulateResult();

	// 메시 정점 업데이트
	UpdateVerticesFromCloth();
}

void UClothComponent::UpdateVerticesFromCloth()
{
	// 시뮬레이션 결과를 메시 정점에 반영
	// 실제 렌더링 메시 업데이트 로직은 프로젝트 구조에 따라 다름
	
	if (!SkeletalMesh || PreviousParticles.Num() == 0)
		return;

	// TODO: Dynamic mesh update implementation
	// 예: GetSkeletalMeshData()->Vertices 업데이트
}

void UClothComponent::BuildClothMesh()
{ 
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData())
		return;

	// 스켈레탈 메시에서 정점 데이터 추출
	const auto& vertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
	const auto& indices = SkeletalMesh->GetSkeletalMeshData()->Indices;

	ClothParticles.Empty();
	ClothIndices.Empty();

	// 정점 데이터 변환 (PxVec4: xyz = position, w = inverse mass)
	for (const auto& vertex : vertices)
	{
		float invMass = 1.0f; // 기본 역질량 (0이면 고정)
		ClothParticles.Add(physx::PxVec4(vertex.Position.X, vertex.Position.Y, vertex.Position.Z, invMass));
	}

	// 인덱스 복사
	for (uint32 idx : indices)
	{
		ClothIndices.Add(idx);	
	}

	// 이전 프레임 파티클 초기화
	PreviousParticles = ClothParticles;
}
