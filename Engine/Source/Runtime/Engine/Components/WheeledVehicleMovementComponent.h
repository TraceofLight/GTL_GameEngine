#pragma once

#include "Source/Runtime/Core/Object/ActorComponent.h"
#include "Source/Runtime/Engine/Vehicle/VehicleTypes.h"
#include "PxPhysicsAPI.h"
#include "UWheeledVehicleMovementComponent.generated.h"

using namespace physx;

/**
 * @brief 4륜 차량 이동 컴포넌트
 * @details PhysX Vehicle SDK를 이용한 4륜 구동 차량 물리 시뮬레이션
 *
 * @param VehicleSetup 차량 설정 (섀시, 엔진, 휠 등)
 * @param VehicleState 런타임 상태 (RPM, 기어, 속도 등)
 * @param PxVehicle PhysX 차량 객체
 * @param VehicleActor PhysX 강체 액터
 * @param MeshComponent 스켈레탈 메시 컴포넌트 (휠 본 애니메이션)
 *
 * UE Reference: WheeledVehicleMovementComponent.h
 * PhysX Reference: PxVehicleDrive4W.h, PxVehicleUpdate.h
 */
UCLASS(DisplayName="차량 이동 컴포넌트", Description="4륜 차량 물리 시뮬레이션")
class UWheeledVehicleMovementComponent : public UActorComponent
{
	GENERATED_REFLECTION_BODY()

public:
	// 생성자/소멸자
	UWheeledVehicleMovementComponent();
	~UWheeledVehicleMovementComponent() override;

	// UActorComponent 인터페이스
	void InitializeComponent() override;
	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;

	// 차량 설정
	void SetVehicleSetup(const FVehicleSetupData& InSetup);
	void SetSkeletalMeshComponent(class USkeletalMeshComponent* InMeshComponent);

	// 입력 제어 (정규화된 값 0.0~1.0 또는 -1.0~1.0)
	void SetThrottleInput(float Value);          // [0, 1] 가속 입력
	void SetBrakeInput(float Value);             // [0, 1] 브레이크 입력
	void SetSteerInput(float Value);             // [-1, 1] 조향 입력 (-1=좌, +1=우)
	void SetHandbrakeInput(float Value);         // [0, 1] 핸드브레이크 입력
	void SetGearUp();                            // 기어 업
	void SetGearDown();                          // 기어 다운
	void SetTargetGear(int32 Gear, bool bImmediate = false);  // 특정 기어로 설정

	// 상태 조회
	const FVehicleState& GetVehicleState() const { return VehicleState; }
	float GetForwardSpeed() const;               // [m/s]
	float GetEngineRPM() const;                  // [RPM]
	int32 GetCurrentGear() const;                // 현재 기어
	PxRigidDynamic* GetVehicleActor() const { return VehicleActor; }  // PhysX 액터

protected:
	// 차량 설정 및 상태
	FVehicleSetupData VehicleSetup;
	FVehicleState VehicleState;

	// PhysX 객체들
	PxVehicleDrive4W* PxVehicle = nullptr;       // PhysX 4륜 차량
	PxRigidDynamic* VehicleActor = nullptr;      // PhysX 강체 액터
	PxScene* PhysicsScene = nullptr;             // PhysX 씬 (참조)

	// 레이캐스트 (서스펜션)
	PxBatchQuery* BatchQuery = nullptr;
	PxRaycastQueryResult* RaycastResults = nullptr;
	PxRaycastHit* RaycastHitBuffer = nullptr;

	// 휠 쿼리 결과 (PxVehicleUpdates 출력)
	PxWheelQueryResult WheelQueryResultBuffer[4];         // 실제 결과 저장 버퍼
	PxVehicleWheelQueryResult VehicleWheelQueryResult;    // Vehicle에 전달할 구조체

	// 휠 Shape 메시 (공유)
	PxConvexMesh* WheelConvexMesh = nullptr;

	// 입력 처리 (PhysX)
	PxVehicleDrive4WRawInputData RawInputData;
	PxVehiclePadSmoothingData PadSmoothingData;
	PxFixedSizeLookupTable<8> SteerVsForwardSpeedTable;

	// 스켈레탈 메시 연동
	class USkeletalMeshComponent* MeshComponent = nullptr;
	int32 WheelBoneIndices[4] = {-1, -1, -1, -1};
	FName WheelBoneNames[4] = {"wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr"};
	FTransform WheelBoneRefPose[4];  // 휠 본 참조 포즈 (스켈레탈 메시 기준)

private:
	// 차량 생성/파괴
	void CreateVehicle();
	void DestroyVehicle();

	// PhysX 설정 변환
	void SetupWheelsSimData(PxVehicleWheelsSimData& WheelsSimData);
	void SetupDriveSimData(PxVehicleDriveSimData4W& DriveSimData);

	// 시뮬레이션
	void PerformSuspensionRaycasts();
	void UpdateVehicleState();
	void UpdateWheelTransforms();

	// 유틸리티
	void ComputeSprungMasses();
	void CacheWheelBoneIndices();
	void InitInputSmoothing();

	// 휠 메시 생성 (원기둥 근사)
	PxConvexMesh* CreateWheelConvexMesh(float Radius, float Width);
};
