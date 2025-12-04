#include "pch.h"
#include "WheeledVehicleMovementComponent.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsManager.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Vehicle/VehicleHelpers.h"
#include "vehicle/PxVehicleUtilSetup.h"
#include "vehicle/PxVehicleUtilControl.h"

// ============================================================================
// Vehicle Scene Query Filter Shader
// ============================================================================
// word3: DRIVABLE_SURFACE = 1, UNDRIVABLE_SURFACE = 0
// 서스펜션 레이캐스트는 DRIVABLE_SURFACE인 오브젝트만 히트
static PxQueryHitType::Enum WheelRaycastPreFilter(
	PxFilterData filterData0, PxFilterData filterData1,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& queryFlags)
{
	PX_UNUSED(constantBlock);
	PX_UNUSED(constantBlockSize);
	PX_UNUSED(filterData0);
	PX_UNUSED(queryFlags);

	// filterData1.word3: 히트된 오브젝트의 쿼리 필터 데이터
	// DRIVABLE_SURFACE = 1이면 블로킹 히트, 아니면 무시
	return ((filterData1.word3 & 1) == 0) ?
		PxQueryHitType::eNONE : PxQueryHitType::eBLOCK;
}

UWheeledVehicleMovementComponent::UWheeledVehicleMovementComponent()
{
	bTickEnabled = true;

	// 기본 차량 설정 초기화
	VehicleSetup = VehicleHelpers::CreateDefaultVehicleSetup();
}

UWheeledVehicleMovementComponent::~UWheeledVehicleMovementComponent()
{
	DestroyVehicle();
}

void UWheeledVehicleMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World || !World->bPie)
	{
		return;
	}

	PhysicsScene = World->GetPhysicsScene();
	if (!PhysicsScene)
	{
		return;
	}

	InitInputSmoothing();
	CreateVehicle();

	if (MeshComponent)
	{
		CacheWheelBoneIndices();
	}
}

void UWheeledVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PxVehicle)
	{
		InitializeComponent();
	}
}


void UWheeledVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!PxVehicle || !VehicleActor)
	{
		return;
	}

	// 1. 입력 스무딩 적용
	PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(
		PadSmoothingData,
		SteerVsForwardSpeedTable,
		RawInputData,
		DeltaTime,
		false,  // isVehicleInAir (현재는 항상 false)
		*PxVehicle
	);

	// 차량 Actor 깨우기 (sleep 방지)
	if (VehicleActor->isSleeping())
	{
		VehicleActor->wakeUp();
	}

	// 2. 서스펜션 레이캐스트 수행
	PerformSuspensionRaycasts();

	// 3. PhysX Scene simulate가 완료될 때까지 대기
	// PxVehicleUpdates는 scene step 완료 후에 호출되어야 함
	PhysicsScene->fetchResults(true);  // blocking wait

	// 4. PhysX 차량 시뮬레이션 업데이트
	FPhysicsManager& PhysicsManager = FPhysicsManager::GetInstance();
	PxVec3 Gravity = PhysicsScene->getGravity();

	PxVehicleWheels* Vehicles[1] = {PxVehicle};
	PxVehicleWheelQueryResult VehicleQueryResults[1] = {VehicleWheelQueryResult};

	PxVehicleUpdates(
		DeltaTime,
		Gravity,
		*PhysicsManager.GetFrictionPairs(),
		1,                                    // NumVehicles
		Vehicles,
		VehicleQueryResults                   // 올바른 형식: PxVehicleWheelQueryResult* 배열
	);

	// 4. 차량 상태 업데이트
	UpdateVehicleState();

	// 5. PhysX Actor Transform을 Owner Actor에 동기화
	PxTransform PxPose = VehicleActor->getGlobalPose();
	FTransform NewTransform(
		FVector(PxPose.p.x, PxPose.p.y, PxPose.p.z),
		FQuat(PxPose.q.x, PxPose.q.y, PxPose.q.z, PxPose.q.w),
		FVector(1, 1, 1)
	);
	GetOwner()->SetActorTransform(NewTransform);

	// 6. 스켈레탈 메시 휠 본 업데이트
	if (MeshComponent)
	{
		UpdateWheelTransforms();
	}
}

void UWheeledVehicleMovementComponent::SetVehicleSetup(const FVehicleSetupData& InSetup)
{
	VehicleSetup = InSetup;

	// 차량이 이미 생성되어 있으면 재생성
	if (PxVehicle)
	{
		DestroyVehicle();
		CreateVehicle();
	}
}

