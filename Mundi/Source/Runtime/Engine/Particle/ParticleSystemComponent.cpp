#include "pch.h"
#include "ParticleSystemComponent.h"
#include "ParticleEmitterInstance.h"
#include "ParticleSpriteEmitterInstance.h"
#include "ParticleMeshEmitterInstance.h"
#include "ParticleBeamEmitterInstance.h"
#include "ParticleDataContainer.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleTypes.h"
#include "TypeData/ParticleModuleTypeDataBase.h"
#include "Spawn/ParticleModuleSpawn.h"
#include "ParticleModuleRequired.h"
#include "Lifetime/ParticleModuleLifetime.h"
#include "Velocity/ParticleModuleVelocity.h"
#include "Size/ParticleModuleSize.h"
#include "Color/ParticleModuleColor.h"
#include "Source/Runtime/Core/Object/ObjectFactory.h"
#include "Source/Runtime/AssetManagement/ResourceManager.h"
#include "Source/Runtime/AssetManagement/StaticMesh.h"
#include "Source/Runtime/Renderer/Material.h"
#include "Source/Runtime/Engine/Components/BillboardComponent.h"
#include "Source/Editor/Gizmo/GizmoArrowComponent.h"

/**
 * 생성자: 파티클 시스템 컴포넌트 초기화
 */
UParticleSystemComponent::UParticleSystemComponent()
	: Template(nullptr)              // 파티클 시스템 템플릿 (설계도)
	, AccumulatedTime(0.0f)           // 누적 시간 (Burst 타이밍 계산용)
	, CurrentDynamicData(nullptr)     // 렌더 데이터 (초기엔 nullptr)
{
	// 파티클 업데이트를 위해 매 프레임 Tick 활성화
	bCanEverTick = true;
	InitializeComponent();
	//ActivateSystem(true);
}

/**
 * 소멸자: 파티클 시스템 컴포넌트 정리
 */
UParticleSystemComponent::~UParticleSystemComponent()
{
	// 모든 에미터 인스턴스 삭제
	ClearEmitterInstances();

	// 렌더 데이터 메모리 정리
	if (CurrentDynamicData)
	{
		// FParticleDynamicData 내부의 모든 에미터 데이터 삭제
		for (FDynamicEmitterDataBase* EmitterData : CurrentDynamicData->DynamicEmitterDataArray)
		{
			if (EmitterData)
			{
				delete EmitterData;
			}
		}
		CurrentDynamicData->DynamicEmitterDataArray.clear();

		// FParticleDynamicData 자체 삭제
		delete CurrentDynamicData;
		CurrentDynamicData = nullptr;
	}
}

// ============== Lifecycle (생명주기) ==============

/**
 * 월드에 등록될 때 호출 - 에디터 전용 아이콘 생성
 */
void UParticleSystemComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	// PIE가 아닐 때만 에디터 전용 빌보드 생성
	if (!SpriteComponent && InWorld && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTexture(GDataDir + "/Default/Icon/S_Emitter.PNG");
	}

	// PIE가 아닐 때만 에디터 전용 방향 기즈모 생성 (초록색 화살표)
	if (!DirectionGizmo && InWorld && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// 기즈모 메쉬 설정 (DirectionalLight와 동일한 화살표)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Default/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// 월드 스케일 사용 (스크린 상수 스케일 아님)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// 기본 스케일 설정
		DirectionGizmo->SetDefaultScale(FVector(2.0f, 2.0f, 2.0f));

		// 초록색 기즈모
		DirectionGizmo->SetColor(FVector(0.2f, 0.8f, 0.2f));

		// 기즈모 업데이트
		UpdateDirectionGizmo();
	}
}

/**
 * 컴포넌트 초기화 (씬에 추가될 때 한 번 호출)
 */
void UParticleSystemComponent::InitializeComponent()
{
	// 부모 클래스 초기화 먼저 호출
	UPrimitiveComponent::InitializeComponent();

	// Template이 없을 때만 기본 파티클 시스템 생성 (Serialize로 로드한 경우 유지)
	// if (!Template)
	// {
	// 	UParticleSystem* FlareSystem = UParticleSystemComponent::CreateAppleMeshParticleSystem();
	// 	Template = FlareSystem;
	// }

	// 템플릿(설계도)이 있으면 에미터 인스턴스 생성
	if (Template)
	{
		InitializeEmitters();
	}
}

