#include "pch.h"
#include "GizmoGeometry.h"
#include "Vector.h"
#include <d3d11.h>

// Gizmo 상수 정의
namespace
{
    constexpr int32 QuarterRingSegments = 48;
    constexpr float QuarterRingStartAngle = 0.0f;
    constexpr float QuarterRingEndAngle = PI / 2.0f;  // 90 degrees
}

void FGizmoGeometry::GenerateCircleLineMesh(const FVector& Axis0, const FVector& Axis1,
                                             float Radius, float Thickness,
                                             TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
    OutVertices.Empty();
    OutIndices.Empty();

    constexpr int32 NumSegments = 64;
    constexpr int32 NumThicknessSegments = 6;
    const float HalfThickness = Thickness * 0.5f;

    FVector RotationAxis = FVector::Cross(Axis0, Axis1);
    RotationAxis = RotationAxis.GetNormalized();

    for (int32 i = 0; i <= NumSegments; ++i)
    {
        const float Angle = (2.0f * PI * static_cast<float>(i)) / NumSegments;

        const FQuat Rot = FQuat::FromAxisAngle(RotationAxis, Angle);
        FVector CircleDir = Rot.RotateVector(Axis0);
        CircleDir = CircleDir.GetNormalized();

        const FVector CirclePoint = CircleDir * Radius;

        FVector Tangent = FVector::Cross(RotationAxis, CircleDir);
        Tangent = Tangent.GetNormalized();

        const FVector ThicknessAxis1 = CircleDir;
        const FVector ThicknessAxis2 = Tangent;

        for (int32 j = 0; j < NumThicknessSegments; ++j)
        {
            const float ThickAngle = (2.0f * PI * static_cast<float>(j)) / NumThicknessSegments;
            const float CosThick = std::cosf(ThickAngle);
            const float SinThick = std::sinf(ThickAngle);

            const FVector Offset = ThicknessAxis1 * (HalfThickness * CosThick) + ThicknessAxis2 * (HalfThickness * SinThick);
            FNormalVertex Vtx;
            Vtx.pos = CirclePoint + Offset;
            Vtx.normal = Offset.GetNormalized();
            Vtx.color = FVector4(1, 1, 1, 1);
            Vtx.tex = FVector2D(0, 0);
            Vtx.Tangent = FVector4(1, 0, 0, 1);
            OutVertices.Add(Vtx);
        }
    }

    for (int32 i = 0; i < NumSegments; ++i)
    {
        for (int32 j = 0; j < NumThicknessSegments; ++j)
        {
            const int32 Current = i * NumThicknessSegments + j;
            const int32 Next = i * NumThicknessSegments + ((j + 1) % NumThicknessSegments);
            const int32 NextRing = (i + 1) * NumThicknessSegments + j;
            const int32 NextRingNext = (i + 1) * NumThicknessSegments + ((j + 1) % NumThicknessSegments);

            OutIndices.Add(Current);
            OutIndices.Add(NextRing);
            OutIndices.Add(Next);

            OutIndices.Add(Next);
            OutIndices.Add(NextRing);
            OutIndices.Add(NextRingNext);
        }
    }
}