void UWheeledVehicleMovementComponent::SetSkeletalMeshComponent(USkeletalMeshComponent* InMeshComponent)
{
	MeshComponent = InMeshComponent;

	if (MeshComponent)
	{
		CacheWheelBoneIndices();
	}
}

// ============================================================================
// 입력 처리
// ============================================================================

void UWheeledVehicleMovementComponent::SetThrottleInput(float Value)
{
	RawInputData.setAnalogAccel(std::max(0.0f, std::min(1.0f, Value)));
	VehicleState.ThrottleInput = Value;

	// 입력이 있으면 차량 깨우기
	if (Value > 0.0f && VehicleActor)
	{
		VehicleActor->wakeUp();
	}
}

void UWheeledVehicleMovementComponent::SetBrakeInput(float Value)
{
	RawInputData.setAnalogBrake(std::max(0.0f, std::min(1.0f, Value)));
	VehicleState.BrakeInput = Value;

	if (Value > 0.0f && VehicleActor)
	{
		VehicleActor->wakeUp();
	}
}

void UWheeledVehicleMovementComponent::SetSteerInput(float Value)
{
	RawInputData.setAnalogSteer(std::max(-1.0f, std::min(1.0f, Value)));
	VehicleState.SteerInput = Value;

	if (Value != 0.0f && VehicleActor)
	{
		VehicleActor->wakeUp();
	}
}

void UWheeledVehicleMovementComponent::SetHandbrakeInput(float Value)
{
	RawInputData.setAnalogHandbrake(std::max(0.0f, std::min(1.0f, Value)));
	VehicleState.HandbrakeInput = Value;

	if (Value > 0.0f && VehicleActor)
	{
		VehicleActor->wakeUp();
	}
}

void UWheeledVehicleMovementComponent::SetGearUp()
{
	RawInputData.setGearUp(true);
}

void UWheeledVehicleMovementComponent::SetGearDown()
{
	RawInputData.setGearDown(true);
}

void UWheeledVehicleMovementComponent::SetTargetGear(int32 Gear, bool bImmediate)
{
	if (!PxVehicle)
	{
		return;
	}

	// PhysX 기어: 0=Reverse, 1=Neutral, 2+=Forward
	PxU32 TargetGear = static_cast<PxU32>(std::max(0, Gear));

	if (bImmediate)
	{
		// 즉시 기어 변경 (클러치 무시)
		PxVehicle->mDriveDynData.forceGearChange(TargetGear);
	}
	else
	{
		// 정상 기어 변경 (클러치 사용)
		PxVehicle->mDriveDynData.startGearChange(TargetGear);
	}
}

// ============================================================================
// 상태 조회
// ============================================================================

float UWheeledVehicleMovementComponent::GetForwardSpeed() const
{
	return VehicleState.ForwardSpeed;
}

float UWheeledVehicleMovementComponent::GetEngineRPM() const
{
	return VehicleState.EngineRPM;
}

int32 UWheeledVehicleMovementComponent::GetCurrentGear() const
{
	return VehicleState.CurrentGear;
}

// ============================================================================
// 차량 생성/파괴
// ============================================================================

