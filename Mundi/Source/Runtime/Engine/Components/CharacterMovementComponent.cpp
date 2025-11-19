// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.cpp
// Character 이동 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "CharacterMovementComponent.h"
#include "Character.h"
#include "BoxComponent.h"
#include "World.h"
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

	// 사망한 상태면 조기 리턴
	//UWorld* World = CharacterOwner->GetWorld();
	//
	//if (World)
	//{
	//	GameMode = (World->GetGameMode());
	//	if (GameMode &&
	//		GameMode->GetGameState()->GetGameState() == EGameState::GameOver)
	//	{
	//		return;
	//	}
	//}
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

	// 5. 이동 모드 업데이트
	if (bIsNowGrounded && !bWasGrounded)
	{
		// 착지 - 중력 방향의 속도 성분 제거
		SetMovementMode(EMovementMode::Walking);
		float VerticalSpeed = FVector::Dot(Velocity, GravityDirection);
		Velocity -= GravityDirection * VerticalSpeed;
		TimeInAir = 0.0f;
		bIsJumping = false;
	}
	else if (!bIsNowGrounded && bWasGrounded)
	{
		// 낙하 시작
		SetMovementMode(EMovementMode::Falling);
	}

	// 6. 공중 시간 체크
	if (IsFalling())
	{
		TimeInAir += DeltaTime;

		// 너무 오래 공중에 있으면 GameOver 처리
		if (TimeInAir > MaxAirTime)
		{
			//if (GameMode)
			//{
			//	GameMode->OnPlayerDeath(CharacterOwner);
			//}
			TimeInAir = 0.0f;
		}
	}

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
	// 점프 가능 조건 체크
	if (!bCanJump || !IsGrounded())
	{
		return false;
	}

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
	// 점프 키를 뗐을 때 상승 속도를 줄임
	// 중력 반대 방향으로의 속도만 감소
	FVector UpDirection = GravityDirection * -1.0f;
	float UpwardSpeed = FVector::Dot(Velocity, UpDirection);

	if (bIsJumping && UpwardSpeed > 0.0f)
	{
		// 상승 속도 성분만 감소
		Velocity -= UpDirection * (UpwardSpeed * 0.5f);
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

	// 중력 가속도 적용 (방향 벡터 사용)
	float GravityMagnitude = DefaultGravity * GravityScale;
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
	if (Velocity.SizeSquared() == 0.0f)
	{
		return;
	}

	// 새 위치 계산
	FVector CurrentLocation = CharacterOwner->GetActorLocation();
	FVector Delta = Velocity * DeltaTime;
	FVector NewLocation = CurrentLocation + Delta;

	// 위치 업데이트
	CharacterOwner->SetActorLocation(NewLocation);

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

	// Z 위치가 0 이하이면 지면에 있는 것으로 간주
	FVector CurrentLocation = CharacterOwner->GetActorLocation();
	if (CurrentLocation.Z <= 0.0f)
	{
		// 위치를 0으로 고정
		CurrentLocation.Z = 0.0f;
		CharacterOwner->SetActorLocation(CurrentLocation);
		return true;
	}

	return false;
}
