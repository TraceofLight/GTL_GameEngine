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