void UWheeledVehicleMovementComponent::CreateVehicle()
{
	if (!PhysicsScene)
	{
		return;
	}

	FPhysicsManager& PhysicsManager = FPhysicsManager::GetInstance();
	PxPhysics* Physics = PhysicsManager.GetPhysics();
	PxMaterial* Material = PhysicsManager.GetDefaultMaterial();

	if (!Physics || !Material)
	{
		return;
	}

	// MaxDroop 강제 설정 (레이캐스트 길이 확보)
	for (int32 i = 0; i < 4; ++i)
	{
		this->VehicleSetup.Suspensions[i].MaxDroop = 1.0f;
	}

	// Sprung Mass 계산
	ComputeSprungMasses();

	// 1. VehicleActor 생성 (PxRigidDynamic)
	FTransform OwnerTransform = GetOwner()->GetActorTransform();
	PxTransform StartPose(
		PxVec3(OwnerTransform.Translation.X, OwnerTransform.Translation.Y, OwnerTransform.Translation.Z),
		PxQuat(OwnerTransform.Rotation.X, OwnerTransform.Rotation.Y, OwnerTransform.Rotation.Z, OwnerTransform.Rotation.W)
	);

	VehicleActor = Physics->createRigidDynamic(StartPose);
	VehicleActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);

	// 2. 휠 ConvexMesh 생성 (모든 휠 공유)
	float WheelRadius = this->VehicleSetup.Wheels[0].Radius;
	float WheelWidth = this->VehicleSetup.Wheels[0].Width;
	WheelConvexMesh = CreateWheelConvexMesh(WheelRadius, WheelWidth);

	if (!WheelConvexMesh)
	{
		return;
	}

	// 3. 휠 Shape 추가 (FL, FR, RL, RR - 인덱스 0~3)
	// 문서: "Add all the wheel shapes to the actor"
	PxConvexMeshGeometry WheelGeom(WheelConvexMesh);

	// 휠 Query Filter: 서스펜션 레이캐스트에서 자신의 휠을 무시하도록
	PxFilterData WheelQryFilterData;
	WheelQryFilterData.word0 = 0;  // Non-drivable (휠은 주행 가능 표면이 아님)
	WheelQryFilterData.word3 = 0;  // UNDRIVABLE_SURFACE

	// 휠 Simulation Filter: 지면과 충돌하지 않음 (서스펜션이 처리)
	PxFilterData WheelSimFilterData;
	WheelSimFilterData.word0 = 0x00001000;  // COLLISION_FLAG_WHEEL
	WheelSimFilterData.word1 = 0x00001000;  // COLLISION_FLAG_WHEEL_AGAINST (다른 휠과만)

	for (int32 i = 0; i < 4; ++i)
	{
		PxShape* WheelShape = PxRigidActorExt::createExclusiveShape(*VehicleActor, WheelGeom, *Material);
		WheelShape->setQueryFilterData(WheelQryFilterData);
		WheelShape->setSimulationFilterData(WheelSimFilterData);
		WheelShape->setLocalPose(PxTransform(PxIdentity));
	}

	// 4. 섀시 Shape 추가 (인덱스 4)
	// 일반 세단 크기: 길이 4.5m, 폭 1.8m, 높이 1.5m (half-extents: 2.25, 0.9, 0.75)
	PxBoxGeometry ChassisGeom(2.25f, 0.9f, 0.75f);
	PxShape* ChassisShape = PxRigidActorExt::createExclusiveShape(*VehicleActor, ChassisGeom, *Material);

	// 섀시 Query Filter: 주행 불가 (차량 자체)
	PxFilterData ChassisQryFilterData;
	ChassisQryFilterData.word0 = 0;
	ChassisQryFilterData.word3 = 0;  // UNDRIVABLE_SURFACE
	ChassisShape->setQueryFilterData(ChassisQryFilterData);

	// 섀시 Simulation Filter: 지면과 충돌하지 않음 (서스펜션이 지면 상호작용 담당)
	PxFilterData ChassisSimFilterData;
	ChassisSimFilterData.word0 = 0x00000001;  // COLLISION_FLAG_CHASSIS
	ChassisSimFilterData.word1 = 0;  // 지면과 충돌 안 함 - 서스펜션 레이캐스트만 사용
	ChassisShape->setSimulationFilterData(ChassisSimFilterData);

	// Chassis 중심 위치: 휠 중심(0.4m) - 휠 반경(0.35m) + ground clearance(0.15m) + chassis half height(0.75m)
	float ChassisZ = 0.4f - 0.35f + 0.15f + 0.75f;  // = 0.95m
	ChassisShape->setLocalPose(PxTransform(PxVec3(0, 0, ChassisZ)));

	// 5. 질량 및 관성 설정
	VehicleActor->setMass(this->VehicleSetup.Chassis.Mass);
	VehicleActor->setMassSpaceInertiaTensor(PxVec3(
		this->VehicleSetup.Chassis.MOI.X,
		this->VehicleSetup.Chassis.MOI.Y,
		this->VehicleSetup.Chassis.MOI.Z
	));
	VehicleActor->setCMassLocalPose(PxTransform(PxVec3(
		this->VehicleSetup.Chassis.CMOffset.X,
		this->VehicleSetup.Chassis.CMOffset.Y,
		this->VehicleSetup.Chassis.CMOffset.Z
	)));

	// 6. Scene에 추가
	PhysicsScene->addActor(*VehicleActor);

	// 7. PxVehicleDrive4W 생성
	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(4);
	SetupWheelsSimData(*WheelsSimData);

	PxVehicleDriveSimData4W DriveSimData;
	SetupDriveSimData(DriveSimData);

	PxVehicle = PxVehicleDrive4W::allocate(4);
	PxVehicle->setup(Physics, VehicleActor, *WheelsSimData, DriveSimData, 0);
	WheelsSimData->free();

	// 첫 기어를 1단으로 설정
	PxVehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);

	// 9. 배치 레이캐스트 설정 (서스펜션)
	RaycastResults = new PxRaycastQueryResult[4];
	RaycastHitBuffer = new PxRaycastHit[4];

	PxBatchQueryDesc BatchQueryDesc(4, 0, 0);
	BatchQueryDesc.queryMemory.userRaycastResultBuffer = RaycastResults;
	BatchQueryDesc.queryMemory.userRaycastTouchBuffer = RaycastHitBuffer;
	BatchQueryDesc.queryMemory.raycastTouchBufferSize = 4;
	// 문서: "sqDesc.preFilterShader = myFilterShader"
	// 서스펜션 레이캐스트가 DRIVABLE_SURFACE만 감지하도록 필터 설정
	BatchQueryDesc.preFilterShader = WheelRaycastPreFilter;

	BatchQuery = PhysicsScene->createBatchQuery(BatchQueryDesc);

	// 10. 휠 쿼리 결과 버퍼 초기화
	// 문서: "PxVehicleWheelQueryResult vehicleWheelQueryResults[1] = {{wheelQueryResults, 4}}"
	for (int32 i = 0; i < 4; ++i)
	{
		WheelQueryResultBuffer[i] = PxWheelQueryResult();  // 초기화
	}
	VehicleWheelQueryResult.wheelQueryResults = WheelQueryResultBuffer;
	VehicleWheelQueryResult.nbWheelQueryResults = 4;
}

