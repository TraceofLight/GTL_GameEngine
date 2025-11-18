#include "pch.h"
#include "BlendSpace2D.h"
#include "AnimSequence.h"
#include <filesystem>

IMPLEMENT_CLASS(UBlendSpace2D)

UBlendSpace2D::UBlendSpace2D()
	: XAxisMin(0.0f)
	, XAxisMax(400.0f)
	, YAxisMin(-180.0f)
	, YAxisMax(180.0f)
	, XAxisName("X")
	, YAxisName("Y")
	, XAxisBlendWeight(1.0f)
	, YAxisBlendWeight(1.0f)
	, SyncGroupName("Default")
	, bUseSyncMarkers(false)
	, EditorPreviewParameter(FVector2D::Zero())
	, EditorSkeletalMeshPath("")
{
}

UBlendSpace2D::~UBlendSpace2D()
{
}

/**
 * @brief BlendSpace2D 직렬화
 */
void UBlendSpace2D::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UAnimationAsset::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 로드
		FJsonSerializer::ReadFloat(InOutHandle, "XAxisMin", XAxisMin, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "XAxisMax", XAxisMax, 400.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "YAxisMin", YAxisMin, -180.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "YAxisMax", YAxisMax, 180.0f);
		FJsonSerializer::ReadString(InOutHandle, "XAxisName", XAxisName, "X");
		FJsonSerializer::ReadString(InOutHandle, "YAxisName", YAxisName, "Y");
		FJsonSerializer::ReadFloat(InOutHandle, "XAxisBlendWeight", XAxisBlendWeight, 1.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "YAxisBlendWeight", YAxisBlendWeight, 1.0f);
		FJsonSerializer::ReadString(InOutHandle, "SyncGroupName", SyncGroupName, "Default");
		FJsonSerializer::ReadBool(InOutHandle, "bUseSyncMarkers", bUseSyncMarkers, false);

		// 에디터 설정 로드
		FJsonSerializer::ReadFloat(InOutHandle, "EditorPreviewParameterX", EditorPreviewParameter.X, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "EditorPreviewParameterY", EditorPreviewParameter.Y, 0.0f);
		FJsonSerializer::ReadString(InOutHandle, "EditorSkeletalMeshPath", EditorSkeletalMeshPath, "");

		// 샘플 포인트 로드
		Samples.Empty();
		int32 SampleCount = 0;
		FJsonSerializer::ReadInt32(InOutHandle, "SampleCount", SampleCount, 0);

		for (int32 i = 0; i < SampleCount; ++i)
		{
			FString Key = "Sample_" + std::to_string(i);
			JSON SampleJson;
			if (FJsonSerializer::ReadObject(InOutHandle, Key.c_str(), SampleJson, JSON::Make(JSON::Class::Object)))
			{
				FBlendSample Sample;

				FJsonSerializer::ReadFloat(SampleJson, "PositionX", Sample.Position.X, 0.0f);
				FJsonSerializer::ReadFloat(SampleJson, "PositionY", Sample.Position.Y, 0.0f);
				FJsonSerializer::ReadFloat(SampleJson, "RateScale", Sample.RateScale, 1.0f);

				// 애니메이션 경로로 로드
				FString AnimPath;
				FJsonSerializer::ReadString(SampleJson, "AnimationPath", AnimPath, "");

				if (!AnimPath.empty())
				{
					// ResourceManager에서 로드 시도
					Sample.Animation = UResourceManager::GetInstance().Load<UAnimSequence>(AnimPath);

					if (!Sample.Animation)
					{
						UE_LOG("Warning: Failed to load animation '%s' for BlendSpace sample", AnimPath.c_str());
					}
					else
					{
						// Sync Markers 로드
						int32 MarkerCount = 0;
						FJsonSerializer::ReadInt32(SampleJson, "SyncMarkerCount", MarkerCount, 0);

						for (int32 m = 0; m < MarkerCount; ++m)
						{
							FString MarkerKey = "SyncMarker_" + std::to_string(m);
							JSON MarkerJson;

							if (FJsonSerializer::ReadObject(SampleJson, MarkerKey.c_str(), MarkerJson, JSON::Make(JSON::Class::Object)))
							{
								FString MarkerName;
								float MarkerTime = 0.0f;

								FJsonSerializer::ReadString(MarkerJson, "Name", MarkerName, "");
								FJsonSerializer::ReadFloat(MarkerJson, "Time", MarkerTime, 0.0f);

								Sample.Animation->AddSyncMarker(MarkerName, MarkerTime);
							}
						}
					}
				}
				else
				{
					Sample.Animation = nullptr;
				}

				Samples.Add(Sample);
			}
		}

		// Triangles 로드
		Triangles.Empty();
		int32 TriangleCount = 0;
		FJsonSerializer::ReadInt32(InOutHandle, "TriangleCount", TriangleCount, 0);

		for (int32 i = 0; i < TriangleCount; ++i)
		{
			FString TriKey = "Triangle_" + std::to_string(i);
			JSON TriJson;

			if (FJsonSerializer::ReadObject(InOutHandle, TriKey.c_str(), TriJson, JSON::Make(JSON::Class::Object)))
			{
				FBlendTriangle Tri;
				FJsonSerializer::ReadInt32(TriJson, "Index0", Tri.Index0, -1);
				FJsonSerializer::ReadInt32(TriJson, "Index1", Tri.Index1, -1);
				FJsonSerializer::ReadInt32(TriJson, "Index2", Tri.Index2, -1);

				if (Tri.IsValid())
				{
					Triangles.Add(Tri);
				}
			}
		}
	}
	else
	{
		// 저장
		InOutHandle["XAxisMin"] = XAxisMin;
		InOutHandle["XAxisMax"] = XAxisMax;
		InOutHandle["YAxisMin"] = YAxisMin;
		InOutHandle["YAxisMax"] = YAxisMax;
		InOutHandle["XAxisName"] = XAxisName;
		InOutHandle["YAxisName"] = YAxisName;
		InOutHandle["XAxisBlendWeight"] = XAxisBlendWeight;
		InOutHandle["YAxisBlendWeight"] = YAxisBlendWeight;
		InOutHandle["SyncGroupName"] = SyncGroupName;
		InOutHandle["bUseSyncMarkers"] = bUseSyncMarkers;

		// 에디터 설정 저장
		InOutHandle["EditorPreviewParameterX"] = EditorPreviewParameter.X;
		InOutHandle["EditorPreviewParameterY"] = EditorPreviewParameter.Y;
		InOutHandle["EditorSkeletalMeshPath"] = EditorSkeletalMeshPath;

		// 샘플 포인트 저장
		InOutHandle["SampleCount"] = static_cast<int>(Samples.Num());

		for (int32 i = 0; i < Samples.Num(); ++i)
		{
			const FBlendSample& Sample = Samples[i];
			JSON SampleJson = JSON::Make(JSON::Class::Object);

			SampleJson["PositionX"] = Sample.Position.X;
			SampleJson["PositionY"] = Sample.Position.Y;
			SampleJson["RateScale"] = Sample.RateScale;

			// 애니메이션 경로 저장 (파일 경로 전체 저장)
			if (Sample.Animation)
			{
				FString AnimPath = Sample.Animation->GetFilePath();
				if (AnimPath.empty())
				{
					AnimPath = Sample.Animation->GetName(); // Fallback
				}
				SampleJson["AnimationPath"] = AnimPath;

				// Sync Markers도 저장
				const TArray<FAnimSyncMarker>& Markers = Sample.Animation->GetSyncMarkers();
				SampleJson["SyncMarkerCount"] = static_cast<int>(Markers.Num());

				for (int32 m = 0; m < Markers.Num(); ++m)
				{
					JSON MarkerJson = JSON::Make(JSON::Class::Object);
					MarkerJson["Name"] = Markers[m].MarkerName;
					MarkerJson["Time"] = Markers[m].Time;

					FString MarkerKey = "SyncMarker_" + std::to_string(m);
					SampleJson[MarkerKey] = MarkerJson;
				}
			}
			else
			{
				SampleJson["AnimationPath"] = "";
				SampleJson["SyncMarkerCount"] = 0;
			}

			FString Key = "Sample_" + std::to_string(i);
			InOutHandle[Key] = SampleJson;
		}

		// Triangles 저장
		InOutHandle["TriangleCount"] = static_cast<int>(Triangles.Num());
		for (int32 i = 0; i < Triangles.Num(); ++i)
		{
			const FBlendTriangle& Tri = Triangles[i];
			JSON TriJson = JSON::Make(JSON::Class::Object);

			TriJson["Index0"] = Tri.Index0;
			TriJson["Index1"] = Tri.Index1;
			TriJson["Index2"] = Tri.Index2;

			FString TriKey = "Triangle_" + std::to_string(i);
			InOutHandle[TriKey] = TriJson;
		}
	}
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

	// 삼각분할 재생성
	GenerateTriangulation();
}