/**
 * 매 프레임 호출되는 업데이트 함수
 * @param DeltaTime - 이전 프레임으로부터 경과한 시간 (초 단위)
 */
void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	// 부모 클래스 Tick 먼저 호출
	UPrimitiveComponent::TickComponent(DeltaTime);

	// 비활성 상태거나 템플릿이 없으면 업데이트 안 함
	if (!bIsActive || !Template)
	{
		return;
	}

	// 누적 시간 업데이트 (Burst 타이밍 계산용)
	AccumulatedTime += DeltaTime;

	// 모든 에미터 업데이트 (파티클 생성 + 업데이트)
	UpdateEmitters(DeltaTime);

	// ========== 렌더 데이터 수집 (게임 스레드 → 렌더 스레드) ==========
	// 렌더 팀원이 UpdateDynamicData()를 호출해서 렌더 데이터를 가져감
	// 여기서는 파티클 로직 업데이트만 수행
}

// ============== System Control (시스템 제어) ==============

/**
 * 파티클 시스템 활성화
 * @param bReset - true면 기존 파티클 초기화하고 처음부터 시작
 */
void UParticleSystemComponent::ActivateSystem(bool bReset)
{
	// 템플릿(설계도)이 없으면 활성화 불가
	if (!Template)
	{
		return;
	}

	// 리셋 요청이 있으면 모든 파티클 초기화
	// (기존 파티클 삭제, 시간 리셋)
	if (bReset)
	{
		ResetParticles();
	}

	// 에미터 인스턴스가 없으면 새로 생성
	// (처음 활성화할 때)
	if (EmitterInstances.empty())
	{
		CreateEmitterInstances();
	}

	bIsActive = true;           // 파티클 시스템 활성화 (Tick 시작)
	AccumulatedTime = 0.0f;     // 누적 시간 초기화
}

/**
 * 파티클 시스템 비활성화
 * 파티클 생성/업데이트 중단 (기존 파티클은 유지)
 */
void UParticleSystemComponent::DeactivateSystem()
{
	bIsActive = false;  // 비활성화 (Tick에서 업데이트 안 함)
}

/**
 * 파티클 시스템 템플릿(설계도) 변경
 * @param NewTemplate - 새로운 파티클 시스템 템플릿
 */
void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	// 같은 템플릿이면 변경할 필요 없음
	if (Template == NewTemplate)
	{
		return;
	}

	// 현재 시스템 비활성화
	DeactivateSystem();

	// 기존 에미터 인스턴스 모두 삭제
	ClearEmitterInstances();

	// 새 템플릿 설정
	Template = NewTemplate;

	// 새 템플릿이 유효하면 에미터 인스턴스 재생성
	if (Template)
	{
		InitializeEmitters();
	}
}

/**
 * 모든 파티클 초기화 (삭제)
 * 파티클 개수만 0으로, 메모리는 유지
 */
void UParticleSystemComponent::ResetParticles()
{
	// 모든 에미터 인스턴스의 파티클 초기화
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->ActiveParticles = 0;   // 활성 파티클 개수 0
			Instance->ParticleCounter = 0;   // 파티클 ID 카운터 초기화
			Instance->SpawnFraction = 0.0f;  // 서브프레임 누적값 초기화
		}
	}

	AccumulatedTime = 0.0f;  // 누적 시간 초기화
}

/**
 * EmitterInstance 업데이트 (Editor에서 프로퍼티 변경 시)
 * 기존 Instance를 파괴하고 새로 생성하여 변경사항 반영
 *
 * @param bEmptyInstances - true면 완전히 파괴 후 재생성
 */
void UParticleSystemComponent::UpdateInstances(bool bEmptyInstances)
{
	if (!Template)
	{
		return;
	}

	// 기존 파티클 리셋
	if (bEmptyInstances)
	{
		// 완전히 파괴 후 재생성
		ClearEmitterInstances();
	}
	else
	{
		// 파티클만 리셋 (Instance 유지)
		ResetParticles();
	}

	// 시스템 재초기화 (EmitterInstance 재생성)
	InitializeEmitters();

	// 자동 활성화가 켜져 있으면 재활성화
	if (bIsActive)
	{
		ActivateSystem(false);
	}
}

