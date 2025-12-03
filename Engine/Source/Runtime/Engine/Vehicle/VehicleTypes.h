#pragma once

#include "Source/Runtime/Core/Containers/UEContainer.h"
#include "Source/Runtime/Core/Math/Vector.h"

/**
 * @brief 차량 관련 타입 정의
 * @details PhysX Vehicle SDK와 연동되는 차량 설정 및 상태 구조체들
 *
 * UE Reference: Engine/Source/Runtime/PhysicsCore/Public/PhysicsPublic.h (Vehicle 관련)
 * PhysX Reference: PxVehicleComponents.h, PxVehicleWheels.h
 */

// 휠 인덱스 순서 (PhysX 표준 순서)
enum class EVehicleWheelOrder : uint8
{
	FrontLeft = 0,
	FrontRight = 1,
	RearLeft = 2,
	RearRight = 3,
	MaxWheels = 4
};

// 차동장치 타입
enum class EVehicleDifferentialType : uint8
{
	LimitedSlip4WD,          // 제한 슬립 4WD (전후륜 토크 분배)
	LimitedSlipFrontWD,      // 제한 슬립 전륜 구동
	LimitedSlipRearWD,       // 제한 슬립 후륜 구동 (일반적)
	Open4WD,                 // 오픈 디퍼렌셜 4WD
	OpenFrontWD,             // 오픈 디퍼렌셜 전륜
	OpenRearWD               // 오픈 디퍼렌셜 후륜
};

/**
 * @brief 샤시 데이터
 * @details 차량 강체의 물리적 특성
 */
struct FVehicleChassisData
{
	float Mass = 1500.0f;                         // [kg] 차량 질량
	FVector MOI = FVector(1000, 1000, 1000);      // [kg·m²] 관성 모멘트 (X, Y, Z)
	FVector CMOffset = FVector(0, 0, 0);          // [m] 질량 중심 오프셋 (로컬 좌표)

	FVehicleChassisData() = default;
};

/**
 * @brief 엔진 데이터
 * @details 엔진 토크 커브 및 회전 특성
 * PhysX Reference: PxVehicleEngineData
 */
struct FVehicleEngineData
{
	// 토크 특성
	float MaxTorque = 500.0f;                     // [Nm] 최대 토크
	float MaxRPM = 600.0f;                        // [rad/s] 최대 각속도 (약 5730 RPM)
	float IdleRPM = 100.0f;                       // [rad/s] 공회전 각속도

	// 관성 및 댐핑
	float MOI = 1.0f;                             // [kg·m²] 엔진 관성 모멘트
	float DampingRateFullThrottle = 0.15f;        // 최대 가속 시 댐핑
	float DampingRateZeroThrottleClutchEngaged = 2.0f;     // 클러치 연결 시 댐핑
	float DampingRateZeroThrottleClutchDisengaged = 0.35f; // 클러치 분리 시 댐핑

	// 토크 커브 (정규화된 RPM vs 정규화된 토크)
	// X: 정규화된 RPM (0~1), Y: 정규화된 토크 (0~1)
	TArray<FVector2D> TorqueCurve = {
		{0.0f, 0.8f},       // 저RPM: 80% 토크
		{0.33f, 1.0f},      // 중RPM: 100% 토크 (최대)
		{1.0f, 0.8f}        // 고RPM: 80% 토크 (회전 손실)
	};

	FVehicleEngineData() = default;
};

/**
 * @brief 기어 데이터
 * @details 변속기 기어비 및 변속 시간
 * PhysX Reference: PxVehicleGearsData
 */
struct FVehicleGearData
{
	TArray<float> ForwardGearRatios = {4.0f, 2.0f, 1.5f, 1.1f, 1.0f};  // 1단~5단 기어비
	float ReverseGearRatio = -4.0f;                                     // 후진 기어비
	float FinalDriveRatio = 4.0f;                                       // 최종 구동비
	float SwitchTime = 0.5f;                                            // [s] 변속 시간

	FVehicleGearData() = default;
};

/**
 * @brief 클러치 데이터
 * @details 클러치 강도 (엔진-변속기 연결)
 * PhysX Reference: PxVehicleClutchData
 */
