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
	// 1. Foundation
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
	if (!Foundation) { MessageBoxA(nullptr, "PxCreateFoundation failed!", "Error", MB_OK); return; }

	// 2. Physics
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, PxTolerancesScale());

	// 3. Scene Setting
	PxSceneDesc sceneDesc(Physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Z-Up 기준 중력 (아래 방향)

	// CPU Dispatcher (스레드)
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
	int NumWorkerThreads = PxMax(1, (int)(SysInfo.dwNumberOfProcessors - 1));
	Dispatcher = PxDefaultCpuDispatcherCreate(NumWorkerThreads);
	sceneDesc.cpuDispatcher = Dispatcher;

	// 4. Filter Shader & Event Callback 설정
	SimulationCallback = new FPhysicsEventCallback(); // 여기서 생성
	sceneDesc.simulationEventCallback = SimulationCallback;
	sceneDesc.filterShader = CoreSimulationFilterShader;

	// 5. Scene 생성
	Scene = Physics->createScene(sceneDesc);

	// 6. 기본 머티리얼
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);
}

void FPhysicsManager::Shutdown()
{
	// 역순 파괴
	if (Scene) { Scene->release(); Scene = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }
	if (Physics) { Physics->release(); Physics = nullptr; }
	if (SimulationCallback) { delete SimulationCallback; SimulationCallback = nullptr; }
	if (Foundation) { Foundation->release(); Foundation = nullptr; }
}

void FPhysicsManager::Simulate(float DeltaTime)
{
	if (!Scene) return;

	// Sub-stepping 구현 (프레임 튀는 현상 방지)
	Accumulator += DeltaTime;
	if (Accumulator < StepSize) return;

	// 누적된 시간이 StepSize보다 크면 여러 번 시뮬레이션
	while (Accumulator >= StepSize)
	{
		Scene->simulate(StepSize);
		Scene->fetchResults(true); // true = 블로킹
		Accumulator -= StepSize;
	}
}
