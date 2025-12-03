#pragma once
#include "ActorComponent.h"
#include "Source/Runtime/Engine/Vehicle/VehicleTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleDrive4W.h"
#include "vehicle/PxVehicleUtil.h"
#include "UWheeledVehicleMovementComponent.generated.h"

using namespace physx;

/**
 * @brief 4륜 차량 물리 이동 컴포넌트
 * @details PhysX PxVehicleDrive4W를 래핑하여 차량 물리 시뮬레이션 제공
 *
 * @param VehicleSetup 차량 설정 데이터
 * @param VehicleState 차량 런타임 상태
 * @param PxVehicle PhysX 차량 객체
 * @param VehicleActor PhysX RigidDynamic 액터
 */
UCLASS(DisplayName="차량 이동 컴포넌트", Description="4륜 차량의 물리 시뮬레이션을 담당하는 컴포넌트입니다")
class UWheeledVehicleMovementComponent : public UActorComponent
{
	GENERATED_REFLECTION_BODY()

public:
	UWheeledVehicleMovementComponent();
	~UWheeledVehicleMovementComponent() override;

	// Lifecycle
	void InitializeComponent() override;
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;
	void EndPlay() override;

	// Setup
	void SetVehicleSetup(const FVehicleSetupData& InSetup) { VehicleSetup = InSetup; }
	const FVehicleSetupData& GetVehicleSetup() const { return VehicleSetup; }

	// Input
	void SetThrottleInput(float Value);
	void SetBrakeInput(float Value);
	void SetSteerInput(float Value);
	void SetHandbrakeInput(float Value);
	void SetGearUp();
	void SetGearDown();

	// State Query
	const FVehicleState& GetVehicleState() const { return VehicleState; }
	float GetForwardSpeed() const { return VehicleState.ForwardSpeed; }
	float GetEngineRPM() const { return VehicleState.EngineRPM; }
	int32 GetCurrentGear() const { return VehicleState.CurrentGear; }

	// SkeletalMesh 연동
	void SetSkeletalMeshComponent(USkeletalMeshComponent* InMeshComp) { MeshComponent = InMeshComp; }
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return MeshComponent; }

	// PhysX 직접 접근 (디버그용)
	PxVehicleDrive4W* GetPxVehicle() const { return PxVehicle; }
	PxRigidDynamic* GetVehicleActor() const { return VehicleActorInternal; }

	// Serialization
	void DuplicateSubObjects() override;

	// 설정 데이터 (UPROPERTY는 public이어야 함)
	UPROPERTY(EditAnywhere, Category="Vehicle")
	FVehicleSetupData VehicleSetup;

	// 스켈레탈 메시 컴포넌트 참조
	UPROPERTY(EditAnywhere, Category="Vehicle")
	USkeletalMeshComponent* MeshComponent = nullptr;

protected:
	// PhysX Vehicle 생성/파괴
	void CreateVehicle();
	void DestroyVehicle();

	// 서스펜션 레이캐스트 수행
	void PerformSuspensionRaycasts();

	// 차량 상태 업데이트
	void UpdateVehicleState();

	// 스켈레탈 메시 본 업데이트
	void UpdateWheelBones();

	// PhysX 설정 변환 헬퍼
	void SetupWheelsSimData(PxVehicleWheelsSimData& WheelsSimData);
	void SetupDriveSimData(PxVehicleDriveSimData4W& DriveSimData);
	PxRigidDynamic* CreateVehicleActor();

	// 런타임 상태
	FVehicleState VehicleState;

	// PhysX 객체
	PxVehicleDrive4W* PxVehicle = nullptr;
	PxRigidDynamic* VehicleActorInternal = nullptr;
	PxBatchQuery* BatchQuery = nullptr;

	// 레이캐스트 결과 버퍼
	PxRaycastQueryResult* RaycastResults = nullptr;
	PxRaycastHit* RaycastHitBuffer = nullptr;

	// 휠 쿼리 결과 (PxVehicleUpdates에서 채워짐)
	PxWheelQueryResult WheelQueryResults[4];

	// 입력 스무딩 데이터
	PxVehicleDrive4WRawInputData RawInputData;
	PxVehiclePadSmoothingData PadSmoothingData;
	PxFixedSizeLookupTable<8> SteerVsForwardSpeedTable;

	// 휠 본 인덱스 캐시
	int32 WheelBoneIndices[4] = {-1, -1, -1, -1};

	// 초기화 플래그
	bool bVehicleCreated = false;
};
