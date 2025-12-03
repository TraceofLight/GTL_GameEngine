#pragma once

#include "VehicleTypes.h"

/**
 * @brief 차량 헬퍼 함수
 * @details 차량 설정 생성 및 유틸리티 함수들
 */
namespace VehicleHelpers
{
	/**
	 * @brief 기본 차량 설정 생성
	 * @details 테스트용 표준 세단 설정
	 */
	inline FVehicleSetupData CreateDefaultVehicleSetup()
	{
		FVehicleSetupData Setup;

		// ========== 섀시 ==========
		Setup.Chassis.Mass = 1500.0f;  // 1.5톤
		Setup.Chassis.MOI = FVector(1000, 1500, 1200);
		Setup.Chassis.CMOffset = FVector(0, 0, 0.5f);  // 질량중심 약간 위

		// ========== 엔진 ==========
		Setup.Engine.MaxTorque = 500.0f;  // 500 Nm (증가)
		Setup.Engine.MaxRPM = 600.0f;     // 약 5730 RPM
		Setup.Engine.IdleRPM = 100.0f;
		Setup.Engine.MOI = 1.0f;
		Setup.Engine.DampingRateFullThrottle = 0.15f;
		Setup.Engine.DampingRateZeroThrottleClutchEngaged = 2.0f;
		Setup.Engine.DampingRateZeroThrottleClutchDisengaged = 0.35f;
		Setup.Engine.TorqueCurve = {
			{0.0f, 0.8f},
			{0.33f, 1.0f},
			{1.0f, 0.8f}
		};

		// ========== 기어 ==========
		// 기어비 낮춤: 더 빠른 가속 (1단 4.0→2.5, FinalDrive 4.0→3.5)
		Setup.Gears.ForwardGearRatios = {2.5f, 2.0f, 1.5f, 1.2f, 1.0f};
		Setup.Gears.ReverseGearRatio = -2.5f;
		Setup.Gears.FinalDriveRatio = 3.5f;
		Setup.Gears.SwitchTime = 0.3f;

		// ========== 클러치 ==========
		// 클러치 강도 대폭 증가 (UE 기본값 수준)
		Setup.Clutch.Strength = 500.0f;

		// ========== 차동장치 ==========
		Setup.Differential.Type = EVehicleDifferentialType::LimitedSlipRearWD;
		Setup.Differential.FrontRearSplit = 0.45f;
		Setup.Differential.FrontLeftRightSplit = 0.5f;
		Setup.Differential.RearLeftRightSplit = 0.5f;
		Setup.Differential.CentreBias = 1.3f;
		Setup.Differential.FrontBias = 1.3f;
		Setup.Differential.RearBias = 1.3f;

		// ========== Ackermann ==========
		Setup.Ackermann.Accuracy = 1.0f;
		Setup.Ackermann.FrontWidth = 1.6f;
		Setup.Ackermann.RearWidth = 1.6f;
		Setup.Ackermann.AxleSeparation = 2.7f;

		// ========== 휠 설정 (FL, FR, RL, RR) ==========
		for (int32 i = 0; i < 4; ++i)
		{
			FVehicleWheelData& Wheel = Setup.Wheels[i];
			Wheel.Radius = 0.35f;
			Wheel.Width = 0.25f;
			Wheel.Mass = 20.0f;
			Wheel.MOI = 1.0f;
			Wheel.DampingRate = 0.25f;
			Wheel.MaxBrakeTorque = 1500.0f;
			Wheel.MaxHandBrakeTorque = 0.0f;
			Wheel.MaxSteerAngle = 0.0f;
			Wheel.ToeAngle = 0.0f;

			// 전륜 조향 설정
			if (i == 0 || i == 1)  // FL, FR
			{
				Wheel.MaxSteerAngle = 0.6f;  // 약 35도
			}

			// 후륜 핸드브레이크 설정
			if (i == 2 || i == 3)  // RL, RR
			{
				Wheel.MaxHandBrakeTorque = 3000.0f;
			}
		}

		// ========== 서스펜션 설정 (FL, FR, RL, RR) ==========
		float WheelBaseHalf = Setup.Ackermann.AxleSeparation * 0.5f;
		float TrackWidthHalf = Setup.Ackermann.FrontWidth * 0.5f;
		float SuspensionHeight = 0.4f;
		float CMOffsetZ = Setup.Chassis.CMOffset.Z;  // 0.5f

		// PhysX Vehicle SDK는 모든 오프셋을 CM 기준으로 요구
		// WheelCentreOffset: Actor 기준 휠 중심에서 CM 오프셋을 뺌
		// SuspForceAppOffset/TireForceAppOffset: CM 아래 약 0.3m 위치

		// FL
		Setup.Suspensions[0].WheelCentreOffset = FVector(WheelBaseHalf, -TrackWidthHalf, SuspensionHeight - CMOffsetZ);
		Setup.Suspensions[0].SpringStrength = 35000.0f;
		Setup.Suspensions[0].SpringDamperRate = 4500.0f;
		Setup.Suspensions[0].MaxCompression = 0.3f;
		Setup.Suspensions[0].MaxDroop = 1.0f;  // 레이캐스트 길이 확보 (RayLen = 1.0 + 0.3 + 0.35 = 1.65m)
		Setup.Suspensions[0].SuspensionDirection = FVector(0, 0, -1);
		Setup.Suspensions[0].SuspensionForceOffset = FVector(WheelBaseHalf, -TrackWidthHalf, -0.3f);
		Setup.Suspensions[0].TireForceOffset = FVector(WheelBaseHalf, -TrackWidthHalf, -0.3f);

		// FR
		Setup.Suspensions[1].WheelCentreOffset = FVector(WheelBaseHalf, TrackWidthHalf, SuspensionHeight - CMOffsetZ);
		Setup.Suspensions[1].SpringStrength = 35000.0f;
		Setup.Suspensions[1].SpringDamperRate = 4500.0f;
		Setup.Suspensions[1].MaxCompression = 0.3f;
		Setup.Suspensions[1].MaxDroop = 1.0f;  // 레이캐스트 길이 확보
		Setup.Suspensions[1].SuspensionDirection = FVector(0, 0, -1);
		Setup.Suspensions[1].SuspensionForceOffset = FVector(WheelBaseHalf, TrackWidthHalf, -0.3f);
		Setup.Suspensions[1].TireForceOffset = FVector(WheelBaseHalf, TrackWidthHalf, -0.3f);

		// RL
		Setup.Suspensions[2].WheelCentreOffset = FVector(-WheelBaseHalf, -TrackWidthHalf, SuspensionHeight - CMOffsetZ);
		Setup.Suspensions[2].SpringStrength = 35000.0f;
		Setup.Suspensions[2].SpringDamperRate = 4500.0f;
		Setup.Suspensions[2].MaxCompression = 0.3f;
		Setup.Suspensions[2].MaxDroop = 1.0f;  // 레이캐스트 길이 확보
		Setup.Suspensions[2].SuspensionDirection = FVector(0, 0, -1);
		Setup.Suspensions[2].SuspensionForceOffset = FVector(-WheelBaseHalf, -TrackWidthHalf, -0.3f);
		Setup.Suspensions[2].TireForceOffset = FVector(-WheelBaseHalf, -TrackWidthHalf, -0.3f);

		// RR
		Setup.Suspensions[3].WheelCentreOffset = FVector(-WheelBaseHalf, TrackWidthHalf, SuspensionHeight - CMOffsetZ);
		Setup.Suspensions[3].SpringStrength = 35000.0f;
		Setup.Suspensions[3].SpringDamperRate = 4500.0f;
		Setup.Suspensions[3].MaxCompression = 0.3f;
		Setup.Suspensions[3].MaxDroop = 1.0f;  // 레이캐스트 길이 확보
		Setup.Suspensions[3].SuspensionDirection = FVector(0, 0, -1);
		Setup.Suspensions[3].SuspensionForceOffset = FVector(-WheelBaseHalf, TrackWidthHalf, -0.3f);
		Setup.Suspensions[3].TireForceOffset = FVector(-WheelBaseHalf, TrackWidthHalf, -0.3f);

		// ========== 타이어 설정 (공통) ==========
		for (int32 i = 0; i < 4; ++i)
		{
			FVehicleTireData& Tire = Setup.Tires[i];
			Tire.LatStiffX = 2.0f;
			Tire.LatStiffY = 17.19f;
			Tire.LongitudinalStiffnessPerUnitGravity = 1000.0f;
			Tire.CamberStiffnessPerUnitGravity = 0.0f;
			Tire.TireType = 0;
			Tire.FrictionVsSlipGraph = {
				{0.0f, 1.0f},
				{0.1f, 1.0f},
				{1.0f, 0.8f}
			};
		}

		return Setup;
	}

