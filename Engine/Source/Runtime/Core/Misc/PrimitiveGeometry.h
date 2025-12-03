#pragma once
#include "VertexData.h"
#include "Vector.h"
#include <cmath>

/**
 * @brief 프리미티브 메시 생성 유틸리티
 * @details Sphere, Box, Capsule 등의 기본 도형 메시 데이터 생성
 */
class FPrimitiveGeometry
{
public:
    /**
     * @brief 구(Sphere) 메시 생성
     * @param OutMesh 출력 메시 데이터
     * @param Radius 반지름
     * @param Slices 가로 분할 수 (경도)
     * @param Stacks 세로 분할 수 (위도)
     * @param Color 버텍스 컬러 (RGBA)
     */
    static void GenerateSphere(FMeshData& OutMesh, float Radius, int32 Slices = 16, int32 Stacks = 8, const FVector4& Color = FVector4(0, 0.8f, 0, 0.4f))
    {
        OutMesh.Vertices.clear();
        OutMesh.Indices.clear();
        OutMesh.Color.clear();
        OutMesh.Normal.clear();

        // 버텍스 생성
        for (int32 i = 0; i <= Stacks; ++i)
        {
            float phi = PI * static_cast<float>(i) / static_cast<float>(Stacks);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int32 j = 0; j <= Slices; ++j)
            {
                float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(Slices);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                FVector normal(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
                FVector position = normal * Radius;

                OutMesh.Vertices.Add(position);
                OutMesh.Normal.Add(normal);
                OutMesh.Color.Add(Color);
            }
        }

        // 인덱스 생성 (삼각형)
        for (int32 i = 0; i < Stacks; ++i)
        {
            for (int32 j = 0; j < Slices; ++j)
            {
                int32 first = i * (Slices + 1) + j;
                int32 second = first + Slices + 1;

                // 첫 번째 삼각형
                OutMesh.Indices.Add(first);
                OutMesh.Indices.Add(second);
                OutMesh.Indices.Add(first + 1);

                // 두 번째 삼각형
                OutMesh.Indices.Add(second);
                OutMesh.Indices.Add(second + 1);
                OutMesh.Indices.Add(first + 1);
            }
        }
    }

    /**
     * @brief 박스(Box) 메시 생성
     * @param OutMesh 출력 메시 데이터
     * @param HalfExtent 각 축의 절반 크기
     * @param Color 버텍스 컬러 (RGBA)
     */
    static void GenerateBox(FMeshData& OutMesh, const FVector& HalfExtent, const FVector4& Color = FVector4(0, 0, 0.8f, 0.4f))
    {
        OutMesh.Vertices.clear();
        OutMesh.Indices.clear();
        OutMesh.Color.clear();
        OutMesh.Normal.clear();

        float hx = HalfExtent.X;
        float hy = HalfExtent.Y;
        float hz = HalfExtent.Z;

        // 8개의 코너 정점 (각 면마다 별도의 버텍스 - 노멀이 다르므로)
        // Front face (+Z)
        AddQuad(OutMesh,
            FVector(-hx, -hy, hz), FVector(hx, -hy, hz), FVector(hx, hy, hz), FVector(-hx, hy, hz),
            FVector(0, 0, 1), Color);

        // Back face (-Z)
        AddQuad(OutMesh,
            FVector(hx, -hy, -hz), FVector(-hx, -hy, -hz), FVector(-hx, hy, -hz), FVector(hx, hy, -hz),
            FVector(0, 0, -1), Color);

        // Right face (+X)
        AddQuad(OutMesh,
            FVector(hx, -hy, hz), FVector(hx, -hy, -hz), FVector(hx, hy, -hz), FVector(hx, hy, hz),
            FVector(1, 0, 0), Color);

        // Left face (-X)
        AddQuad(OutMesh,
            FVector(-hx, -hy, -hz), FVector(-hx, -hy, hz), FVector(-hx, hy, hz), FVector(-hx, hy, -hz),
            FVector(-1, 0, 0), Color);

        // Top face (+Y)
        AddQuad(OutMesh,
            FVector(-hx, hy, hz), FVector(hx, hy, hz), FVector(hx, hy, -hz), FVector(-hx, hy, -hz),
            FVector(0, 1, 0), Color);

        // Bottom face (-Y)
        AddQuad(OutMesh,
            FVector(-hx, -hy, -hz), FVector(hx, -hy, -hz), FVector(hx, -hy, hz), FVector(-hx, -hy, hz),
            FVector(0, -1, 0), Color);
    }

