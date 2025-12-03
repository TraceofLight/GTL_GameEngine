#pragma once
#include "Source/Runtime/Engine/GameFramework/VehicleActor.h"
#include "VehicleTypes.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Runtime/Engine/GameFramework/World.h"

class UWorld;
class USkeletalMesh;

/**
 * @brief Vehicle 테스트 및 유틸리티 헬퍼 함수들
 */
namespace VehicleHelpers
{
	/**
	 * @brief 스켈레탈 메시의 본 계층 구조 출력 (휠 본 이름 확인용)
	 * @param SkelMesh 분석할 스켈레탈 메시
	 */
	inline void PrintBoneHierarchy(USkeletalMesh* SkelMesh)
	{
		if (!SkelMesh)
		{
			UE_LOG("[Vehicle] PrintBoneHierarchy: SkelMesh is null");
			return;
		}

		const FSkeletalMeshData* MeshData = SkelMesh->GetSkeletalMeshData();
		if (!MeshData)
		{
			UE_LOG("[Vehicle] PrintBoneHierarchy: MeshData is null");
			return;
		}

		const FSkeleton& Skeleton = MeshData->Skeleton;
		UE_LOG("[Vehicle] ===== Bone Hierarchy for: %s =====", MeshData->PathFileName.c_str());
		UE_LOG("[Vehicle] Total bones: %d", Skeleton.Bones.Num());

		for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
		{
			const FBone& Bone = Skeleton.Bones[i];
			FString ParentName = (Bone.ParentIndex >= 0) ? Skeleton.Bones[Bone.ParentIndex].Name : "ROOT";
			UE_LOG("[Vehicle] [%d] %s (parent: %s)", i, Bone.Name.c_str(), ParentName.c_str());
		}
		UE_LOG("[Vehicle] ===== End Bone Hierarchy =====");
	}

	/**
	 * @brief Dodge Viper SRT10 차량에 맞는 설정 생성
	 * @return Dodge Viper용 차량 설정
	 * @note 실제 FBX의 휠 본 이름에 맞게 수정 필요
	 */
	inline FVehicleSetupData CreateDodgeViperSetup()
	{
		FVehicleSetupData Setup = FVehicleSetupData::CreateDefault();

		// Dodge Viper는 후륜구동 스포츠카
		Setup.Chassis.Mass = 1550.0f;  // kg
		Setup.Chassis.MOI = FVector(2000, 5000, 5000);

		// 엔진: V10 8.4L
		Setup.Engine.MaxTorque = 600.0f;  // Nm
		Setup.Engine.MaxRPM = 700.0f;     // rad/s (약 6700 RPM)
		Setup.Engine.IdleRPM = 85.0f;

		// 6단 변속기
		Setup.Gears.ForwardGearRatios = {3.07f, 2.14f, 1.51f, 1.12f, 0.92f, 0.74f};
		Setup.Gears.ReverseGearRatio = -3.82f;
		Setup.Gears.FinalDriveRatio = 3.21f;

		// 후륜구동
		Setup.Differential.Type = EVehicleDifferentialType::LimitedSlipRearWD;

		// 휠 크기 (18-19인치 휠 기준)
		float WheelRadius = 0.35f;  // 약 35cm
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Wheels[i].Radius = WheelRadius;
			Setup.Wheels[i].Width = 0.28f;
			Setup.Wheels[i].Mass = 25.0f;
		}

		// 휠 위치 (축간거리 약 2.51m, 트레드 약 1.55m)
		float FrontAxle = 1.2f;   // 전방 축
		float RearAxle = -1.3f;   // 후방 축
		float TrackHalf = 0.78f;  // 트레드 절반

		Setup.Suspensions[0].WheelCentreOffset = FVector(FrontAxle, -TrackHalf, -0.2f);  // FL
		Setup.Suspensions[1].WheelCentreOffset = FVector(FrontAxle, TrackHalf, -0.2f);   // FR
		Setup.Suspensions[2].WheelCentreOffset = FVector(RearAxle, -TrackHalf, -0.2f);   // RL
		Setup.Suspensions[3].WheelCentreOffset = FVector(RearAxle, TrackHalf, -0.2f);    // RR

		// 스포츠카용 단단한 서스펜션
		for (int32 i = 0; i < 4; ++i)
		{
			Setup.Suspensions[i].SpringStrength = 50000.0f;
			Setup.Suspensions[i].SpringDamperRate = 6000.0f;
			Setup.Suspensions[i].MaxCompression = 0.15f;
			Setup.Suspensions[i].MaxDroop = 0.1f;
		}

		// 휠 본 이름 (실제 FBX 확인 후 수정 필요)
		// 일반적인 패턴들: wheel_fl, Wheel_Front_L, WheelFrontLeft 등
		Setup.Wheels[0].BoneName = FName("wheel_fl");
		Setup.Wheels[1].BoneName = FName("wheel_fr");
		Setup.Wheels[2].BoneName = FName("wheel_rl");
		Setup.Wheels[3].BoneName = FName("wheel_rr");

		return Setup;
	}

	/**
	 * @brief World에 테스트용 차량 스폰
	 * @param World 차량을 스폰할 월드
	 * @param MeshPath 스켈레탈 메시 경로
	 * @param SpawnTransform 스폰 위치/회전
	 * @return 생성된 차량 액터
	 */
	inline AVehicleActor* SpawnTestVehicle(UWorld* World, const FString& MeshPath, const FTransform& SpawnTransform)
	{
		if (!World)
		{
			UE_LOG("[Vehicle] SpawnTestVehicle: World is null");
			return nullptr;
		}

		// 차량 액터 스폰
		AVehicleActor* Vehicle = World->SpawnActor<AVehicleActor>(SpawnTransform);
		if (!Vehicle)
		{
			UE_LOG("[Vehicle] SpawnTestVehicle: Failed to spawn VehicleActor");
			return nullptr;
		}

		// 스켈레탈 메시 설정
		Vehicle->SetSkeletalMesh(MeshPath);

		// Dodge Viper 설정 적용
		FVehicleSetupData Setup = CreateDodgeViperSetup();
		Vehicle->SetVehicleSetup(Setup);

		// 플레이어 제어 활성화
		Vehicle->SetPlayerControlled(true);

		UE_LOG("[Vehicle] Test vehicle spawned at (%.1f, %.1f, %.1f)",
			SpawnTransform.Translation.X,
			SpawnTransform.Translation.Y,
			SpawnTransform.Translation.Z);

		return Vehicle;
	}
}
