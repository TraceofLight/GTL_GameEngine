#include "pch.h"
#include "BlendSpace2D.h"
#include "AnimSequence.h"

IMPLEMENT_CLASS(UBlendSpace2D)

UBlendSpace2D::UBlendSpace2D()
	: XAxisMin(0.0f)
	, XAxisMax(400.0f)
	, YAxisMin(-180.0f)
	, YAxisMax(180.0f)
	, XAxisName("X")
	, YAxisName("Y")
{
}

UBlendSpace2D::~UBlendSpace2D()
{
}

/**
 * @brief 샘플 포인트 추가
 */
void UBlendSpace2D::AddSample(FVector2D Position, UAnimSequence* Animation)
{
	if (!Animation)
	{
		return;
	}

	Samples.Add(FBlendSample(Position, Animation));
}

/**
 * @brief 샘플 포인트 제거
 */
void UBlendSpace2D::RemoveSample(int32 Index)
{
	if (Index >= 0 && Index < Samples.Num())
	{
		Samples.RemoveAt(Index);
	}
}

/**
 * @brief 모든 샘플 제거
 */
void UBlendSpace2D::ClearSamples()
{
	Samples.Empty();
}

/**
 * @brief 파라미터를 정규화된 좌표로 변환 (0~1 범위)
 */
FVector2D UBlendSpace2D::NormalizeParameter(FVector2D Param) const
{
	float NormX = (Param.X - XAxisMin) / (XAxisMax - XAxisMin);
	float NormY = (Param.Y - YAxisMin) / (YAxisMax - YAxisMin);

	// 0~1 범위로 클램핑
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);

	return FVector2D(NormX, NormY);
}

/**
 * @brief 정규화된 좌표를 파라미터로 역변환
 */
FVector2D UBlendSpace2D::DenormalizeParameter(FVector2D NormalizedParam) const
{
	float X = XAxisMin + NormalizedParam.X * (XAxisMax - XAxisMin);
	float Y = YAxisMin + NormalizedParam.Y * (YAxisMax - YAxisMin);

	return FVector2D(X, Y);
}

/**
 * @brief 주어진 파라미터에서 블렌드 가중치 계산
 */
void UBlendSpace2D::GetBlendWeights(
	FVector2D BlendParameter,
	TArray<int32>& OutSampleIndices,
	TArray<float>& OutWeights) const
{
	OutSampleIndices.Empty();
	OutWeights.Empty();

	// 샘플이 없으면 중단
	if (Samples.Num() == 0)
	{
		return;
	}

	// 샘플이 1개면 그것만 사용
	if (Samples.Num() == 1)
	{
		OutSampleIndices.Add(0);
		OutWeights.Add(1.0f);
		return;
	}

	// 파라미터 정규화
	FVector2D NormParam = NormalizeParameter(BlendParameter);

	// 샘플이 2개면 선형 보간
	if (Samples.Num() == 2)
	{
		FVector2D NormPos0 = NormalizeParameter(Samples[0].Position);
		FVector2D NormPos1 = NormalizeParameter(Samples[1].Position);

		float DistTotal = (NormPos1 - NormPos0).Length();
		float Dist0 = (NormParam - NormPos0).Length();

		if (DistTotal < 0.0001f)
		{
			// 두 샘플이 같은 위치
			OutSampleIndices.Add(0);
			OutWeights.Add(1.0f);
			return;
		}

		float Alpha = FMath::Clamp(Dist0 / DistTotal, 0.0f, 1.0f);

		OutSampleIndices.Add(0);
		OutSampleIndices.Add(1);
		OutWeights.Add(1.0f - Alpha);
		OutWeights.Add(Alpha);
		return;
	}

	// 샘플이 3개 이상이면 삼각형 보간
	int32 Index0, Index1, Index2;
	float Weight0, Weight1, Weight2;

	FindClosestTriangle(NormParam, Index0, Index1, Index2, Weight0, Weight1, Weight2);

	// 가중치가 유효한 샘플만 추가
	if (Weight0 > 0.001f)
	{
		OutSampleIndices.Add(Index0);
		OutWeights.Add(Weight0);
	}
	if (Weight1 > 0.001f)
	{
		OutSampleIndices.Add(Index1);
		OutWeights.Add(Weight1);
	}
	if (Weight2 > 0.001f)
	{
		OutSampleIndices.Add(Index2);
		OutWeights.Add(Weight2);
	}

	// 가중치 합이 0이면 가장 가까운 샘플 사용
	if (OutWeights.Num() == 0)
	{
		OutSampleIndices.Add(Index0);
		OutWeights.Add(1.0f);
	}
	else
	{
		// 가중치 정규화 (합 = 1.0)
		float TotalWeight = 0.0f;
		for (float W : OutWeights)
		{
			TotalWeight += W;
		}

		if (TotalWeight > 0.001f)
		{
			for (float& W : OutWeights)
			{
				W /= TotalWeight;
			}
		}
	}
}

