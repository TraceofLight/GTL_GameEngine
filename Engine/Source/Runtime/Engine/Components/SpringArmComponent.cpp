// ────────────────────────────────────────────────────────────────────────────
// SpringArmComponent.cpp
// SpringArmComponent 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "SpringArmComponent.h"
#include "Actor.h"
#include "Pawn.h"
#include "Controller.h"
#include "World.h"
#include "PxPhysicsAPI.h"
#include "CharacterMovementComponent.h"
#include "PrimitiveComponent.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

USpringArmComponent::USpringArmComponent()
	: TargetArmLength(3.0f)
	, CurrentArmLength(3.0f)
	, SocketOffset(FVector())
	, TargetOffset(FVector(0.0f, 0.0f, 0.5f))
	, bEnableCameraLag(false)
	, CameraLagSpeed(1.0f)
	, CameraLagMaxDistance(0.0f)
	, PreviousDesiredLocation(FVector())
	, PreviousActorLocation(FVector())
	, bEnableCameraRotationLag(false)
	, CameraRotationLagSpeed(1.0f)
	, PreviousDesiredRotation(FQuat::Identity())
	, bDoCollisionTest(true)
	, ProbeSize(0.12f)
	, bDrawDebugCollision(false)
	, bUsePawnControlRotation(false)
	, SocketLocation(FVector())
	, SocketRotation(FQuat::Identity())
{
	bCanEverTick = true;
}

