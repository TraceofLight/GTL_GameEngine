#include "pch.h"
#include "VehicleActor.h"
#include "Source/Runtime/Engine/Components/WheeledVehicleMovementComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/InputComponent.h"
#include "PxPhysicsAPI.h"

using namespace physx;

IMPLEMENT_CLASS(AVehicleActor)

AVehicleActor::AVehicleActor()
	: APawn()
{
	// 스켈레탈 메시 컴포넌트 생성
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("VehicleMesh");
	if (MeshComponent)
	{
		RootComponent = MeshComponent;
		MeshComponent->SetOwner(this);
	}

	// 차량 이동 컴포넌트 생성
	VehicleMovement = CreateDefaultSubobject<UWheeledVehicleMovementComponent>("VehicleMovement");
	if (VehicleMovement)
	{
		VehicleMovement->SetOwner(this);
		VehicleMovement->SetSkeletalMeshComponent(MeshComponent);
	}
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();

	// PIE가 아닌 Editor World에서는 컴포넌트 BeginPlay 호출하지 않음
	UWorld* World = GetWorld();
	if (!World || !World->bPie)
	{
		return;
	}

	// 컴포넌트들 BeginPlay 호출
	if (MeshComponent)
	{
		MeshComponent->BeginPlay();
	}
	if (VehicleMovement)
	{
		VehicleMovement->BeginPlay();
	}
}

void AVehicleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 입력 상태에 따라 VehicleMovement에 입력 전달
	if (VehicleMovement)
	{
		// 아케이드 스타일 조작: W=전진, S=후진 (자동 기어)
		constexpr float MaxForwardSpeed = 30.0f;   // 최대 전진 속도 [m/s] (~108 km/h)
		constexpr float MaxReverseSpeed = -10.0f;  // 최대 후진 속도 [m/s] (~36 km/h)

		float CurrentSpeed = VehicleMovement->GetForwardSpeed();
		int32 CurrentGear = VehicleMovement->GetCurrentGear();

		if (bThrottlePressed)
		{
			// 전진: 후진 중이면 먼저 브레이크, 아니면 가속
			if (CurrentSpeed < -0.5f)
			{
				// 후진 중 → 브레이크로 감속
				VehicleMovement->SetBrakeInput(1.0f);
				VehicleMovement->SetThrottleInput(0.0f);
			}
			else
			{
				// 전진 기어로 전환 (필요시)
				if (CurrentGear == 0)
				{
					VehicleMovement->SetTargetGear(2, true);  // 1단 기어
				}

				// 속도 제한 체크
				if (CurrentSpeed < MaxForwardSpeed)
				{
					VehicleMovement->SetThrottleInput(1.0f);
				}
				else
				{
					VehicleMovement->SetThrottleInput(0.0f);  // 속도 제한 도달
				}
				VehicleMovement->SetBrakeInput(0.0f);
			}
		}
		else if (bBrakePressed)
		{
			// 후진: 전진 중이면 먼저 브레이크, 아니면 후진 가속
			if (CurrentSpeed > 0.5f)
			{
				// 전진 중 → 브레이크로 감속
				VehicleMovement->SetBrakeInput(1.0f);
				VehicleMovement->SetThrottleInput(0.0f);
			}
			else
			{
				// 후진 기어로 전환 (필요시)
				if (CurrentGear != 0)
				{
					VehicleMovement->SetTargetGear(0, true);  // 후진 기어
				}

				// 속도 제한 체크 (후진은 음수)
				if (CurrentSpeed > MaxReverseSpeed)
				{
					VehicleMovement->SetThrottleInput(1.0f);  // 후진 기어에서 throttle = 후진
				}
				else
				{
					VehicleMovement->SetThrottleInput(0.0f);  // 속도 제한 도달
				}
				VehicleMovement->SetBrakeInput(0.0f);
			}
		}
		else
		{
			// 아무 키도 안 누름 → 자연 감속
			VehicleMovement->SetThrottleInput(0.0f);
			VehicleMovement->SetBrakeInput(0.0f);
		}

		// Steering 처리 (매 프레임 설정)
		float SteerValue = 0.0f;
		if (bSteerLeftPressed)
		{
			SteerValue = -1.0f;
		}
		else if (bSteerRightPressed)
		{
			SteerValue = 1.0f;
		}
		VehicleMovement->SetSteerInput(SteerValue);

		// Handbrake
		VehicleMovement->SetHandbrakeInput(bHandbrakePressed ? 1.0f : 0.0f);

		// 컴포넌트 틱
		VehicleMovement->TickComponent(DeltaTime);

		// 물리 → 비주얼 트랜스폼 동기화
		SyncTransformFromPhysics();
	}

	if (MeshComponent)
	{
		MeshComponent->TickComponent(DeltaTime);
	}
}

void AVehicleActor::SetSkeletalMesh(const FString& MeshPath)
{
	if (MeshComponent)
	{
		MeshComponent->SetSkeletalMesh(MeshPath);
	}
}

