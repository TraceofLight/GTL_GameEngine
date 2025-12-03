#pragma once
#include "Pawn.h"
#include "Source/Runtime/Engine/Vehicle/VehicleTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/WheeledVehicleMovementComponent.h"
#include "AVehicleActor.generated.h"

/**
 * @brief 4륜 차량 폰
 * @details 스켈레탈 메시와 차량 물리 컴포넌트를 가진 차량 폰
 *
 * @param MeshComponent 차량 스켈레탈 메시
 * @param VehicleMovement 차량 이동 컴포넌트
 * @param bIsPlayerControlled 플레이어 제어 여부
 */
UCLASS(DisplayName="차량", Description="4륜 차량 폰입니다")
class AVehicleActor : public APawn
{
public:
	GENERATED_REFLECTION_BODY()

	AVehicleActor();
	~AVehicleActor() override = default;

	// Lifecycle
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	// Input Setup (APawn override)
	void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

	// Setup
	void SetSkeletalMesh(const FString& MeshPath);
	void SetVehicleSetup(const FVehicleSetupData& Setup);

	// Input Handlers (키바인딩에서 호출됨)
	void ThrottlePressed();
	void ThrottleReleased();
	void BrakePressed();
	void BrakeReleased();
	void SteerLeftPressed();
	void SteerLeftReleased();
	void SteerRightPressed();
	void SteerRightReleased();
	void HandbrakePressed();
	void HandbrakeReleased();

	// Component Access
	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }
	UWheeledVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }

	// State Query
	float GetSpeed() const;
	float GetEngineRPM() const;
	int32 GetCurrentGear() const;

	// Serialization
	void DuplicateSubObjects() override;

	// UPROPERTY는 public이어야 함
	UPROPERTY(EditAnywhere, Category="Vehicle")
	USkeletalMeshComponent* MeshComponent = nullptr;

	UPROPERTY(EditAnywhere, Category="Vehicle")
	UWheeledVehicleMovementComponent* VehicleMovement = nullptr;

protected:
	// 입력 상태 추적
	bool bThrottlePressed = false;
	bool bBrakePressed = false;
	bool bSteerLeftPressed = false;
	bool bSteerRightPressed = false;
	bool bHandbrakePressed = false;

	// 컴포넌트 Transform 동기화
	void SyncTransformFromPhysics();
};
