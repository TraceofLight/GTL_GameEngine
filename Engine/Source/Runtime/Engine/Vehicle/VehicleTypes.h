#pragma once
#include "Vector.h"
#include "Name.h"
#include "UEContainer.h"

/**
 * @brief 휠 위치 열거형
 */
enum class EVehicleWheelOrder : uint8
{
	FrontLeft = 0,
	FrontRight = 1,
	RearLeft = 2,
	RearRight = 3,
	MaxWheels = 4
};

/**
 * @brief 차량 엔진 설정 데이터
 * @details PhysX PxVehicleEngineData에 대응
 *
 * @param MaxTorque 최대 토크 (Nm)
 * @param MaxRPM 최대 RPM (rad/s)
 * @param IdleRPM 공회전 RPM
 * @param MOI 엔진 관성 모멘트
 * @param DampingRateFullThrottle 풀 스로틀 댐핑
 * @param DampingRateZeroThrottleClutchEngaged 클러치 연결 시 제로 스로틀 댐핑
 * @param DampingRateZeroThrottleClutchDisengaged 클러치 분리 시 제로 스로틀 댐핑
 */
struct FVehicleEngineData
{
	float MaxTorque = 500.0f;
	float MaxRPM = 600.0f;  // rad/s (약 5730 RPM)
	float IdleRPM = 100.0f;
	float MOI = 1.0f;
	float DampingRateFullThrottle = 0.15f;
	float DampingRateZeroThrottleClutchEngaged = 2.0f;
	float DampingRateZeroThrottleClutchDisengaged = 0.35f;

	// 토크 커브 (정규화된 RPM vs 정규화된 토크)
	// 기본값: 저RPM 80% -> 중RPM 100% -> 고RPM 80%
	TArray<FVector2D> TorqueCurve = {
		{0.0f, 0.8f},
		{0.33f, 1.0f},
		{1.0f, 0.8f}
	};
};

/**
 * @brief 차량 기어 설정 데이터
 * @details PhysX PxVehicleGearsData에 대응
 */
struct FVehicleGearData
{
	TArray<float> ForwardGearRatios = {4.0f, 2.0f, 1.5f, 1.1f, 1.0f};  // 1단~5단
	float ReverseGearRatio = -4.0f;
	float FinalDriveRatio = 4.0f;
	float SwitchTime = 0.5f;  // 기어 변속 시간 (초)
};

/**
 * @brief 차량 클러치 설정 데이터
 */
struct FVehicleClutchData
{
	float Strength = 10.0f;
};

/**
 * @brief 차량 오토박스 설정 데이터
 */
struct FVehicleAutoBoxData
{
	float UpRatio = 0.65f;    // 업시프트 RPM 비율
	float DownRatio = 0.50f;  // 다운시프트 RPM 비율
	float Latency = 2.0f;     // 변속 지연 시간 (초)
};

/**
 * @brief 디퍼렌셜 타입
 */
enum class EVehicleDifferentialType : uint8
{
	LimitedSlip4WD,
	LimitedSlipFrontWD,
	LimitedSlipRearWD,
	Open4WD,
	OpenFrontWD,
	OpenRearWD
};

/**
 * @brief 차량 디퍼렌셜 설정 데이터
 */
struct FVehicleDifferentialData
{
	EVehicleDifferentialType Type = EVehicleDifferentialType::LimitedSlipRearWD;
	float FrontRearSplit = 0.45f;       // 전후 토크 분배 (>0.5: 전륜 더 많음)
	float FrontLeftRightSplit = 0.5f;   // 전륜 좌우 분배
	float RearLeftRightSplit = 0.5f;    // 후륜 좌우 분배
	float CentreBias = 1.3f;
	float FrontBias = 1.3f;
	float RearBias = 1.3f;
};

/**
 * @brief 휠 설정 데이터
 * @details PhysX PxVehicleWheelData에 대응
 */
struct FVehicleWheelData
{
	float Radius = 0.35f;           // 휠 반경 (m)
	float Width = 0.25f;            // 휠 너비 (m)
	float Mass = 20.0f;             // 휠 질량 (kg)
	float MOI = 1.0f;               // 관성 모멘트
	float DampingRate = 0.25f;      // 댐핑률
	float MaxBrakeTorque = 1500.0f; // 최대 브레이크 토크
	float MaxHandBrakeTorque = 3000.0f; // 최대 핸드브레이크 토크 (후륜에만 적용)
	float MaxSteerAngle = 0.6f;     // 최대 조향각 (rad, 약 35도)
	float ToeAngle = 0.0f;          // 토우 각도

	// 휠 본 이름 (스켈레탈 메시와 연동용)
	FName BoneName;
};

/**
 * @brief 서스펜션 설정 데이터
 * @details PhysX PxVehicleSuspensionData에 대응
 */
struct FVehicleSuspensionData
{
	float SpringStrength = 35000.0f;    // 스프링 강성 (N/m)
	float SpringDamperRate = 4500.0f;   // 스프링 댐퍼율
	float MaxCompression = 0.3f;        // 최대 압축 (m)
	float MaxDroop = 0.1f;              // 최대 이완 (m)
	float SprungMass = 0.0f;            // 스프링 지지 질량 (자동 계산됨)

	// 캠버 설정
	float CamberAtRest = 0.0f;
	float CamberAtMaxCompression = 0.0f;
	float CamberAtMaxDroop = 0.0f;

	// 서스펜션 방향 (로컬 스페이스, 보통 -Z)
	FVector SuspensionDirection = FVector(0, 0, -1);

	// 휠 중심 오프셋 (로컬 스페이스)
	FVector WheelCentreOffset = FVector(0, 0, 0);

