#include "pch.h"
#include "WheeledVehicleMovementComponent.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsManager.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "vehicle/PxVehicleUtilSetup.h"
#include "extensions/PxRigidBodyExt.h"

IMPLEMENT_CLASS(UWheeledVehicleMovementComponent)

UWheeledVehicleMovementComponent::UWheeledVehicleMovementComponent()
{
	// 기본 차량 설정 적용
	VehicleSetup = FVehicleSetupData::CreateDefault();

	// 입력 스무딩 설정
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL] = 6.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE] = 6.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE] = 12.0f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 2.5f;
	PadSmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 2.5f;

	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL] = 10.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE] = 10.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE] = 12.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 5.0f;
	PadSmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 5.0f;

	// 속도에 따른 스티어링 감도 테이블
	SteerVsForwardSpeedTable.addPair(0.0f, 0.75f);
	SteerVsForwardSpeedTable.addPair(5.0f, 0.75f);
	SteerVsForwardSpeedTable.addPair(30.0f, 0.125f);
	SteerVsForwardSpeedTable.addPair(120.0f, 0.1f);
}

UWheeledVehicleMovementComponent::~UWheeledVehicleMovementComponent()
{
	DestroyVehicle();
}

void UWheeledVehicleMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UWheeledVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Vehicle SDK가 초기화되었는지 확인
	if (!PHYSICS.IsVehicleSDKInitialized())
	{
		UE_LOG("[Vehicle] Vehicle SDK not initialized!");
		return;
	}

	// 차량 생성
	CreateVehicle();
}

void UWheeledVehicleMovementComponent::EndPlay()
{
	DestroyVehicle();
	Super::EndPlay();
}

void UWheeledVehicleMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!bVehicleCreated || !PxVehicle)
	{
		return;
	}

	// 1. 서스펜션 레이캐스트
	PerformSuspensionRaycasts();

	// 2. 입력 스무딩 및 적용
	bool bIsInAir = true;
	for (int32 i = 0; i < 4; ++i)
	{
		if (VehicleState.Wheels[i].bInContact)
		{
			bIsInAir = false;
			break;
		}
	}

	PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(
		PadSmoothingData,
		SteerVsForwardSpeedTable,
		RawInputData,
		DeltaTime,
		bIsInAir,
		*PxVehicle
	);

	// 3. 차량 물리 업데이트
	UWorld* World = GetWorld();
	if (World)
	{
		PxVehicleWheels* Vehicles[1] = {PxVehicle};
		PxVehicleWheelQueryResult VehicleQueryResult;
		VehicleQueryResult.wheelQueryResults = WheelQueryResults;
		VehicleQueryResult.nbWheelQueryResults = 4;

		const PxVec3 Gravity = PxVec3(0, 0, -9.81f);

		PxVehicleUpdates(
			DeltaTime,
			Gravity,
			*PHYSICS.GetFrictionPairs(),
			1,
			Vehicles,
			&VehicleQueryResult
		);
	}

	// 4. 상태 업데이트
	UpdateVehicleState();

	// 5. 스켈레탈 메시 본 업데이트
	UpdateWheelBones();
}

void UWheeledVehicleMovementComponent::SetThrottleInput(float Value)
{
	VehicleState.ThrottleInput = FMath::Clamp(Value, 0.0f, 1.0f);
	RawInputData.setAnalogAccel(VehicleState.ThrottleInput);
}

void UWheeledVehicleMovementComponent::SetBrakeInput(float Value)
{
	VehicleState.BrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
	RawInputData.setAnalogBrake(VehicleState.BrakeInput);
}

void UWheeledVehicleMovementComponent::SetSteerInput(float Value)
{
	VehicleState.SteerInput = FMath::Clamp(Value, -1.0f, 1.0f);

	// PhysX 스티어 입력: -1 (좌) ~ +1 (우)
	RawInputData.setAnalogSteer(VehicleState.SteerInput);
}

void UWheeledVehicleMovementComponent::SetHandbrakeInput(float Value)
{
	VehicleState.HandbrakeInput = FMath::Clamp(Value, 0.0f, 1.0f);
	RawInputData.setAnalogHandbrake(VehicleState.HandbrakeInput);
}

