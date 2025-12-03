#include "pch.h"
#include "PhysicsManager.h"

// 필터 셰이더
// filterData.word2 = Ragdoll Owner ID (같은 래그돌 내 Body들은 동일한 ID)
static PxFilterFlags CoreSimulationFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	// 같은 래그돌 내 Body들 간 Self-Collision 무시
	// word2가 0이 아니고 같으면 = 같은 래그돌 소속
	if (filterData0.word2 != 0 && filterData0.word2 == filterData1.word2)
	{
		return PxFilterFlag::eSUPPRESS;
	}

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
		UE_LOG("Physics: Initialize: PxInitExtensions failed");
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

	// 8. Vehicle SDK 초기화
	InitVehicleSDK();
}

void FPhysicsManager::Shutdown()
{
	// Vehicle SDK 종료 (Physics release 전에)
	ShutdownVehicleSDK();

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
	if (!Handle.Scene)
		return;

	Handle.Accumulator += DeltaSeconds;

	if (Handle.Accumulator < Handle.StepSize)
		return;

	Handle.Accumulator -= Handle.StepSize;

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

// ===== Vehicle SDK 관련 =====

void FPhysicsManager::InitVehicleSDK()
{
	if (!Physics)
	{
		return;
	}

	// Vehicle SDK 초기화
	if (!PxInitVehicleSDK(*Physics))
	{
		UE_LOG("Physics: InitVehicleSDK: PxInitVehicleSDK failed");
		return;
	}

	// 좌표계 설정: Z-Up, Y-Forward (프로젝트 좌표계에 맞춤)
	PxVehicleSetBasisVectors(PxVec3(0, 0, 1), PxVec3(0, 1, 0));

	// 업데이트 모드: 속도 변화 방식 (더 안정적)
	PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);

	// 마찰 페어 설정
	SetupFrictionPairs();

	bVehicleSDKInitialized = true;
	UE_LOG("Physics: InitVehicleSDK: Vehicle SDK initialized");
}

void FPhysicsManager::ShutdownVehicleSDK()
{
	if (!bVehicleSDKInitialized)
	{
		return;
	}

	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}

	PxCloseVehicleSDK();
	bVehicleSDKInitialized = false;
}

void FPhysicsManager::SetupFrictionPairs()
{
	// 타이어 타입 정의 (일반 타이어)
	constexpr PxU32 NumTireTypes = 1;
	constexpr PxU32 NumSurfaceTypes = 4;

	// 노면 타입별 머티리얼 (Asphalt, Grass, Mud, Ice)
	const PxMaterial* SurfaceMaterials[NumSurfaceTypes] = {
		DefaultMaterial,  // Asphalt
		DefaultMaterial,  // Grass
		DefaultMaterial,  // Mud
		DefaultMaterial   // Ice
	};

	// 노면 타입 정의
	PxVehicleDrivableSurfaceType SurfaceTypes[NumSurfaceTypes];
	SurfaceTypes[0].mType = 0;  // Asphalt
	SurfaceTypes[1].mType = 1;  // Grass
	SurfaceTypes[2].mType = 2;  // Mud
	SurfaceTypes[3].mType = 3;  // Ice

	// 마찰 페어 생성
	FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(NumTireTypes, NumSurfaceTypes);
	FrictionPairs->setup(NumTireTypes, NumSurfaceTypes, SurfaceMaterials, SurfaceTypes);

	// 마찰 계수 설정 (TireType 0에 대해)
	// [타이어타입][노면타입] = 마찰계수
	FrictionPairs->setTypePairFriction(0, 0, 1.0f);   // Normal tire on Asphalt
	FrictionPairs->setTypePairFriction(0, 1, 0.7f);   // Normal tire on Grass
	FrictionPairs->setTypePairFriction(0, 2, 0.4f);   // Normal tire on Mud
	FrictionPairs->setTypePairFriction(0, 3, 0.1f);   // Normal tire on Ice
}

PxBatchQuery* FPhysicsManager::GetVehicleBatchQuery(FPhysicsSceneHandle& Handle)
{
	if (!Handle.Scene)
	{
		return nullptr;
	}

	// BatchQuery 생성 (Scene당 하나씩 캐싱하는 것이 좋지만, 간단히 매번 생성)
	// 실제로는 FPhysicsSceneHandle에 캐싱하는 것이 좋음
	constexpr PxU32 MaxWheels = 4;  // 4륜 차량 기준

	PxBatchQueryDesc BatchQueryDesc(MaxWheels, 0, 0);
	BatchQueryDesc.queryMemory.userRaycastResultBuffer = new PxRaycastQueryResult[MaxWheels];
	BatchQueryDesc.queryMemory.userRaycastTouchBuffer = new PxRaycastHit[MaxWheels];
	BatchQueryDesc.queryMemory.raycastTouchBufferSize = MaxWheels;

	return Handle.Scene->createBatchQuery(BatchQueryDesc);
}
