// ────────────────────────────────────────────────────────────────────────────
// Character.cpp
// Character 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Character.h"
#include "CharacterMovementComponent.h"
#include "SceneComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "InputComponent.h"
#include "ObjectFactory.h"
#include "GameModeBase.h"
#include "World.h"

//IMPLEMENT_CLASS(ACharacter)
//
//BEGIN_PROPERTIES(ACharacter)
//	MARK_AS_SPAWNABLE("Character", "이동, 점프 등의 기능을 가진 Character 클래스입니다.")
//END_PROPERTIES()

// ────────────────────────────────────────────────────────────────────────────
// 생성자 / 소멸자
// ────────────────────────────────────────────────────────────────────────────

ACharacter::ACharacter()
	: CharacterMovement(nullptr)
	, SkeletalMeshComponent(nullptr)
	, AnimStateMachine(nullptr)
	, bIsCrouched(false)
	, CrouchedHeightRatio(0.5f)
{
	// CharacterMovementComponent 생성
	CharacterMovement = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovement");
	if (CharacterMovement)
	{
		CharacterMovement->SetOwner(this);
	}

	// SkeletalMesh 컴포넌트 생성 (애니메이션 지원)
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMesh");
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->SetOwner(this);
		SetRootComponent(SkeletalMeshComponent);

		// 테스트용 스켈레탈 메시 로드
		SkeletalMeshComponent->SetSkeletalMesh(GDataDir + "/Test.fbx");

		UE_LOG("[Character] SkeletalMeshComponent created!");
	}
}

ACharacter::~ACharacter()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::BeginPlay()
{
	Super::BeginPlay();

	// AnimationStateMachine 생성 및 초기화
	if (SkeletalMeshComponent)
	{
		AnimStateMachine = NewObject<UAnimationStateMachine>();
		if (AnimStateMachine)
		{
			// State Machine 초기화
			AnimStateMachine->Initialize(this);

			// TODO: 애니메이션 에셋 로드 및 등록
			// UAnimSequence* IdleAnim = LoadAnimation(GDataDir + "/Idle.fbx");
			// UAnimSequence* WalkAnim = LoadAnimation(GDataDir + "/Walk.fbx");
			// UAnimSequence* RunAnim = LoadAnimation(GDataDir + "/Run.fbx");
			//
			// AnimStateMachine->RegisterStateAnimation(EAnimState::Idle, IdleAnim);
			// AnimStateMachine->RegisterStateAnimation(EAnimState::Walk, WalkAnim);
			// AnimStateMachine->RegisterStateAnimation(EAnimState::Run, RunAnim);

			// SkeletalMeshComponent에 State Machine 설정
			SkeletalMeshComponent->SetAnimationStateMachine(AnimStateMachine);

			UE_LOG("[Character] AnimationStateMachine initialized!");
		}
	}
}

void ACharacter::Tick(float DeltaSeconds)
{
	// 게임이 시작되지 않았으면 Lua Tick을 실행하지 않음
	//if (!bGameStarted)
	//{
	//	// 입력 벡터 초기화
	//	if (CharacterMovement)
	//	{
	//		CharacterMovement->ConsumeInputVector();
	//	}
	//	// Super::Tick 호출 안 함 = Lua Tick 실행 안 됨
	//	return;
	//}

	// 게임 시작됨 - 정상 Tick (Lua Tick 포함)
	Super::Tick(DeltaSeconds);
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	// 입력 바인딩은 파생 클래스(예: RunnerCharacter)의 Lua 스크립트에서 처리
	// C++에서는 기본 바인딩을 하지 않음
	UE_LOG("[Character] SetupPlayerInputComponent called - input bindings should be done in Lua");
}

// ────────────────────────────────────────────────────────────────────────────
// 이동 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::AddMovementInput(FVector WorldDirection, float ScaleValue)
{
	if (CharacterMovement)
	{
		CharacterMovement->AddInputVector(WorldDirection, ScaleValue);
	}
}

void ACharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// Forward 방향으로 이동
		FVector Forward = GetActorForward();
		AddMovementInput(Forward, Value);
	}
}

void ACharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// Right 방향으로 이동
		FVector Right = GetActorRight();
		AddMovementInput(Right, Value);
	}
}

void ACharacter::Turn(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerYawInput(Value);
	}
}

void ACharacter::LookUp(float Value)
{
	if (Value != 0.0f)
	{
		AddControllerPitchInput(Value);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Character 동작
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::Jump()
{
	if (CharacterMovement)
	{
		CharacterMovement->Jump();
	}
}

void ACharacter::StopJumping()
{
	if (CharacterMovement)
	{
		CharacterMovement->StopJumping();
	}
}

bool ACharacter::CanJump() const
{
	return CharacterMovement && CharacterMovement->bCanJump && IsGrounded();
}

void ACharacter::Crouch()
{
	if (bIsCrouched)
	{
		return;
	}

	bIsCrouched = true;

	// 웅크릴 때 이동 속도 감소 (옵션)
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed *= CrouchedHeightRatio;
	}
}

void ACharacter::UnCrouch()
{
	if (!bIsCrouched)
	{
		return;
	}

	bIsCrouched = false;

	// 이동 속도 복원
	if (CharacterMovement)
	{
		CharacterMovement->MaxWalkSpeed /= CrouchedHeightRatio;
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 상태 조회
// ────────────────────────────────────────────────────────────────────────────

FVector ACharacter::GetVelocity() const
{
	if (CharacterMovement)
	{
		return CharacterMovement->GetVelocity();
	}

	return FVector();
}

bool ACharacter::IsGrounded() const
{
	return CharacterMovement && CharacterMovement->IsGrounded();
}

bool ACharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}

