// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.cpp
// Character 이동 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CharacterMovementComponent.h"
#include "Character.h"
#include "World.h"
#include "PxPhysicsAPI.h"
#include "PrimitiveComponent.h"

using namespace physx;
//
//IMPLEMENT_CLASS(UCharacterMovementComponent)
//
//BEGIN_PROPERTIES(UCharacterMovementComponent)
//	ADD_PROPERTY(float, MaxWalkSpeed, "Movement", true, "최대 걷기 속도 (cm/s)")
//	ADD_PROPERTY(float, JumpZVelocity, "Movement", true, "점프 초기 속도 (cm/s)")
//	ADD_PROPERTY(float, GravityScale, "Movement", true, "중력 스케일 (1.0 = 기본 중력)")
//END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

UCharacterMovementComponent::UCharacterMovementComponent()
	: CharacterOwner(nullptr)
	, Velocity(FVector())
	, PendingInputVector(FVector())
	, MovementMode(EMovementMode::Falling)
	, TimeInAir(0.0f)
	, bIsJumping(false)
	, bJumpHeld(false)
	, CoyoteTimeCounter(0.0f)
	, JumpBufferCounter(0.0f)
	, bWasGroundedLastFrame(false)
	, bIsRotating(false)
	// 이동 설정
	, MaxWalkSpeed(60.0f)           // 2.0 m/s
	, MaxAcceleration(15.0f)        // 4.0 m/s²
	, GroundFriction(8.0f)
	, AirControl(0.05f)
	, BreakingDeceleration(20.48f)
	// 중력 설정
	, GravityScale(1.0f)
	, GravityDirection(0.0f, 0.0f, -1.0f) // 기본값: 아래 방향
	// 점프 설정
	, JumpZVelocity(20.2f)          // 4.2 m/s
	, MaxAirTime(2.0f)
	, bCanJump(true)
	, JumpGravityScale(0.8f)        // 상승 중 중력 80%
	, FallGravityScale(1.5f)        // 하강 중 중력 150%
	, JumpCutMultiplier(0.4f)       // 점프 키 놓으면 상승 속도 40%로 감소
	, CoyoteTime(0.1f)              // 0.1초 코요테 타임
	, JumpBufferTime(0.1f)          // 0.1초 점프 버퍼
	// 캡슐 스윕 설정
	, bUseSweepForGroundCheck(false) // 기본값: 비활성화
	, CapsuleRadius(0.03f)
	, CapsuleHalfHeight(0.03f)
	, GroundCheckDistance(0.001f)
	, MaxWalkableSlopeAngle(45.0f)  // 45도까지 걸을 수 있음
{
	bCanEverTick = true;
}

UCharacterMovementComponent::~UCharacterMovementComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Owner를 Character로 캐스팅
	CharacterOwner = Cast<ACharacter>(GetOwner());
}

void UCharacterMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!CharacterOwner)
	{
		return;
	}

	// 1. 속도 업데이트 (입력, 마찰, 가속)
	UpdateVelocity(DeltaTime);

	// 2. 중력 적용
	ApplyGravity(DeltaTime);

	// 3. 위치 업데이트
	MoveUpdatedComponent(DeltaTime);

	// 4. 지면 체크
	bool bWasGrounded = IsGrounded();
	bool bIsNowGrounded = CheckGround();

	// 5. 코요테 타임 및 점프 버퍼 업데이트
	if (bIsNowGrounded)
	{
		// 땅에 있으면 코요테 타임 리셋
		CoyoteTimeCounter = CoyoteTime;
	}
	else
	{
		// 공중에 있으면 코요테 타임 감소
		CoyoteTimeCounter -= DeltaTime;
		if (CoyoteTimeCounter < 0.0f)
		{
			CoyoteTimeCounter = 0.0f;
		}
	}

	// 점프 버퍼 카운터 감소
	if (JumpBufferCounter > 0.0f)
	{
		JumpBufferCounter -= DeltaTime;
	}

	// 6. 이동 모드 업데이트
	if (bIsNowGrounded && !bWasGrounded)
	{
		// 착지 - 중력 방향의 속도 성분 제거
		SetMovementMode(EMovementMode::Walking);
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
		TimeInAir = 0.0f;
		bIsJumping = false;

		// 착지 시 점프 버퍼 체크 - 착지 직전에 점프를 눌렀다면 즉시 점프
		if (JumpBufferCounter > 0.0f && bCanJump)
		{
			Jump();
		}
	}
	else if (!bIsNowGrounded && bWasGrounded)
	{
		// 낙하 시작
		SetMovementMode(EMovementMode::Falling);
	}

	// 7. 공중 시간 체크
	if (IsFalling())
	{
		TimeInAir += DeltaTime;

		// 너무 오래 공중에 있으면 리셋
		if (TimeInAir > MaxAirTime)
		{
			TimeInAir = 0.0f;
		}
	}

	// 마지막 프레임 땅 상태 저장
	bWasGroundedLastFrame = bIsNowGrounded;

	// 입력 초기화
	PendingInputVector = FVector();
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 함수
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::AddInputVector(FVector WorldDirection, float ScaleValue)
{
	if (ScaleValue == 0.0f || WorldDirection.SizeSquared() == 0.0f)
	{
		return;
	}

	// 중력 방향에 수직인 평면으로 입력 제한
	// 입력 벡터에서 중력 방향 성분을 제거 (투영 후 빼기)
	float DotWithGravity = FVector::Dot(WorldDirection, GravityDirection);
	FVector HorizontalDirection = WorldDirection - (GravityDirection * DotWithGravity);

	// 방향 벡터가 0이면 무시
	if (HorizontalDirection.SizeSquared() < 0.0001f)
	{
		return;
	}

	FVector NormalizedDirection = HorizontalDirection.GetNormalized();
 	PendingInputVector += NormalizedDirection * ScaleValue;
}

bool UCharacterMovementComponent::Jump()
{
	if (!bCanJump)
	{
		return false;
	}

	// 점프 버퍼 설정 - 점프 입력 기록
	JumpBufferCounter = JumpBufferTime;
	bJumpHeld = true;

	// 점프 가능 조건 체크: 땅에 있거나 코요테 타임 내
	bool bCanJumpNow = IsGrounded() || (CoyoteTimeCounter > 0.0f);

	if (!bCanJumpNow)
	{
		return false;
	}

	// 코요테 타임 소비
	CoyoteTimeCounter = 0.0f;
	JumpBufferCounter = 0.0f;

	// 기존 수직 속도 제거 후 점프 속도 적용 (더 일관된 점프 높이)
	float CurrentVerticalSpeed = FVector::Dot(Velocity, GravityDirection);
	Velocity -= GravityDirection * CurrentVerticalSpeed;

	// 중력 반대 방향으로 점프 속도 적용
	FVector JumpVelocity = GravityDirection * -1.0f * JumpZVelocity;
	Velocity += JumpVelocity;

	// 이동 모드 변경
	SetMovementMode(EMovementMode::Falling);
	bIsJumping = true;

	return true;
}