void UWheeledVehicleMovementComponent::DestroyVehicle()
{
	if (PxVehicle)
	{
		PxVehicle->free();
		PxVehicle = nullptr;
	}

	if (VehicleActor)
	{
		if (PhysicsScene)
		{
			PhysicsScene->removeActor(*VehicleActor);
		}
		VehicleActor->release();
		VehicleActor = nullptr;
	}

	// 휠 ConvexMesh 해제
	if (WheelConvexMesh)
	{
		WheelConvexMesh->release();
		WheelConvexMesh = nullptr;
	}

	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}

	if (RaycastResults)
	{
		delete[] RaycastResults;
		RaycastResults = nullptr;
	}

	if (RaycastHitBuffer)
	{
		delete[] RaycastHitBuffer;
		RaycastHitBuffer = nullptr;
	}

	// WheelQueryResult 버퍼 초기화 (배열이므로 delete 불필요)
	VehicleWheelQueryResult.wheelQueryResults = nullptr;
	VehicleWheelQueryResult.nbWheelQueryResults = 0;
}

// ============================================================================
// PhysX 설정 변환
// ============================================================================

void UWheeledVehicleMovementComponent::SetupWheelsSimData(PxVehicleWheelsSimData& WheelsSimData)
{
	// 휠별 설정 (FL, FR, RL, RR)
	for (int32 i = 0; i < 4; ++i)
	{
		const FVehicleWheelData& WheelData = this->VehicleSetup.Wheels[i];
		const FVehicleSuspensionData& SuspensionData = this->VehicleSetup.Suspensions[i];
		const FVehicleTireData& TireData = this->VehicleSetup.Tires[i];

		// 휠 데이터
		PxVehicleWheelData PxWheel;
		PxWheel.mRadius = WheelData.Radius;
		PxWheel.mWidth = WheelData.Width;
		PxWheel.mMass = WheelData.Mass;
		PxWheel.mMOI = WheelData.MOI;
		PxWheel.mDampingRate = WheelData.DampingRate;
		PxWheel.mMaxBrakeTorque = WheelData.MaxBrakeTorque;
		PxWheel.mMaxHandBrakeTorque = WheelData.MaxHandBrakeTorque;
		PxWheel.mMaxSteer = WheelData.MaxSteerAngle;
		PxWheel.mToeAngle = WheelData.ToeAngle;

		WheelsSimData.setWheelData(i, PxWheel);

		// 서스펜션 데이터
		PxVehicleSuspensionData PxSuspension;
		PxSuspension.mSpringStrength = SuspensionData.SpringStrength;
		PxSuspension.mSpringDamperRate = SuspensionData.SpringDamperRate;
		PxSuspension.mMaxCompression = SuspensionData.MaxCompression;
		PxSuspension.mMaxDroop = SuspensionData.MaxDroop;
		PxSuspension.mSprungMass = SuspensionData.SprungMass;
		PxSuspension.mCamberAtRest = SuspensionData.CamberAtRest;
		PxSuspension.mCamberAtMaxCompression = SuspensionData.CamberAtMaxCompression;
		PxSuspension.mCamberAtMaxDroop = SuspensionData.CamberAtMaxDroop;

		WheelsSimData.setSuspensionData(i, PxSuspension);

		// 타이어 데이터
		PxVehicleTireData PxTire;
		PxTire.mLatStiffX = TireData.LatStiffX;
		PxTire.mLatStiffY = TireData.LatStiffY;
		PxTire.mLongitudinalStiffnessPerUnitGravity = TireData.LongitudinalStiffnessPerUnitGravity;
		PxTire.mCamberStiffnessPerUnitGravity = TireData.CamberStiffnessPerUnitGravity;
		PxTire.mType = TireData.TireType;

		WheelsSimData.setTireData(i, PxTire);

		// 서스펜션 오프셋
		PxVec3 SuspTravelDir(
			SuspensionData.SuspensionDirection.X,
			SuspensionData.SuspensionDirection.Y,
			SuspensionData.SuspensionDirection.Z
		);
		PxVec3 WheelCentreOffset(
			SuspensionData.WheelCentreOffset.X,
			SuspensionData.WheelCentreOffset.Y,
			SuspensionData.WheelCentreOffset.Z
		);
		PxVec3 SuspForceAppOffset(
			SuspensionData.SuspensionForceOffset.X,
			SuspensionData.SuspensionForceOffset.Y,
			SuspensionData.SuspensionForceOffset.Z
		);
		PxVec3 TireForceAppOffset(
			SuspensionData.TireForceOffset.X,
			SuspensionData.TireForceOffset.Y,
			SuspensionData.TireForceOffset.Z
		);

		WheelsSimData.setSuspTravelDirection(i, SuspTravelDir);
		WheelsSimData.setWheelCentreOffset(i, WheelCentreOffset);
		WheelsSimData.setSuspForceAppPointOffset(i, SuspForceAppOffset);
		WheelsSimData.setTireForceAppPointOffset(i, TireForceAppOffset);

		// 휠-Shape 매핑 설정 (Shape 인덱스 0~3 = 휠, 4 = 섀시)
		// 문서: "PxVehicleWheelsSimData::setWheelShapeMapping(i,i)"
		WheelsSimData.setWheelShapeMapping(i, i);

		// Scene Query Filter 설정 (서스펜션 레이캐스트용)
		// 문서: "wheelsSimData->setSceneQueryFilterData(i, qryFilterData)"
		// word3 = DRIVABLE_SURFACE 플래그 (지면만 히트되도록)
		PxFilterData SuspQryFilterData;
		SuspQryFilterData.word0 = 0;
		SuspQryFilterData.word1 = 0;
		SuspQryFilterData.word2 = 0;
		SuspQryFilterData.word3 = 1;  // DRIVABLE_SURFACE 플래그 (지면 감지용)
		WheelsSimData.setSceneQueryFilterData(i, SuspQryFilterData);
	}
}

