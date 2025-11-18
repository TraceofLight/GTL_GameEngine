#pragma once
#include "AnimationAsset.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

class UAnimSequence;

/**
 * @brief Blend Space 2D 샘플 포인트
 *
 * 2D 공간상의 한 점에 애니메이션을 매핑합니다.
 * 예: (속도=200, 방향=0) → WalkForward 애니메이션
 */
struct FBlendSample
{
	FVector2D Position;        // 2D 공간상의 위치
	UAnimSequence* Animation;  // 해당 위치의 애니메이션
	float RateScale;           // 재생 속도 배율 (1.0 = 정상 속도, 2.0 = 2배속)

	FBlendSample()
		: Position(FVector2D::Zero())
		, Animation(nullptr)
		, RateScale(1.0f)
	{
	}

	FBlendSample(FVector2D InPos, UAnimSequence* InAnim, float InRateScale = 1.0f)
		: Position(InPos)
		, Animation(InAnim)
		, RateScale(InRateScale)
	{
	}
};

/**
 * @brief Delaunay 삼각분할용 삼각형
 *
 * 3개의 샘플 포인트 인덱스로 구성된 삼각형
 */
struct FBlendTriangle
{
	int32 Index0;  // 첫 번째 샘플 인덱스
	int32 Index1;  // 두 번째 샘플 인덱스
	int32 Index2;  // 세 번째 샘플 인덱스

	FBlendTriangle()
		: Index0(-1)
		, Index1(-1)
		, Index2(-1)
	{
	}

	FBlendTriangle(int32 InIdx0, int32 InIdx1, int32 InIdx2)
		: Index0(InIdx0)
		, Index1(InIdx1)
		, Index2(InIdx2)
	{
	}

	// 삼각형이 유효한지 확인
	bool IsValid() const
	{
		return Index0 >= 0 && Index1 >= 0 && Index2 >= 0 &&
			Index0 != Index1 && Index1 != Index2 && Index2 != Index0;
	}

	// 특정 인덱스를 포함하는지 확인
	bool ContainsVertex(int32 VertexIndex) const
	{
		return Index0 == VertexIndex || Index1 == VertexIndex || Index2 == VertexIndex;
	}

	// 특정 변(edge)을 포함하는지 확인
	bool ContainsEdge(int32 EdgeIndex0, int32 EdgeIndex1) const
	{
		return (ContainsVertex(EdgeIndex0) && ContainsVertex(EdgeIndex1));
	}
};

/**
 * @brief Blend Space 2D 애셋
 *
 * 2차원 파라미터(예: 속도, 방향)로 여러 애니메이션을 블렌딩합니다.
 *
 * 사용 예시:
 * - 8방향 이동: X=속도(0~400), Y=방향(-180~180)
 * - 조준 오프셋: X=Yaw(-90~90), Y=Pitch(-45~45)
 */
class UBlendSpace2D : public UAnimationAsset
{
public:
	DECLARE_CLASS(UBlendSpace2D, UAnimationAsset)

	UBlendSpace2D();
	virtual ~UBlendSpace2D() override;

	// ===== 직렬화 =====
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ===== 파일 저장/로드 =====

	/**
	 * @brief BlendSpace2D를 파일로 저장
	 * @param FilePath 저장할 파일 경로 (예: "Data/Animation/Locomotion.blend2d")
	 * @return 성공 여부
	 */
	bool SaveToFile(const FString& FilePath);

	/**
	 * @brief 파일로부터 BlendSpace2D 로드
	 * @param FilePath 로드할 파일 경로
	 * @return 로드된 BlendSpace2D 객체 (실패 시 nullptr)
	 */
	static UBlendSpace2D* LoadFromFile(const FString& FilePath);

	// ===== 파라미터 범위 =====
	float XAxisMin;  // X축 최소값
	float XAxisMax;  // X축 최대값
	float YAxisMin;  // Y축 최소값
	float YAxisMax;  // Y축 최대값

	FString XAxisName;  // X축 이름 (예: "Speed")
	FString YAxisName;  // Y축 이름 (예: "Direction")

