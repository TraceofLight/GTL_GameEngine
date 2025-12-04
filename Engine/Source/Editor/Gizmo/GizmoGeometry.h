#pragma once
#include "VertexData.h"
#include "UEContainer.h"
#include "Vector.h"

/**
 * @brief Gizmo 메쉬 생성 유틸리티 구조체
 * - 다양한 기하학적 메쉬 생성 기능 제공 (원, 링, 아크, 각도 눈금)
 * - 생성된 메쉬는 FGizmoBatchRenderer를 통해 렌더링됨
 */
struct FGizmoGeometry
{
    /**
     * @brief 원형 라인 메쉬 생성 (채워지지 않은 원, 두께 있는 튜브)
     * @param Axis0 원 평면의 첫 번째 축
     * @param Axis1 원 평면의 두 번째 축
     * @param Radius 원의 반지름
     * @param Thickness 라인의 두께
     * @param OutVertices 생성된 정점 배열 (출력)
     * @param OutIndices 생성된 인덱스 배열 (출력)
     */
    static void GenerateCircleLineMesh(const FVector& Axis0, const FVector& Axis1,
                                       float Radius, float Thickness,
                                       TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices);

    /**
     * @brief 회전 각도 표시용 3D Arc 메쉬 생성
     * @param Axis0 시작 방향 벡터
     * @param Axis1 회전 평면을 정의하는 두 번째 축
     * @param InnerRadius 안쪽 반지름
     * @param OuterRadius 바깥 반지름
     * @param Thickness 링의 3D 두께
     * @param AngleInRadians 회전 각도 (라디안, 양수/음수 모두 지원, 360도 이상 가능)
     * @param StartDirection Arc의 시작 방향 벡터
     * @param OutVertices 생성된 정점 배열 (출력)
     * @param OutIndices 생성된 인덱스 배열 (출력)
     */
    static void GenerateRotationArcMesh(const FVector& Axis0, const FVector& Axis1,
                                        float InnerRadius, float OuterRadius, float Thickness, float AngleInRadians,
                                        const FVector& StartDirection, TArray<FNormalVertex>& OutVertices,
                                        TArray<uint32>& OutIndices);

    /**
     * @brief 회전 각도 눈금 메쉬 생성
     * @param Axis0 시작 방향 벡터
     * @param Axis1 회전 평면을 정의하는 두 번째 축
     * @param InnerRadius 링의 안쪽 반지름
     * @param OuterRadius 링의 바깥쪽 반지름
     * @param Thickness 눈금의 두께
     * @param SnapAngleDegrees 스냅 각도 (도)
     * @param OutVertices 생성된 정점 배열 (출력)
     * @param OutIndices 생성된 인덱스 배열 (출력)
     */
    static void GenerateAngleTickMarks(const FVector& Axis0, const FVector& Axis1,
                                       float InnerRadius, float OuterRadius, float Thickness, float SnapAngleDegrees,
                                       TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices);

    /**
     * @brief QuarterRing 메쉬를 3D 토러스 형태로 생성
     * @param Axis0 시작 방향 벡터 (0도)
     * @param Axis1 끝 방향 벡터 (90도)
     * @param InnerRadius 안쪽 반지름
     * @param OuterRadius 바깥 반지름
     * @param Thickness 링의 3D 두께
     * @param OutVertices 생성된 정점 배열 (출력)
     * @param OutIndices 생성된 인덱스 배열 (출력)
     */
    static void GenerateQuarterRingMesh(const FVector& Axis0, const FVector& Axis1,
                                        float InnerRadius, float OuterRadius, float Thickness,
                                        TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices);
};