	// 서스펜션 포스 적용 오프셋 (로컬 스페이스)
	FVector SuspensionForceOffset = FVector(0, 0, 0);
};

/**
 * @brief 타이어 설정 데이터
 * @details PhysX PxVehicleTireData에 대응
 */
struct FVehicleTireData
{
	float LatStiffX = 2.0f;
	float LatStiffY = 17.19f;  // 0.3125 * (180/PI)
	float LongitudinalStiffnessPerUnitGravity = 1000.0f;
	float CamberStiffnessPerUnitGravity = 0.0f;
	uint32 TireType = 0;  // FrictionPairs의 타이어 타입 인덱스

	// 마찰 vs 슬립 그래프 (3점)
	FVector2D FrictionVsSlipGraph[3] = {
		{0.0f, 1.0f},   // 슬립 0에서 마찰 1.0
		{0.1f, 1.0f},   // 슬립 0.1에서 최대 마찰 1.0
		{1.0f, 1.0f}    // 슬립 1.0에서 마찰 1.0
	};
};

/**
 * @brief 차량 샤시 설정 데이터
 */
struct FVehicleChassisData
{
	float Mass = 1500.0f;               // 차량 질량 (kg)
	FVector MOI = FVector(1000, 1000, 1000);  // 관성 모멘트
	FVector CMOffset = FVector(0, 0, 0);      // 질량 중심 오프셋
};

/**
 * @brief 애커만 스티어링 설정 데이터
 */
struct FVehicleAckermannData
{
	float Accuracy = 1.0f;      // 애커만 정확도 (0~1)
	float FrontWidth = 1.5f;    // 전륜 간 거리 (m)
	float RearWidth = 1.5f;     // 후륜 간 거리 (m)
	float AxleSeparation = 2.5f; // 축간 거리 (m)
};

/**
 * @brief 4륜 차량 전체 설정 데이터
 * @details 하나의 구조체로 차량 전체 설정을 관리
 */
struct FVehicleSetupData
{
	// 샤시
	FVehicleChassisData Chassis;

	// 드라이브트레인
	FVehicleEngineData Engine;
	FVehicleGearData Gears;
	FVehicleClutchData Clutch;
	FVehicleAutoBoxData AutoBox;
	FVehicleDifferentialData Differential;
	FVehicleAckermannData Ackermann;

	// 휠별 설정 (FL, FR, RL, RR)
	FVehicleWheelData Wheels[4];
	FVehicleSuspensionData Suspensions[4];
	FVehicleTireData Tires[4];

	/**
	 * @brief 기본 차량 설정 생성
	 * @return 기본 설정이 적용된 FVehicleSetupData
	 */
	static FVehicleSetupData CreateDefault()
	{
		FVehicleSetupData Setup;

		// 휠 본 이름 기본값 설정
		Setup.Wheels[0].BoneName = FName("wheel_front_left");
		Setup.Wheels[1].BoneName = FName("wheel_front_right");
		Setup.Wheels[2].BoneName = FName("wheel_rear_left");
		Setup.Wheels[3].BoneName = FName("wheel_rear_right");

		// 전륜: 조향 가능
		Setup.Wheels[0].MaxSteerAngle = 0.6f;
		Setup.Wheels[1].MaxSteerAngle = 0.6f;
		Setup.Wheels[2].MaxSteerAngle = 0.0f;  // 후륜은 조향 불가
		Setup.Wheels[3].MaxSteerAngle = 0.0f;

		// 후륜: 핸드브레이크 적용
		Setup.Wheels[0].MaxHandBrakeTorque = 0.0f;
		Setup.Wheels[1].MaxHandBrakeTorque = 0.0f;
		Setup.Wheels[2].MaxHandBrakeTorque = 3000.0f;
		Setup.Wheels[3].MaxHandBrakeTorque = 3000.0f;

		// 휠 위치 오프셋 (기본값)
		Setup.Suspensions[0].WheelCentreOffset = FVector(1.25f, -0.75f, -0.3f);  // FL
		Setup.Suspensions[1].WheelCentreOffset = FVector(1.25f, 0.75f, -0.3f);   // FR
		Setup.Suspensions[2].WheelCentreOffset = FVector(-1.25f, -0.75f, -0.3f); // RL
		Setup.Suspensions[3].WheelCentreOffset = FVector(-1.25f, 0.75f, -0.3f);  // RR

		// Sprung Mass 자동 계산 (차량 질량을 4등분)
		float QuarterMass = Setup.Chassis.Mass / 4.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Suspensions[i].SprungMass = QuarterMass;
		}

		return Setup;
	}
};

/**
 * @brief 차량 런타임 상태 데이터
 * @details 매 프레임 업데이트되는 차량 상태
 */
struct FVehicleState
{
	// 엔진 상태
	float EngineRPM = 0.0f;
	float EngineOmega = 0.0f;  // rad/s
	int32 CurrentGear = 1;     // 0=Reverse, 1=Neutral, 2=First, ...

	// 휠 상태
	struct FWheelState
	{
		float RotationAngle = 0.0f;    // 회전 각도 (rad)
		float SteerAngle = 0.0f;       // 조향 각도 (rad)
		float SuspensionJounce = 0.0f; // 서스펜션 압축량 (m)
		bool bInContact = false;       // 지면 접촉 여부
		float TireSlip = 0.0f;         // 타이어 슬립
	};
	FWheelState Wheels[4];

	// 차량 속도
	float ForwardSpeed = 0.0f;  // 전진 속도 (m/s)
	float LateralSpeed = 0.0f;  // 횡방향 속도 (m/s)

	// 입력 상태
	float ThrottleInput = 0.0f;
	float BrakeInput = 0.0f;
	float SteerInput = 0.0f;
	float HandbrakeInput = 0.0f;
};