void UCharacterMovementComponent::StopJumping()
{
	bJumpHeld = false;

	// 점프 키를 뗐을 때 상승 중이면 속도 감소 (가변 점프 높이)
	FVector UpDirection = GravityDirection * -1.0f;
	float UpwardSpeed = FVector::Dot(Velocity, UpDirection);

	if (bIsJumping && UpwardSpeed > 0.0f)
	{
		// 상승 속도를 JumpCutMultiplier 비율로 감소
		Velocity -= UpDirection * (UpwardSpeed * (1.0f - JumpCutMultiplier));
	}
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMode)
{
	if (MovementMode == NewMode)
	{
		return;
	}

	EMovementMode PrevMode = MovementMode;
	MovementMode = NewMode;

	// 모드 전환 시 처리
	if (MovementMode == EMovementMode::Walking)
	{
		// 착지 시 중력 방향의 속도 성분 제거
		FVector UpDirection = GravityDirection * -1.0f;
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 내부 이동 로직
// ────────────────────────────────────────────────────────────────────────────

void UCharacterMovementComponent::UpdateVelocity(float DeltaTime)
{
    float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
    FVector HorizontalVelocity = Velocity - (GravityDirection * VerticalSpeed);

    // 입력 벡터 처리 (회전 중이면 입력 무시)
    FVector InputVector = (bIsRotating) ? FVector::Zero() : PendingInputVector;

    // 입력이 있는 경우 (가속)
    if (InputVector.SizeSquared() > 0.0f)
    {
        FVector InputDirection = InputVector.GetNormalized();

        // 공중/지상에 따른 제어력 및 가속도 설정
        float CurrentControl = IsGrounded() ? 1.0f : AirControl;
        float AccelRate = MaxAcceleration * CurrentControl;

        // 목표 속도
        FVector TargetVelocity = InputDirection * MaxWalkSpeed;

        // 목표 속도도 수평 성분만 추출 (Input이 중력 방향을 포함할 수도 있으므로 안전장치)
        float TargetVertical = FVector::Dot(TargetVelocity, GravityDirection);
        FVector HorizontalTarget = TargetVelocity - (GravityDirection * TargetVertical);

        // 가속 적용 (목표 속도를 향해 보간)
        FVector Delta = HorizontalTarget - HorizontalVelocity;
        float DeltaSize = Delta.Size();

        if (DeltaSize > 0.0f)
        {
            FVector AccelDir = Delta / DeltaSize;
            // DeltaTime 동안 가속할 수 있는 양만큼만 가속 (오버슈팅 방지)
            float AccelAmount = FMath::Min(DeltaSize, AccelRate * DeltaTime);

            HorizontalVelocity += AccelDir * AccelAmount;
        }

        // 최대 속도 제한
        if (HorizontalVelocity.SizeSquared() > (MaxWalkSpeed * MaxWalkSpeed))
        {
            HorizontalVelocity = HorizontalVelocity.GetNormalized() * MaxWalkSpeed;
        }
    }
    // 입력이 없는 경우 (감속/마찰)
    else
    {
        float CurrentSpeed = HorizontalVelocity.Size();

        if (CurrentSpeed > 0.0f)
        {
            float DecelAmount;
            if (IsGrounded())
            {
                // 지상: 마찰력 * 제동력 (Friction이 높으면 더 빨리 멈춤)
                DecelAmount = BreakingDeceleration * FMath::Max(1.0f, GroundFriction) * DeltaTime;
            }
            else
            {
                // 공중
                DecelAmount = AirControl * DeltaTime;
            }

            // 선형 감속 적용 (속도를 0 이하로 떨어뜨리지 않음)
            float NewSpeed = FMath::Max(0.0f, CurrentSpeed - DecelAmount);

            // 벡터 길이 조절
            HorizontalVelocity = HorizontalVelocity * (NewSpeed / CurrentSpeed);
        }
    }

    // 3. 최종 속도 재조립 (수직 속도는 건드리지 않고 그대로 붙임)
    // 참고: 중력 가속도는 보통 ApplyGravity() 같은 별도 함수에서 VerticalSpeed를 갱신해줘야 함
    Velocity = HorizontalVelocity + (GravityDirection * VerticalSpeed);
}

void UCharacterMovementComponent::ApplyGravity(float DeltaTime)
{
	// 회전 중에는 중력 적용 안 함
	if (bIsRotating)
	{
		return;
	}

	// 지면에 있으면 중력 적용 안 함
	if (IsGrounded())
	{
		return;
	}

	// 현재 수직 속도 계산 (중력 방향 기준)
	FVector UpDirection = -GravityDirection;
	float VerticalVelocity = FVector::Dot(Velocity, UpDirection);

	// 상승 중인지 하강 중인지에 따라 다른 중력 스케일 적용
	float CurrentGravityScale = GravityScale;
	if (VerticalVelocity > 0.0f)
	{
		// 상승 중 - JumpGravityScale 적용 (낮으면 더 높이 뜸)
		CurrentGravityScale *= JumpGravityScale;
	}
	else
	{
		// 하강 중 - FallGravityScale 적용 (높으면 빨리 떨어짐)
		CurrentGravityScale *= FallGravityScale;
	}

	// 중력 가속도 적용 (방향 벡터 사용)
	float GravityMagnitude = DefaultGravity * CurrentGravityScale;
	FVector GravityVector = GravityDirection * GravityMagnitude;
	Velocity += GravityVector * DeltaTime;

	// 최대 낙하 속도 제한 (터미널 속도)
	// 중력 방향으로의 속도 성분을 체크
	float VelocityInGravityDir = FVector::Dot(Velocity, GravityDirection);
	constexpr float MaxFallSpeed = 40.0f; // 40 m/s
	if (VelocityInGravityDir > MaxFallSpeed)
	{
		// 중력 방향 속도 성분만 제한
		FVector GravityComponent = GravityDirection * VelocityInGravityDir;
		FVector OtherComponent = Velocity - GravityComponent;
		Velocity = OtherComponent + GravityDirection * MaxFallSpeed;
	}
}

void UCharacterMovementComponent::SetGravityDirection(const FVector& NewDirection)
{
	// 벡터를 정규화하여 저장
	if (NewDirection.SizeSquared() > 0.0f)
	{
		GravityDirection = NewDirection.GetNormalized();
	}
	else
	{
		// 유효하지 않은 방향이면 기본값으로
		GravityDirection = FVector(0.0f, 0.0f, -1.0f);
	}
}

void UCharacterMovementComponent::SetIsRotating(bool bInIsRotating)
{
	bIsRotating = bInIsRotating;

	// 회전 시작 시: 현재 중력 방향의 속도 성분을 제거 (낙하 중이었어도 멈춤)
	if (bIsRotating)
	{
		// 중력 방향의 속도 성분 계산
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);

		// 중력 방향 속도 제거 (수평 속도만 유지)
		Velocity -= GravityDirection * VerticalSpeed;
	}
}

void UCharacterMovementComponent::MoveUpdatedComponent(float DeltaTime)
{
	if (!CharacterOwner || Velocity.SizeSquared() == 0.0f)
	{
		return;
	}

	FVector CurrentLocation = CharacterOwner->GetActorLocation();
	FVector Delta = Velocity * DeltaTime;
	float DeltaSize = Delta.Size();

	if (DeltaSize < 0.0001f)
	{
		return;
	}

	// 스윕 비활성화 시 단순 이동
	if (!bUseSweepForGroundCheck)
	{
		FVector NewLocation = CurrentLocation + Delta;
		CharacterOwner->SetActorLocation(NewLocation);
		return;
	}

	// PIE 체크 - 물리 씬이 없으면 스윕 없이 이동
	UWorld* World = CharacterOwner->GetWorld();
	if (!World || !World->bPie || !World->GetPhysicsScene())
	{
		// 물리 씬 없음 - 단순 이동
		FVector NewLocation = CurrentLocation + Delta;
		CharacterOwner->SetActorLocation(NewLocation);
		return;
	}

	// 이동 방향으로 캡슐 스윕 수행
	FVector MoveDirection = Delta.GetNormalized();

	FGroundHitResult HitResult;
	bool bHit = SweepCapsule(CurrentLocation, MoveDirection, DeltaSize, HitResult);

	if (bHit && HitResult.Distance < DeltaSize)
	{
		// 충돌한 표면이 걸을 수 있는 경사인지 확인
		if (IsWalkableSurface(HitResult.ImpactNormal))
		{
			// 걸을 수 있는 경사 - 경사면을 따라 이동 (슬로프 워킹)
			// 이동 벡터를 경사면에 투영하여 경사를 따라 이동
			float VelocityAlongNormal = FVector::Dot(Delta, HitResult.ImpactNormal);
			FVector SlideVector = Delta - HitResult.ImpactNormal * VelocityAlongNormal;

			FVector NewLocation = CurrentLocation + SlideVector;
			CharacterOwner->SetActorLocation(NewLocation);
		}
		else
		{
			// 걸을 수 없는 벽 - 충돌 지점까지만 이동
			float SafeDistance = FMath::Max(0.0f, HitResult.Distance - 0.1f); // 약간의 여유
			FVector NewLocation = CurrentLocation + MoveDirection * SafeDistance;
			CharacterOwner->SetActorLocation(NewLocation);

			// 충돌면에 대해 속도 조정 (슬라이딩)
			// 속도에서 충돌 노말 방향 성분을 제거
			float VelocityAlongNormal = FVector::Dot(Velocity, HitResult.ImpactNormal);
			if (VelocityAlongNormal < 0.0f) // 표면을 향해 이동 중일 때만
			{
				Velocity -= HitResult.ImpactNormal * VelocityAlongNormal;
			}
		}
	}
	else
	{
		// 충돌 없음 - 전체 이동
		FVector NewLocation = CurrentLocation + Delta;
		CharacterOwner->SetActorLocation(NewLocation);
	}
}


void UCharacterMovementComponent::SetOnWallCollisionCallback(sol::function Callback)
{
	WallCollisionLuaCallback = Callback;
}

bool UCharacterMovementComponent::CheckGround()
{
	if (!CharacterOwner)
	{
		return false;
	}

	FVector CurrentLocation = CharacterOwner->GetActorLocation();

	// 스윕 비활성화 시 기존 Z==0 로직 사용
	if (!bUseSweepForGroundCheck)
	{
		if (CurrentLocation.Z <= 0.0f)
		{
			// 위치를 0으로 고정
			CurrentLocation.Z = 0.0f;
			CharacterOwner->SetActorLocation(CurrentLocation);

			// 중력 방향 속도 제거 (낙하 중이었다면)
			float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
			if (VerticalSpeed > 0.0f)
			{
				Velocity -= GravityDirection * VerticalSpeed;
			}

			return true;
		}
		return false;
	}

	// === 스윕 기반 지면 체크 ===

	// PIE 체크 - PIE가 아니면 스윕 불가능하므로 false 반환 (낙하 상태)
	UWorld* World = CharacterOwner->GetWorld();
	if (!World || !World->bPie || !World->GetPhysicsScene())
	{
		// 물리 씬이 없으면 지면 체크 불가 - 낙하 상태로 처리
		CurrentFloor = FGroundHitResult();
		return false;
	}

	// 스윕 시작점
	FVector SweepStart = CurrentLocation;

	// 스윕 방향: 중력 방향 (보통 아래)
	FVector SweepDirection = GravityDirection;

	// 스윕 거리: 지면 체크 거리 + 캡슐 반높이 (지면 위에 서있을 때 캡슐 중심에서 바닥까지의 거리)
	float SweepDistance = GroundCheckDistance + CapsuleHalfHeight;

	FGroundHitResult HitResult;
	bool bHit = SweepCapsule(SweepStart, SweepDirection, SweepDistance, HitResult);

	// 스윕이 아무것도 안 맞으면 지면 없음
	if (!bHit)
	{
		CurrentFloor = FGroundHitResult();
		return false;
	}

	if (IsWalkableSurface(HitResult.ImpactNormal))
	{
		CurrentFloor = HitResult;

		// 지면과의 거리가 GroundCheckDistance 이내이면 지면에 있는 것으로 판정
		// HitResult.Distance는 캡슐 중심에서 충돌까지의 거리
		if (HitResult.Distance <= GroundCheckDistance + CapsuleHalfHeight)
		{
			// 캐릭터를 지면 위에 정확히 배치
			// 충돌 지점에서 캡슐 반높이만큼 위로 올려야 캡슐이 지면 위에 정확히 위치
			FVector GroundPosition = HitResult.ImpactPoint - (GravityDirection * CapsuleHalfHeight);

			// 현재 위치가 지면 아래로 파묻혀 있다면 보정
			float CurrentDepthAlongGravity = FVector::Dot(CurrentLocation - GroundPosition, GravityDirection);
			if (CurrentDepthAlongGravity > 0.01f) // 지면 아래에 있음
			{
				CharacterOwner->SetActorLocation(GroundPosition);
			}

			// 중력 방향 속도 제거 (낙하 중이었다면)
			float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
			if (VerticalSpeed > 0.0f) // 중력 방향으로 이동 중 (낙하 중)
			{
				Velocity -= GravityDirection * VerticalSpeed;
			}

			return true;
		}
	}

	// 지면이 너무 멀거나 걸을 수 없는 경사
	CurrentFloor = FGroundHitResult();
	return false;
}

bool UCharacterMovementComponent::SweepCapsule(const FVector& Start, const FVector& Direction, float Distance, FGroundHitResult& OutHit)
{
	if (!CharacterOwner)
	{
		return false;
	}

	UWorld* World = CharacterOwner->GetWorld();
	if (!World || !World->bPie)
	{
		// PIE가 아닌 경우 스윕 수행 불가
		return false;
	}

	PxScene* Scene = World->GetPhysicsScene();
	if (!Scene)
	{
		return false;
	}

	// PhysX 캡슐 지오메트리 생성
	// PhysX의 캡슐은 X축을 따라 정렬됨, 우리는 Z축(수직) 정렬이 필요
	PxCapsuleGeometry CapsuleGeom(CapsuleRadius, CapsuleHalfHeight);

	// 시작 위치 및 방향 설정
	PxVec3 PxStart(Start.X, Start.Y, Start.Z);
	PxVec3 PxDir(Direction.X, Direction.Y, Direction.Z);
	PxDir.normalize();

	// 캡슐 회전: PhysX 캡슐은 X축 정렬, 우리는 Z축(up) 정렬이 필요
	// X축을 Z축으로 회전 (Y축 기준 90도 회전)
	PxQuat CapsuleRotation(PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
	PxTransform StartPose(PxStart, CapsuleRotation);

	// 스윕 결과 버퍼
	PxSweepBuffer SweepHit;

	// 필터 데이터: 정적 오브젝트만 충돌 (캐릭터 자신의 동적 바디 제외)
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC;

	// Scene 락 필요 (스윕 전)
	Scene->lockRead();

	// 캡슐 스윕 수행
	bool bHit = Scene->sweep(
		CapsuleGeom,
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

bool UCharacterMovementComponent::IsWalkableSurface(const FVector& SurfaceNormal) const
{
	// 표면 노말과 중력 반대 방향(Up) 사이의 각도 계산
	FVector UpDirection = -GravityDirection;

	// 두 벡터 사이의 내적
	float DotProduct = FVector::Dot(SurfaceNormal, UpDirection);

	// 각도 계산 (라디안 → 도)
	float AngleRad = std::acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));
	float AngleDeg = RadiansToDegrees(AngleRad);

	// MaxWalkableSlopeAngle보다 작으면 걸을 수 있는 표면
	return AngleDeg <= MaxWalkableSlopeAngle;
}

// ────────────────────────────────────────────────────────────────────────────
// Lua Binding Helper Functions
// ────────────────────────────────────────────────────────────────────────────

FVector UCharacterMovementComponent::GetActorForwardVector() const
{
	if (CharacterOwner)
	{
		return CharacterOwner->GetActorForward();
	}
	return FVector(1.0f, 0.0f, 0.0f); // 기본값: X축 방향
}

FVector UCharacterMovementComponent::GetActorRightVector() const
{
	if (CharacterOwner)
	{
		return CharacterOwner->GetActorRight();
	}
	return FVector(0.0f, 1.0f, 0.0f); // 기본값: Y축 방향
}
