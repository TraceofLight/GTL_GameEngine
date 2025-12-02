#include "pch.h"
#include "PhysicsManager.h"

// 둘중 1개라도 Trigger면 Trigger 기본 동작 수행
// 그 외는 Default Contact
// Filter 설정을 통해서 Touch Found 콜백 호출
// 필터 셰이더
static PxFilterFlags CoreSimulationFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	// triggers 처리
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	if ((filterData0.word0 & filterData1.word1) &&
		(filterData1.word0 & filterData0.word1))
	{
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
	}

	return PxFilterFlag::eDEFAULT;
}

//TODO: setupFiltering https://nvidiagameworks.github.io/PhysX/4.1/documentation/physxguide/Manual/RigidBodyCollision.html

void FPhysicsManager::Initialize()
{
	// 1. Foundation (공유 리소스)
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
	if (!Foundation) { MessageBoxA(nullptr, "PxCreateFoundation failed!", "Error", MB_OK); return; }



	//TOOD: Release모드에서 PVD가 안붙도록 
	// PVD 연결 
	Pvd = PxCreatePvd(*Foundation);

	Transport =
		PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	Pvd->connect(*Transport, PxPvdInstrumentationFlag::eALL);
	 

	// 2. Physics (공유 리소스)
	// PVD on/off를 정할 수 있음
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale() , /*pvd설정*/true, Pvd);

	// 3. CPU Dispatcher (공유 리소스)
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
	int NumWorkerThreads = PxMax(1, (int)(SysInfo.dwNumberOfProcessors - 1));
	Dispatcher = PxDefaultCpuDispatcherCreate(NumWorkerThreads);

	// 4. 기본 머티리얼 (공유 리소스)
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);

	// 7. Cooking interface
	PxCookingParams cookParams(Physics->getTolerancesScale());
	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, cookParams);

}

void FPhysicsManager::Shutdown()
{ 
	// 역순 파괴 (공유 리소스만)
	if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }
	if (Cooking) { Cooking->release(); Cooking = nullptr; }
	if (Physics) { Physics->release(); Physics = nullptr; }
	if (Pvd) { Pvd->release();	 Pvd = nullptr; }
	if (Transport) { Transport->release(); Transport = nullptr;  }
	if (Foundation) { Foundation->release(); Foundation = nullptr; }
}

FPhysicsSceneHandle FPhysicsManager::CreateScene()
{
	FPhysicsSceneHandle Handle;

	if (!Physics || !Dispatcher)
	{
		return Handle;
	}

	// Scene 설정
	PxSceneDesc sceneDesc(Physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Z-Up 기준 중력
	sceneDesc.cpuDispatcher = Dispatcher;

	// 이 Scene 전용 콜백 생성
	Handle.Callback = new FPhysicsEventCallback();
	sceneDesc.simulationEventCallback = Handle.Callback;
	sceneDesc.filterShader = CoreSimulationFilterShader;

	// Scene 생성
	Handle.Scene = Physics->createScene(sceneDesc);

	// PVD 연결
	PxPvdSceneClient* pvdClient = Handle.Scene->getScenePvdClient();

	if (pvdClient)
	{
		pvdClient->setScenePvdFlags(
			PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
			PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES |
			PxPvdSceneFlag::eTRANSMIT_CONTACTS
		);
	}

	return Handle;
}

void FPhysicsManager::DestroyScene(FPhysicsSceneHandle& Handle)
{
	if (Handle.Scene)
	{
		Handle.Scene->release();
		Handle.Scene = nullptr;
	}

	if (Handle.Callback)
	{
		delete Handle.Callback;
		Handle.Callback = nullptr;
	}

	Handle.Accumulator = 0.0f;
}

void FPhysicsManager::SimulateScene(FPhysicsSceneHandle& Handle, float DeltaTime)
{
	BeginSimulate(Handle, DeltaTime);
	EndSimulate(Handle, true);
}

void FPhysicsManager::BeginSimulate(FPhysicsSceneHandle& Handle, float DeltaSeconds)
{
	// phsx scene이 없으면 패스 
	if (!Handle.Scene)
		return;

	Handle.Accumulator += DeltaSeconds;

	//아직 시뮬레이션을 돌릴 틱이 안모였으면 패스
	if (Handle.Accumulator < Handle.StepSize)
		return;

	Handle.Accumulator -= Handle.StepSize;

	// Physx를 통해서 시뮬레이션 시작
	Handle.Scene->simulate(Handle.StepSize); 
	Handle.bSimulationRunning = true;
}

bool FPhysicsManager::TryFetch(FPhysicsSceneHandle& Handle)
{
	if (!Handle.Scene || !Handle.bSimulationRunning)
		return false;

	// 아직 안끝났으면 return false ;
	const bool bDone = Handle.Scene->fetchResults(false);
	if (bDone)
	{
		Handle.bSimulationRunning = false;
	}

	return bDone;
}

void FPhysicsManager::EndSimulate(FPhysicsSceneHandle& Handle, bool bBlock)
{
	if (!Handle.Scene || !Handle.bSimulationRunning)
		return;

	if (bBlock)
	{
		Handle.Scene->fetchResults(true);
		Handle.bSimulationRunning = false; 
	}
	else
	{
		TryFetch(Handle);
	}
}
