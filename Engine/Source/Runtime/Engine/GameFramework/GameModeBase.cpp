#include "pch.h"
#include "GameModeBase.h"
#include "GameStateBase.h"
#include "PlayerController.h"
#include "Pawn.h"
#include "World.h"
#include "VehicleActor.h"
//#include "DeltaTimeManager.h"
#include "PlayerCameraManager.h"
//#include "CameraBlendPresetLibrary.h"

//// AGameModeBase를 ObjectFactory에 등록
//IMPLEMENT_CLASS(AGameModeBase)
//
//// 프로퍼티 등록
//BEGIN_PROPERTIES(AGameModeBase)
//	// PlayerSpawnLocation 프로퍼티 등록
//	ADD_PROPERTY(FVector, PlayerSpawnLocation, "GameMode", true, "플레이어 스폰 위치입니다.")
//END_PROPERTIES()

AGameModeBase::AGameModeBase()
	: GameState(nullptr)
	, PlayerSpawnLocation(FVector(0.0f, 0.0f, 100.0f))
	, DefaultPawnClass(nullptr)
	, PlayerControllerClass(nullptr)
	, PlayerController(nullptr)
	, bGameStarted(false)
	, bAutoSpawnPlayer(true)
{
	// 기본 클래스 설정
	PlayerControllerClass = APlayerController::StaticClass();
	DefaultPawnClass = APawn::StaticClass();
}

void AGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG("GameMode: BeginPlay");

	// 카메라 블렌드 프리셋 라이브러리 초기화
	//UCameraBlendPresetLibrary* PresetLibrary = UCameraBlendPresetLibrary::GetInstance();
	//if (PresetLibrary)
	//{
	//	PresetLibrary->PrintAllPresets();  // 디버그: 등록된 프리셋 목록 출력
	//}

	//// World의 PIE 설정을 GameMode에 적용
	//if (World)
	//{
	//	// World 설정이 있으면 우선 사용
	//	if (World->GetDefaultPawnClass())
	//	{
	//		DefaultPawnClass = World->GetDefaultPawnClass();
	//		UE_LOG("[GameMode] DefaultPawnClass set from World: %s", DefaultPawnClass->Name);
	//	}
	//
	//	if (World->GetPlayerControllerClass())
	//	{
	//		PlayerControllerClass = World->GetPlayerControllerClass();
	//		UE_LOG("[GameMode] PlayerControllerClass set from World: %s", PlayerControllerClass->Name);
	//	}
	//
	//	PlayerSpawnLocation = World->GetPlayerSpawnLocation();
	//	UE_LOG("[GameMode] PlayerSpawnLocation set from World: (%.1f, %.1f, %.1f)",
	//		PlayerSpawnLocation.X, PlayerSpawnLocation.Y, PlayerSpawnLocation.Z);
	//}

	// GameState 자동 생성 (없는 경우)
	if (!GameState)
	{
		if (World)
		{
			AGameStateBase* NewGameState = World->SpawnActor<AGameStateBase>();
			if (NewGameState)
			{
				GameState = NewGameState;
				UE_LOG("GameMode: BeginPlay: GameState created");
			}
		}
	}

	// 플레이어 초기화
	if (bAutoSpawnPlayer)
	{
		InitPlayer();
	}
}

void AGameModeBase::EndPlay()
{
	Super::EndPlay();
	//USoundManager::GetInstance().StopAllSounds();
}

void AGameModeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 추가 게임 로직 필요 시 여기에 구현
}

void AGameModeBase::StartGame()
{
	if (bGameStarted)
	{
		UE_LOG("GameMode: 게임이 이미 시작되었습니다.");
		return;
	}

	// 이전 게임에서 적용되었던 모든 델타타임 효과 취소
	//GWorld->GetDeltaTimeManager()->CancelAllEffects();

	// 이전 게임에서 적용되었던 모든 포스트 프로세스 설정 초기화
	//PlayerController->GetPlayerCameraManager()->ResetPostProcessSettings();

	bGameStarted = true;

	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameStarted.Broadcast();

	UE_LOG("GameMode: 게임 시작");
}

void AGameModeBase::EndGame(bool bVictory)
{
	if (!bGameStarted)
	{
		return;
	}

	// GameState 상태 변경
	if (GameState)
	{
		if (bVictory)
		{
			GameState->SetGameState(EGameState::Victory);
		}
		else
		{
			GameState->SetGameState(EGameState::GameOver);
		}
	}

	// 델리게이트 브로드캐스트
	OnGameEnded.Broadcast(bVictory);
}

void AGameModeBase::RestartGame()
{
	// GameState 초기화
	if (GameState)
	{
		GameState->SetGameState(EGameState::NotStarted);
		GameState->SetScore(0);
		GameState->ResetTimer();
	}

	bGameStarted = false;

	// 델리게이트 브로드캐스트
	OnGameRestarted.Broadcast();

	UE_LOG("GameMode: 게임 재시작");

	// 자동으로 게임 시작
	StartGame();
}

void AGameModeBase::PauseGame()
{
	if (!bGameStarted)
	{
		UE_LOG("GameMode: 게임이 시작되지 않았습니다.");
		return;
	}
	bGameStarted = false;
	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Paused);
	}

	// 델리게이트 브로드캐스트
	OnGamePaused.Broadcast();

	UE_LOG("GameMode: 게임 일시정지!");
}

void AGameModeBase::ResumeGame()
{
	bGameStarted = true;
	if (!bGameStarted)
	{
		UE_LOG("GameMode: 게임이 시작되지 않았습니다.");
		return;
	}

	// GameState 상태 변경
	if (GameState)
	{
		GameState->SetGameState(EGameState::Playing);
	}

	// 델리게이트 브로드캐스트
	OnGameResumed.Broadcast();

	UE_LOG("GameMode: 게임 재개!");
}

