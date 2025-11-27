#pragma once
#include "pch.h"
/**
 * Container for particle data arrays
 * Manages memory allocation for particle data and indices
 */
struct FParticleDataContainer
{
	/** Total size of allocated memory block in bytes */
	// OS로부터 할당받은 메모리 덩어리 전체 바이트 크기
	// = MaxParticles * ParticleStride + MaxParticles * sizeof(uint16)
	int32 MemBlockSize;

	/** Size of particle data array in bytes */
	// 전체 메모리 블록 중 순수하게 파티클 정보만 저장된 앞부분의 크기(Byte)
	// = MaxParticles * ParticleStride
	// 이 크기만큼 건너뛰어야 그 다음부터 인덱스 데이터가 나옴을 계산할 때 씀
	// ParticleIndices 의 시작 주소를 계산하는 기준점이 됨
	int32 ParticleDataNumBytes;

	/** Size of particle indices array in shorts (uint16) */
	// 뒤쪽 구역에 저장된 인덱스(uint16)의 개수 (바이트 크기 아님)
	// 최대 몇개 파티클을 관리할 수 있는지 MaxCount 의미도 가짐
	int32 ParticleIndicesNumShorts;

	/** Pointer to particle data array (also the base of the allocated memory block) */
	// 할당된 전체 메모리 블록의 '시작 주소'
	// FBaseParticle* 같은 특정 타입이 아닌 이유는 파티클 크기(Stride)가 모듈 구성에 따라 달라질 수 있기 때문
	// 1바이트 단위(uint8)로 포인터 연산해서 정확한 위치를 찾아가기 위함
	uint8* ParticleData;

	/** Pointer to particle indices array (located at the end of the memory block, not separately allocated) */
	// 할당된 메모리 블록의 뒷부분,즉 인덱스 배열이 시작되는 주소를 가리킴
	// ParticleData에서 ParticleDataNumBytes를 더한 위치를 가리키도록 세팅만 해줌
	// 메모리 아끼려고 인덱스를 int(4바이트)대신 uint16(2바이트로 씀)
	// 주의: ParticleData를 delete[] 하면 얘가 가리키던 메모리도 같이 날아가니, 절대 따로 delete하지 말 것
	uint16* ParticleIndices;

	FParticleDataContainer()
		: MemBlockSize(0)
		, ParticleDataNumBytes(0)
		, ParticleIndicesNumShorts(0)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
	{
	}

	~FParticleDataContainer()
	{
		Free();
	}
	// 해당 컨테이너는 무거워서 복사해서 쓸 일이 없으므로 복사 방지
	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;
	/**
	* @brief Allocate
	*/
	void Allocate(int32 InMaxParticles, int32 InParticleStride)
	{
		// Free existing memory first
		Free();
		if (InMaxParticles <= 0 || InParticleStride <= 0)
		{
			return;
		}

		// Calculate required memory
		ParticleDataNumBytes = InMaxParticles * InParticleStride;
		ParticleIndicesNumShorts = InMaxParticles;
		int32 IndicesBytes = ParticleIndicesNumShorts * sizeof(uint16);
		MemBlockSize = ParticleDataNumBytes + IndicesBytes;

		// Allocate single block for both particle data and indices
		// 주소가 무조건 16의 배수(0x...0)로 시작함.
		const int32 Alignment = 16;
		ParticleData = (uint8*)_aligned_malloc(MemBlockSize, Alignment);
		memset(ParticleData, 0, MemBlockSize); // 메모리 잡자마자 0으로 초기화

		// ParticleIndices points to the end of ParticleData
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);

		// Initialize indices to 0, 1, 2, 3 ... (identity mapping)
		for (int32 i = 0; i < ParticleIndicesNumShorts;++i)
		{
			ParticleIndices[i] = static_cast<uint16>(i);
		}
	}
	/**
	* @brief Free allocated memory
	*/
	void Free()
	{
		if (ParticleData)
		{
			_aligned_free(ParticleData);

			ParticleData = nullptr;
			ParticleIndices = nullptr; // Don't delete separtely - same memory block
		}

		MemBlockSize = 0;
		ParticleDataNumBytes = 0;
		ParticleIndicesNumShorts = 0;
	}
	/**
	* @brief Check if memory is allocated
	*/
	bool IsAllocated() const
	{
		return ParticleData != nullptr;
	}
	/**
	* @brief Get Particle at index
	* @param Index - Particle index to retrieve
	* @param ParticleStride - Size of each particle in bytes
	* @return Pointer to particle data, or nullptr if invalid
	*/
	template<typename ParticleType>
	ParticleType* GetParticle(int32 Index, int32 ParticleStride) const
	{
		if (!ParticleData || Index < 0 || Index >= ParticleIndicesNumShorts)
		{
			return nullptr;
		}
		// Calculate offset: Index * ParticleStride
		// 참고) 하나의 에미터 안에서는 모든 파티클의 크기(Stride)가 같음
		uint8* ParticleAddress = ParticleData + (Index * ParticleStride);

		// Cast to request type and return
		return reinterpret_cast<ParticleType*>(ParticleAddress);
	}
};