void UWheeledVehicleMovementComponent::SetGearUp()
{
	RawInputData.setGearUp(true);
}

void UWheeledVehicleMovementComponent::SetGearDown()
{
	RawInputData.setGearDown(true);
}

void UWheeledVehicleMovementComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	// MeshComponent는 Actor가 복제 시 설정
}

void UWheeledVehicleMovementComponent::CreateVehicle()
{
	if (bVehicleCreated)
	{
		return;
	}

	PxPhysics* Physics = PHYSICS.GetPhysics();
	if (!Physics)
	{
		UE_LOG("[Vehicle] Physics not available");
		return;
	}

	// 1. Vehicle Actor 생성
	VehicleActorInternal = CreateVehicleActor();
	if (!VehicleActorInternal)
	{
		UE_LOG("[Vehicle] Failed to create vehicle actor");
		return;
	}

	// 2. Wheels SimData 설정
	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(4);
	SetupWheelsSimData(*WheelsSimData);

	// 3. Drive SimData 설정
	PxVehicleDriveSimData4W DriveSimData;
	SetupDriveSimData(DriveSimData);

	// 4. Vehicle 생성
	PxVehicle = PxVehicleDrive4W::allocate(4);
	PxVehicle->setup(Physics, VehicleActorInternal, *WheelsSimData, DriveSimData, 0);

	// 5. 초기 상태 설정
	PxVehicle->setToRestState();
	PxVehicle->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);

	// 6. Scene에 추가
	UWorld* World = GetWorld();
	if (World && World->GetPhysicsSceneHandle().Scene)
	{
		World->GetPhysicsSceneHandle().Scene->addActor(*VehicleActorInternal);
	}

	// 7. 레이캐스트 버퍼 할당
	RaycastResults = new PxRaycastQueryResult[4];
	RaycastHitBuffer = new PxRaycastHit[4];

	// 8. 휠 본 인덱스 캐싱
	if (MeshComponent)
	{
		USkeletalMesh* SkelMesh = MeshComponent->GetSkeletalMesh();
		if (SkelMesh)
		{
			const FSkeletalMeshData* MeshData = SkelMesh->GetSkeletalMeshData();
			if (MeshData)
			{
				const FSkeleton& Skeleton = MeshData->Skeleton;
				for (int32 i = 0; i < 4; ++i)
				{
					const FName& BoneName = VehicleSetup.Wheels[i].BoneName;
					auto It = Skeleton.BoneNameToIndex.find(BoneName.ToString());
					if (It != Skeleton.BoneNameToIndex.end())
					{
						WheelBoneIndices[i] = It->second;
						UE_LOG("[Vehicle] Wheel bone found: %s -> index %d", BoneName.ToString().c_str(), WheelBoneIndices[i]);
					}
					else
					{
						UE_LOG("[Vehicle] Wheel bone not found: %s", BoneName.ToString().c_str());
					}
				}
			}
		}
	}

	// 메모리 정리
	WheelsSimData->free();

	bVehicleCreated = true;
	UE_LOG("[Vehicle] Vehicle created successfully");
}

void UWheeledVehicleMovementComponent::DestroyVehicle()
{
	if (!bVehicleCreated)
	{
		return;
	}

	// BatchQuery 정리
	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}

	// 레이캐스트 버퍼 정리
	delete[] RaycastResults;
	RaycastResults = nullptr;
	delete[] RaycastHitBuffer;
	RaycastHitBuffer = nullptr;

	// Vehicle 정리
	if (PxVehicle)
	{
		PxVehicle->free();
		PxVehicle = nullptr;
	}

	// Actor는 Scene에서 자동 제거됨 (release 시)
	if (VehicleActorInternal)
	{
		VehicleActorInternal->release();
		VehicleActorInternal = nullptr;
	}

	bVehicleCreated = false;
}

