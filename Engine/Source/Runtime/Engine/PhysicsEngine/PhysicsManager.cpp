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

	// 2. Physics (공유 리소스)
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale());

	// 3. CPU Dispatcher (공유 리소스)
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
	int NumWorkerThreads = PxMax(1, (int)(SysInfo.dwNumberOfProcessors - 1));
	Dispatcher = PxDefaultCpuDispatcherCreate(NumWorkerThreads);

	// 4. 기본 머티리얼 (공유 리소스)
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);
}

void FPhysicsManager::Shutdown()
{
	// 역순 파괴 (공유 리소스만)
	if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }
	if (Physics) { Physics->release(); Physics = nullptr; }
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

	UE_LOG("[Physics] CreateScene: Scene=%p", Handle.Scene);

	return Handle;
}

void FPhysicsManager::DestroyScene(FPhysicsSceneHandle& Handle)
{
	if (Handle.Scene)
	{
		UE_LOG("[Physics] DestroyScene: Scene=%p", Handle.Scene);
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
	if (!Handle.Scene) return;

	// Sub-stepping 구현 (프레임 튀는 현상 방지)
	Handle.Accumulator += DeltaTime;
	if (Handle.Accumulator < Handle.StepSize) return;

	// 누적된 시간이 StepSize보다 크면 여러 번 시뮬레이션
	while (Handle.Accumulator >= Handle.StepSize)
	{
		Handle.Scene->simulate(Handle.StepSize);
		Handle.Scene->fetchResults(true); // true = 블로킹
		Handle.Accumulator -= Handle.StepSize;
	}
}
