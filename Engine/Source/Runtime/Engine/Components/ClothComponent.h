#pragma once

#include "SkinnedMeshComponent.h"
#include "NvCloth/Factory.h"
#include "NvCloth/Fabric.h"
#include "NvCloth/Cloth.h"
#include "NvCloth/Solver.h"
#include "NvCloth/Callbacks.h"
#include "NvClothExt/ClothFabricCooker.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
#include "UClothComponent.generated.h"

/**
 * @brief NvCloth용 간단한 Allocator
 */
class NvClothAllocator : public physx::PxAllocatorCallback
{
public:
	void* allocate(size_t size, const char* typeName, const char* filename, int line) override
	{
		return _aligned_malloc(size, 16);
	}

	void deallocate(void* ptr) override
	{
		_aligned_free(ptr);
	}
};

/**
 * @brief NvCloth용 간단한 ErrorCallback
 */
class NvClothErrorCallback : public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
	{
		// 에러 로그 출력 (필요에 따라 엔진의 로깅 시스템 사용)
		const char* errorCodeStr = "Unknown";
		switch (code)
		{
		case physx::PxErrorCode::eNO_ERROR:          errorCodeStr = "NoError"; break;
		case physx::PxErrorCode::eDEBUG_INFO:        errorCodeStr = "Info"; break;
		case physx::PxErrorCode::eDEBUG_WARNING:     errorCodeStr = "Warning"; break;
		case physx::PxErrorCode::eINVALID_PARAMETER: errorCodeStr = "InvalidParam"; break;
		case physx::PxErrorCode::eINVALID_OPERATION: errorCodeStr = "InvalidOp"; break;
		case physx::PxErrorCode::eOUT_OF_MEMORY:     errorCodeStr = "OutOfMemory"; break;
		case physx::PxErrorCode::eINTERNAL_ERROR:    errorCodeStr = "InternalError"; break;
		case physx::PxErrorCode::eABORT:             errorCodeStr = "Abort"; break;
		case physx::PxErrorCode::ePERF_WARNING:      errorCodeStr = "PerfWarning"; break;
		}
		printf("[NvCloth %s] %s (%s:%d)\n", errorCodeStr, message, file, line);
	}
};

/**
 * @brief NvCloth용 간단한 AssertHandler
 */
class NvClothAssertHandler : public nv::cloth::PxAssertHandler
{
public:
	void operator()(const char* exp, const char* file, int line, bool& ignore) override
	{
		printf("[NvCloth Assert] %s (%s:%d)\n", exp, file, line);
	}
};

/**
 * @brief Cloth 시뮬레이션 설정
 */
struct FClothSimulationSettings
{
	// Physics properties
	float Mass = 1.0f;                          // 전체 질량
	float Damping = 0.2f;                       // 감쇠 (0-1)
	float LinearDrag = 0.2f;                    // 선형 저항
	float AngularDrag = 0.2f;                   // 각속도 저항
	float Friction = 0.5f;                      // 마찰

	// Stretch constraints (Horizontal/Vertical)
	float StretchStiffness = 1.0f;              // 신축 강성 (0-1)
	float StretchStiffnessMultiplier = 1.0f;    // 강성 배율
	float CompressionLimit = 1.0f;              // 압축 제한
	float StretchLimit = 1.0f;                  // 신장 제한

	// Bend constraints
	float BendStiffness = 1.0f;                 // 굽힘 강성 (0-1)
	float BendStiffnessMultiplier = 0.5f;       // 굽힘 배율

	// Shear constraints
	float ShearStiffness = 1.0f;                // 전단 강성 (0-1)
	float ShearStiffnessMultiplier = 0.75f;     // 전단 배율

	// Collision
	float CollisionMassScale = 0.0f;            // 충돌 질량 스케일
	bool bEnableContinuousCollision = true;     // 연속 충돌 검사
	float CollisionFriction = 0.0f;             // 충돌 마찰

	// Self collision
	bool bEnableSelfCollision = false;          // 자체 충돌 활성화
	float SelfCollisionDistance = 0.0f;         // 자체 충돌 거리
	float SelfCollisionStiffness = 1.0f;        // 자체 충돌 강성

	// Solver
	int32 SolverFrequency = 300;                // Solver 주파수 (Hz)
	int32 StiffnessFrequency = 10;              // 강성 업데이트 주파수

	// Gravity
	bool bUseGravity = true;                    // 중력 사용
	FVector GravityOverride = FVector(0, 0, -9.8f); // 중력 오버라이드 (cm/s^2)

	// Wind
	FVector WindVelocity = FVector(0, 0, 0);    // 바람 속도
	float WindDrag = 0.0f;                      // 바람 저항
	float WindLift = 0.0f;                      // 바람 양력

	// Tether constraints (거리 제약)
	bool bUseTethers = true;                    // Tether 사용
	float TetherStiffness = 0.5f;               // Tether 강성
	float TetherScale = 1.2f;                   // Tether 스케일

	// Inertia
	float LinearInertiaScale = 1.0f;            // 선형 관성 스케일
	float AngularInertiaScale = 1.0f;           // 각 관성 스케일
	float CentrifugalInertiaScale = 1.0f;       // 원심력 관성 스케일
};

/**
 * @brief Cloth 제약 조건 타입
 */
enum class EClothConstraintType : uint8
{
	Fixed,          // 완전 고정
	Limited         // 제한된 이동
};

/**
 * @brief Cloth 정점 제약 정보
 */