void AVehicleActor::SetVehicleSetup(const FVehicleSetupData& Setup)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetVehicleSetup(Setup);
	}
}

// ============================================================================
// 입력 바인딩 설정
// ============================================================================

void AVehicleActor::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	if (!InInputComponent)
	{
		return;
	}

	// Action 바인딩 (KeyCode와 Pressed/Released 함수 지정)
	// W키: Throttle
	InInputComponent->BindAction("Throttle", 'W', this, &AVehicleActor::ThrottlePressed, &AVehicleActor::ThrottleReleased);

	// S키: Brake
	InInputComponent->BindAction("Brake", 'S', this, &AVehicleActor::BrakePressed, &AVehicleActor::BrakeReleased);

	// Space: Handbrake
	InInputComponent->BindAction("Handbrake", VK_SPACE, this, &AVehicleActor::HandbrakePressed, &AVehicleActor::HandbrakeReleased);

	// A키: 좌회전
	InInputComponent->BindAction("SteerLeft", 'A', this, &AVehicleActor::SteerLeftPressed, &AVehicleActor::SteerLeftReleased);

	// D키: 우회전
	InInputComponent->BindAction("SteerRight", 'D', this, &AVehicleActor::SteerRightPressed, &AVehicleActor::SteerRightReleased);
}

// ============================================================================
// 입력 핸들러 함수
// ============================================================================

void AVehicleActor::ThrottlePressed()
{
	bThrottlePressed = true;
}

void AVehicleActor::ThrottleReleased()
{
	bThrottlePressed = false;
}

void AVehicleActor::BrakePressed()
{
	bBrakePressed = true;
}

void AVehicleActor::BrakeReleased()
{
	bBrakePressed = false;
}

void AVehicleActor::SteerLeftPressed()
{
	bSteerLeftPressed = true;
}

void AVehicleActor::SteerLeftReleased()
{
	bSteerLeftPressed = false;
}

void AVehicleActor::SteerRightPressed()
{
	bSteerRightPressed = true;
}

void AVehicleActor::SteerRightReleased()
{
	bSteerRightPressed = false;
}

void AVehicleActor::HandbrakePressed()
{
	bHandbrakePressed = true;
}

void AVehicleActor::HandbrakeReleased()
{
	bHandbrakePressed = false;
}

float AVehicleActor::GetSpeed() const
{
	if (VehicleMovement)
	{
		return VehicleMovement->GetForwardSpeed();
	}
	return 0.0f;
}

float AVehicleActor::GetEngineRPM() const
{
	if (VehicleMovement)
	{
		return VehicleMovement->GetEngineRPM();
	}
	return 0.0f;
}

int32 AVehicleActor::GetCurrentGear() const
{
	if (VehicleMovement)
	{
		return VehicleMovement->GetCurrentGear();
	}
	return 0;
}

void AVehicleActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 복제된 컴포넌트를 OwnedComponents에서 다시 찾아야 함
	// (Super::DuplicateSubObjects가 새 컴포넌트를 생성하지만 멤버 포인터는 업데이트 안 함)
	VehicleMovement = nullptr;
	MeshComponent = nullptr;

	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Component)
		{
			continue;
		}

		if (UWheeledVehicleMovementComponent* VehicleComp = dynamic_cast<UWheeledVehicleMovementComponent*>(Component))
		{
			VehicleMovement = VehicleComp;
		}
		else if (USkeletalMeshComponent* MeshComp = dynamic_cast<USkeletalMeshComponent*>(Component))
		{
			MeshComponent = MeshComp;
			RootComponent = MeshComp;
		}
	}

	// 컴포넌트 간 참조 재설정
	if (VehicleMovement && MeshComponent)
	{
		VehicleMovement->SetSkeletalMeshComponent(MeshComponent);
	}
}

void AVehicleActor::SyncTransformFromPhysics()
{
	if (!VehicleMovement)
	{
		return;
	}

	PxRigidDynamic* PhysActor = VehicleMovement->GetVehicleActor();
	if (!PhysActor)
	{
		return;
	}

	// PhysX Transform -> Actor Transform
	PxTransform PxTrans = PhysActor->getGlobalPose();

	FTransform NewTransform;
	NewTransform.Translation = FVector(PxTrans.p.x, PxTrans.p.y, PxTrans.p.z);
	NewTransform.Rotation = FQuat(PxTrans.q.x, PxTrans.q.y, PxTrans.q.z, PxTrans.q.w);
	NewTransform.Scale3D = GetActorScale();

	// 비주얼 메시 Z 오프셋 보정
	// PhysX 액터 원점과 비주얼 메시 원점의 차이를 보정
	// 메시 원점이 바닥에 있고, 물리 액터가 CM 기준이면 오프셋 필요
	constexpr float VisualMeshZOffset = 0.5f;  // 필요시 조정
	NewTransform.Translation.Z += VisualMeshZOffset;

	// Actor Transform 업데이트
	SetActorTransform(NewTransform);

	// MeshComponent도 동기화 (RootComponent이므로 자동으로 따라감)
}