// ============== Emitter Management (에미터 관리) ==============

/**
 * 에미터 인스턴스 초기화
 * 기존 에미터를 모두 삭제하고 새로 생성
 */
void UParticleSystemComponent::InitializeEmitters()
{
	// 템플릿이 없으면 초기화할 수 없음
	if (!Template)
	{
		return;
	}

	// 기존 에미터 인스턴스 모두 삭제
	ClearEmitterInstances();

	// 새 에미터 인스턴스 생성
	CreateEmitterInstances();
}

void UParticleSystemComponent::UpdateEmitters(float DeltaTime)
{
	// Update each emitter instance
	// 모든 에미터 인스턴스 업데이트
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		// 에미터 인스턴스나 LOD 레벨이 없으면 스킵
		if (!Instance || !Instance->CurrentLODLevel)
		{
			continue;
		}

		// LOD 레벨 또는 에미터가 비활성화되어 있으면 스킵
		if (!Instance->CurrentLODLevel->bEnabled)
		{
			continue;
		}

		// 에미터 자체의 활성화 상태 체크
		if (Instance->SpriteTemplate && !Instance->SpriteTemplate->bIsEnabled)
		{
			continue;
		}

		// ========== 1단계: 파티클 생성 (Spawning) ==========

		// Duration/Loops 체크: 유한 루프 에미터가 완료되었는지 확인
		bool bEmitterCompleted = false;
		UParticleModuleRequired* RequiredModule = Instance->CurrentLODLevel->RequiredModule;
		if (RequiredModule)
		{
			int32 EmitterLoops = RequiredModule->GetEmitterLoops();

			// EmitterLoops > 0 이면 유한 루프 (0 = 무한 루프)
			if (EmitterLoops > 0)
			{
				float Duration = RequiredModule->GetEmitterDuration();
				float TotalDuration = Duration * static_cast<float>(EmitterLoops);

				// 누적 시간이 총 지속 시간을 초과하면 스폰 완료
				if (AccumulatedTime >= TotalDuration)
				{
					bEmitterCompleted = true;
				}
			}
		}

		// 현재 LOD 레벨의 스폰 모듈 가져오기
		UParticleModuleSpawn* SpawnModule = Instance->CurrentLODLevel->SpawnModule;
		if (SpawnModule && !bEmitterCompleted)
		{
			// 이번 프레임에 생성할 파티클 개수 계산
			int32 SpawnNumber = 0;      // 실제 생성할 개수
			float SpawnRate = 0.0f;     // 초당 생성률 (디버깅/통계용)

			// ----------------------------------------------------------------
			// [방법 1] SpawnRate 기반 생성 (연속적 생성)
			// ----------------------------------------------------------------
			// 예: SpawnRate=30 → 초당 30개 생성
			// DeltaTime=0.016초(60fps) → 0.48개 생성
			//
			// SpawnFraction 누적 방식:
			//   - Instance->SpawnFraction에 소수점 누적 저장
			//   - 1.0 이상이 되면 그때 파티클 1개 생성
			//   - 이렇게 하면 프레임마다 부드럽게 파티클이 나옴
			// ----------------------------------------------------------------
			bool bShouldSpawn = SpawnModule->GetSpawnAmount(
				DeltaTime,                     // DeltaTime (경과 시간)
				SpawnNumber,                   // OutNumber (출력: 생성할 개수)
				SpawnRate,                     // OutRate (출력: 초당 생성률, 디버깅/통계용)
				Instance->SpawnFraction        // InOutSpawnFraction (입출력: 소수점 누적값)
			);

			// [방법 2] Burst 기반 생성 (특정 시간에 대량 생성)
			// 예: t=0.0초에 50개, t=2.0초에 100개 폭발적 생성
			if (SpawnModule->bProcessBurstList)
			{
				// 에미터 지속 시간 가져오기 (루프 계산용)
				float Duration = 0.0f;
				if (Instance->CurrentLODLevel->RequiredModule)
				{
					Duration = Instance->CurrentLODLevel->RequiredModule->GetEmitterDuration();
				}

				// OldTime과 NewTime 사이에 Burst가 있는지 체크
				int32 BurstCount = SpawnModule->GetBurstCount(
					AccumulatedTime - DeltaTime,  // OldTime (이전 시간)
					AccumulatedTime,               // NewTime (현재 시간)
					Duration                       // Duration (에미터 지속 시간)
				);
				SpawnNumber += BurstCount;  // Rate 생성 + Burst 생성
			}

			// 실제 파티클 생성 수행
			if (SpawnNumber > 0)
			{
				// 생성 위치: 컴포넌트의 월드 위치
				FVector SpawnLocation = GetWorldLocation();

				// 초기 속도: 0 (모듈에서 덮어쓸 수 있음)
				FVector InitialVelocity = FVector::Zero();

				// 서브프레임 분산: 프레임 내에서 균등하게 시간 분산
				// 예: 10개 생성, DeltaTime=0.016초
				//     Increment=0.0016초 → 0.0000, 0.0016, 0.0032, ... 시간에 생성
				//     이렇게 하면 부드러운 이펙트 효과
				float Increment = (SpawnNumber > 1) ? (DeltaTime / SpawnNumber) : 0.0f;

				// SpawnParticles 호출
				Instance->SpawnParticles(
					SpawnNumber,      // Count (생성할 개수)
					0.0f,             // StartTime (첫 파티클 생성 시간, 0=프레임 시작)
					Increment,        // Increment (파티클 간 시간 간격)
					SpawnLocation,    // InitialLocation (초기 위치)
					InitialVelocity   // InitialVelocity (초기 속도)
				);
			}
		}

		// ========== 2단계: 파티클 업데이트 (Update) ==========

		// 기존에 살아있는 파티클들 업데이트
		// - 수명 체크 (RelativeTime >= 1.0이면 제거)
		// - 물리 시뮬레이션 (위치, 회전 업데이트)
		// - 모듈 업데이트 (Color, Size, Velocity 등)
		Instance->Tick(DeltaTime);
	}
}

