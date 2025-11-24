#pragma once
#include "pch.h"
#include "Particle.h"
#include "ParticleHelper.h"
#include "Source/Runtime/Core/Memory/Memory.h"
#include "ParticleLODLevel.h"
#include "ParticleModule.h"
#include "ParticleEmitter.h"
#include "DynamicEmitterReplayDataBase.h"
#include "ParticleModuleRequired.h"
#include "ParticleSystemComponent.h"

// Forward declarations
class UParticleSystemComponent;
struct FParticleEventInstancePayload;

/**
 * Runtime instance of a particle emitter
 * Manages particle spawning, updating, and lifetime for a single emitter
 */
struct FParticleEmitterInstance
{
	/** Template emitter that this instance is based on */
	// 이 인스턴스가 따라야 할 설계도 (원본 에셋)
	UParticleEmitter* SpriteTemplate;

	/** Owner particle system component */
	// 내가 지금 어디에(Location) 있는지 알려면 Component에 물어봐야 함.
	UParticleSystemComponent* Component;

	// ============== LOD ==============
	/** Current LOD level index being used */
	// 현재 사용 중인 LOD 단계 인덱스
	int32 CurrentLODLevelIndex;

	/** Current LOD level being used */
	// 현재 LOD 단계의 설정값들 (여기에 사용할 모듈 리스트가 들어있음)
	// ex) 지금 카메라랑 머니까 LOD 1번 매뉴얼(모듈 2개만 사용)대로 작업한다.
	UParticleLODLevel* CurrentLODLevel;

	// ============== 메모리 접근 ==============
	// 언리얼 엔진 방식: 직접 메모리 관리 (FParticleDataContainer 사용 안 함)

	/** Pointer to the particle data array */
	// 실제 파티클 데이터들이 저장된 메모리 블록의 시작 주소
	// FMemory::Realloc으로 동적 리사이징 가능
	uint8* ParticleData;

	/** Pointer to the particle index array */
	// 살아있는 파티클들의 번호(Index)가 적힌 배열
	// ex) 3, 7, 9번 파티클이 살아있으니까 얘네만 업데이트
	uint16* ParticleIndices;

	/** Pointer to the instance data array */
	// 인스턴스별 데이터 (파티클 개별 데이터 말고, 에미터 자체 변수 값 등)
	uint8* InstanceData;

	// ============== 메모리 계산기 ==============
	// uint8* 포인터에서 정확한 위치를 찾기 위한 변수들

	/** The size of the Instance data array in bytes */
	// 인스턴스 데이터 크기
	int32 InstancePayloadSize;

	/** The offset to the particle data in bytes */
	// 파티클 데이터 내에서 모듈 데이터(Payload)가 시작되는 오프셋
	int32 PayloadOffset;

	/** The total size of a single particle in bytes */
	// 기본 파티클(FBaseParticle) 하나의 크기 (고정값)
	int32 ParticleSize;

	/** The stride between particles in the ParticleData array in bytes */
	// 파티클 하나가 차지하는 진짜 총 크기 (기본 + 모듈 데이터)
	// 이미터 인스턴스 내에서 파티클들의 Stride는 같음
	// ex) 이번 파티클은 기본 50 바이트에 컬러 모듈 16바이트 추가해서 총 66바이트 간격(Stride)
	// Data + (Index * ParticleStride)
	int32 ParticleStride;

	// ============== 상태 관리(State) ==============
	/** The number of particles currently active in the emitter */
	// 활성화된 파티클들 개수, Loop 최적화
	int32 ActiveParticles;

	/** Monotonically increasing counter for particle IDs */
	// 파티클 고유 ID 부여를 위한 카운터 (계속 증가만 함)
	uint32 ParticleCounter;

	/** The maximum number of active particles that can be held in the particle data array */
	// 메모리 풀에서 수용 가능한 최대 파티클 개수
	int32 MaxActiveParticles;

