#pragma once
#include "UEContainer.h"

// 파티클 통계 구조체
// 씬에 렌더링되는 파티클들의 통계를 추적
struct FParticleStats
{
	// 파티클 시스템 개수
	uint32 TotalParticleSystems = 0;        // 씬에 있는 전체 파티클 시스템 수
	uint32 VisibleParticleSystems = 0;      // 렌더링된 파티클 시스템 수
	
	// 에미터 개수
	uint32 TotalEmitters = 0;               // 전체 에미터 수
	uint32 VisibleEmitters = 0;             // 렌더링된 에미터 수
	
	// 파티클 개수
	uint32 TotalParticles = 0;              // 전체 파티클 수 (활성화된 파티클)
	uint32 RenderedParticles = 0;           // 실제 렌더링된 파티클 수
	
	// 에미터 타입별 통계
	uint32 SpriteEmitters = 0;              // 스프라이트 에미터 수
	uint32 MeshEmitters = 0;                // 메시 에미터 수
	
	// 렌더링 통계
	uint32 TotalDrawCalls = 0;              // 총 드로우 콜 수
	uint32 TotalInsertedVertices = 0;               // 총 정점 수
	uint32 TotalDrawedVertices = 0;				// 총 인덱스 개수
	uint32 TotalDrawedTriangles = 0;              // 총 삼각형 수
	uint32 TotalInsertedInstances = 0;
	
	// 성능 지표
	double RenderTimeMS = 0.0;              // 파티클 렌더링 시간 (밀리초)
	double AverageTimePerSystem = 0.0;      // 시스템당 평균 시간
	double AverageTimePerEmitter = 0.0;     // 에미터당 평균 시간
	double AverageTimePerParticle = 0.0;    // 파티클당 평균 시간
	
	// 메모리 통계
	uint64 VertexBufferMemoryBytes = 0;     // 정점 버퍼 메모리 사용량
	uint64 IndexBufferMemoryBytes = 0;      // 인덱스 버퍼 메모리 사용량
	uint64 InstanceBufferMemoryBytes = 0;   // 인스턴스 버퍼 메모리 사용량 (메시 파티클용)
	
	// 모든 통계를 0으로 리셋
	void Reset()
	{
		TotalParticleSystems = 0;
		VisibleParticleSystems = 0;
		TotalEmitters = 0;
		VisibleEmitters = 0;
		TotalParticles = 0;
		RenderedParticles = 0;
		SpriteEmitters = 0;
		MeshEmitters = 0;
		TotalDrawCalls = 0;
		TotalInsertedVertices = 0;
		TotalDrawedVertices = 0;
		TotalDrawedTriangles = 0;
		TotalInsertedInstances = 0;
		RenderTimeMS = 0.0;
		AverageTimePerSystem = 0.0;
		AverageTimePerEmitter = 0.0;
		AverageTimePerParticle = 0.0;
		VertexBufferMemoryBytes = 0;
		IndexBufferMemoryBytes = 0;
		InstanceBufferMemoryBytes = 0;
	}
	
	// 파생 통계 계산 (평균 등)
	void CalculateDerivedStats()
	{
		// 시스템당 평균 렌더 시간
		if (VisibleParticleSystems > 0)
		{
			AverageTimePerSystem = RenderTimeMS / static_cast<double>(VisibleParticleSystems);
		}
		
		// 에미터당 평균 렌더 시간
		if (VisibleEmitters > 0)
		{
			AverageTimePerEmitter = RenderTimeMS / static_cast<double>(VisibleEmitters);
		}
		
		// 파티클당 평균 렌더 시간 (마이크로초 단위로 표시)
		if (RenderedParticles > 0)
		{
			AverageTimePerParticle = (RenderTimeMS * 1000.0) / static_cast<double>(RenderedParticles);
		}
	}
	
	// 메모리 사용량 계산 (MB 단위)
	float GetTotalMemoryMB() const
	{
		uint64 TotalBytes = VertexBufferMemoryBytes + IndexBufferMemoryBytes + InstanceBufferMemoryBytes;
		return static_cast<float>(TotalBytes) / (1024.0f * 1024.0f);
	}
};

// 파티클 통계 전역 매니저 (싱글톤)
// UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
class FParticleStatManager
{
public:
	static FParticleStatManager& GetInstance()
	{
		static FParticleStatManager Instance;
		return Instance;
	}
	
	// 통계 업데이트
	void UpdateStats(const FParticleStats& InStats)
	{
		CurrentStats = InStats;
	}
	
	// 통계 조회
	const FParticleStats& GetStats() const
	{
		return CurrentStats;
	}
	
	// 통계 리셋
	void ResetStats()
	{
		CurrentStats.Reset();
	}
	
	// 프레임 시작 시 통계 초기화
	void BeginFrame()
	{
		CurrentStats.Reset();
	}

private:
	FParticleStatManager() = default;
	~FParticleStatManager() = default;
	FParticleStatManager(const FParticleStatManager&) = delete;
	FParticleStatManager& operator=(const FParticleStatManager&) = delete;
	
	FParticleStats CurrentStats;
};