PxRigidDynamic* UWheeledVehicleMovementComponent::CreateVehicleActor()
{
	PxPhysics* Physics = PHYSICS.GetPhysics();
	PxMaterial* Material = PHYSICS.GetDefaultMaterial();

	if (!Physics || !Material)
	{
		return nullptr;
	}

	// 초기 위치/회전 설정
	FTransform WorldTransform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform();
	PxTransform Transform(
		PxVec3(WorldTransform.Translation.X, WorldTransform.Translation.Y, WorldTransform.Translation.Z),
		PxQuat(WorldTransform.Rotation.X, WorldTransform.Rotation.Y, WorldTransform.Rotation.Z, WorldTransform.Rotation.W)
	);

	// RigidDynamic 생성
	PxRigidDynamic* Actor = Physics->createRigidDynamic(Transform);
	if (!Actor)
	{
		return nullptr;
	}

	// 차체 충돌 Shape 추가 (간단한 박스)
	const FVector& CMOffset = VehicleSetup.Chassis.CMOffset;
	PxBoxGeometry ChassisGeom(1.0f, 0.5f, 0.25f);  // 반 크기 (2m x 1m x 0.5m)
	PxShape* ChassisShape = Physics->createShape(ChassisGeom, *Material);
	if (ChassisShape)
	{
		ChassisShape->setLocalPose(PxTransform(PxVec3(CMOffset.X, CMOffset.Y, CMOffset.Z + 0.3f)));
		Actor->attachShape(*ChassisShape);
		ChassisShape->release();
	}

	// 휠 Shape 추가 (4개)
	for (int32 i = 0; i < 4; ++i)
	{
		const FVehicleWheelData& WheelData = VehicleSetup.Wheels[i];
		const FVehicleSuspensionData& SuspData = VehicleSetup.Suspensions[i];

		PxSphereGeometry WheelGeom(WheelData.Radius);
		PxShape* WheelShape = Physics->createShape(WheelGeom, *Material);
		if (WheelShape)
		{
			// 쿼리 전용으로 설정 (충돌은 서스펜션 레이캐스트로 처리)
			WheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			WheelShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);

			const FVector& Offset = SuspData.WheelCentreOffset;
			WheelShape->setLocalPose(PxTransform(PxVec3(Offset.X, Offset.Y, Offset.Z)));
			Actor->attachShape(*WheelShape);
			WheelShape->release();
		}
	}

	// 질량 및 관성 설정
	Actor->setMass(VehicleSetup.Chassis.Mass);
	Actor->setMassSpaceInertiaTensor(PxVec3(
		VehicleSetup.Chassis.MOI.X,
		VehicleSetup.Chassis.MOI.Y,
		VehicleSetup.Chassis.MOI.Z
	));
	Actor->setCMassLocalPose(PxTransform(PxVec3(CMOffset.X, CMOffset.Y, CMOffset.Z)));

	// 안정성을 위한 설정
	Actor->setLinearDamping(0.1f);
	Actor->setAngularDamping(0.5f);
	Actor->setSolverIterationCounts(8, 4);

	return Actor;
}