	/** The fraction of time left over from spawning (for sub-frame spawning accuracy) */
	// 서브 프레임 스폰을 위한 시간 찌꺼기 저장 변수
	// 초당 30 마리를 생성한다고 가정하자. 근데 프레임이 60FPS (DeltaTime = 0.016s)
	// 이번 프레임에 생성해야 할 개수 = 30 x 0.016 = 0.48마리
	// 0.48마리를 생성할 수 없으므로 이 변수에 0.48 저장
	// 다음 프레임에도 0.96 값이라 생성할 수 없어서 또 저장
	// 다다음 프레임에 1.44가 되어서 1마리 생성하고 0.44를 남기는 방식
	// 이게 있어야 파티클이 부드럽게 이어져서 나옴
	float SpawnFraction;

	FParticleEmitterInstance()
		: SpriteTemplate(nullptr)
		, Component(nullptr)
		, CurrentLODLevelIndex(0)
		, CurrentLODLevel(nullptr)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
		, InstanceData(nullptr)
		, InstancePayloadSize(0)
		, PayloadOffset(0)
		, ParticleSize(0)
		, ParticleStride(0)
		, ActiveParticles(0)
		, ParticleCounter(0)
		, MaxActiveParticles(0)
		, SpawnFraction(0.0f)
	{
	}

	~FParticleEmitterInstance()
	{
		// 메모리 해제 (언리얼 방식)
		if (ParticleData)
		{
			FMemory::Free(ParticleData);
			ParticleData = nullptr;
		}

		if (ParticleIndices)
		{
			FMemory::Free(ParticleIndices);
			ParticleIndices = nullptr;
		}

		if (InstanceData)
		{
			FMemory::Free(InstanceData);
			InstanceData = nullptr;
		}
	}

	/**
	 * Spawns particles in the emitter
	 * 에미터에서 파티클을 생성하는 핵심 함수
	 *
	 * @param Count - Number of particles to spawn (생성할 파티클 개수)
	 * @param StartTime - Starting time for the first particle (첫 파티클의 시작 시간, 서브프레임 보정용)
	 * @param Increment - Time increment between each particle spawn (파티클 간 시간 간격)
	 * @param InitialLocation - Initial location for spawned particles (초기 위치, 컴포넌트 월드 위치)
	 * @param InitialVelocity - Initial velocity for spawned particles (초기 속도, 모듈에서 덮어쓸 수 있음)
	 * @param EventPayload - Event payload data (optional) (이벤트 데이터, 현재 미사용)
	 *
	 * @note MaxActiveParticles를 초과하면 생성이 중단됨
	 * @note DECLARE_PARTICLE_PTR 매크로로 파티클 메모리 주소 계산
	 */
	void SpawnParticles(
		int32 Count,
		float StartTime,
		float Increment,
		const FVector& InitialLocation,
		const FVector& InitialVelocity,
		FParticleEventInstancePayload* EventPayload = nullptr
	)
	{
		// 안전성 체크 
		if (!CurrentLODLevel || !ParticleData || !ParticleIndices)
		{
			return;
		}

		// 생성 루프
		for (int32 i = 0; i < Count; i++)
		{
			// 꽉 차면 더 이상 생성 안 되도록
			if (ActiveParticles >= MaxActiveParticles)
			{
				break;
			}

			// 매크로를 사용해서 Particle 참조 생성
			DECLARE_PARTICLE_PTR

			// 이번 파티클의 스폰 시간 계산
			float SpawnTime = StartTime + (i * Increment);
			float Interp = 0.0f; // 보간값 (서브프레임용)

			// PreSpawn: 기본값 초기화
			Particle.Location = InitialLocation;
			Particle.Velocity = InitialVelocity;
			Particle.BaseVelocity = InitialVelocity;
			Particle.RelativeTime = 0.0f;
			Particle.Lifetime = 1.0f; // 기본 1초 (모듈에서 덮어씀)
			Particle.Rotation = 0.0f;
			Particle.RotationRate = 0.0f;
			Particle.Size = FVector::One();
			Particle.Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

			// Spawn 모듈 실행
			for (int32 ModuleIndex = 0; ModuleIndex < CurrentLODLevel->SpawnModules.Num(); ModuleIndex++)
			{
			    UParticleModule* Module = CurrentLODLevel->SpawnModules[ModuleIndex];
			    if (Module && Module->IsSpawnModule())
			    {
			        Module->Spawn(this, PayloadOffset, SpawnTime, &Particle);
			    }
			}

			// PostSpawn: 서브프레임 보정 및 등록
			// 프레임 중간에 태어났다면 이동시켜줌
			if (SpawnTime > 0.0f)
			{
				Particle.Location += Particle.Velocity * SpawnTime;
			}

			// 활성 파티클 개수 증가 (중요!)
			ActiveParticles++;

			// 고유 ID 증가
			ParticleCounter++;
		}
	}