void UWheeledVehicleMovementComponent::SetupDriveSimData(PxVehicleDriveSimData4W& DriveSimData)
{
	// 엔진
	PxVehicleEngineData PxEngine;
	PxEngine.mPeakTorque = this->VehicleSetup.Engine.MaxTorque;
	PxEngine.mMaxOmega = this->VehicleSetup.Engine.MaxRPM;
	PxEngine.mDampingRateFullThrottle = this->VehicleSetup.Engine.DampingRateFullThrottle;
	PxEngine.mDampingRateZeroThrottleClutchEngaged = this->VehicleSetup.Engine.DampingRateZeroThrottleClutchEngaged;
	PxEngine.mDampingRateZeroThrottleClutchDisengaged = this->VehicleSetup.Engine.DampingRateZeroThrottleClutchDisengaged;
	PxEngine.mMOI = this->VehicleSetup.Engine.MOI;

	DriveSimData.setEngineData(PxEngine);

	// 기어
	PxVehicleGearsData PxGears;
	PxGears.mSwitchTime = this->VehicleSetup.Gears.SwitchTime;
	PxGears.mFinalRatio = this->VehicleSetup.Gears.FinalDriveRatio;

	for (int32 i = 0; i < (int32)this->VehicleSetup.Gears.ForwardGearRatios.size() && i < PxVehicleGearsData::eGEARSRATIO_COUNT; ++i)
	{
		PxGears.mRatios[i + PxVehicleGearsData::eFIRST] = this->VehicleSetup.Gears.ForwardGearRatios[i];
	}
	PxGears.mRatios[PxVehicleGearsData::eREVERSE] = this->VehicleSetup.Gears.ReverseGearRatio;
	PxGears.mNbRatios = (uint32)this->VehicleSetup.Gears.ForwardGearRatios.size() + 1;  // +1 for reverse

	DriveSimData.setGearsData(PxGears);

	// 클러치
	PxVehicleClutchData PxClutch;
	PxClutch.mStrength = this->VehicleSetup.Clutch.Strength;
	DriveSimData.setClutchData(PxClutch);

	// 차동장치
	PxVehicleDifferential4WData PxDiff;
	switch (this->VehicleSetup.Differential.Type)
	{
	case EVehicleDifferentialType::LimitedSlip4WD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
		break;
	case EVehicleDifferentialType::LimitedSlipFrontWD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_FRONTWD;
		break;
	case EVehicleDifferentialType::LimitedSlipRearWD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_REARWD;
		break;
	case EVehicleDifferentialType::Open4WD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;
		break;
	case EVehicleDifferentialType::OpenFrontWD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_FRONTWD;
		break;
	case EVehicleDifferentialType::OpenRearWD:
		PxDiff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_REARWD;
		break;
	}

	PxDiff.mFrontRearSplit = this->VehicleSetup.Differential.FrontRearSplit;
	PxDiff.mFrontLeftRightSplit = this->VehicleSetup.Differential.FrontLeftRightSplit;
	PxDiff.mRearLeftRightSplit = this->VehicleSetup.Differential.RearLeftRightSplit;
	PxDiff.mCentreBias = this->VehicleSetup.Differential.CentreBias;
	PxDiff.mFrontBias = this->VehicleSetup.Differential.FrontBias;
	PxDiff.mRearBias = this->VehicleSetup.Differential.RearBias;

	DriveSimData.setDiffData(PxDiff);

	// Ackermann 스티어링
	PxVehicleAckermannGeometryData PxAckermann;
	PxAckermann.mAccuracy = this->VehicleSetup.Ackermann.Accuracy;
	PxAckermann.mAxleSeparation = this->VehicleSetup.Ackermann.AxleSeparation;
	PxAckermann.mFrontWidth = this->VehicleSetup.Ackermann.FrontWidth;
	PxAckermann.mRearWidth = this->VehicleSetup.Ackermann.RearWidth;

	DriveSimData.setAckermannGeometryData(PxAckermann);
}