    /**
     * @brief 캡슐(Capsule) 메시 생성
     * @param OutMesh 출력 메시 데이터
     * @param Radius 반지름
     * @param HalfHeight 원기둥 부분의 절반 높이 (반구 제외)
     * @param Slices 가로 분할 수
     * @param Stacks 반구의 세로 분할 수
     * @param Color 버텍스 컬러 (RGBA)
     */
    static void GenerateCapsule(FMeshData& OutMesh, float Radius, float HalfHeight, int32 Slices = 16, int32 HemiStacks = 4, const FVector4& Color = FVector4(0.8f, 0, 0.8f, 0.4f))
    {
        OutMesh.Vertices.clear();
        OutMesh.Indices.clear();
        OutMesh.Color.clear();
        OutMesh.Normal.clear();

        // 상단 반구 버텍스
        for (int32 i = 0; i <= HemiStacks; ++i)
        {
            float phi = (PI * 0.5f) * static_cast<float>(i) / static_cast<float>(HemiStacks);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int32 j = 0; j <= Slices; ++j)
            {
                float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(Slices);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                FVector normal(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
                FVector position = normal * Radius + FVector(0, 0, HalfHeight);

                OutMesh.Vertices.Add(position);
                OutMesh.Normal.Add(normal);
                OutMesh.Color.Add(Color);
            }
        }

        int32 topHemiVertStart = 0;
        int32 topHemiVertEnd = (HemiStacks + 1) * (Slices + 1);

        // 원기둥 부분 (상단 링 + 하단 링)
        int32 cylinderTopStart = OutMesh.Vertices.Num();
        for (int32 j = 0; j <= Slices; ++j)
        {
            float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(Slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            FVector normal(cosTheta, sinTheta, 0);

            // 상단 링
            OutMesh.Vertices.Add(FVector(Radius * cosTheta, Radius * sinTheta, HalfHeight));
            OutMesh.Normal.Add(normal);
            OutMesh.Color.Add(Color);
        }

        int32 cylinderBottomStart = OutMesh.Vertices.Num();
        for (int32 j = 0; j <= Slices; ++j)
        {
            float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(Slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            FVector normal(cosTheta, sinTheta, 0);

            // 하단 링
            OutMesh.Vertices.Add(FVector(Radius * cosTheta, Radius * sinTheta, -HalfHeight));
            OutMesh.Normal.Add(normal);
            OutMesh.Color.Add(Color);
        }

        // 하단 반구 버텍스
        int32 bottomHemiStart = OutMesh.Vertices.Num();
        for (int32 i = 0; i <= HemiStacks; ++i)
        {
            float phi = (PI * 0.5f) + (PI * 0.5f) * static_cast<float>(i) / static_cast<float>(HemiStacks);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (int32 j = 0; j <= Slices; ++j)
            {
                float theta = TWO_PI * static_cast<float>(j) / static_cast<float>(Slices);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                FVector normal(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
                FVector position = normal * Radius + FVector(0, 0, -HalfHeight);

                OutMesh.Vertices.Add(position);
                OutMesh.Normal.Add(normal);
                OutMesh.Color.Add(Color);
            }
        }

        // 상단 반구 인덱스
        for (int32 i = 0; i < HemiStacks; ++i)
        {
            for (int32 j = 0; j < Slices; ++j)
            {
                int32 first = topHemiVertStart + i * (Slices + 1) + j;
                int32 second = first + Slices + 1;

                OutMesh.Indices.Add(first);
                OutMesh.Indices.Add(first + 1);
                OutMesh.Indices.Add(second);

                OutMesh.Indices.Add(second);
                OutMesh.Indices.Add(first + 1);
                OutMesh.Indices.Add(second + 1);
            }
        }

        // 원기둥 인덱스
        for (int32 j = 0; j < Slices; ++j)
        {
            int32 top = cylinderTopStart + j;
            int32 bottom = cylinderBottomStart + j;

            OutMesh.Indices.Add(top);
            OutMesh.Indices.Add(top + 1);
            OutMesh.Indices.Add(bottom);

            OutMesh.Indices.Add(bottom);
            OutMesh.Indices.Add(top + 1);
            OutMesh.Indices.Add(bottom + 1);
        }

        // 하단 반구 인덱스
        for (int32 i = 0; i < HemiStacks; ++i)
        {
            for (int32 j = 0; j < Slices; ++j)
            {
                int32 first = bottomHemiStart + i * (Slices + 1) + j;
                int32 second = first + Slices + 1;

                OutMesh.Indices.Add(first);
                OutMesh.Indices.Add(first + 1);
                OutMesh.Indices.Add(second);

                OutMesh.Indices.Add(second);
                OutMesh.Indices.Add(first + 1);
                OutMesh.Indices.Add(second + 1);
            }
        }
    }

    /**
     * @brief 타원형 원뿔(Elliptical Cone) 메시 생성 - Constraint Swing 범위 시각화용
     * @param OutMesh 출력 메시 데이터
     * @param Swing1Angle Swing1 각도 (degrees) - Y축 방향
     * @param Swing2Angle Swing2 각도 (degrees) - Z축 방향
     * @param Length 원뿔 길이
     * @param Slices 원주 분할 수
     * @param Color 버텍스 컬러 (RGBA)
     */
    static void GenerateSwingCone(FMeshData& OutMesh, float Swing1Angle, float Swing2Angle, float Length, int32 Slices = 16, const FVector4& Color = FVector4(0, 0.8f, 0, 0.3f))
    {
        OutMesh.Vertices.clear();
        OutMesh.Indices.clear();
        OutMesh.Color.clear();
        OutMesh.Normal.clear();

        if (Swing1Angle <= 0.0f && Swing2Angle <= 0.0f)
            return;

        float Swing1Rad = Swing1Angle * (PI / 180.0f);
        float Swing2Rad = Swing2Angle * (PI / 180.0f);

        // 원뿔 꼭지점 (원점)
        int32 apexIndex = 0;
        OutMesh.Vertices.Add(FVector(0, 0, 0));
        OutMesh.Normal.Add(FVector(-1, 0, 0));
        OutMesh.Color.Add(Color);

        // 원뿔 테두리 점들 생성
        for (int32 i = 0; i <= Slices; ++i)
        {
            float Angle = TWO_PI * static_cast<float>(i) / static_cast<float>(Slices);

            // 타원형 원뿔: Y방향으로 Swing1, Z방향으로 Swing2
            float YOffset = std::sin(Swing1Rad) * std::sin(Angle);
            float ZOffset = std::sin(Swing2Rad) * std::cos(Angle);
            float XOffset = std::cos(std::max(Swing1Rad, Swing2Rad));

            FVector Dir = FVector(XOffset, YOffset, ZOffset);
            Dir = Dir.GetNormalized();
            FVector EdgePoint = Dir * Length;

            OutMesh.Vertices.Add(EdgePoint);
            OutMesh.Normal.Add(Dir);
            OutMesh.Color.Add(Color);
        }

        // 원뿔 측면 삼각형 (꼭지점에서 테두리로)
        for (int32 i = 0; i < Slices; ++i)
        {
            OutMesh.Indices.Add(apexIndex);
            OutMesh.Indices.Add(i + 1);
            OutMesh.Indices.Add(i + 2);
        }

        // 원뿔 바닥 (선택적 - 반투명이면 필요없을 수도)
        // 바닥 중심점
        int32 baseCenterIdx = OutMesh.Vertices.Num();
        float avgX = Length * std::cos(std::max(Swing1Rad, Swing2Rad));
        OutMesh.Vertices.Add(FVector(avgX, 0, 0));
        OutMesh.Normal.Add(FVector(1, 0, 0));
        OutMesh.Color.Add(Color);

        // 바닥 삼각형
        for (int32 i = 0; i < Slices; ++i)
        {
            OutMesh.Indices.Add(baseCenterIdx);
            OutMesh.Indices.Add(i + 2);
            OutMesh.Indices.Add(i + 1);
        }
    }

    /**
     * @brief Twist 호(Arc) 메시 생성 - Constraint Twist 범위 시각화용
     * @param OutMesh 출력 메시 데이터
     * @param TwistAngle Twist 각도 (degrees, 양방향)
     * @param Radius 호 반지름
     * @param Thickness 호 두께
     * @param Slices 분할 수
     * @param Color 버텍스 컬러 (RGBA)
     */
    static void GenerateTwistArc(FMeshData& OutMesh, float TwistAngle, float Radius, float Thickness = 0.5f, int32 Slices = 16, const FVector4& Color = FVector4(1.0f, 0.6f, 0, 0.4f))
    {
        OutMesh.Vertices.clear();
        OutMesh.Indices.clear();
        OutMesh.Color.clear();
        OutMesh.Normal.clear();

        if (TwistAngle <= 0.0f)
            return;

        float TwistRad = TwistAngle * (PI / 180.0f);

        // 부채꼴(Fan) 형태로 생성: 중심점에서 방사형으로 삼각형 생성
        // 중심점 (인덱스 0)
        FVector CenterPos(0, 0, 0);
        OutMesh.Vertices.Add(CenterPos);
        OutMesh.Normal.Add(FVector(-1, 0, 0));
        OutMesh.Color.Add(Color);

        // 외곽 버텍스들 (인덱스 1 ~ Slices+1)
        for (int32 i = 0; i <= Slices; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(Slices);
            float Angle = -TwistRad + t * 2.0f * TwistRad;

            float cosA = std::cos(Angle);
            float sinA = std::sin(Angle);

            FVector EdgePos(0, cosA * Radius, sinA * Radius);
            OutMesh.Vertices.Add(EdgePos);
            OutMesh.Normal.Add(FVector(-1, 0, 0));
            OutMesh.Color.Add(Color);
        }

        // 삼각형으로 연결 (중심점 기준 Fan)
        for (int32 i = 0; i < Slices; ++i)
        {
            // 중심점(0) -> 외곽점(i+1) -> 외곽점(i+2)
            OutMesh.Indices.Add(0);
            OutMesh.Indices.Add(i + 1);
            OutMesh.Indices.Add(i + 2);
        }
    }

private:
    /**
     * @brief 쿼드(사각형) 추가 헬퍼
     */
    static void AddQuad(FMeshData& OutMesh,
        const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3,
        const FVector& Normal, const FVector4& Color)
    {
        int32 baseIndex = OutMesh.Vertices.Num();

        OutMesh.Vertices.Add(V0);
        OutMesh.Vertices.Add(V1);
        OutMesh.Vertices.Add(V2);
        OutMesh.Vertices.Add(V3);

        OutMesh.Normal.Add(Normal);
        OutMesh.Normal.Add(Normal);
        OutMesh.Normal.Add(Normal);
        OutMesh.Normal.Add(Normal);

        OutMesh.Color.Add(Color);
        OutMesh.Color.Add(Color);
        OutMesh.Color.Add(Color);
        OutMesh.Color.Add(Color);

        // 두 개의 삼각형으로 쿼드 구성
        OutMesh.Indices.Add(baseIndex);
        OutMesh.Indices.Add(baseIndex + 1);
        OutMesh.Indices.Add(baseIndex + 2);

        OutMesh.Indices.Add(baseIndex);
        OutMesh.Indices.Add(baseIndex + 2);
        OutMesh.Indices.Add(baseIndex + 3);
    }
};