	/**
	 * Kills a particle at the specified index
	 * 지정된 인덱스의 파티클을 제거 (Swap-and-Pop 기법 사용)
	 *
	 * @param Index - Index of the particle to kill (제거할 파티클의 활성 인덱스, 0 ~ ActiveParticles-1)
	 *
	 * @note 마지막 파티클과 자리를 바꾼 뒤 ActiveParticles를 감소시킴
	 * @note 이 방식으로 중간에 빈 구멍이 생기지 않아 메모리 효율적
	 * @warning 순회 중 호출 시 역순으로 순회해야 인덱스 꼬임 방지
	 */
	void KillParticle(int32 Index)
	{
		// 범위 체크
		if (Index < 0 || Index >= ActiveParticles)
		{
			return;
		}

		// [핵심 아이디어] 배열의 마지막 파티클과 자리를 바꾸고 ActiveParticles를 줄임
		// 예: [0, 1, 2, 3, 4] 에서 2번을 죽이면 -> [0, 1, 4, 3] 이 되고 ActiveParticles = 4
		// 이렇게 하면 중간에 빈 구멍이 안 생김 (메모리 효율)

		if (Index < ActiveParticles - 1)
		{
			// 죽일 파티클의 인덱스와 마지막 파티클의 인덱스를 교환
			uint16 Temp = ParticleIndices[Index];
			ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
			ParticleIndices[ActiveParticles - 1] = Temp;
		}

		// 활성 파티클 개수 감소
		// 더 이상 ActiveParticles 범위 안에 포함되지 않아서 Update 루프에서 처리되지 않음, 즉 렌더링되지 않음
		// 오브젝트 풀 패턴이라 생각하자.
		ActiveParticles--;
	}

	/**
	 * Update all active particles
	 * 모든 활성 파티클의 수명, 위치, 회전을 업데이트
	 *
	 * @param DeltaTime - Time elapsed since last update (이전 프레임으로부터 경과 시간, 초 단위)
	 *
	 * @note Pass 1 & 2: 수명 관리 및 기본 물리 이동 (역순 순회로 안전한 Kill 처리)
	 * @note Pass 3: 모듈 업데이트 실행 (모듈마다 모든 파티클을 한 번에 처리, O(M*N) 복잡도)
	 * @note RelativeTime이 1.0 이상이면 자동으로 KillParticle 호출
	 */
	void Tick(float DeltaTime)
	{
		if (!ParticleData || !ParticleIndices || ActiveParticles <= 0)
		{
			return;
		}

		// --- Pass 1 & 2: 수명 관리 및 기본 물리 이동 ---
		// BEGIN_UPDATE_LOOP 매크로 사용 (역순 순회로 안전한 Kill 처리)
		BEGIN_UPDATE_LOOP

		// 수명 업데이트
		Particle.RelativeTime += DeltaTime / Particle.Lifetime;

		// 죽었는지 체크
		if (Particle.RelativeTime >= 1.0f)
		{
			KillParticle(i);
			continue; // 죽었으면 물리 연산 할 필요 없음
		}

		// 기본 물리 업데이트 (위치 이동)
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Rotation += Particle.RotationRate * DeltaTime;

		END_UPDATE_LOOP

		// --- Pass 3: 모듈 업데이트 (파티클 루프 밖으로!) ---
		// 모듈 하나가 "살아있는 모든 파티클"을 한 번에 처리 (Instruction Cache 효율 극대화)
		// 각 모듈 내부에서 BEGIN_UPDATE_LOOP 매크로를 통해 다시 루프를 돔
		for (int32 ModuleIndex = 0; ModuleIndex < CurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
		{
		    UParticleModule* Module = CurrentLODLevel->UpdateModules[ModuleIndex];
		    if (Module && Module->IsUpdateModule())
		    {
		        Module->Update(this, PayloadOffset, DeltaTime);
		    }
		}
	}

	/**
	 * Resize particle memory (Unreal Engine style)
	 * 파티클 메모리 리사이징 (언리얼 엔진 방식)
	 *
	 * @param NewMaxActiveParticles - New maximum particle count
	 * @param bSetMaxActiveCount - If true, update peak active particles
	 * @return true if successful
	 */
	bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true)
	{
		// 이미 충분한 크기면 리턴
		if (NewMaxActiveParticles <= MaxActiveParticles)
		{
			return true;
		}

		// Reallocate particle data (preserves existing data)
		ParticleData = (uint8*)FMemory::Realloc(ParticleData, ParticleStride * NewMaxActiveParticles);
		if (!ParticleData)
		{
			return false;
		}

		// Reallocate particle indices
		if (ParticleIndices == nullptr)
		{
			// First allocation - clear max count
			MaxActiveParticles = 0;
		}
		ParticleIndices = (uint16*)FMemory::Realloc(ParticleIndices, sizeof(uint16) * (NewMaxActiveParticles + 1));
		if (!ParticleIndices)
		{
			return false;
		}

		// Fill in default 1:1 mapping for new indices
		for (int32 i = MaxActiveParticles; i < NewMaxActiveParticles; i++)
		{
			ParticleIndices[i] = static_cast<uint16>(i);
		}

		// Update max count
		MaxActiveParticles = NewMaxActiveParticles;

		return true;
	}