void UWheeledVehicleMovementComponent::SetupWheelsSimData(PxVehicleWheelsSimData& WheelsSimData)
{
	// 휠 데이터 설정
	for (PxU32 i = 0; i < 4; ++i)
	{
		const FVehicleWheelData& WheelData = VehicleSetup.Wheels[i];
		const FVehicleSuspensionData& SuspData = VehicleSetup.Suspensions[i];
		const FVehicleTireData& TireData = VehicleSetup.Tires[i];

		// Wheel Data
		PxVehicleWheelData PxWheelData;
		PxWheelData.mRadius = WheelData.Radius;
		PxWheelData.mWidth = WheelData.Width;
		PxWheelData.mMass = WheelData.Mass;
		PxWheelData.mMOI = WheelData.MOI > 0.0f ? WheelData.MOI : 0.5f * WheelData.Mass * WheelData.Radius * WheelData.Radius;
		PxWheelData.mDampingRate = WheelData.DampingRate;
		PxWheelData.mMaxBrakeTorque = WheelData.MaxBrakeTorque;
		PxWheelData.mMaxHandBrakeTorque = WheelData.MaxHandBrakeTorque;
		PxWheelData.mMaxSteer = WheelData.MaxSteerAngle;
		PxWheelData.mToeAngle = WheelData.ToeAngle;
		WheelsSimData.setWheelData(i, PxWheelData);

		// Suspension Data
		PxVehicleSuspensionData PxSuspData;
		PxSuspData.mSpringStrength = SuspData.SpringStrength;
		PxSuspData.mSpringDamperRate = SuspData.SpringDamperRate;
		PxSuspData.mMaxCompression = SuspData.MaxCompression;
		PxSuspData.mMaxDroop = SuspData.MaxDroop;
		PxSuspData.mSprungMass = SuspData.SprungMass;
		PxSuspData.mCamberAtRest = SuspData.CamberAtRest;
		PxSuspData.mCamberAtMaxCompression = SuspData.CamberAtMaxCompression;
		PxSuspData.mCamberAtMaxDroop = SuspData.CamberAtMaxDroop;
		WheelsSimData.setSuspensionData(i, PxSuspData);

		// Tire Data
		PxVehicleTireData PxTireData;
		PxTireData.mLatStiffX = TireData.LatStiffX;
		PxTireData.mLatStiffY = TireData.LatStiffY;
		PxTireData.mLongitudinalStiffnessPerUnitGravity = TireData.LongitudinalStiffnessPerUnitGravity;
		PxTireData.mCamberStiffnessPerUnitGravity = TireData.CamberStiffnessPerUnitGravity;
		PxTireData.mType = TireData.TireType;
		PxTireData.mFrictionVsSlipGraph[0][0] = TireData.FrictionVsSlipGraph[0].X;
		PxTireData.mFrictionVsSlipGraph[0][1] = TireData.FrictionVsSlipGraph[0].Y;
		PxTireData.mFrictionVsSlipGraph[1][0] = TireData.FrictionVsSlipGraph[1].X;
		PxTireData.mFrictionVsSlipGraph[1][1] = TireData.FrictionVsSlipGraph[1].Y;
		PxTireData.mFrictionVsSlipGraph[2][0] = TireData.FrictionVsSlipGraph[2].X;
		PxTireData.mFrictionVsSlipGraph[2][1] = TireData.FrictionVsSlipGraph[2].Y;
		WheelsSimData.setTireData(i, PxTireData);

		// Suspension Travel Direction
		WheelsSimData.setSuspTravelDirection(i, PxVec3(
			SuspData.SuspensionDirection.X,
			SuspData.SuspensionDirection.Y,
			SuspData.SuspensionDirection.Z
		));

		// Wheel Centre Offset
		WheelsSimData.setWheelCentreOffset(i, PxVec3(
			SuspData.WheelCentreOffset.X,
			SuspData.WheelCentreOffset.Y,
			SuspData.WheelCentreOffset.Z
		));

		// Suspension Force Application Point Offset
		WheelsSimData.setSuspForceAppPointOffset(i, PxVec3(
			SuspData.SuspensionForceOffset.X,
			SuspData.SuspensionForceOffset.Y,
			SuspData.SuspensionForceOffset.Z
		));

		// Tire Force Application Point Offset (휠 중심)
		WheelsSimData.setTireForceAppPointOffset(i, PxVec3(
			SuspData.WheelCentreOffset.X,
			SuspData.WheelCentreOffset.Y,
			SuspData.WheelCentreOffset.Z
		));

		// Wheel Shape Mapping (Actor의 Shape 인덱스)
		// Shape 0 = 차체, Shape 1-4 = 휠
		WheelsSimData.setWheelShapeMapping(i, static_cast<PxI32>(i + 1));

		// Scene Query Filter Data
		PxFilterData QueryFilterData;
		QueryFilterData.word0 = 0;  // 기본 필터
		WheelsSimData.setSceneQueryFilterData(i, QueryFilterData);
	}

	// Tire Load Filter
	PxVehicleTireLoadFilterData TireLoadFilter;
	WheelsSimData.setTireLoadFilterData(TireLoadFilter);
}