USpringArmComponent::~USpringArmComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::TickComponent(float DeltaSeconds)
{
	Super::TickComponent(DeltaSeconds);

	FVector DesiredSocketLocation;
	FQuat DesiredSocketRotation;

	UpdateDesiredArmLocation(DeltaSeconds, DesiredSocketLocation, DesiredSocketRotation);

	// Collision Test
	FVector FinalSocketLocation = DesiredSocketLocation;
	if (bDoCollisionTest)
	{
		DoCollisionTest(DesiredSocketLocation, FinalSocketLocation);
	}

	// Socket 위치/회전 저장 (GetSocketLocation/Rotation에서 사용)
	SocketLocation = FinalSocketLocation;
	SocketRotation = DesiredSocketRotation;

	// 자식 컴포넌트(카메라)를 Socket 위치로 이동
	for (USceneComponent* Child : GetAttachChildren())
	{
		if (Child)
		{
			Child->SetWorldLocation(SocketLocation);
			Child->SetWorldRotation(SocketRotation);
		}
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Socket Transform 조회
// ────────────────────────────────────────────────────────────────────────────

FVector USpringArmComponent::GetSocketLocation() const
{
	return SocketLocation;
}

FQuat USpringArmComponent::GetSocketRotation() const
{
	return SocketRotation;
}

FTransform USpringArmComponent::GetSocketTransform() const
{
	return FTransform(SocketLocation, SocketRotation, FVector(1.0f, 1.0f, 1.0f));
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 함수
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::UpdateDesiredArmLocation(float DeltaTime, FVector& OutDesiredLocation, FQuat& OutDesiredRotation)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutDesiredLocation = GetWorldLocation();
		OutDesiredRotation = GetWorldRotation();
		return;
	}

	// 회전 결정 - bUsePawnControlRotation이면 Controller 회전, 아니면 Owner 회전
	FQuat OwnerRotation = OwnerActor->GetActorRotation();

	if (bUsePawnControlRotation)
	{
		// Owner가 Pawn이면 Controller의 회전을 사용
		if (APawn* Pawn = Cast<APawn>(OwnerActor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				OwnerRotation = Controller->GetControlRotation();
			}
		}
	}

	// 기본 위치: Owner의 위치 + TargetOffset (Owner의 로컬 좌표계 기준)
	FVector OwnerLocation = OwnerActor->GetActorLocation();
	FVector RotatedTargetOffset = OwnerRotation.RotateVector(TargetOffset);
	FVector TargetLocation = OwnerLocation + RotatedTargetOffset;

	// Spring Arm 방향 계산 (뒤쪽)
	FVector ArmDirection = OwnerRotation.GetForwardVector() * -1.0f; // Backward direction

	// SocketOffset도 Owner 회전 적용
	FVector RotatedSocketOffset = OwnerRotation.RotateVector(SocketOffset);

	// Desired Location
	FVector UnlaggedDesiredLocation = TargetLocation + ArmDirection * TargetArmLength + RotatedSocketOffset;

	// Camera Lag 적용
	if (bEnableCameraLag)
	{
		// 이전 위치가 초기값이면 즉시 설정
		if (PreviousDesiredLocation.IsZero())
		{
			PreviousDesiredLocation = UnlaggedDesiredLocation;
			PreviousActorLocation = OwnerLocation;
		}

		// Lag 적용
		FVector LagVector = UnlaggedDesiredLocation - PreviousDesiredLocation;
		float LagDistance = LagVector.Size();

		// MaxDistance 제한
		if (CameraLagMaxDistance > 0.0f && LagDistance > CameraLagMaxDistance)
		{
			PreviousDesiredLocation = UnlaggedDesiredLocation - LagVector.GetNormalized() * CameraLagMaxDistance;
		}

		// Lerp
		OutDesiredLocation = FVector::Lerp(PreviousDesiredLocation, UnlaggedDesiredLocation,
			FMath::Min(1.0f, DeltaTime * CameraLagSpeed));
		PreviousDesiredLocation = OutDesiredLocation;
		PreviousActorLocation = OwnerLocation;
	}
	else
	{
		OutDesiredLocation = UnlaggedDesiredLocation;
	}

	// Rotation Lag 적용
	if (bEnableCameraRotationLag)
	{
		if (PreviousDesiredRotation.IsIdentity())
		{
			PreviousDesiredRotation = OwnerRotation;
		}

		float Alpha = FMath::Min(1.0f, DeltaTime * CameraRotationLagSpeed);
		OutDesiredRotation = FQuat::Slerp(PreviousDesiredRotation, OwnerRotation, Alpha);
		PreviousDesiredRotation = OutDesiredRotation;
	}
	else
	{
		OutDesiredRotation = OwnerRotation;
	}
}

bool USpringArmComponent::DoCollisionTest(const FVector& DesiredLocation, FVector& OutLocation)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutLocation = DesiredLocation;
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutLocation = DesiredLocation;
		return false;
	}

	PxScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		OutLocation = DesiredLocation;
		return false;
	}

	// 스윕 시작점: Owner 위치 + TargetOffset
	FQuat OwnerRotation = OwnerActor->GetActorRotation();
	FVector OwnerLocation = OwnerActor->GetActorLocation();
	FVector RotatedTargetOffset = OwnerRotation.RotateVector(TargetOffset);
	FVector SweepStart = OwnerLocation + RotatedTargetOffset;

	// 스윕 끝점: 원하는 카메라 위치
	FVector SweepEnd = DesiredLocation;
	FVector SweepDirection = (SweepEnd - SweepStart);
	float SweepDistance = SweepDirection.Size();
	SweepDirection.Normalize();

	FGroundHitResult HitResult;
	bool bHit = SweepCapsule(SweepStart, SweepDirection, SweepDistance, HitResult);

	if (bHit && HitResult.bBlockingHit)
	{
		// 초기 겹침 (Distance가 너무 작음) - 무시
		if (HitResult.Distance < 0.1f)
		{
			if (bDrawDebugCollision)
			{
				AActor* HitActor = HitResult.HitActor;
				UE_LOG("[SpringArm] IGNORED (InitialOverlap) Dist:%.2f HitActor:%s",
					HitResult.Distance,
					HitActor ? HitActor->GetName().c_str() : "null");
			}
			OutLocation = DesiredLocation;
			CurrentArmLength = TargetArmLength;
			return false;
		}

		// Distance를 사용해서 충돌 위치 계산
		FVector SweepDirection = (SweepEnd - SweepStart).GetNormalized();
		FVector HitLocation = SweepStart + SweepDirection * HitResult.Distance;

		// 충돌 위치를 카메라 위치로 사용
		OutLocation = HitLocation;

		// 현재 암 길이 업데이트
		CurrentArmLength = HitResult.Distance;

		// 디버그 로그
		if (bDrawDebugCollision)
		{
			AActor* HitActor = HitResult.HitActor;
			UE_LOG("[SpringArm] HIT! Dist:%.2f ArmLen:%.2f HitActor:%s",
				HitResult.Distance,
				CurrentArmLength,
				HitActor ? HitActor->GetName().c_str() : "null");
		}
		return true;
	}

	// 충돌 없음 - 원래 위치 사용
	OutLocation = DesiredLocation;
	CurrentArmLength = TargetArmLength;

	// 디버그 로그
	if (bDrawDebugCollision)
	{
		UE_LOG("[SpringArm] NoHit Start:(%.2f,%.2f,%.2f) End:(%.2f,%.2f,%.2f)",
			SweepStart.X, SweepStart.Y, SweepStart.Z,
			OutLocation.X, OutLocation.Y, OutLocation.Z);
	}
	return false;
}