	/**
	 * Initialize the emitter instance
	 * 에미터 인스턴스를 초기화 (템플릿 연결 및 상태 초기화)
	 *
	 * @param InTemplate - Template emitter to use (사용할 에미터 템플릿, 설계도 역할)
	 * @param InComponent - Owner component (소유자 컴포넌트, 월드 위치 정보 제공)
	 *
	 * @note LOD 레벨 설정, Stride 계산, 메모리 할당 수행
	 */
	void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
	{
		SpriteTemplate = InTemplate;
		Component = InComponent;

		MaxActiveParticles = 0;
		ActiveParticles = 0;
		ParticleCounter = 0;
		SpawnFraction = 0.0f;

		// 1. LOD 레벨 설정 (일단 0번 LOD 사용)
		CurrentLODLevelIndex = 0;
		if (InTemplate && InTemplate->GetNumLODs() > 0)
		{
			CurrentLODLevel = InTemplate->GetLODLevel(0);
		}

		// 2. Stride 계산 (가장 중요!)
		// 기본 파티클 크기
		ParticleSize = sizeof(FBaseParticle);
		ParticleStride = ParticleSize;

		// TODO: 모듈들이 요구하는 추가 메모리(Payload) 계산
		if (CurrentLODLevel)
		{
		    for (int32 i = 0; i < CurrentLODLevel->Modules.Num(); i++)
		    {
		        UParticleModule* Module = CurrentLODLevel->Modules[i];
		        ParticleStride += Module->RequiredBytes(CurrentLODLevel->TypeDataModule);
		    }
		}

		// 2-1. Stride 16바이트 정렬 (SIMD 최적화)
		// InParticleStride가 50이면 -> 64로, 100이면 -> 112로
		const int32 Alignment = 16;
		ParticleStride = (ParticleStride + (Alignment - 1)) & ~(Alignment - 1);

		// 3. PayloadOffset 계산 (기본 파티클 뒤에 모듈 데이터가 시작됨)
		PayloadOffset = ParticleSize;

		// 3. 메모리 할당 목표치 설정
		int32 TargetMaxParticles = 1000; // 기본값
		if (InTemplate)
		{
			TargetMaxParticles = InTemplate->GetPeakActiveParticles();
		}

		// 4. 초기 할당 (Resize 호출)
		if (TargetMaxParticles > 0)
		{
			int32 InitialCount = 10;
			if (InTemplate && InTemplate->InitialAllocationCount > 0)
			{
				InitialCount = InTemplate->InitialAllocationCount;
			}
			else if (CurrentLODLevel && CurrentLODLevel->PeakActiveParticles > 0)
			{
				InitialCount = CurrentLODLevel->PeakActiveParticles;
			}

			// 최소 10개, 최대 100개로 초기 할당 제한 (실무적 최적화)
			InitialCount = FMath::Clamp(InitialCount, 10, 100);

			Resize(InitialCount);
		}
	}