/**
 * @brief 삼각형 보간을 위한 가장 가까운 3개 샘플 찾기
 *
 * 단순화된 방법: 거리가 가장 가까운 3개의 샘플을 찾아서 무게중심 좌표로 블렌딩
 */
void UBlendSpace2D::FindClosestTriangle(
	FVector2D Point,
	int32& OutIndex0,
	int32& OutIndex1,
	int32& OutIndex2,
	float& OutWeight0,
	float& OutWeight1,
	float& OutWeight2) const
{
	const int32 NumSamples = Samples.Num();

	// 거리 순으로 정렬된 샘플 인덱스
	TArray<int32> SortedIndices;
	TArray<float> Distances;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		FVector2D NormPos = NormalizeParameter(Samples[i].Position);
		float Dist = (Point - NormPos).Length();

		SortedIndices.Add(i);
		Distances.Add(Dist);
	}

	// 거리 기준 정렬 (버블 정렬 - 간단하게)
	for (int32 i = 0; i < NumSamples - 1; ++i)
	{
		for (int32 j = 0; j < NumSamples - i - 1; ++j)
		{
			if (Distances[j] > Distances[j + 1])
			{
				// Swap distances
				float TempDist = Distances[j];
				Distances[j] = Distances[j + 1];
				Distances[j + 1] = TempDist;

				// Swap indices
				int32 TempIdx = SortedIndices[j];
				SortedIndices[j] = SortedIndices[j + 1];
				SortedIndices[j + 1] = TempIdx;
			}
		}
	}

	// 가장 가까운 3개 선택
	OutIndex0 = SortedIndices[0];
	OutIndex1 = (NumSamples > 1) ? SortedIndices[1] : SortedIndices[0];
	OutIndex2 = (NumSamples > 2) ? SortedIndices[2] : SortedIndices[0];

	// 무게중심 좌표 계산
	FVector2D A = NormalizeParameter(Samples[OutIndex0].Position);
	FVector2D B = NormalizeParameter(Samples[OutIndex1].Position);
	FVector2D C = NormalizeParameter(Samples[OutIndex2].Position);

	CalculateBarycentricWeights(Point, A, B, C, OutWeight0, OutWeight1, OutWeight2);
}

/**
 * @brief 무게중심 좌표 계산 (Barycentric coordinates)
 *
 * 삼각형 ABC 내부의 점 P의 무게중심 좌표를 계산합니다.
 * P = w0*A + w1*B + w2*C (w0 + w1 + w2 = 1)
 */
void UBlendSpace2D::CalculateBarycentricWeights(
	FVector2D Point,
	FVector2D A,
	FVector2D B,
	FVector2D C,
	float& OutWeightA,
	float& OutWeightB,
	float& OutWeightC) const
{
	// 벡터 계산
	FVector2D v0 = B - A;
	FVector2D v1 = C - A;
	FVector2D v2 = Point - A;

	// 내적 계산 (FVector2D는 Dot 메서드가 없으므로 직접 계산)
	float d00 = v0.X * v0.X + v0.Y * v0.Y;
	float d01 = v0.X * v1.X + v0.Y * v1.Y;
	float d11 = v1.X * v1.X + v1.Y * v1.Y;
	float d20 = v2.X * v0.X + v2.Y * v0.Y;
	float d21 = v2.X * v1.X + v2.Y * v1.Y;

	float denom = d00 * d11 - d01 * d01;

	// 삼각형이 퇴화된 경우 (일직선 상의 점들)
	if (FMath::Abs(denom) < 0.0001f)
	{
		// 가장 가까운 점의 가중치를 1로 설정
		float DistA = (Point - A).Length();
		float DistB = (Point - B).Length();
		float DistC = (Point - C).Length();

		if (DistA <= DistB && DistA <= DistC)
		{
			OutWeightA = 1.0f;
			OutWeightB = 0.0f;
			OutWeightC = 0.0f;
		}
		else if (DistB <= DistC)
		{
			OutWeightA = 0.0f;
			OutWeightB = 1.0f;
			OutWeightC = 0.0f;
		}
		else
		{
			OutWeightA = 0.0f;
			OutWeightB = 0.0f;
			OutWeightC = 1.0f;
		}
		return;
	}

	// 무게중심 좌표 계산
	OutWeightB = (d11 * d20 - d01 * d21) / denom;
	OutWeightC = (d00 * d21 - d01 * d20) / denom;
	OutWeightA = 1.0f - OutWeightB - OutWeightC;

	// 음수 가중치를 0으로 클램핑 (삼각형 외부의 점)
	OutWeightA = FMath::Max(0.0f, OutWeightA);
	OutWeightB = FMath::Max(0.0f, OutWeightB);
	OutWeightC = FMath::Max(0.0f, OutWeightC);
}