	// ===== 축별 블렌드 가중치 =====
	float XAxisBlendWeight;  // X축 가중치 (1.0 = 정상, 높을수록 X축이 더 중요)
	float YAxisBlendWeight;  // Y축 가중치 (1.0 = 정상, 높을수록 Y축이 더 중요)

	// ===== 샘플 포인트 =====
	TArray<FBlendSample> Samples;

	// ===== 삼각분할 데이터 =====
	TArray<FBlendTriangle> Triangles;

	// ===== 샘플 관리 =====

	/**
	 * @brief 샘플 포인트 추가
	 */
	void AddSample(FVector2D Position, UAnimSequence* Animation);

	/**
	 * @brief 샘플 포인트 제거
	 */
	void RemoveSample(int32 Index);

	/**
	 * @brief 모든 샘플 제거
	 */
	void ClearSamples();

	/**
	 * @brief 샘플 개수 반환
	 */
	int32 GetNumSamples() const { return Samples.Num(); }

	// ===== 삼각분할 =====

	/**
	 * @brief Delaunay 삼각분할 생성 (Bowyer-Watson 알고리즘)
	 *
	 * 샘플 포인트를 기반으로 Delaunay 삼각분할을 생성합니다.
	 * 이 메서드는 샘플이 추가/제거/수정될 때 호출되어야 합니다.
	 */
	void GenerateTriangulation();

	// ===== 블렌딩 계산 =====

	/**
	 * @brief 주어진 파라미터에서 블렌드 가중치 계산
	 *
	 * @param BlendParameter 입력 파라미터 (예: FVector2D(Speed, Direction))
	 * @param OutSampleIndices 블렌딩에 사용될 샘플 인덱스들
	 * @param OutWeights 각 샘플의 가중치 (합 = 1.0)
	 */
	void GetBlendWeights(
		FVector2D BlendParameter,
		TArray<int32>& OutSampleIndices,
		TArray<float>& OutWeights) const;

	/**
	 * @brief 파라미터를 정규화된 좌표로 변환 (0~1 범위)
	 */
	FVector2D NormalizeParameter(FVector2D Param) const;

	/**
	 * @brief 정규화된 좌표를 파라미터로 역변환
	 */
	FVector2D DenormalizeParameter(FVector2D NormalizedParam) const;

private:
	/**
	 * @brief 삼각형 보간을 위한 가장 가까운 3개 샘플 찾기
	 *
	 * Delaunay 삼각분할이 존재하면 그것을 사용하고,
	 * 없으면 거리 기반으로 가장 가까운 3개를 찾습니다.
	 */
	void FindClosestTriangle(
		FVector2D Point,
		int32& OutIndex0,
		int32& OutIndex1,
		int32& OutIndex2,
		float& OutWeight0,
		float& OutWeight1,
		float& OutWeight2) const;

	/**
	 * @brief 점이 포함된 Delaunay 삼각형 찾기
	 *
	 * @return 삼각형을 찾으면 true, 못 찾으면 false
	 */
	bool FindContainingTriangle(
		FVector2D Point,
		int32& OutIndex0,
		int32& OutIndex1,
		int32& OutIndex2,
		float& OutWeight0,
		float& OutWeight1,
		float& OutWeight2) const;

	/**
	 * @brief 무게중심 좌표 계산 (Barycentric coordinates)
	 */
	void CalculateBarycentricWeights(
		FVector2D Point,
		FVector2D A,
		FVector2D B,
		FVector2D C,
		float& OutWeightA,
		float& OutWeightB,
		float& OutWeightC) const;

	// ===== Delaunay 삼각분할 헬퍼 메서드 =====

	/**
	 * @brief 점이 삼각형의 외접원 내부에 있는지 확인
	 */
	bool IsPointInCircumcircle(
		FVector2D Point,
		FVector2D A,
		FVector2D B,
		FVector2D C) const;

	/**
	 * @brief Super Triangle 생성 (모든 샘플을 포함하는 큰 삼각형)
	 */
	void CreateSuperTriangle(
		TArray<FVector2D>& OutVertices,
		FBlendTriangle& OutTriangle) const;
};