void FGizmoGeometry::GenerateRotationArcMesh(const FVector& Axis0, const FVector& Axis1,
                                              float InnerRadius, float OuterRadius, float Thickness, float AngleInRadians,
                                              const FVector& StartDirection, TArray<FNormalVertex>& OutVertices,
                                              TArray<uint32>& OutIndices)
{
    OutVertices.Empty();
    OutIndices.Empty();

    if (std::abs(AngleInRadians) < 0.001f)
    {
        return;
    }

    const float AbsAngle = std::abs(AngleInRadians);
    const float AngleRatio = AbsAngle / (2.0f * PI);
    const int32 NumArcPoints = std::max(2, static_cast<int32>(QuarterRingSegments * 4 * AngleRatio)) + 1;
    constexpr int32 NumThicknessSegments = 6;

    FVector ZAxis = FVector::Cross(Axis0, Axis1);
    ZAxis = ZAxis.GetNormalized();

    FVector StartAxis = Axis0;
    if (StartDirection.SizeSquared() > 0.001f)
    {
        FVector ProjectedStart = StartDirection - ZAxis * FVector::Dot(StartDirection, ZAxis);
        if (ProjectedStart.SizeSquared() > 0.001f)
        {
            StartAxis = ProjectedStart.GetNormalized();
        }
    }
    const float SignedAngle = AngleInRadians;

    const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
    const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
    const float HalfThickness = Thickness * 0.5f;

    for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
    {
        const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
        const float CurrentAngle = ArcPercent * SignedAngle;

        const FQuat ArcRotQuat = FQuat::FromAxisAngle(ZAxis, CurrentAngle);
        FVector ArcDir = ArcRotQuat.RotateVector(StartAxis);
        ArcDir = ArcDir.GetNormalized();

        const FVector ArcCenter = ArcDir * MidRadius;

        const FVector& ThicknessAxis = ZAxis;
        const FVector WidthAxis = ArcDir;

        for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
        {
            const float ThickAngle = (2.0f * PI * static_cast<float>(ThickIdx)) / NumThicknessSegments;
            const float CosThick = std::cosf(ThickAngle);
            const float SinThick = std::sinf(ThickAngle);

            const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
            const FVector VertexPos = ArcCenter + Offset;

            FVector Normal = Offset;
            Normal = Normal.GetNormalized();

            FNormalVertex Vertex;
            Vertex.pos = VertexPos;
            Vertex.normal = Normal;
            Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 흰색 (Batch Color 사용)
            Vertex.tex = FVector2D(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);
            Vertex.Tangent = FVector4(1, 0, 0, 1);

            OutVertices.Add(Vertex);
        }
    }

    for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
    {
        for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
        {
            const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

            const int32 i0 = ArcIdx * NumThicknessSegments + ThickIdx;
            const int32 i1 = ArcIdx * NumThicknessSegments + NextThickIdx;
            const int32 i2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
            const int32 i3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

            OutIndices.Add(i0);
            OutIndices.Add(i2);
            OutIndices.Add(i1);

            OutIndices.Add(i1);
            OutIndices.Add(i2);
            OutIndices.Add(i3);
        }
    }
}

void FGizmoGeometry::GenerateAngleTickMarks(const FVector& Axis0, const FVector& Axis1,
                                             float InnerRadius, float OuterRadius, float Thickness, float SnapAngleDegrees,
                                             TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
    OutVertices.Empty();
    OutIndices.Empty();

    FVector ZAxis = FVector::Cross(Axis0, Axis1);
    ZAxis = ZAxis.GetNormalized();

    uint32 BaseVertexIndex = 0;

    int32 AngleStep = static_cast<int32>(SnapAngleDegrees);
    if (AngleStep <= 0)
    {
        AngleStep = 10;
    }

    for (int32 Degree = 0; Degree < 360; Degree += AngleStep)
    {
        const float AngleRad = DegreesToRadians(static_cast<float>(Degree));

        const bool bIsLargeTick = (Degree % 90 == 0);
        const float TickStartRadius = bIsLargeTick ? OuterRadius * 1.00f : OuterRadius * 0.95f;
        const float TickEndRadius = bIsLargeTick ? InnerRadius * 1.00f : InnerRadius * 1.05f;
        const float TickThickness = Thickness * (bIsLargeTick ? 0.8f : 0.5f);

        const FQuat RotQuat = FQuat::FromAxisAngle(ZAxis, AngleRad);
        FVector TickDir = RotQuat.RotateVector(Axis0);
        TickDir = TickDir.GetNormalized();

        const FVector TickStart = TickDir * TickStartRadius;
        const FVector TickEnd = TickDir * TickEndRadius;

        const FVector ThicknessOffset = ZAxis * (TickThickness * 0.5f);

        FNormalVertex Vtx0, Vtx1, Vtx2, Vtx3;
        Vtx0.pos = TickStart + ThicknessOffset;
        Vtx1.pos = TickStart - ThicknessOffset;
        Vtx2.pos = TickEnd + ThicknessOffset;
        Vtx3.pos = TickEnd - ThicknessOffset;

        FVector Normal = TickDir;
        Vtx0.normal = Vtx1.normal = Vtx2.normal = Vtx3.normal = Normal;

        const FVector4 TickColor(1.0f, 1.0f, 0.0f, 1.0f);
        Vtx0.color = Vtx1.color = Vtx2.color = Vtx3.color = TickColor;

        Vtx0.tex = Vtx1.tex = Vtx2.tex = Vtx3.tex = FVector2D(0, 0);
        Vtx0.Tangent = Vtx1.Tangent = Vtx2.Tangent = Vtx3.Tangent = FVector4(1, 0, 0, 1);

        OutVertices.Add(Vtx0);
        OutVertices.Add(Vtx1);
        OutVertices.Add(Vtx2);
        OutVertices.Add(Vtx3);

        OutIndices.Add(BaseVertexIndex + 0);
        OutIndices.Add(BaseVertexIndex + 2);
        OutIndices.Add(BaseVertexIndex + 1);

        OutIndices.Add(BaseVertexIndex + 1);
        OutIndices.Add(BaseVertexIndex + 2);
        OutIndices.Add(BaseVertexIndex + 3);

        BaseVertexIndex += 4;
    }
}