// ============================================================================
// 시뮬레이션
// ============================================================================

void UWheeledVehicleMovementComponent::PerformSuspensionRaycasts()
{
	if (!BatchQuery || !PxVehicle)
	{
		return;
	}

	PxVehicleWheels* Vehicles[1] = {PxVehicle};
	PxVehicleSuspensionRaycasts(BatchQuery, 1, Vehicles, 4, RaycastResults);
}

void UWheeledVehicleMovementComponent::UpdateVehicleState()
{
	if (!PxVehicle)
	{
		return;
	}

	// 엔진 상태
	VehicleState.EngineOmega = PxVehicle->mDriveDynData.getEngineRotationSpeed();
	VehicleState.EngineRPM = VehicleState.EngineOmega * 9.549296585f;  // rad/s → RPM
	VehicleState.CurrentGear = PxVehicle->mDriveDynData.getCurrentGear();

	// 차량 속도 (로컬 좌표)
	PxVec3 LinearVelocity = VehicleActor->getLinearVelocity();
	PxTransform VehicleTransform = VehicleActor->getGlobalPose();
	PxVec3 LocalVelocity = VehicleTransform.rotateInv(LinearVelocity);

	VehicleState.ForwardSpeed = LocalVelocity.x;  // X축 = Forward
	VehicleState.LateralSpeed = LocalVelocity.y;  // Y축 = Right

	// 휠 상태 (FL, FR, RL, RR)
	// WheelQueryResultBuffer[i]에서 직접 읽음 (PxVehicleUpdates가 채움)
	for (int32 i = 0; i < 4; ++i)
	{
		const PxWheelQueryResult& WheelResult = WheelQueryResultBuffer[i];

		// 휠 회전 속도에서 회전 각도 계산
		// mWheelsDynData에서 직접 가져옴
		VehicleState.Wheels[i].RotationAngle = PxVehicle->mWheelsDynData.getWheelRotationAngle(i);
		VehicleState.Wheels[i].SteerAngle = WheelResult.steerAngle;
		VehicleState.Wheels[i].SuspensionJounce = WheelResult.suspJounce;
		VehicleState.Wheels[i].bInContact = (WheelResult.isInAir == false);
		VehicleState.Wheels[i].TireSlip = sqrtf(
			WheelResult.longitudinalSlip * WheelResult.longitudinalSlip +
			WheelResult.lateralSlip * WheelResult.lateralSlip
		);

		// 접촉점 및 법선
		VehicleState.Wheels[i].ContactPoint = FVector(
			WheelResult.tireContactPoint.x,
			WheelResult.tireContactPoint.y,
			WheelResult.tireContactPoint.z
		);
		VehicleState.Wheels[i].ContactNormal = FVector(
			WheelResult.tireContactNormal.x,
			WheelResult.tireContactNormal.y,
			WheelResult.tireContactNormal.z
		);
	}
}

