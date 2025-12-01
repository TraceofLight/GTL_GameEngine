#include "pch.h"
#include "PhysicsManager.h"

// 필터 셰이더
static PxFilterFlags CoreSimulationFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	// 기본적으로 충돌 처리
	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	// 충돌 시작/종료 이벤트 활성화
	pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
	pairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;
		
	return PxFilterFlag::eDEFAULT;
}

void FPhysicsManager::Initialize()
{
	// 1. Foundation (공유 리소스)
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
	if (!Foundation) { MessageBoxA(nullptr, "PxCreateFoundation failed!", "Error", MB_OK); return; }

	// 2. PVD 설정 (PhysX Visual Debugger)
	Pvd = PxCreatePvd(*Foundation);
	if (Pvd)
	{
		Transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
		if (Transport)
		{
			Pvd->connect(*Transport, PxPvdInstrumentationFlag::eALL);
		}
	}

	// 3. Physics (공유 리소스)
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale(), true, Pvd);

	// 4. Extensions 초기화 (D6 Joint 등 사용을 위해 필수)
	if (!PxInitExtensions(*Physics, Pvd))
	{
		UE_LOG("[Physics] PxInitExtensions failed!");
	}

	// 5. CPU Dispatcher (공유 리소스)
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

	// Extensions 종료 (Physics release 전에 호출)
	PxCloseExtensions();

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

	// PVD Scene Client 설정 (PVD가 연결된 경우에만)
	if (Pvd && Pvd->isConnected())
	{
		PxPvdSceneClient* pvdClient = Handle.Scene->getScenePvdClient();
		if (pvdClient)
		{
			pvdClient->setScenePvdFlags(
				PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS |
				PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES |
				PxPvdSceneFlag::eTRANSMIT_CONTACTS
			);
		}
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
