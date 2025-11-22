#pragma once
#include "pch.h"
#define DECLARE_PARTICLE_PTR \
	/* 지금 활성화된 개수(ActivateParticles)가 곧 새로운 파티클이 들어갈 자리 */ \
	int32 CurrentIndex = ParticleIndices[ActivateParticles]; \
	\
	/* Stride를 곱해서 정확한 메모리 번지로 점프 */ \
	uint8* Ptr = ParticleData + (CurrentIndex * ParticleStride); \
	\
	/* FBaseParticle 타입으로 캐스팅해서 Particle이라는 이름의 참조 변수 생성 */\
	FBaseParticle& Particle = *((FBaseParticle*)Ptr); 

// Forward declarations
class UParticleEmitter;
class UParticleSystemComponent;
class UParticleLODLevel;
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
	// FParticleDataContainer 안의 포인터들 캐싱: 접근속도 최적화

	/** Pointer to the particle data array */
	// 실제 파티클 데이터들이 저장된 메모리 블록의 시작 주소
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
		// TODO: Implement cleanup logic
	}

	/**
	 * Spawns particles in the emitter
	 * @param Count - Number of particles to spawn
	 * @param StartTime - Starting time for the first particle
	 * @param Increment - Time increment between each particle spawn
	 * @param InitialLocation - Initial location for spawned particles
	 * @param InitialVelocity - Initial velocity for spawned particles
	 * @param EventPayload - Event payload data (optional)
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

			// TODO: Spawn 모듈들 실행 (나중에 UParticleLODLevel 구현 후 활성화)
			// for (int32 ModuleIndex = 0; ModuleIndex < CurrentLODLevel->SpawnModules.Num(); ModuleIndex++)
			// {
			//     UParticleModule* Module = CurrentLODLevel->SpawnModules[ModuleIndex];
			//     if (Module && Module->bEnabled)
			//     {
			//         Module->Spawn(this, PayloadOffset, SpawnTime);
			//     }
			// }

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
	 * @param Index - Index of the particle to kill
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
		ActiveParticles--;
	}

	/**
	 * Update all active particles
	 * @param DeltaTime - Time elapsed since last update
	 */
	void Tick(float DeltaTime)
	{
		if (!ParticleData || !ParticleIndices || ActiveParticles <= 0)
		{
			return;
		}

		// 역순으로 순회 (중간에 죽이면 인덱스가 꼬이니까)
		for (int32 i = ActiveParticles - 1; i >= 0; i--)
		{
			// 파티클 데이터 가져오기
			int32 ParticleIndex = ParticleIndices[i];
			uint8* ParticlePtr = ParticleData + (ParticleIndex * ParticleStride);
			FBaseParticle& Particle = *((FBaseParticle*)ParticlePtr);

			// 수명 업데이트
			Particle.RelativeTime += DeltaTime / Particle.Lifetime;

			// 죽었는지 체크
			if (Particle.RelativeTime >= 1.0f)
			{
				KillParticle(i);
				continue;
			}

			// 물리 업데이트
			Particle.Location += Particle.Velocity * DeltaTime;
			Particle.Rotation += Particle.RotationRate * DeltaTime;

			// TODO: Update 모듈들 실행 (나중에 UParticleLODLevel 구현 후 활성화)
			// for (UParticleModule* Module : CurrentLODLevel->UpdateModules)
			// {
			//     if (Module && Module->bEnabled)
			//     {
			//         Module->Update(this, PayloadOffset, DeltaTime);
			//     }
			// }
		}
	}

	/**
	 * Initialize the emitter instance
	 * @param InTemplate - Template emitter to use
	 * @param InComponent - Owner component
	 */
	void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
	{
		SpriteTemplate = InTemplate;
		Component = InComponent;

		// TODO: 나중에 템플릿에서 MaxParticles 정보를 가져와서 메모리 할당
		// MaxActiveParticles = InTemplate->GetMaxParticleCount();
		// ParticleSize = sizeof(FBaseParticle);
		// ParticleStride = ParticleSize; // 모듈 없으면 기본 크기만

		ActiveParticles = 0;
		ParticleCounter = 0;
		SpawnFraction = 0.0f;
	}
};