void UWheeledVehicleMovementComponent::UpdateWheelTransforms()
{
	if (!MeshComponent)
	{
		return;
	}

	for (int32 i = 0; i < 4; ++i)
	{
		if (WheelBoneIndices[i] == -1)
		{
			continue;
		}

		const FWheelState& WheelState = VehicleState.Wheels[i];

		// 휠 스핀: Y축 회전 (측면 축, 바퀴가 굴러가는 방향)
		// 좌측 휠(FL, RL)은 반대 방향으로 회전 (미러링)
		float SpinAngle = WheelState.RotationAngle;
		if (i == 0 || i == 2)  // FL, RL (좌측)
		{
			SpinAngle = -SpinAngle;
		}

		// 휠 스핀 회전 (로컬 Y축)
		FQuat SpinRotation = FQuat::FromAxisAngle(FVector(0, 1, 0), SpinAngle);

		// 전륜 조향 (월드 Z축 회전)
		FQuat SteerRotation = FQuat::Identity();
		if (i == 0 || i == 1)  // FL, FR
		{
			SteerRotation = FQuat::FromAxisAngle(FVector(0, 0, 1), -WheelState.SteerAngle);
		}

		// 최종 회전: 조향(월드) * 참조포즈 * 스핀(로컬)
		FQuat WheelRotation = SteerRotation * WheelBoneRefPose[i].Rotation * SpinRotation;

		// 본 위치: 참조 포즈 위치 + 서스펜션 jounce (Z offset)
		FVector BoneLocation = WheelBoneRefPose[i].Translation;
		float ClampedJounce = std::max(-0.15f, std::min(0.3f, WheelState.SuspensionJounce));
		BoneLocation.Z += ClampedJounce;

		FTransform BoneTransform(BoneLocation, WheelRotation, WheelBoneRefPose[i].Scale3D);
		MeshComponent->SetBoneLocalTransform(WheelBoneIndices[i], BoneTransform);
	}
}

// ============================================================================
// 유틸리티
// ============================================================================