void UWheeledVehicleMovementComponent::SetupDriveSimData(PxVehicleDriveSimData4W& DriveSimData)
{
	// Engine Data
	PxVehicleEngineData EngineData;
	EngineData.mMOI = VehicleSetup.Engine.MOI;
	EngineData.mPeakTorque = VehicleSetup.Engine.MaxTorque;
	EngineData.mMaxOmega = VehicleSetup.Engine.MaxRPM;
	EngineData.mDampingRateFullThrottle = VehicleSetup.Engine.DampingRateFullThrottle;
	EngineData.mDampingRateZeroThrottleClutchEngaged = VehicleSetup.Engine.DampingRateZeroThrottleClutchEngaged;
	EngineData.mDampingRateZeroThrottleClutchDisengaged = VehicleSetup.Engine.DampingRateZeroThrottleClutchDisengaged;

	// 토크 커브 설정
	EngineData.mTorqueCurve.clear();
	for (const FVector2D& Point : VehicleSetup.Engine.TorqueCurve)
	{
		EngineData.mTorqueCurve.addPair(Point.X, Point.Y);
	}
	DriveSimData.setEngineData(EngineData);

	// Gears Data
	PxVehicleGearsData GearsData;
	GearsData.mFinalRatio = VehicleSetup.Gears.FinalDriveRatio;
	GearsData.mSwitchTime = VehicleSetup.Gears.SwitchTime;
	GearsData.mRatios[PxVehicleGearsData::eREVERSE] = VehicleSetup.Gears.ReverseGearRatio;
	GearsData.mRatios[PxVehicleGearsData::eNEUTRAL] = 0.0f;
	for (size_t i = 0; i < VehicleSetup.Gears.ForwardGearRatios.size() && i < 30; ++i)
	{
		GearsData.mRatios[PxVehicleGearsData::eFIRST + i] = VehicleSetup.Gears.ForwardGearRatios[i];
	}
	GearsData.mNbRatios = static_cast<PxU32>(2 + VehicleSetup.Gears.ForwardGearRatios.size());
	DriveSimData.setGearsData(GearsData);

	// Clutch Data
	PxVehicleClutchData ClutchData;
	ClutchData.mStrength = VehicleSetup.Clutch.Strength;
	DriveSimData.setClutchData(ClutchData);

	// AutoBox Data
	PxVehicleAutoBoxData AutoBoxData;
	for (PxU32 i = 0; i < PxVehicleGearsData::eGEARSRATIO_COUNT; ++i)
	{
		AutoBoxData.mUpRatios[i] = VehicleSetup.AutoBox.UpRatio;
		AutoBoxData.mDownRatios[i] = VehicleSetup.AutoBox.DownRatio;
	}
	AutoBoxData.setLatency(VehicleSetup.AutoBox.Latency);
	DriveSimData.setAutoBoxData(AutoBoxData);

	// Differential Data
	PxVehicleDifferential4WData DiffData;
	DiffData.mType = static_cast<PxVehicleDifferential4WData::Enum>(VehicleSetup.Differential.Type);
	DiffData.mFrontRearSplit = VehicleSetup.Differential.FrontRearSplit;
	DiffData.mFrontLeftRightSplit = VehicleSetup.Differential.FrontLeftRightSplit;
	DiffData.mRearLeftRightSplit = VehicleSetup.Differential.RearLeftRightSplit;
	DiffData.mCentreBias = VehicleSetup.Differential.CentreBias;
	DiffData.mFrontBias = VehicleSetup.Differential.FrontBias;
	DiffData.mRearBias = VehicleSetup.Differential.RearBias;
	DriveSimData.setDiffData(DiffData);

	// Ackermann Data
	PxVehicleAckermannGeometryData AckermannData;
	AckermannData.mAccuracy = VehicleSetup.Ackermann.Accuracy;
	AckermannData.mFrontWidth = VehicleSetup.Ackermann.FrontWidth;
	AckermannData.mRearWidth = VehicleSetup.Ackermann.RearWidth;
	AckermannData.mAxleSeparation = VehicleSetup.Ackermann.AxleSeparation;
	DriveSimData.setAckermannGeometryData(AckermannData);
}

void UWheeledVehicleMovementComponent::PerformSuspensionRaycasts()
{
	if (!PxVehicle || !VehicleActorInternal)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->GetPhysicsSceneHandle().Scene)
	{
		return;
	}

	PxScene* Scene = World->GetPhysicsSceneHandle().Scene;

	// BatchQuery가 없으면 생성
	if (!BatchQuery)
	{
		PxBatchQueryDesc BatchQueryDesc(4, 0, 0);
		BatchQueryDesc.queryMemory.userRaycastResultBuffer = RaycastResults;
		BatchQueryDesc.queryMemory.userRaycastTouchBuffer = RaycastHitBuffer;
		BatchQueryDesc.queryMemory.raycastTouchBufferSize = 4;
		BatchQuery = Scene->createBatchQuery(BatchQueryDesc);
	}

	// 서스펜션 레이캐스트 수행
	PxVehicleWheels* Vehicles[1] = {PxVehicle};
	PxVehicleSuspensionRaycasts(BatchQuery, 1, Vehicles, 4, RaycastResults);
}

