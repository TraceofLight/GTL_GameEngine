#pragma once
#include "PxPhysicsAPI.h"
#include "PhysicsEventCallback.h"

using namespace physx;

// Scene 생성에 필요한 정보를 담은 구조체
struct FPhysicsSceneHandle
{
	PxScene* Scene = nullptr;
	FPhysicsEventCallback* Callback = nullptr;
	float Accumulator = 0.0f;
	float StepSize = 1.0f / 60.0f;

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

private:
	// 공유 PhysX 객체들
	PxFoundation* Foundation = nullptr;
	PxPhysics* Physics = nullptr;
	PxDefaultCpuDispatcher* Dispatcher = nullptr;
	PxMaterial* DefaultMaterial = nullptr;
};
