#pragma once
#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleSDK.h"
#include "vehicle/PxVehicleUpdate.h"
#include "vehicle/PxVehicleUtil.h"
#include "vehicle/PxVehicleTireFriction.h"
#include "PhysicsEventCallback.h"

using namespace physx;

enum class EPhysicsPipelineMode
{
	FetchBeforeRender,
	FetchAfterRender,
};

// Scene 생성에 필요한 정보를 담은 구조체
struct FPhysicsSceneHandle
{
	PxScene* Scene = nullptr;
	FPhysicsEventCallback* Callback = nullptr;
	float Accumulator = 0.0f;
	float StepSize = 1.0f / 60.0f;

	bool bSimulationRunning = false;
	bool IsValid() const { return Scene != nullptr; }
};

class FPhysicsManager
{
public:
	static FPhysicsManager& GetInstance()
	{
		static FPhysicsManager Instance;
		return Instance;
	}

	void Initialize();
	void Shutdown();

	// World별 Scene 생성/파괴 헬퍼
	FPhysicsSceneHandle CreateScene();
	void DestroyScene(FPhysicsSceneHandle& Handle);
	void SimulateScene(FPhysicsSceneHandle& Handle, float DeltaTime);

	// 공유 리소스 접근
	PxFoundation* GetFoundation() { return Foundation; }
	PxPhysics* GetPhysics() { return Physics; }
	PxMaterial* GetDefaultMaterial() { return DefaultMaterial; }
	PxDefaultCpuDispatcher* GetDispatcher() { return Dispatcher; }
	PxCooking* GetCooking() { return Cooking; }

	// Vehicle SDK 관련
	bool IsVehicleSDKInitialized() const { return bVehicleSDKInitialized; }
	PxBatchQuery* GetVehicleBatchQuery(FPhysicsSceneHandle& Handle);
	PxVehicleDrivableSurfaceToTireFrictionPairs* GetFrictionPairs() { return FrictionPairs; }

	// simulate, fetch 분리 함수
	void SetPipelineMode(EPhysicsPipelineMode InMode) { PipelineMode = InMode; }
	EPhysicsPipelineMode GetPipelineMode() const { return PipelineMode; }

	void BeginSimulate(FPhysicsSceneHandle& Handle, float DeltaSeconds);
	bool TryFetch(FPhysicsSceneHandle& Handle);
	void EndSimulate(FPhysicsSceneHandle& Handle, bool bBlock = true);

private:

	EPhysicsPipelineMode PipelineMode = EPhysicsPipelineMode::FetchAfterRender;

	// 공유 PhysX 객체들
	PxFoundation* Foundation = nullptr;
	PxPhysics* Physics = nullptr;
	PxDefaultCpuDispatcher* Dispatcher = nullptr;
	PxCooking* Cooking = nullptr;
	PxMaterial* DefaultMaterial = nullptr;

	//PVD용 객체들
	PxPvd* Pvd;
	PxPvdTransport* Transport;

	// Vehicle SDK 관련
	bool bVehicleSDKInitialized = false;
	PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	void InitVehicleSDK();
	void ShutdownVehicleSDK();
	void SetupFrictionPairs();
};