	/**
	 * @brief Dodge Viper 설정 생성
	 * @details 고성능 스포츠카 설정 (테스트용)
	 */
	inline FVehicleSetupData CreateDodgeViperSetup()
	{
		FVehicleSetupData Setup = CreateDefaultVehicleSetup();

		// 섀시: 더 가볍고 낮음
		Setup.Chassis.Mass = 1200.0f;
		Setup.Chassis.CMOffset = FVector(0, 0, 0.3f);

		// 엔진: 더 강력한 토크
		Setup.Engine.MaxTorque = 600.0f;
		Setup.Engine.MaxRPM = 700.0f;

		// 기어: 더 촘촘한 레이시오
		Setup.Gears.ForwardGearRatios = {3.5f, 2.2f, 1.6f, 1.2f, 1.0f, 0.8f};
		Setup.Gears.FinalDriveRatio = 3.5f;

		// 서스펜션: 더 단단함
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Suspensions[i].SpringStrength = 50000.0f;
			Setup.Suspensions[i].SpringDamperRate = 6000.0f;
			Setup.Suspensions[i].MaxCompression = 0.2f;
			Setup.Suspensions[i].MaxDroop = 0.08f;
		}

		// 타이어: 더 높은 그립
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Tires[i].LongitudinalStiffnessPerUnitGravity = 1500.0f;
			Setup.Tires[i].FrictionVsSlipGraph = {
				{0.0f, 1.2f},
				{0.1f, 1.2f},
				{1.0f, 1.0f}
			};
		}

		return Setup;
	}

	/**
	 * @brief 트럭 설정 생성
	 * @details 무거운 차량 설정 (테스트용)
	 */
	inline FVehicleSetupData CreateTruckSetup()
	{
		FVehicleSetupData Setup = CreateDefaultVehicleSetup();

		// 섀시: 더 무겁고 높음
		Setup.Chassis.Mass = 3000.0f;
		Setup.Chassis.CMOffset = FVector(0, 0, 1.0f);

		// 엔진: 낮은 RPM, 높은 토크
		Setup.Engine.MaxTorque = 800.0f;
		Setup.Engine.MaxRPM = 400.0f;

		// 기어: 더 낮은 레이시오
		Setup.Gears.ForwardGearRatios = {5.0f, 3.0f, 2.0f, 1.5f, 1.0f};
		Setup.Gears.FinalDriveRatio = 5.0f;

		// 서스펜션: 더 부드러움
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Suspensions[i].SpringStrength = 25000.0f;
			Setup.Suspensions[i].SpringDamperRate = 3000.0f;
			Setup.Suspensions[i].MaxCompression = 0.5f;
			Setup.Suspensions[i].MaxDroop = 0.2f;
		}

		// 휠: 더 큰 반경
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Wheels[i].Radius = 0.5f;
			Setup.Wheels[i].Mass = 30.0f;
		}

		return Setup;
	}
}
