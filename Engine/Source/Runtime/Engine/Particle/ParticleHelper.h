#pragma once
#include "Particle.h"

/*-----------------------------------------------------------------------------
	Particle Sorting Helper
-----------------------------------------------------------------------------*/
struct FParticleOrder
{
	int32 ParticleIndex;

	union
	{
		float Z;
		uint32 C;
	};

	FParticleOrder(int32 InParticleIndex, float InZ) :
		ParticleIndex(InParticleIndex),
		Z(InZ)
	{
	}

	FParticleOrder(int32 InParticleIndex, uint32 InC) :
		ParticleIndex(InParticleIndex),
		C(InC)
	{
	}
};

#define DECLARE_PARTICLE(Name,Address)		\
	FBaseParticle& Name = *((FBaseParticle*) (Address));

/**
 * @brief 파티클 포인터 선언 매크로
 * @details 현재 ActiveParticles 인덱스를 사용하여 새 파티클의 메모리 주소를 계산하고
 *          FBaseParticle 참조 변수 'Particle'을 생성
 */
#define DECLARE_PARTICLE_PTR \
	/* 지금 활성화된 개수(ActiveParticles)가 곧 새로운 파티클이 들어갈 자리 */ \
	int32 CurrentIndex = ParticleIndices[ActiveParticles]; \
	\
	/* Stride를 곱해서 정확한 메모리 번지로 점프 */ \
	uint8* ParticlePtr = ParticleData + (CurrentIndex * ParticleStride); \
	\
	/* FBaseParticle 타입으로 캐스팅해서 Particle이라는 이름의 참조 변수 생성 */\
	FBaseParticle& Particle = *((FBaseParticle*)ParticlePtr);

/**
 * @brief 업데이트 루프 시작 매크로
 * @details 활성화된 모든 파티클을 순회하기 위한 루프 시작
 *          역순으로 순회하여 중간에 파티클을 제거해도 안전함
 */
#define BEGIN_UPDATE_LOOP \
	for (int32 i = ActiveParticles - 1; i >= 0; i--) \
	{ \
		int32 CurrentIndex = ParticleIndices[i]; \
		uint8* ParticlePtr = ParticleData + (CurrentIndex * ParticleStride); \
		FBaseParticle& Particle = *((FBaseParticle*)ParticlePtr);

/**
 * @brief 업데이트 루프 종료 매크로
 * @details BEGIN_UPDATE_LOOP와 짝을 이루어 사용
 */
#define END_UPDATE_LOOP \
	}
