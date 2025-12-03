#include "pch.h"
#include "VehicleActor.h"
#include "Source/Runtime/Engine/Components/WheeledVehicleMovementComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/InputCore/InputManager.h"
#include "PxPhysicsAPI.h"

using namespace physx;

IMPLEMENT_CLASS(AVehicleActor)

AVehicleActor::AVehicleActor()
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

	// 플레이어 제어 시 키보드 입력 처리
	if (bIsPlayerControlled)
	{
		ProcessKeyboardInput(DeltaTime);
	}

	// 컴포넌트 틱
	if (VehicleMovement)
	{
		VehicleMovement->TickComponent(DeltaTime);
	}
	if (MeshComponent)
	{
		MeshComponent->TickComponent(DeltaTime);
	}

	// Physics -> Actor Transform 동기화
	SyncTransformFromPhysics();
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

void AVehicleActor::SetThrottleInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetThrottleInput(Value);
	}
}

void AVehicleActor::SetBrakeInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetBrakeInput(Value);
	}
}

void AVehicleActor::SetSteerInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetSteerInput(Value);
	}
}

void AVehicleActor::SetHandbrakeInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetHandbrakeInput(Value);
	}
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

	// 컴포넌트 복제 후 참조 재설정
	if (VehicleMovement && MeshComponent)
	{
		VehicleMovement->SetSkeletalMeshComponent(MeshComponent);
	}
}

void AVehicleActor::ProcessKeyboardInput(float DeltaTime)
{
	UInputManager& Input = UInputManager::GetInstance();

	// 액셀 (W) / 브레이크 (S)
	float Throttle = 0.0f;
	float Brake = 0.0f;

	if (Input.IsKeyDown('W'))
	{
		Throttle = 1.0f;
	}
	if (Input.IsKeyDown('S'))
	{
		Brake = 1.0f;
	}

	// 후진 처리: S키만 눌린 상태에서 정지 또는 후진 중이면 후진
	const float ForwardSpeed = GetSpeed();
	if (Brake > 0.0f && ForwardSpeed <= 0.5f)
	{
		// 정지 상태에서 S키 = 후진
		Throttle = 0.5f;  // 약간의 후진 토크
		Brake = 0.0f;
		// 기어를 후진으로 변경해야 함 (현재는 단순 처리)
	}

	SetThrottleInput(Throttle);
	SetBrakeInput(Brake);

	// 스티어링 (A/D)
	float Steer = 0.0f;
	if (Input.IsKeyDown('A'))
	{
		Steer -= 1.0f;
	}
	if (Input.IsKeyDown('D'))
	{
		Steer += 1.0f;
	}
	SetSteerInput(Steer);

	// 핸드브레이크 (Space)
	float Handbrake = Input.IsKeyDown(VK_SPACE) ? 1.0f : 0.0f;
	SetHandbrakeInput(Handbrake);
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

	// Actor Transform 업데이트
	SetActorTransform(NewTransform);

	// MeshComponent도 동기화 (RootComponent이므로 자동으로 따라감)
}