/**
 * @brief 샘플 포인트 제거
 */
void UBlendSpace2D::RemoveSample(int32 Index)
{
	if (Index >= 0 && Index < Samples.Num())
	{
		Samples.RemoveAt(Index);

		// 삼각분할 재생성
		GenerateTriangulation();
	}
}

/**
 * @brief 모든 샘플 제거
 */
void UBlendSpace2D::ClearSamples()
{
	Samples.Empty();
	Triangles.Empty();
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
 * Delaunay 삼각분할이 존재하면 그것을 사용하고,
 * 없으면 거리 기반으로 가장 가까운 3개를 찾습니다.
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
	// Delaunay 삼각분할이 존재하면 사용
	if (Triangles.Num() > 0)
	{
		bool bFound = FindContainingTriangle(Point, OutIndex0, OutIndex1, OutIndex2,
			OutWeight0, OutWeight1, OutWeight2);

		if (bFound)
		{
			return;
		}
	}

	// Fallback: 거리 기반으로 가장 가까운 3개 찾기
	const int32 NumSamples = Samples.Num();

	// 거리 순으로 정렬된 샘플 인덱스
	TArray<int32> SortedIndices;
	TArray<float> Distances;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		FVector2D NormPos = NormalizeParameter(Samples[i].Position);

		// 축별 가중치를 적용한 거리 계산
		FVector2D Diff = Point - NormPos;
		float WeightedDiffX = Diff.X * XAxisBlendWeight;
		float WeightedDiffY = Diff.Y * YAxisBlendWeight;
		float Dist = std::sqrt(WeightedDiffX * WeightedDiffX + WeightedDiffY * WeightedDiffY);

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
 * @brief 점이 포함된 Delaunay 삼각형 찾기
 *
 * 모든 삼각형을 순회하며 점이 내부에 있는지 확인합니다.
 * 무게중심 좌표가 모두 양수이면 점이 삼각형 내부에 있습니다.
 */
bool UBlendSpace2D::FindContainingTriangle(
	FVector2D Point,
	int32& OutIndex0,
	int32& OutIndex1,
	int32& OutIndex2,
	float& OutWeight0,
	float& OutWeight1,
	float& OutWeight2) const
{
	for (const FBlendTriangle& Tri : Triangles)
	{
		if (!Tri.IsValid())
		{
			continue;
		}

		// 삼각형의 3개 정점
		FVector2D A = NormalizeParameter(Samples[Tri.Index0].Position);
		FVector2D B = NormalizeParameter(Samples[Tri.Index1].Position);
		FVector2D C = NormalizeParameter(Samples[Tri.Index2].Position);

		// 무게중심 좌표 계산
		float WeightA, WeightB, WeightC;
		CalculateBarycentricWeights(Point, A, B, C, WeightA, WeightB, WeightC);

		// 모든 가중치가 0 이상이면 점이 삼각형 내부에 있음
		// 약간의 허용 오차를 두어 경계선 케이스 처리
		const float Epsilon = -0.001f;
		if (WeightA >= Epsilon && WeightB >= Epsilon && WeightC >= Epsilon)
		{
			OutIndex0 = Tri.Index0;
			OutIndex1 = Tri.Index1;
			OutIndex2 = Tri.Index2;
			OutWeight0 = WeightA;
			OutWeight1 = WeightB;
			OutWeight2 = WeightC;
			return true;
		}
	}

	// 포함하는 삼각형을 못 찾음 (경계 밖)
	// 가장 가까운 삼각형 찾기
	float MinDist = FLT_MAX;
	int32 ClosestTriIndex = -1;

	for (int32 i = 0; i < Triangles.Num(); ++i)
	{
		const FBlendTriangle& Tri = Triangles[i];
		if (!Tri.IsValid())
		{
			continue;
		}

		// 삼각형 중심점까지의 거리
		FVector2D A = NormalizeParameter(Samples[Tri.Index0].Position);
		FVector2D B = NormalizeParameter(Samples[Tri.Index1].Position);
		FVector2D C = NormalizeParameter(Samples[Tri.Index2].Position);
		FVector2D Center((A.X + B.X + C.X) / 3.0f, (A.Y + B.Y + C.Y) / 3.0f);

		float Dist = (Point - Center).Length();
		if (Dist < MinDist)
		{
			MinDist = Dist;
			ClosestTriIndex = i;
		}
	}

	if (ClosestTriIndex >= 0)
	{
		const FBlendTriangle& Tri = Triangles[ClosestTriIndex];
		FVector2D A = NormalizeParameter(Samples[Tri.Index0].Position);
		FVector2D B = NormalizeParameter(Samples[Tri.Index1].Position);
		FVector2D C = NormalizeParameter(Samples[Tri.Index2].Position);

		OutIndex0 = Tri.Index0;
		OutIndex1 = Tri.Index1;
		OutIndex2 = Tri.Index2;

		CalculateBarycentricWeights(Point, A, B, C, OutWeight0, OutWeight1, OutWeight2);
		return true;
	}

	return false;
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

/**
 * @brief BlendSpace2D를 파일로 저장
 */
bool UBlendSpace2D::SaveToFile(const FString& FilePath)
{
	JSON JsonData = JSON::Make(JSON::Class::Object);
	Serialize(false, JsonData);

	std::filesystem::path Path(FilePath);
	bool bSuccess = FJsonSerializer::SaveJsonToFile(JsonData, Path);

	if (bSuccess)
	{
		UE_LOG("BlendSpace2D saved to: %s", FilePath.c_str());
	}
	else
	{
		UE_LOG("Failed to save BlendSpace2D to: %s", FilePath.c_str());
	}

	return bSuccess;
}

/**
 * @brief 파일로부터 BlendSpace2D 로드
 */
UBlendSpace2D* UBlendSpace2D::LoadFromFile(const FString& FilePath)
{
	JSON JsonData;
	std::filesystem::path Path(FilePath);

	if (!FJsonSerializer::LoadJsonFromFile(JsonData, Path))
	{
		UE_LOG("Failed to load BlendSpace2D from: %s", FilePath.c_str());
		return nullptr;
	}

	// 새 BlendSpace2D 객체 생성 및 역직렬화
	UBlendSpace2D* BlendSpace = NewObject<UBlendSpace2D>();
	BlendSpace->SetFilePath(FilePath);
	BlendSpace->Serialize(true, JsonData);

	UE_LOG("BlendSpace2D loaded from: %s", FilePath.c_str());
	return BlendSpace;
}

// ========================================
// Delaunay 삼각분할 구현
// ========================================

/**
 * @brief Delaunay 삼각분할 생성 (Bowyer-Watson 알고리즘)
 *
 * 샘플이 3개 미만이면 삼각분할을 생성하지 않습니다.
 * 3개 이상일 때 Bowyer-Watson 알고리즘으로 최적의 삼각분할을 생성합니다.
 */
void UBlendSpace2D::GenerateTriangulation()
{
	Triangles.Empty();

	// 샘플이 3개 미만이면 삼각분할 불가능
	if (Samples.Num() < 3)
	{
		return;
	}

	// 정규화된 좌표로 변환된 샘플 포인트 배열
	TArray<FVector2D> Points;
	Points.Reserve(Samples.Num() + 3); // +3 for super triangle

	// Super Triangle 생성
	FBlendTriangle SuperTriangle;
	CreateSuperTriangle(Points, SuperTriangle);

	// 초기 삼각형 목록 (Super Triangle만 포함)
	TArray<FBlendTriangle> TempTriangles;
	TempTriangles.Add(SuperTriangle);

	// 샘플 포인트를 정규화하여 추가
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		Points.Add(NormalizeParameter(Samples[i].Position));
	}

	// Bowyer-Watson 알고리즘: 각 샘플 포인트를 하나씩 추가
	for (int32 PointIndex = 3; PointIndex < Points.Num(); ++PointIndex) // 3부터 시작 (0~2는 Super Triangle)
	{
		FVector2D Point = Points[PointIndex];

		// Step 1: 이 점이 외접원 내부에 있는 "불량 삼각형" 찾기
		TArray<FBlendTriangle> BadTriangles;
		for (const FBlendTriangle& Tri : TempTriangles)
		{
			FVector2D A = Points[Tri.Index0];
			FVector2D B = Points[Tri.Index1];
			FVector2D C = Points[Tri.Index2];

			if (IsPointInCircumcircle(Point, A, B, C))
			{
				BadTriangles.Add(Tri);
			}
		}

		// Step 2: 불량 삼각형의 경계(Edge) 찾기
		struct FEdge
		{
			int32 V0, V1;
			FEdge(int32 InV0, int32 InV1) : V0(InV0), V1(InV1) {}
		};
		TArray<FEdge> Polygon;

		for (const FBlendTriangle& BadTri : BadTriangles)
		{
			// 3개의 변
			FEdge Edge0(BadTri.Index0, BadTri.Index1);
			FEdge Edge1(BadTri.Index1, BadTri.Index2);
			FEdge Edge2(BadTri.Index2, BadTri.Index0);

			// 각 변이 다른 불량 삼각형과 공유되지 않으면 Polygon에 추가
			auto AddEdgeIfUnique = [&](const FEdge& Edge)
			{
				bool bShared = false;
				for (const FBlendTriangle& OtherTri : BadTriangles)
				{
					if (OtherTri.Index0 == BadTri.Index0 &&
						OtherTri.Index1 == BadTri.Index1 &&
						OtherTri.Index2 == BadTri.Index2)
					{
						continue; // 같은 삼각형은 스킵
					}

					// 변이 공유되는지 확인 (양방향)
					if (OtherTri.ContainsEdge(Edge.V0, Edge.V1) ||
						OtherTri.ContainsEdge(Edge.V1, Edge.V0))
					{
						bShared = true;
						break;
					}
				}

				if (!bShared)
				{
					Polygon.Add(Edge);
				}
			};

			AddEdgeIfUnique(Edge0);
			AddEdgeIfUnique(Edge1);
			AddEdgeIfUnique(Edge2);
		}

		// Step 3: 불량 삼각형 제거
		for (const FBlendTriangle& BadTri : BadTriangles)
		{
			for (int32 i = TempTriangles.Num() - 1; i >= 0; --i)
			{
				const FBlendTriangle& Tri = TempTriangles[i];
				if (Tri.Index0 == BadTri.Index0 &&
					Tri.Index1 == BadTri.Index1 &&
					Tri.Index2 == BadTri.Index2)
				{
					TempTriangles.RemoveAt(i);
					break;
				}
			}
		}

		// Step 4: 새 점과 Polygon의 각 변으로 새 삼각형 생성
		for (const FEdge& Edge : Polygon)
		{
			FBlendTriangle NewTri(Edge.V0, Edge.V1, PointIndex);
			if (NewTri.IsValid())
			{
				TempTriangles.Add(NewTri);
			}
		}
	}

	// Step 5: Super Triangle과 연결된 삼각형 제거
	for (const FBlendTriangle& Tri : TempTriangles)
	{
		// Super Triangle의 인덱스는 0, 1, 2
		if (Tri.ContainsVertex(0) || Tri.ContainsVertex(1) || Tri.ContainsVertex(2))
		{
			continue; // Super Triangle 포함 삼각형은 스킵
		}

		// 샘플 인덱스를 조정 (Points 배열의 3~N을 Samples 배열의 0~N-3으로 매핑)
		FBlendTriangle FinalTri(
			Tri.Index0 - 3,
			Tri.Index1 - 3,
			Tri.Index2 - 3
		);

		if (FinalTri.IsValid() &&
			FinalTri.Index0 >= 0 && FinalTri.Index0 < Samples.Num() &&
			FinalTri.Index1 >= 0 && FinalTri.Index1 < Samples.Num() &&
			FinalTri.Index2 >= 0 && FinalTri.Index2 < Samples.Num())
		{
			Triangles.Add(FinalTri);
		}
	}

	UE_LOG("Delaunay Triangulation: %d samples -> %d triangles", Samples.Num(), Triangles.Num());
}

/**
 * @brief 점이 삼각형의 외접원 내부에 있는지 확인
 *
 * Determinant 방법 사용 (빠르고 정확함)
 */
bool UBlendSpace2D::IsPointInCircumcircle(
	FVector2D Point,
	FVector2D A,
	FVector2D B,
	FVector2D C) const
{
	// 행렬식 계산
	float ax = A.X - Point.X;
	float ay = A.Y - Point.Y;
	float bx = B.X - Point.X;
	float by = B.Y - Point.Y;
	float cx = C.X - Point.X;
	float cy = C.Y - Point.Y;

	float det =
		(ax * ax + ay * ay) * (bx * cy - cx * by) -
		(bx * bx + by * by) * (ax * cy - cx * ay) +
		(cx * cx + cy * cy) * (ax * by - bx * ay);

	return det > 0.0f;
}

/**
 * @brief Super Triangle 생성
 *
 * 모든 샘플을 포함할 수 있을 만큼 큰 삼각형을 생성합니다.
 * 정규화된 좌표 (0~1)를 사용하므로 (-10, -10), (10, -10), (0, 20) 정도로 충분합니다.
 */
void UBlendSpace2D::CreateSuperTriangle(
	TArray<FVector2D>& OutVertices,
	FBlendTriangle& OutTriangle) const
{
	// Super Triangle의 3개 정점 (매우 큰 삼각형)
	OutVertices.Add(FVector2D(-10.0f, -10.0f));  // Index 0
	OutVertices.Add(FVector2D(10.0f, -10.0f));   // Index 1
	OutVertices.Add(FVector2D(0.0f, 20.0f));     // Index 2

	OutTriangle = FBlendTriangle(0, 1, 2);
}