/**
 * 다이나믹 데이터 업데이트
 * 렌더 스레드가 파티클을 그리기 위해 필요한 데이터를 수집
 *
 * @note 렌더 팀원이 렌더 커맨드에서 호출
 * @note GetCurrentDynamicData()로 결과를 가져가면 됨
 */
void UParticleSystemComponent::UpdateDynamicData()
{
	// ========== 1단계: FParticleDynamicData 생성/재사용 ==========

	// 첫 호출이면 FParticleDynamicData 생성
	if (!CurrentDynamicData)
	{
		CurrentDynamicData = new FParticleDynamicData();
	}

	// ========== 2단계: 이전 프레임 렌더 데이터 정리 ==========

	// 이전 프레임의 에미터 데이터들 삭제
	for (FDynamicEmitterDataBase* EmitterData : CurrentDynamicData->DynamicEmitterDataArray)
	{
		if (EmitterData)
		{
			delete EmitterData;  // GetDynamicData()로 new 했던 메모리 해제
		}
	}

	// 배열 비우기 (포인터만 제거, 위에서 메모리는 이미 해제함)
	CurrentDynamicData->DynamicEmitterDataArray.clear();

	// ========== 3단계: 새 렌더 데이터 수집 ==========

	// 비활성 상태거나 템플릿이 없으면 빈 데이터 반환
	if (!bIsActive || !Template)
	{
		return;  // DynamicEmitterDataArray가 비어있음 → 렌더링 안 함
	}

	// 배열 크기 미리 확보 (재할당 방지)
	CurrentDynamicData->DynamicEmitterDataArray.reserve(EmitterInstances.size());

	// 모든 에미터 인스턴스에서 렌더 데이터 수집
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;  // 유효하지 않은 인스턴스는 스킵
		}

		// LOD 레벨 또는 에미터가 비활성화되어 있으면 스킵 (렌더링 제외)
		if (Instance->CurrentLODLevel && !Instance->CurrentLODLevel->bEnabled)
		{
			CurrentDynamicData->DynamicEmitterDataArray.Add(nullptr);
			continue;
		}

		// 에미터 자체의 활성화 상태 체크
		if (Instance->SpriteTemplate && !Instance->SpriteTemplate->bIsEnabled)
		{
			CurrentDynamicData->DynamicEmitterDataArray.Add(nullptr);
			continue;
		}

		// RenderMode가 None이면 렌더링 스킵
		if (Instance->SpriteTemplate && Instance->SpriteTemplate->EmitterRenderMode == EEmitterRenderMode::None)
		{
			CurrentDynamicData->DynamicEmitterDataArray.Add(nullptr);
			continue;
		}

		// 렌더 스레드용 데이터 생성 (파티클 데이터 스냅샷)
		// bSelected = false (에디터에서 선택 안 됨)
		FDynamicEmitterDataBase* NewEmitterData = Instance->GetDynamicData(false);

		// GetDynamicData()는 파티클이 없으면 nullptr 반환 가능
		// nullptr이어도 배열에 추가 (인덱스 유지를 위해)
		CurrentDynamicData->DynamicEmitterDataArray.Add(NewEmitterData);
	}

}

