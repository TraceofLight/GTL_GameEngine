// ────────────────────────────────────────────────────────────────────────────
// Character.cpp
// Character 클래스 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "Character.h"
#include "CharacterMovementComponent.h"
#include "SceneComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Components/CameraComponent.h"
#include "Source/Runtime/Engine/Animation/AnimStateMachine.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimInstance.h"
#include "Source/Editor/FBXLoader.h"
#include "InputComponent.h"
#include "ObjectFactory.h"
#include "GameModeBase.h"
#include "LuaScriptComponent.h"
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
	, CameraComponent(nullptr)
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

		// X Bot 스켈레탈 메시 로드
		SkeletalMeshComponent->SetSkeletalMesh(GDataDir + "/X Bot.fbx");

		UE_LOG("[Character] SkeletalMeshComponent created!");
	}

	// 카메라 컴포넌트 생성 (3인칭 뷰)
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("Camera");
	if (CameraComponent && SkeletalMeshComponent)
	{
		// SkeletalMesh에 Attach
		CameraComponent->SetupAttachment(SkeletalMeshComponent);

		// 3인칭 카메라 위치: 캐릭터 뒤 위
		CameraComponent->SetRelativeLocation(FVector(-0.0f, 0.0f, 5.0f));

		UE_LOG("[Character] CameraComponent created and attached to SkeletalMeshComponent!");
		UE_LOG("[Character] CameraComponent parent: %p (SkeletalMesh: %p)",
			CameraComponent->GetAttachParent(), SkeletalMeshComponent);
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
	UE_LOG("[Character] BeginPlay called!");

	Super::BeginPlay();

	// AnimationStateMachine 생성 및 초기화
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMesh())
	{
		// 애니메이션 에셋 로드
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMesh();
		UE_LOG("[Character] SkeletalMesh: %p", SkeletalMesh);

		if (SkeletalMesh)
		{
			const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
			UE_LOG("[Character] Skeleton: %p", Skeleton);

			if (Skeleton)
			{
				UFbxLoader& FbxLoader = UFbxLoader::GetInstance();
				// SkeletalMesh->GetAnimations()[0];
				// 애니메이션 로드
				// TArray<UAnimSequence*> IdleAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Idle.fbx", *Skeleton);
				// TArray<UAnimSequence*> WalkAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Walking.fbx", *Skeleton);
				// TArray<UAnimSequence*> RunAnims = FbxLoader.LoadAllFbxAnimations(GDataDir + "/Animation/XBOT_Slow Run.fbx", *Skeleton);
				//
				// UAnimSequence* IdleAnim = SkeletalMesh->GetAnimations()[0];
				// UAnimSequence* WalkAnim = SkeletalMesh->GetAnimations()[1];
				// UAnimSequence* RunAnim = SkeletalMesh->GetAnimations()[2];

				// ===== BlendSpace2D 예제 (주석 처리 - 나중에 8방향 이동용) =====
				/*
				UBlendSpace2D* BlendSpace = NewObject<UBlendSpace2D>();
				if (BlendSpace)
				{
					// 축 범위 설정
					BlendSpace->XAxisMin = 0.0f;
					BlendSpace->XAxisMax = 60.0f;  // 속도 범위
					BlendSpace->YAxisMin = -180.0f;
					BlendSpace->YAxisMax = 180.0f;   // 방향 범위
					BlendSpace->XAxisName = "Speed";
					BlendSpace->YAxisName = "Direction";

					// 8방향 샘플 포인트 예제
					// BlendSpace->AddSample(FVector2D(0.0f, 0.0f), IdleAnim);            // Idle
					// BlendSpace->AddSample(FVector2D(30.0f, 0.0f), WalkForwardAnim);    // Walk Forward
					// BlendSpace->AddSample(FVector2D(30.0f, 180.0f), WalkBackwardAnim); // Walk Backward
					// BlendSpace->AddSample(FVector2D(30.0f, 90.0f), WalkRightAnim);     // Walk Right
					// BlendSpace->AddSample(FVector2D(30.0f, -90.0f), WalkLeftAnim);     // Walk Left

					// SkeletalMeshComponent에 BlendSpace2D 설정
					SkeletalMeshComponent->SetBlendSpace2D(BlendSpace);
					UE_LOG("[Character] BlendSpace2D initialized!");
				}
				*/
			}
			else
			{
				UE_LOG("[Character] ERROR: Skeleton is null!");
			}
		}
		else
		{
			UE_LOG("[Character] ERROR: SkeletalMesh is null!");
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

	// 캐릭터가 Y=0 아래로 떨어지지 않도록 제한
	FVector CurrentLocation = GetActorLocation();
	if (CurrentLocation.Z < 0.0f)
	{
		CurrentLocation.Z = 0.0f;
		SetActorLocation(CurrentLocation);

		// 낙하 속도도 제거
		if (CharacterMovement)
		{
			FVector Velocity = CharacterMovement->GetVelocity();
			if (Velocity.Z < 0.0f)
			{
				Velocity.Z = 0.0f;
				CharacterMovement->SetVelocity(Velocity);
			}
		}
	}

	// 카메라가 캐릭터를 따라가고 바라보도록 업데이트
	if (CameraComponent)
	{
		FVector CharacterLocation = GetActorLocation();
		FVector CameraOffset = FVector(-5.0f, 0.0f, 2.0f); // 뒤쪽 + 위쪽
		FVector CameraLocation = CharacterLocation + CameraOffset;

		CameraComponent->SetWorldLocation(CameraLocation);

		// 카메라가 캐릭터를 바라보도록 회전 설정
		FVector LookDirection = (CharacterLocation - CameraLocation).GetNormalized();
		CameraComponent->SetForward(LookDirection);
	}
}

void ACharacter::DuplicateSubObjects()
{
	// IMPORTANT: 먼저 부모 클래스의 DuplicateSubObjects를 호출하여 컴포넌트들을 복제
	Super::DuplicateSubObjects();

	// 복제 후 OwnedComponents에서 새로운 컴포넌트를 찾아 멤버 변수 업데이트
	for (UActorComponent* Component : OwnedComponents)
	{
		// CharacterMovement 업데이트
		if (UCharacterMovementComponent* Movement = Cast<UCharacterMovementComponent>(Component))
		{
			CharacterMovement = Movement;
		}
		// SkeletalMeshComponent 업데이트
		else if (USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Component))
		{
			SkeletalMeshComponent = Skeletal;
		}
		// CameraComponent 업데이트
		else if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
		{
			CameraComponent = Camera;
		}
	}

	// RootComponent도 업데이트 (SkeletalMeshComponent가 루트)
	if (SkeletalMeshComponent)
	{
		SetRootComponent(SkeletalMeshComponent);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 바인딩
// ────────────────────────────────────────────────────────────────────────────

void ACharacter::SetupPlayerInputComponent(UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	if (!InInputComponent)
	{
		UE_LOG("[Character] ERROR: InputComponent is null!");
		return;
	}

	// WASD 이동 바인딩
	InInputComponent->BindAxis("MoveForward", 'W', 1.0f, this, &ACharacter::MoveForward);
	InInputComponent->BindAxis("MoveForward", 'S', -1.0f, this, &ACharacter::MoveForward);
	InInputComponent->BindAxis("MoveRight", 'D', 1.0f, this, &ACharacter::MoveRight);
	InInputComponent->BindAxis("MoveRight", 'A', -1.0f, this, &ACharacter::MoveRight);

	// 마우스 회전 바인딩
	InInputComponent->BindAxis("Turn", VK_RIGHT, 1.0f, this, &ACharacter::Turn);
	InInputComponent->BindAxis("Turn", VK_LEFT, -1.0f, this, &ACharacter::Turn);
	InInputComponent->BindAxis("LookUp", VK_UP, 1.0f, this, &ACharacter::LookUp);
	InInputComponent->BindAxis("LookUp", VK_DOWN, -1.0f, this, &ACharacter::LookUp);

	// 점프 바인딩
	InInputComponent->BindAction("Jump", VK_SPACE, this, &ACharacter::Jump, &ACharacter::StopJumping);

	UE_LOG("[Character] Input bindings set up successfully");
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

