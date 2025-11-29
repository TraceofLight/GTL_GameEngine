#pragma once
#include "PxPhysicsAPI.h"
#include "PhysicsEventCallback.h"

using namespace physx;


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

	void Simulate(float DeltaSeconds);
	  
	PxFoundation* GetFoundation() { return Foundation;  }
	PxPhysics* GetPhysics() { return Physics;  }
	PxScene* GetScene() { return Scene;  }
	PxMaterial* GetDefaultMaterial() { return DefaultMaterial;  }
	PxDefaultCpuDispatcher* GetDispatcher() { return Dispatcher;  }
	PxCooking* GetCooking() { return Cooking; }
private:
	// PhysX 객체들
	PxFoundation* Foundation = nullptr;
	PxPhysics* Physics = nullptr;
	PxScene* Scene = nullptr;
	PxMaterial* DefaultMaterial = nullptr;
	PxDefaultCpuDispatcher* Dispatcher = nullptr;
	PxCooking* Cooking = nullptr;

	// 이벤트 콜백 관리
	FPhysicsEventCallback* SimulationCallback = nullptr;
	// 고정 타임스텝 처리를 위한 누적 시간
	float Accumulator = 0.0f;
	const float StepSize = 1.0f / 60.0f; 
};
