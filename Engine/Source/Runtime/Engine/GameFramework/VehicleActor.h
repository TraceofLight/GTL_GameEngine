#pragma once
#include "Actor.h"
#include "Source/Runtime/Engine/Vehicle/VehicleTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/WheeledVehicleMovementComponent.h"
#include "AVehicleActor.generated.h"

/**
 * @brief 4륜 차량 액터
 * @details 스켈레탈 메시와 차량 물리 컴포넌트를 가진 차량 액터
 *
 * @param MeshComponent 차량 스켈레탈 메시
 * @param VehicleMovement 차량 이동 컴포넌트
 * @param bIsPlayerControlled 플레이어 제어 여부
 */
UCLASS(DisplayName="차량", Description="4륜 차량 액터입니다")
class AVehicleActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AVehicleActor();
	~AVehicleActor() override = default;

	// Lifecycle
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	// Setup
	void SetSkeletalMesh(const FString& MeshPath);
	void SetVehicleSetup(const FVehicleSetupData& Setup);

	// Control
	void SetPlayerControlled(bool bControlled) { bIsPlayerControlled = bControlled; }
	bool IsPlayerControlled() const { return bIsPlayerControlled; }

	// Input (플레이어 제어 시)
	void SetThrottleInput(float Value);
	void SetBrakeInput(float Value);
	void SetSteerInput(float Value);
	void SetHandbrakeInput(float Value);

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

	UPROPERTY(EditAnywhere, Category="Vehicle")
	bool bIsPlayerControlled = false;

protected:
	// 키보드 입력 처리
	void ProcessKeyboardInput(float DeltaTime);

	// 컴포넌트 Transform 동기화
	void SyncTransformFromPhysics();
};