void UWheeledVehicleMovementComponent::UpdateVehicleState()
{
	if (!PxVehicle || !VehicleActorInternal)
	{
		return;
	}

	// 엔진 상태
	VehicleState.EngineOmega = PxVehicle->mDriveDynData.getEngineRotationSpeed();
	VehicleState.EngineRPM = VehicleState.EngineOmega * 60.0f / (2.0f * 3.14159f);  // rad/s -> RPM
	VehicleState.CurrentGear = PxVehicle->mDriveDynData.getCurrentGear();

	// 휠 상태 (PxWheelQueryResult에서 데이터 가져오기)
	const PxVehicleWheelsDynData& WheelsDynData = PxVehicle->mWheelsDynData;
	for (int32 i = 0; i < 4; ++i)
	{
		VehicleState.Wheels[i].RotationAngle = WheelsDynData.getWheelRotationAngle(i);

		// 조향각: 전륜(0,1)에만 적용
		if (i < 2)
		{
			VehicleState.Wheels[i].SteerAngle = WheelQueryResults[i].steerAngle;
		}
		else
		{
			VehicleState.Wheels[i].SteerAngle = 0.0f;
		}

		// PxWheelQueryResult에서 서스펜션 및 타이어 상태 가져오기
		VehicleState.Wheels[i].SuspensionJounce = WheelQueryResults[i].suspJounce;
		VehicleState.Wheels[i].bInContact = !WheelQueryResults[i].isInAir;
		VehicleState.Wheels[i].TireSlip = WheelQueryResults[i].longitudinalSlip;
	}

	// 차량 속도
	PxVec3 LinearVelocity = VehicleActorInternal->getLinearVelocity();
	PxTransform GlobalPose = VehicleActorInternal->getGlobalPose();
	PxVec3 Forward = GlobalPose.q.rotate(PxVec3(0, 1, 0));  // Y-Forward
	PxVec3 Right = GlobalPose.q.rotate(PxVec3(1, 0, 0));    // X-Right

	VehicleState.ForwardSpeed = LinearVelocity.dot(Forward);
	VehicleState.LateralSpeed = LinearVelocity.dot(Right);
}

void UWheeledVehicleMovementComponent::UpdateWheelBones()
{
	if (!MeshComponent || !PxVehicle)
	{
		return;
	}

	for (int32 i = 0; i < 4; ++i)
	{
		int32 BoneIndex = WheelBoneIndices[i];
		if (BoneIndex < 0)
		{
			continue;
		}

		const FVehicleState::FWheelState& WheelState = VehicleState.Wheels[i];
		const FVehicleSuspensionData& SuspData = VehicleSetup.Suspensions[i];

		// 현재 본 트랜스폼 가져오기
		FTransform BoneTransform = MeshComponent->GetBoneLocalTransform(BoneIndex);

		// 회전 적용 (휠 회전 + 조향)
		FQuat WheelRotation = FQuat::MakeFromEulerZYX(FVector(0, WheelState.RotationAngle, 0));
		FQuat SteerRotation = FQuat::MakeFromEulerZYX(FVector(0, 0, WheelState.SteerAngle));

		// 조향은 전륜에만 적용
		if (i < 2)
		{
			BoneTransform.Rotation = SteerRotation * WheelRotation;
		}
		else
		{
			BoneTransform.Rotation = WheelRotation;
		}

		// 서스펜션 오프셋 적용
		FVector SuspOffset = SuspData.SuspensionDirection * WheelState.SuspensionJounce;
		BoneTransform.Translation = BoneTransform.Translation + SuspOffset;

		// 본 트랜스폼 업데이트
		MeshComponent->SetBoneLocalTransform(BoneIndex, BoneTransform);
	}

	// 본 트랜스폼 새로고침
	MeshComponent->RefreshBoneTransforms();
}