void UWheeledVehicleMovementComponent::ComputeSprungMasses()
{
	// PhysX 유틸리티로 SprungMass 계산
	// 섀시 질량과 질량중심, 휠 위치 기반으로 각 서스펜션이 지지하는 질량 계산
	float TotalMass = this->VehicleSetup.Chassis.Mass;
	PxVec3 CMOffset(
		this->VehicleSetup.Chassis.CMOffset.X,
		this->VehicleSetup.Chassis.CMOffset.Y,
		this->VehicleSetup.Chassis.CMOffset.Z
	);

	PxVec3 WheelOffsets[4];
	for (int32 i = 0; i < 4; ++i)
	{
		WheelOffsets[i] = PxVec3(
			this->VehicleSetup.Suspensions[i].WheelCentreOffset.X,
			this->VehicleSetup.Suspensions[i].WheelCentreOffset.Y,
			this->VehicleSetup.Suspensions[i].WheelCentreOffset.Z
		);
	}

	PxF32 SprungMasses[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	// gravityDirection: 0=X, 1=Y, 2=Z - Z-Up 엔진이므로 2 사용
	PxVehicleComputeSprungMasses(4, WheelOffsets, CMOffset, TotalMass, 2, SprungMasses);

	for (int32 i = 0; i < 4; ++i)
	{
		this->VehicleSetup.Suspensions[i].SprungMass = SprungMasses[i];
	}
}

void UWheeledVehicleMovementComponent::CacheWheelBoneIndices()
{
	if (!MeshComponent || !MeshComponent->GetSkeletalMesh())
	{
		return;
	}

	const FSkeleton* Skeleton = MeshComponent->GetSkeletalMesh()->GetSkeleton();
	if (!Skeleton)
	{
		return;
	}

	// 접미사 매칭 (소문자)
	const char* Suffixes[4] = {"fl", "fr", "rl", "rr"};

	// 모든 본을 순회하며 접미사로 매칭
	for (const auto& Pair : Skeleton->BoneNameToIndex)
	{
		FString BoneName = Pair.first;
		std::transform(BoneName.begin(), BoneName.end(), BoneName.begin(), ::tolower);

		// 각 휠 접미사 확인
		for (int32 i = 0; i < 4; ++i)
		{
			if (WheelBoneIndices[i] == -1)
			{
				// 접미사로 끝나는지 확인
				size_t SuffixLen = strlen(Suffixes[i]);
				if (BoneName.length() >= SuffixLen)
				{
					FString BoneSuffix = BoneName.substr(BoneName.length() - SuffixLen);
					if (BoneSuffix == Suffixes[i])
					{
						WheelBoneIndices[i] = Pair.second;
						break;
					}
				}
			}
		}
	}

	// 참조 포즈 캐싱
	for (int32 i = 0; i < 4; ++i)
	{
		if (WheelBoneIndices[i] != -1)
		{
			WheelBoneRefPose[i] = MeshComponent->GetBoneLocalTransform(WheelBoneIndices[i]);
		}
	}
}

void UWheeledVehicleMovementComponent::InitInputSmoothing()
{
	// 입력 스무딩 데이터 초기화 (키보드/게임패드)
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL] = 3.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE] = 3.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE] = 10.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 2.5f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 2.5f;

	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL] = 5.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE] = 5.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE] = 10.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 5.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 5.0f;

	// 속도별 조향 제한 테이블 (고속에서 조향각 감소)
	SteerVsForwardSpeedTable.addPair(0.0f, 1.0f);     // 0 m/s: 100% 조향
	SteerVsForwardSpeedTable.addPair(5.0f, 0.8f);     // 5 m/s: 80% 조향
	SteerVsForwardSpeedTable.addPair(10.0f, 0.6f);    // 10 m/s: 60% 조향
	SteerVsForwardSpeedTable.addPair(20.0f, 0.4f);    // 20 m/s: 40% 조향
	SteerVsForwardSpeedTable.addPair(30.0f, 0.3f);    // 30 m/s: 30% 조향
}

PxConvexMesh* UWheeledVehicleMovementComponent::CreateWheelConvexMesh(float Radius, float Width)
{
	// 휠을 다각형 원기둥으로 근사 (16각형 프리즘)
	// PhysX SnippetVehicle4W 참고: createWheelConvexMesh

	FPhysicsManager& PhysicsManager = FPhysicsManager::GetInstance();
	PxPhysics* Physics = PhysicsManager.GetPhysics();
	PxCooking* Cooking = PhysicsManager.GetCooking();

	if (!Physics || !Cooking)
	{
		return nullptr;
	}

	// 원기둥 근사 정점 생성 (16각형 x 2 = 32 정점)
	constexpr int32 NumSlices = 16;
	constexpr int32 NumVerts = NumSlices * 2;
	PxVec3 Verts[NumVerts];

	float HalfWidth = Width * 0.5f;

	for (int32 i = 0; i < NumSlices; ++i)
	{
		float Angle = (2.0f * PxPi * i) / NumSlices;
		float CosA = PxCos(Angle);
		float SinA = PxSin(Angle);

		// Z-Up 좌표계: 휠은 Y축 주위로 회전
		// 원기둥 축 = Y축, 원기둥 반경 방향 = X, Z 평면
		// 전면 (Y = +HalfWidth)
		Verts[i] = PxVec3(Radius * CosA, HalfWidth, Radius * SinA);
		// 후면 (Y = -HalfWidth)
		Verts[i + NumSlices] = PxVec3(Radius * CosA, -HalfWidth, Radius * SinA);
	}

	// ConvexMesh 생성
	PxConvexMeshDesc MeshDesc;
	MeshDesc.points.count = NumVerts;
	MeshDesc.points.stride = sizeof(PxVec3);
	MeshDesc.points.data = Verts;
	MeshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxDefaultMemoryOutputStream WriteBuffer;
	if (!Cooking->cookConvexMesh(MeshDesc, WriteBuffer))
	{
		UE_LOG("Vehicle: CreateWheelConvexMesh: Failed to cook convex mesh");
		return nullptr;
	}

	PxDefaultMemoryInputData ReadBuffer(WriteBuffer.getData(), WriteBuffer.getSize());
	return Physics->createConvexMesh(ReadBuffer);
}