// ============== Protected Helpers (내부 헬퍼 함수들) ==============

/**
 * 에미터 인스턴스 생성
 * 템플릿(설계도)을 기반으로 실제 실행될 에미터 인스턴스들을 생성
 */
void UParticleSystemComponent::CreateEmitterInstances()
{
	// 템플릿이 없으면 생성할 수 없음
	if (!Template)
	{
		return;
	}

	// 템플릿이 가진 에미터의 개수만큼 공간 확보
	int32 NumEmitters = Template->GetNumEmitters();
	EmitterInstances.reserve(NumEmitters);

	// 각 에미터 설계도를 기반으로 에미터 인스턴스 생성
	for (int32 i = 0; i < NumEmitters; i++)
	{
		// i번째 에미터 설계도 가져오기
		UParticleEmitter* Emitter = Template->GetEmitter(i);
		if (!Emitter)
		{
			continue;  // 유효하지 않으면 스킵
		}

		// TypeDataModule 타입에 따라 에미터 인스턴스 생성
		FParticleEmitterInstance* NewInstance = nullptr;

		UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
		if (LODLevel && LODLevel->TypeDataModule)
		{
			EDynamicEmitterType EmitterType = LODLevel->TypeDataModule->GetEmitterType();
			if (EmitterType == EDynamicEmitterType::Mesh)
			{
				NewInstance = new FParticleMeshEmitterInstance();
			}
			else if (EmitterType == EDynamicEmitterType::Beam2)
			{
				NewInstance = new FParticleBeamEmitterInstance();
			}
		}

		// 기본값: 스프라이트
		if (!NewInstance)
		{
			NewInstance = new FParticleSpriteEmitterInstance();
		}

		// 에미터 인스턴스 초기화
		// - Emitter: 설계도 (어떻게 동작할지 정의)
		// - this: 이 컴포넌트 (월드 위치, 회전 등 제공)
		NewInstance->Init(Emitter, this);

		// 에미터 인스턴스 배열에 추가
		EmitterInstances.Add(NewInstance);
	}
}

/**
 * 모든 에미터 인스턴스 삭제
 * 메모리 정리를 위해 동적 할당된 에미터 인스턴스들을 삭제
 */
void UParticleSystemComponent::ClearEmitterInstances()
{
	// 모든 에미터 인스턴스 순회하며 삭제
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			delete Instance;  // 동적 할당 해제
		}
	}

	// 배열 비우기
	EmitterInstances.clear();
}

// ============== Serialize/Duplicate ==============

void UParticleSystemComponent::Serialize(const bool bIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bIsLoading, InOutHandle);

	if (bIsLoading)
	{
		// Template 로드 (ResourceManager 패턴)
		FString TemplatePath;
		if (InOutHandle.hasKey("TemplatePath"))
		{
			TemplatePath = InOutHandle["TemplatePath"].ToString();
		}

		if (!TemplatePath.empty())
		{
			// ResourceManager를 통해 로드 (캐싱 지원)
			Template = UResourceManager::GetInstance().Load<UParticleSystem>(TemplatePath);
		}

		// 로드 후 에미터 인스턴스 재생성
		if (Template)
		{
			ClearEmitterInstances();
			CreateEmitterInstances();
		}
	}
	else
	{
		// Template 저장 (파일 경로 저장)
		if (Template)
		{
			const FString& TemplatePath = Template->GetFilePath();
			if (!TemplatePath.empty())
			{
				InOutHandle["TemplatePath"] = TemplatePath;
			}
		}
	}
}

void UParticleSystemComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// Template 복제
	if (Template)
	{
		UParticleSystem* NewTemplate = NewObject<UParticleSystem>();
		NewTemplate->DuplicateFrom(Template);
		Template = NewTemplate;
	}

	// EmitterInstances는 런타임 데이터이므로 재생성 필요
	// (원본의 인스턴스를 복사하지 않고 Template에서 새로 생성)
	EmitterInstances.clear();
	CurrentDynamicData = nullptr;

	// 에디터 전용 컴포넌트는 복제하지 않음 (OnRegister에서 재생성)
	SpriteComponent = nullptr;
	DirectionGizmo = nullptr;

	if (Template)
	{
		CreateEmitterInstances();
	}
}

// ============== Particle System Creation (파티클 시스템 생성) ==============

/**
 * flare0.dds 텍스처를 사용하여 랜덤한 회전, 속도, 수명을 가진 파티클 시스템 생성
 * @return UParticleSystem* - 생성된 파티클 시스템 템플릿
 */
UParticleSystem* UParticleSystemComponent::CreateFlareParticleSystem()
{
	// ========== 1. 파티클 시스템 생성 ==========
	UParticleSystem* ParticleSystem = NewObject<UParticleSystem>();
	if (!ParticleSystem)
	{
		return nullptr;
	}

	// 시스템 기본 설정
	ParticleSystem->UpdateTime_FPS = 60.0f;
	ParticleSystem->UpdateTime_Delta = 1.0f / 60.0f;
	ParticleSystem->WarmupTime = 0.0f;
	ParticleSystem->WarmupTickRate = 0;
	ParticleSystem->bAutoDeactivate = false;
	ParticleSystem->SecondsBeforeInactive = 0.0f;
	ParticleSystem->Delay = 0.0f;

	// ========== 2. 에미터 생성 ==========
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	if (!Emitter)
	{
		return nullptr;
	}

	// ========== 3. LOD 레벨 생성 ==========
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	if (!LODLevel)
	{
		return nullptr;
	}

	LODLevel->Level = 0;
	LODLevel->bEnabled = true;

	// ========== 4. Required 모듈 생성 및 설정 ==========
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	if (!RequiredModule)
	{
		return nullptr;
	}

	// flare0.dds 텍스처를 사용하는 머티리얼 로드
	UMaterial* FlareMaterial = UResourceManager::GetInstance().Load<UMaterial>("Data/Particle/flare0.dds");
	//UMaterial* FlareMaterial = UResourceManager::GetInstance().Load<UMaterial>("Data/Particle/Boom.dds");
	RequiredModule->SetMaterial(FlareMaterial);
	RequiredModule->SetEmitterDuration(1.0f);
	RequiredModule->SetEmitterLoops(0); // 무한 루프
	RequiredModule->SetScreenAlignment(EParticleScreenAlignment::Square); // 카메라를 향함
	RequiredModule->SetSortMode(EParticleSortMode::ViewProjDepth);
	RequiredModule->SetUseLocalSpace(false); // 월드 공간 사용
	RequiredModule->SetBlendMode(EParticleBlendMode::Additive);

	LODLevel->RequiredModule = RequiredModule;

	// ========== 5. Spawn 모듈 생성 및 설정 ==========
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	if (!SpawnModule)
	{
		return nullptr;
	}

	// 초당 50개 파티클 생성
	SpawnModule->Rate = FFloatDistribution(50.0f);
	SpawnModule->bProcessBurstList = false;

	LODLevel->SpawnModule = SpawnModule;

	// ========== 6. Lifetime 모듈 생성 및 설정 (랜덤 수명) ==========
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	if (!LifetimeModule)
	{
		return nullptr;
	}

	// 수명: 1.0 ~ 3.0초 랜덤
	LifetimeModule->Lifetime = FFloatDistribution(1.0f, 3.0f);

	LODLevel->Modules.Add(LifetimeModule);

	// ========== 7. Velocity 모듈 생성 및 설정 (랜덤 속도) ==========
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	if (!VelocityModule)
	{
		return nullptr;
	}

	// 속도: 각 방향으로 -1 ~ 1 랜덤
	VelocityModule->StartVelocity = FVectorDistribution(
		FVector(-1.0f, -1.0f, 0.0f),
		FVector(1.0f, 1.0f, 1.0f)
	);
	VelocityModule->bInWorldSpace = false;

	LODLevel->Modules.Add(VelocityModule);

	// ========== 8. Size 모듈 생성 및 설정 ==========
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	if (!SizeModule)
	{
		return nullptr;
	}

	// 크기: 10 ~ 20 유닛 랜덤
	SizeModule->StartSize = FVectorDistribution(
		FVector(10.0f, 10.0f, 10.0f),
		FVector(20.0f, 20.0f, 20.0f)
	);
	/*SizeModule->StartSize = FVectorDistribution(
		FVector(1.0f, 1.0f, 1.0f),
		FVector(1.0f, 1.0f, 1.0f)
	);*/

	LODLevel->Modules.Add(SizeModule);

	// ========== 9. Color 모듈 생성 및 설정 ==========
	UParticleModuleColor* ColorModule = NewObject<UParticleModuleColor>();
	if (!ColorModule)
	{
		return nullptr;
	}

	// 색상: 흰색
	ColorModule->StartColor = FColorDistribution(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	ColorModule->StartAlpha = FFloatDistribution(1.0f);
	ColorModule->bClampAlpha = true;

	LODLevel->Modules.Add(ColorModule);

	// ========== 10. 모듈 리스트 업데이트 ==========
	LODLevel->UpdateModuleLists();

	// ========== 11. 에미터에 LOD 레벨 추가 ==========
	Emitter->LODLevels.Add(LODLevel);

	// ========== 12. 파티클 시스템에 에미터 추가 ==========
	ParticleSystem->Emitters.Add(Emitter);

	// ========== 13. 파티클 시스템 빌드 ==========
	ParticleSystem->BuildEmitters();

	return ParticleSystem;
}

/**
 * 물린 사과 메시를 사용하여 메시 파티클 시스템 생성
 * @return UParticleSystem* - 생성된 파티클 시스템 템플릿
 */
UParticleSystem* UParticleSystemComponent::CreateAppleMeshParticleSystem()
{
	// ========== 1. 파티클 시스템 생성 ==========
	UParticleSystem* ParticleSystem = NewObject<UParticleSystem>();
	if (!ParticleSystem)
	{
		return nullptr;
	}

	// 시스템 기본 설정
	ParticleSystem->UpdateTime_FPS = 60.0f;
	ParticleSystem->UpdateTime_Delta = 1.0f / 60.0f;
	ParticleSystem->WarmupTime = 0.0f;
	ParticleSystem->WarmupTickRate = 0;
	ParticleSystem->bAutoDeactivate = false;
	ParticleSystem->SecondsBeforeInactive = 0.0f;
	ParticleSystem->Delay = 0.0f;

	// ========== 2. 에미터 생성 ==========
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	if (!Emitter)
	{
		return nullptr;
	}

	// ========== 3. LOD 레벨 생성 ==========
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>();
	if (!LODLevel)
	{
		return nullptr;
	}

	LODLevel->Level = 0;
	LODLevel->bEnabled = true;

	// ========== 4. TypeDataModule (Mesh) 생성 및 설정 ==========
	UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();
	if (!MeshTypeData)
	{
		return nullptr;
	}

	// 물린 사과 메시 로드
	UStaticMesh* AppleMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/Model/bitten_apple_mid.obj");
	MeshTypeData->Mesh = AppleMesh;
	MeshTypeData->bCastShadows = false;
	MeshTypeData->MeshAlignment = EParticleAxisLock::None;
	MeshTypeData->bOverrideMaterial = false;
	MeshTypeData->SetOwnerSystem(ParticleSystem);

	LODLevel->TypeDataModule = MeshTypeData;

	// ========== 5. Required 모듈 생성 및 설정 ==========
	UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>();
	if (!RequiredModule)
	{
		return nullptr;
	}

	// 기본 머티리얼 설정 (메시의 기본 머티리얼 사용)
	RequiredModule->SetMaterial(nullptr);
	RequiredModule->SetEmitterDuration(1.0f);
	RequiredModule->SetEmitterLoops(0); // 무한 루프
	RequiredModule->SetScreenAlignment(EParticleScreenAlignment::Square);
	RequiredModule->SetSortMode(EParticleSortMode::ViewProjDepth);
	RequiredModule->SetUseLocalSpace(false); // 월드 공간 사용
	RequiredModule->SetBlendMode(EParticleBlendMode::None);

	LODLevel->RequiredModule = RequiredModule;

	// ========== 6. Spawn 모듈 생성 및 설정 ==========
	UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>();
	if (!SpawnModule)
	{
		return nullptr;
	}

	// 초당 30개 파티클 생성
	SpawnModule->Rate = FFloatDistribution(30.0f);
	SpawnModule->bProcessBurstList = false;

	LODLevel->SpawnModule = SpawnModule;

	// ========== 7. Lifetime 모듈 생성 및 설정 (랜덤 수명) ==========
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>();
	if (!LifetimeModule)
	{
		return nullptr;
	}

	// 수명: 2.0 ~ 4.0초 랜덤
	LifetimeModule->Lifetime = FFloatDistribution(2.0f, 4.0f);

	LODLevel->Modules.Add(LifetimeModule);

	// ========== 8. Velocity 모듈 생성 및 설정 (랜덤 속도) ==========
	UParticleModuleVelocity* VelocityModule = NewObject<UParticleModuleVelocity>();
	if (!VelocityModule)
	{
		return nullptr;
	}

	// 속도: 각 방향으로 -2 ~ 2 랜덤 (좀 더 빠르게)
	VelocityModule->StartVelocity = FVectorDistribution(
		FVector(-2.0f, -2.0f, -0.5f),
		FVector(2.0f, 2.0f, 2.0f)
	);
	VelocityModule->bInWorldSpace = false;

	LODLevel->Modules.Add(VelocityModule);

	// ========== 9. Size 모듈 생성 및 설정 ==========
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>();
	if (!SizeModule)
	{
		return nullptr;
	}

	// 크기: 0.5 ~ 1.5 유닛 랜덤 (메시이므로 작게)
	SizeModule->StartSize = FVectorDistribution(
		FVector(0.5f, 0.5f, 0.5f),
		FVector(1.5f, 1.5f, 1.5f)
	);

	LODLevel->Modules.Add(SizeModule);

	// ========== 10. Color 모듈 생성 및 설정 ==========
	UParticleModuleColor* ColorModule = NewObject<UParticleModuleColor>();
	if (!ColorModule)
	{
		return nullptr;
	}

	// 색상: 빨간색 계열
	ColorModule->StartColor = FColorDistribution(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f));
	ColorModule->StartAlpha = FFloatDistribution(1.0f);
	ColorModule->bClampAlpha = true;

	LODLevel->Modules.Add(ColorModule);

	// ========== 11. 모듈 리스트 업데이트 ==========
	LODLevel->UpdateModuleLists();

	// ========== 12. 에미터에 LOD 레벨 추가 ==========
	Emitter->LODLevels.Add(LODLevel);

	// ========== 13. 파티클 시스템에 에미터 추가 ==========
	ParticleSystem->Emitters.Add(Emitter);

	// ========== 14. 파티클 시스템 빌드 ==========
	ParticleSystem->BuildEmitters();

	return ParticleSystem;
}

// ============== Direction Gizmo ==============

/**
 * 방향 기즈모 업데이트 (파티클 시스템의 Forward 방향 표시)
 */
void UParticleSystemComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
	{
		return;
	}

	// Forward 방향 계산 (Z-Up Left-handed 좌표계에서 X축이 Forward)
	FQuat Rotation = GetWorldRotation();
	FVector ForwardDir = Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
	DirectionGizmo->SetDirection(ForwardDir);
}