void FGizmoGeometry::GenerateQuarterRingMesh(const FVector& Axis0, const FVector& Axis1,
                                              float InnerRadius, float OuterRadius, float Thickness,
                                              TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
    OutVertices.Empty();
    OutIndices.Empty();

    constexpr int32 NumArcPoints = static_cast<int32>(QuarterRingSegments * (QuarterRingEndAngle - QuarterRingStartAngle) / (PI / 2.0f)) + 1;
    constexpr int32 NumThicknessSegments = 6;

    FVector ZAxis = FVector::Cross(Axis0, Axis1);
    ZAxis = ZAxis.GetNormalized();

    const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
    const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
    const float HalfThickness = Thickness * 0.5f;

    for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
    {
        const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
        const float ArcAngle = QuarterRingStartAngle + ArcPercent * (QuarterRingEndAngle - QuarterRingStartAngle);
        const float ArcAngleDeg = RadiansToDegrees(ArcAngle);

        const FQuat ArcRotQuat = FQuat::FromAxisAngle(ZAxis, DegreesToRadians(ArcAngleDeg));
        FVector ArcDir = ArcRotQuat.RotateVector(Axis0);
        ArcDir = ArcDir.GetNormalized();

        const FVector ArcCenter = ArcDir * MidRadius;

        const FVector& ThicknessAxis = ZAxis;
        const FVector WidthAxis = ArcDir;

        for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
        {
            const float ThickAngle = (2.0f * PI * static_cast<float>(ThickIdx)) / NumThicknessSegments;
            const float CosThick = std::cosf(ThickAngle);
            const float SinThick = std::sinf(ThickAngle);

            const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
            const FVector VertexPos = ArcCenter + Offset;

            FVector Normal = Offset;
            Normal = Normal.GetNormalized();

            FNormalVertex Vertex;
            Vertex.pos = VertexPos;
            Vertex.normal = Normal;
            Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 흰색 (Batch Color 사용)
            Vertex.tex = FVector2D(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);
            Vertex.Tangent = FVector4(1, 0, 0, 1);

            OutVertices.Add(Vertex);
        }
    }

    for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
    {
        for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
        {
            const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

            const int32 Idx0 = ArcIdx * NumThicknessSegments + ThickIdx;
            const int32 Idx1 = ArcIdx * NumThicknessSegments + NextThickIdx;
            const int32 Idx2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
            const int32 Idx3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

            OutIndices.Add(Idx0);
            OutIndices.Add(Idx2);
            OutIndices.Add(Idx1);

            OutIndices.Add(Idx1);
            OutIndices.Add(Idx2);
            OutIndices.Add(Idx3);
        }
    }
}