// ────────────────────────────────────────────────────────────────────────────
// 복제 및 직렬화
// ────────────────────────────────────────────────────────────────────────────

void USpringArmComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void USpringArmComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: 필요한 멤버 변수 직렬화 추가
}



bool USpringArmComponent::SweepCapsule(const FVector& Start, const FVector& Direction, float Distance, FGroundHitResult& OutHit)
{
	UWorld* World = Owner->GetWorld();

	PxScene* Scene = World->GetPhysicsScene();
	if (!Scene)
	{
		return false;
	}

	// PhysX 캡슐 지오메트리 생성
	// PhysX의 캡슐은 X축을 따라 정렬됨, 우리는 Z축(수직) 정렬이 필요
	PxSphereGeometry SphereGeom(ProbeSize);

	// 시작 위치 및 방향 설정
	PxVec3 PxStart(Start.X, Start.Y, Start.Z);
	PxVec3 PxDir(Direction.X, Direction.Y, Direction.Z);
	PxDir.normalize();

	// 캡슐 회전: PhysX 캡슐은 X축 정렬, 우리는 Z축(up) 정렬이 필요
	// X축을 Z축으로 회전 (Y축 기준 90도 회전)
	PxTransform StartPose(PxStart);

	// 스윕 결과 버퍼
	PxSweepBuffer SweepHit;

	// 필터 데이터: 정적 오브젝트만 충돌 (캐릭터 자신의 동적 바디 제외)
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC;

	// Scene 락 필요 (스윕 전)
	Scene->lockRead();

	// 캡슐 스윕 수행
	bool bHit = Scene->sweep(
		SphereGeom,
		StartPose,
		PxDir,
		Distance,
		SweepHit,
		PxHitFlag::eDEFAULT | PxHitFlag::eNORMAL,
		FilterData
	);

	Scene->unlockRead();

	if (bHit && SweepHit.hasBlock)
	{
		const PxSweepHit& Hit = SweepHit.block;

		OutHit.bBlockingHit = true;
		OutHit.ImpactPoint = FVector(Hit.position.x, Hit.position.y, Hit.position.z);
		OutHit.ImpactNormal = FVector(Hit.normal.x, Hit.normal.y, Hit.normal.z);
		OutHit.Distance = Hit.distance;

		// 충돌한 액터 추출 (있을 경우)
		if (Hit.actor)
		{
			void* UserData = Hit.actor->userData;
			if (UserData)
			{
				// UserData가 UPrimitiveComponent를 가리킬 경우
				UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(UserData);
				if (PrimComp)
				{
					OutHit.HitActor = PrimComp->GetOwner();
				}
			}
		}
		return true;
	}

	return false;
}