struct FVehicleClutchData
{
	float Strength = 10.0f;  // 클러치 강도 (회전속도 차이로부터 토크 생성)

	FVehicleClutchData() = default;
};

/**
 * @brief 차동장치 데이터
 * @details 휠별 토크 분배 특성
 * PhysX Reference: PxVehicleDifferential4WData
 */
struct FVehicleDifferentialData
{
	EVehicleDifferentialType Type = EVehicleDifferentialType::LimitedSlipRearWD;

	// 토크 분배 (0.0~1.0)
	float FrontRearSplit = 0.45f;           // 전후 분배 (0.5 = 50/50)
	float FrontLeftRightSplit = 0.5f;       // 전륜 좌우 분배
	float RearLeftRightSplit = 0.5f;        // 후륜 좌우 분배

	// 바이어스 (높을수록 잠금 강도 증가)
	float CentreBias = 1.3f;                // 중앙 디퍼렌셜 바이어스
	float FrontBias = 1.3f;                 // 전륜 디퍼렌셜 바이어스
	float RearBias = 1.3f;                  // 후륜 디퍼렌셜 바이어스

	FVehicleDifferentialData() = default;
};

/**
 * @brief Ackermann 스티어링 데이터
 * @details 조향 기하학 보정 (코너링 시 내/외륜 각도 차이)
 * PhysX Reference: PxVehicleAckermannGeometryData
 */
struct FVehicleAckermannData
{
	float Accuracy = 1.0f;                  // [0~1] Ackermann 정확도 (1.0 = 완전 보정)
	float FrontWidth = 1.5f;                // [m] 전륜 좌우 간격
	float RearWidth = 1.5f;                 // [m] 후륜 좌우 간격
	float AxleSeparation = 2.5f;            // [m] 전후륜 축간 거리

	FVehicleAckermannData() = default;
};

/**
 * @brief 휠 데이터
 * @details 개별 휠의 물리적 특성
 * PhysX Reference: PxVehicleWheelData
 */
struct FVehicleWheelData
{
	// 물리적 특성
	float Radius = 0.35f;                   // [m] 휠 반경
	float Width = 0.25f;                    // [m] 휠 너비
	float Mass = 20.0f;                     // [kg] 휠 질량
	float MOI = 1.0f;                       // [kg·m²] 관성 모멘트
	float DampingRate = 0.25f;              // 회전 댐핑

	// 제동
	float MaxBrakeTorque = 1500.0f;         // [Nm] 최대 브레이크 토크
	float MaxHandBrakeTorque = 0.0f;        // [Nm] 핸드브레이크 토크 (후륜만 사용)

	// 조향
	float MaxSteerAngle = 0.0f;             // [rad] 최대 조향각 (전륜만 사용)
	float ToeAngle = 0.0f;                  // [rad] 토우 각도

	FVehicleWheelData() = default;
};

/**
 * @brief 서스펜션 데이터
 * @details 개별 휠의 서스펜션 특성
 * PhysX Reference: PxVehicleSuspensionData
 */
struct FVehicleSuspensionData
{
	// 스프링-댐퍼
	float SpringStrength = 35000.0f;        // [N/m] 스프링 강도
	float SpringDamperRate = 4500.0f;       // 댐퍼 계수
	float MaxCompression = 0.3f;            // [m] 최대 압축 거리
	float MaxDroop = 0.1f;                  // [m] 최대 이완 거리
	float SprungMass = 0.0f;                // [kg] 스프링이 지지하는 질량 (자동 계산됨)

	// 캠버 (바퀴 경사각)
	float CamberAtRest = 0.0f;              // [rad] 정지 시 캠버
	float CamberAtMaxCompression = 0.0f;    // [rad] 최대 압축 시 캠버
	float CamberAtMaxDroop = 0.0f;          // [rad] 최대 이완 시 캠버

	// 기하학 설정 (로컬 좌표)
	FVector SuspensionDirection = FVector(0, 0, -1);    // 서스펜션 방향 (기본: -Z, 아래쪽)
	FVector WheelCentreOffset = FVector(0, 0, 0);       // 휠 중심 오프셋
	FVector SuspensionForceOffset = FVector(0, 0, 0);   // 서스펜션 힘 적용점
	FVector TireForceOffset = FVector(0, 0, 0);         // 타이어 힘 적용점

