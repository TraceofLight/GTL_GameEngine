#pragma once
#include "UEContainer.h"

// 스키닝 통계 구조체
// 스켈레탈 메시 스키닝 관련 통계를 추적
struct FSkinningStats
{
	// 스켈레탈 메시 개수
	uint32 TotalSkeletalMeshCount = 0;      // 씬에 있는 전체 스켈레탈 메시 수
	uint32 VisibleSkeletalMeshCount = 0;    // 렌더링된 스켈레탈 메시 수

	// 본 개수
	uint32 TotalBoneCount = 0;              // 전체 본 수
	uint32 MaxBonesPerMesh = 0;             // 메시당 최대 본 수
	float AvgBonesPerMesh = 0.0f;           // 메시당 평균 본 수

	// 모든 통계를 0으로 리셋
	void Reset()
	{
		TotalSkeletalMeshCount = 0;
		VisibleSkeletalMeshCount = 0;
		TotalBoneCount = 0;
		MaxBonesPerMesh = 0;
		AvgBonesPerMesh = 0.0f;
	}

	// 파생 통계 계산 (평균 등)
	void CalculateDerivedStats()
	{
		if (VisibleSkeletalMeshCount > 0)
		{
			AvgBonesPerMesh = static_cast<float>(TotalBoneCount) / static_cast<float>(VisibleSkeletalMeshCount);
		}
	}
};

// 스키닝 통계 전역 매니저 (싱글톤)
class FSkinningStatManager
{
public:
	static FSkinningStatManager& GetInstance()
	{
		static FSkinningStatManager Instance;
		return Instance;
	}

	// 통계 업데이트
	void UpdateStats(const FSkinningStats& InStats)
	{
		CurrentStats = InStats;
	}

	// 통계 조회
	const FSkinningStats& GetStats() const
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
	FSkinningStatManager() = default;
	~FSkinningStatManager() = default;
	FSkinningStatManager(const FSkinningStatManager&) = delete;
	FSkinningStatManager& operator=(const FSkinningStatManager&) = delete;

	FSkinningStats CurrentStats;
};