FVector AGameModeBase::GetPlayerSpawnLocation() const
{
	return PlayerSpawnLocation;
}

void AGameModeBase::SetPlayerSpawnLocation(const FVector& Location)
{
	PlayerSpawnLocation = Location;
}

void AGameModeBase::SetGameState(AGameStateBase* NewGameState)
{
	if (NewGameState)
	{
		GameState = NewGameState;
		UE_LOG("GameMode: GameState 설정됨");
	}
}

void AGameModeBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	// GameMode 복제 시 게임 상태 초기화
	bGameStarted = false;
}

// ────────────────────────────────────────────────────────────────────────────
// 플레이어 초기화
// ────────────────────────────────────────────────────────────────────────────

void AGameModeBase::InitPlayer()
{
	UE_LOG("GameMode: InitPlayer");

	// 1. PlayerController 생성
	PlayerController = SpawnPlayerController();

	if (!PlayerController)
	{
		UE_LOG("GameMode: InitPlayer: Failed to spawn PlayerController");
		return;
	}

	// 2. DefaultPawnClass가 설정되어 있으면 스폰
	if (DefaultPawnClass)
	{
		UE_LOG("GameMode: InitPlayer: Spawning DefaultPawnClass");

		FTransform SpawnTransform;
		SpawnTransform.Translation = PlayerSpawnLocation;
		SpawnTransform.Rotation = FQuat::Identity();
		SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

		APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform);

		if (!SpawnedPawn)
		{
			UE_LOG("GameMode: InitPlayer: Failed to spawn DefaultPawn");
		}
		return;
	}

	// 3. DefaultPawnClass가 None이면 아무것도 하지 않음 (PlayerController만 존재)
	UE_LOG("GameMode: InitPlayer: No DefaultPawnClass, PlayerController created without Pawn");
}

APlayerController* AGameModeBase::SpawnPlayerController()
{
	if (!World || !PlayerControllerClass)
	{
		return nullptr;
	}

	// PlayerController 스폰 (위치는 중요하지 않음)
	AActor* NewActor = World->SpawnActor(PlayerControllerClass);
	APlayerController* NewController = Cast<APlayerController>(NewActor);

	if (NewController)
	{
		UE_LOG("GameMode: SpawnPlayerController: %s", NewController->GetName().c_str());
	}

	return NewController;
}

APawn* AGameModeBase::SpawnDefaultPawnFor(APlayerController* NewPlayer, const FTransform& SpawnTransform)
{
	if (!NewPlayer || !World)
	{
		return nullptr;
	}

	// Pawn 클래스 결정
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);

	if (!PawnClass)
	{
		UE_LOG("GameMode: SpawnDefaultPawn: No DefaultPawnClass");
		return nullptr;
	}

	// Pawn 스폰
	AActor* NewActor = World->SpawnActor(PawnClass, SpawnTransform);

	APawn* NewPawn = Cast<APawn>(NewActor);

	if (NewPawn)
	{
		UE_LOG("GameMode: SpawnDefaultPawn: %s at (%.1f, %.1f, %.1f)",
			   NewPawn->GetName().c_str(),
			   SpawnTransform.Translation.X,
			   SpawnTransform.Translation.Y,
			   SpawnTransform.Translation.Z);

		// PlayerController가 Pawn을 빙의
		NewPlayer->Possess(NewPawn);
	}

	return NewPawn;
}

UClass* AGameModeBase::GetDefaultPawnClassForController(APlayerController* InController)
{
	return DefaultPawnClass;
}

void AGameModeBase::RestartPlayer(APlayerController* Player)
{
	if (!Player)
	{
		return;
	}

	UE_LOG("GameMode: RestartPlayer");

	// 기존 Pawn 제거
	APawn* OldPawn = Player->GetPawn();
	if (OldPawn)
	{
		Player->UnPossess();
		//World->DestroyActor(OldPawn);  // UnPossess 전에 저장한 OldPawn 사용
		OldPawn->Destroy();
		//UE_LOG("[GameMode] Old pawn destroyed: %s", OldPawn->GetName().c_str());
	}

	// 새 Pawn 스폰
	FTransform SpawnTransform;
	SpawnTransform.Translation = PlayerSpawnLocation;
	SpawnTransform.Rotation = FQuat::Identity();
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);

	APawn* NewPawn = SpawnDefaultPawnFor(Player, SpawnTransform);

	if (NewPawn)
	{
		// PIE 모드에서 런타임 스폰된 Pawn은 BeginPlay를 수동으로 호출해야 함
		NewPawn->BeginPlay();
		UE_LOG("GameMode: RestartPlayer: Done");
	}
}

// ────────────────────────────────────────────────────────────────────────────
// Vehicle 찾기
// ────────────────────────────────────────────────────────────────────────────

APawn* AGameModeBase::FindExistingVehicleInLevel()
{
	if (!World)
	{
		return nullptr;
	}

	// Level의 모든 Actor를 순회하며 VehicleActor 찾기
	const TArray<AActor*>& AllActors = World->GetActors();

	for (AActor* Actor : AllActors)
	{
		if (!Actor)
		{
			continue;
		}

		// VehicleActor인지 확인
		AVehicleActor* Vehicle = Cast<AVehicleActor>(Actor);
		if (Vehicle)
		{
			UE_LOG("GameMode: FindVehicle: %s", Vehicle->GetName().c_str());
			return Vehicle;
		}
	}

	return nullptr;
}