	FVehicleSuspensionData() = default;
};

/**
 * @brief 타이어 데이터
 * @details 타이어 마찰 및 슬립 특성
 * PhysX Reference: PxVehicleTireData
 */
struct FVehicleTireData
{
	// 타이어 강성
	float LatStiffX = 2.0f;                             // 횡방향 강성 X
	float LatStiffY = 17.19f;                           // 횡방향 강성 Y (0.3125 * 180/PI)
	float LongitudinalStiffnessPerUnitGravity = 1000.0f;// 종방향 강성 (중력 단위당)
	float CamberStiffnessPerUnitGravity = 0.0f;         // 캠버 강성 (중력 단위당)

	// 타이어 타입 (마찰 페어 인덱스)
	uint32 TireType = 0;

	// 마찰 vs 슬립 그래프 (Pacejka 타이어 모델 근사)
	// X: 슬립률 (0~1), Y: 마찰 계수
	TArray<FVector2D> FrictionVsSlipGraph = {
		{0.0f, 1.0f},       // 슬립 0: 최대 마찰
		{0.1f, 1.0f},       // 슬립 0.1: 최대 마찰
		{1.0f, 0.8f}        // 슬립 1.0: 마찰 감소
	};

	FVehicleTireData() = default;
};

/**
 * @brief 차량 전체 설정 데이터
 * @details 차량 생성에 필요한 모든 설정을 통합
 */
struct FVehicleSetupData
{
	// 샤시
	FVehicleChassisData Chassis;

	// 드라이브트레인
	FVehicleEngineData Engine;
	FVehicleGearData Gears;
	FVehicleClutchData Clutch;
	FVehicleDifferentialData Differential;
	FVehicleAckermannData Ackermann;

	// 휠별 설정 (FL, FR, RL, RR 순서)
	FVehicleWheelData Wheels[4];
	FVehicleSuspensionData Suspensions[4];
	FVehicleTireData Tires[4];

	FVehicleSetupData() = default;
};

/**
 * @brief 개별 휠의 런타임 상태
 */
struct FWheelState
{
	float RotationAngle = 0.0f;             // [rad] 휠 회전각 (애니메이션용)
	float SteerAngle = 0.0f;                // [rad] 조향각
	float SuspensionJounce = 0.0f;          // [m] 서스펜션 압축량 (0 = 정지 위치)
	bool bInContact = false;                // 지면 접촉 여부
	float TireSlip = 0.0f;                  // 타이어 슬립률
	FVector ContactPoint = FVector(0, 0, 0); // 접촉점 (월드 좌표)
	FVector ContactNormal = FVector(0, 0, 1);  // 접촉 법선 (Z-Up)

	FWheelState() = default;
};

/**
 * @brief 차량 런타임 상태
 * @details 매 프레임 업데이트되는 차량 상태 정보
 */
struct FVehicleState
{
	// 엔진 상태
	float EngineRPM = 0.0f;                 // [RPM] 엔진 회전수
	float EngineOmega = 0.0f;               // [rad/s] 엔진 각속도
	int32 CurrentGear = 1;                  // 현재 기어 (0=후진, 1=중립, 2=1단, ...)

	// 휠 상태 (FL, FR, RL, RR)
	FWheelState Wheels[4];

	// 차량 속도 (로컬 좌표계)
	float ForwardSpeed = 0.0f;              // [m/s] 전진 속도 (X축)
	float LateralSpeed = 0.0f;              // [m/s] 측면 속도 (Y축)

	// 현재 입력 (스무딩 적용된 값)
	float ThrottleInput = 0.0f;             // [0~1] 가속 입력
	float BrakeInput = 0.0f;                // [0~1] 브레이크 입력
	float SteerInput = 0.0f;                // [-1~1] 조향 입력
	float HandbrakeInput = 0.0f;            // [0~1] 핸드브레이크 입력

	FVehicleState() = default;
};