	// ============== Virtual Methods for Rendering ==============
	/**
	 * Check if dynamic data is required for rendering
	 * 렌더링을 위한 동적 데이터가 필요한지 체크
	 *
	 * @return true if there are active particles to render
	 */
	virtual bool IsDynamicDataRequired() const
	{
		return ActiveParticles > 0 && CurrentLODLevel != nullptr;
	}

	/**
	 * Retrieves the dynamic data for the emitter (render thread data)
	 * 에미터의 동적 데이터를 가져옴 (렌더 스레드용 데이터)
	 *
	 * @param bSelected - Whether the emitter is selected in the editor
	 * @return FDynamicEmitterDataBase* - The dynamic data, or nullptr if not required
	 *
	 * @note Subclasses should override this to return their specific data type
	 * @note Caller is responsible for deleting the returned pointer
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected)
	{
		// Base implementation returns null - subclasses override
		return nullptr;
	}

	/**
	 * Fill replay data with common particle information
	 * 공통 파티클 정보로 리플레이 데이터를 채움
	 *
	 * @param OutData - Output replay data to fill
	 * @return true if successful, false if no data to fill
	 *
	 * @note This is called by subclasses first, then they add type-specific data
	 */
	virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData)
	{
		if (ActiveParticles <= 0 || !ParticleData || !CurrentLODLevel)
		{
			return false;
		}

		// Fill common particle data
		OutData.ActiveParticleCount = ActiveParticles;
		OutData.ParticleStride = ParticleStride;

		// Allocate and copy particle data to container
		// ActiveParticles 크기만큼만 할당
		OutData.DataContainer.Allocate(ActiveParticles, ParticleStride);
		FMemory::Memcpy(
			OutData.DataContainer.ParticleData,
			ParticleData,
			ActiveParticles * ParticleStride // 활성 파티클만
		);

		// Copy particle indices
		FMemory::Memcpy(
			OutData.DataContainer.ParticleIndices,
			ParticleIndices,
			ActiveParticles * sizeof(uint16)
		);

		// Get scale from component transform
		if (Component)
		{
			OutData.Scale = Component->GetWorldTransform().Scale3D;
		}
		else
		{
			OutData.Scale = FVector::One();
		}

		// Get sort mode from required module
		if (CurrentLODLevel->RequiredModule)
		{
			OutData.SortMode = CurrentLODLevel->RequiredModule->GetSortMode();
		}
		else
		{
			OutData.SortMode = EParticleSortMode::None;
		}

		return true;
	}

	/**
	 * Retrieves replay data for the emitter (simplified version)
	 * 에미터의 리플레이 데이터를 가져옴 (간소화 버전)
	 *
	 * @return FDynamicEmitterReplayDataBase* - The replay data, or nullptr if not available
	 *
	 * @note Subclasses should override this to return their specific replay data type
	 * @note Caller is responsible for deleting the returned pointer
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData()
	{
		// Base implementation returns null - subclasses override
		return nullptr;
	}

	/**
	 * Retrieve the allocated size of this instance
	 * 이 인스턴스가 할당한 메모리 크기 반환
	 *
	 * @param OutNum - The size of this instance (currently used)
	 * @param OutMax - The maximum size of this instance (allocated)
	 */
	virtual void GetAllocatedSize(int32& OutNum, int32& OutMax)
	{
		int32 Size = sizeof(FParticleEmitterInstance);
		int32 ActiveParticleDataSize = (ParticleData != nullptr) ? (ActiveParticles * ParticleStride) : 0;
		int32 MaxActiveParticleDataSize = (ParticleData != nullptr) ? (MaxActiveParticles * ParticleStride) : 0;
		int32 ActiveParticleIndexSize = (ParticleIndices != nullptr) ? (ActiveParticles * sizeof(uint16)) : 0;
		int32 MaxActiveParticleIndexSize = (ParticleIndices != nullptr) ? (MaxActiveParticles * sizeof(uint16)) : 0;

		OutNum = ActiveParticleDataSize + ActiveParticleIndexSize + Size;
		OutMax = MaxActiveParticleDataSize + MaxActiveParticleIndexSize + Size;
	}


};