struct FClothConstraint
{
	int32 VertexIndex;
	EClothConstraintType Type;
	FVector Position;           // Fixed인 경우 고정 위치
	float MaxDistance;          // Limited인 경우 최대 이동 거리
};

/**
 * @brief NvCloth 기반 Cloth 시뮬레이션 컴포넌트
 */
class UClothComponent : public USkinnedMeshComponent
{ 
	GENERATED_REFLECTION_BODY()
	  
public:
	UClothComponent();
	virtual ~UClothComponent();

	// Lifecycle
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void OnCreatePhysicsState() override;
	virtual void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;
	//virtual void OnDestroyPhysicsState() override;

	// Cloth setup
	void SetupClothFromMesh();
	void ReleaseCloth();

	// Constraint management
	//void AddConstraint(int32 VertexIndex, EClothConstraintType Type,
	//	const FVector& Position = FVector::Zero(),
	//	float MaxDistance = 0.0f);
	//void RemoveConstraint(int32 VertexIndex);
	//void ClearConstraints();
	//void UpdateConstraintPositions();

	// Wind control
	void SetWindVelocity(const FVector& Velocity);
	void SetWindParams(float Drag, float Lift);

	//// Simulation control
	//void ResetClothSimulation();
	//void SetClothEnabled(bool bEnabled);
	//bool IsClothEnabled() const { return bClothEnabled; }

	//// Settings
	//void SetClothSettings(const FClothSimulationSettings& NewSettings);
	const FClothSimulationSettings& GetClothSettings() const { return ClothSettings; }

	// Collision
	void AddCollisionSphere(const FVector& Center, float Radius);
	void AddCollisionCapsule(const FVector& Start, const FVector& End, float Radius);
	void AddCollisionPlane(const FVector& Point, const FVector& Normal);
	//void AddTriangleMeshCollision();

	//void AddCollisionConvex(const TArray<FVector>& ConvexPlanes);
	void ClearCollisionShapes();
	 
	void SetupCapeAttachment();
	void AddBodyCollision();

	// Helper functions
	int32 GetBoneIndex(const FName& BoneName) const;
	FTransform GetBoneTransform(int32 BoneIndex) const;
	FVector GetBoneLocation(const FName& BoneName);
	FVector GetAttachmentPosition(int32 AttachmentIndex);
protected:
	// NvCloth objects
	nv::cloth::Factory* ClothFactory = nullptr;
	nv::cloth::Solver* ClothSolver = nullptr;
	nv::cloth::Fabric* ClothFabric = nullptr;
	nv::cloth::Cloth* NvCloth = nullptr;

	// Cloth data
	TArray<physx::PxVec4> ClothParticles;       // 정점 위치 + inverse mass (w)
	TArray<physx::PxVec4> PreviousParticles;    // 이전 프레임 정점
	TArray<physx::PxVec3> ClothNormals;         // 정점 노멀
	TArray<uint32> ClothIndices;                // 인덱스 (triangles)
	TArray<uint32> ClothQuads;                  // 쿼드 인덱스 (optional)
	TArray<float> ClothInvMasses;               // 역질량 (0 = 고정)

	// Attachment data - 망토를 캐릭터에 고정
	TArray<int32> AttachmentVertices;           // 고정할 정점 인덱스
	TArray<FName> AttachmentBoneNames;          // 부착할 본 이름
	TArray<FVector> AttachmentOffsets;          // 본으로부터의 오프셋

	// Constraints
	TArray<FClothConstraint> Constraints;
	TArray<physx::PxVec4> MotionConstraints;    // Position + radius
	TArray<physx::PxVec4> SeparationConstraints; // Separation + radius

	// Collision shapes (NvCloth format)
	TArray<physx::PxVec4> CollisionSpheres;     // Center + radius
	TArray<uint32> CollisionCapsules;           // Sphere index pairs
	TArray<physx::PxVec4> CollisionPlanes;      // Normal + distance
	TArray<uint32> CollisionConvexes;           // Plane indices
	TArray<physx::PxVec4> CollisionTriangles;   // Triangle vertices

	// Settings
	FClothSimulationSettings ClothSettings;
	bool bClothEnabled = true;
	bool bClothInitialized = false;

	// Simulation state
	float AccumulatedTime = 0.0f;
	float SimulationFrequency = 60.0f;          // Hz

private:
	// Internal helpers
	void InitializeNvCloth();
	void CreateClothFabric();
	void CreateClothInstance();
	void CreatePhaseConfig();
	void CreateSolver();

	void SimulateCloth(float DeltaSeconds);
	void RetrievingSimulateResult();
	void RecalculateNormals();

	void ApplyClothProperties();
	void ApplyTetherConstraint();

	void AttachingClothToCharacter();

	//void ApplyClothSettings();
	void UpdateClothSimulation(float DeltaTime);
	void UpdateVerticesFromCloth();
	void BuildClothMesh();
	//void ComputeInvMasses();

	//// Constraint helpers	
	//void ApplyConstraintsToCloth();
	void UpdateMotionConstraints();
	//void UpdateSeparationConstraints();
	void ClearMotionConstraints();

	//// Collision helpers
	//void UpdateCollisionShapes();

	//// Cooking helpers
	//nv::cloth::ClothMeshDesc GetClothMeshDesc();


	nv::cloth::Factory* factory;

	nv::cloth::Fabric* fabric;
	nv::cloth::Cloth* cloth;	
	nv::cloth::Vector<int32_t>::Type phaseTypeInfo;
	nv::cloth::Solver* solver;
	nv::cloth::PhaseConfig* phases;
	 
};
