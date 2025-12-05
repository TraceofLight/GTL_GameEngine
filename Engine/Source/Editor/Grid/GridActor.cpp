#include "pch.h"
#include "Grid/GridActor.h"

using std::to_string;

IMPLEMENT_CLASS(AGridActor)

AGridActor::AGridActor()
{
    LineComponent = NewObject<ULineComponent>();
    LineComponent->SetupAttachment(RootComponent);
    AddOwnedComponent(LineComponent);
}

void AGridActor::Initialize()
{
    RegenerateGrid();
    if (EditorINI.count("GridSpacing"))
    {
        try
        {
            float temp = std::stof(EditorINI["GridSpacing"]);
            SetLineSize(temp);
        }
        catch (...)
        {
            SetLineSize(LineSize);
        }
    }
    else
    {
        SetLineSize(LineSize);
    }
}

AGridActor::~AGridActor()
{

}

namespace
{
    FVector4 GetGridLineColor(int index)
    {
        if (index % 10 == 0)
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 흰색
        if (index % 5 == 0)
            return FVector4(0.4f, 0.4f, 0.4f, 1.0f); // 밝은 회색
        return FVector4(0.1f, 0.1f, 0.1f, 1.0f);     // 어두운 회색
    }
}

void AGridActor::CreateGridLines(int32 InGridSize, float InCellSize, const FVector& Center)
{
    if (!LineComponent) return;

    const float gridTotalSize = InGridSize * InCellSize;

    for (int i = -InGridSize; i <= InGridSize; i++)
    {
        // Axis가 켜져있으면 원점(i==0) 건너뛰기 (컬러 축이 대체)
        // Axis가 꺼져있으면 원점 그리드 라인도 그려서 빈 공간 채우기
        if (i == 0 && bShowOriginAxis) continue;

        const float pos = i * InCellSize;
        const FVector4 color = GetGridLineColor(i);

        // X축 방향 라인
        LineComponent->AddLine(FVector(pos, -gridTotalSize, 0.0f), FVector(pos, gridTotalSize, 0.0f), color);
        // Y축 방향 라인
        LineComponent->AddLine(FVector(-gridTotalSize, pos, 0.0f), FVector(gridTotalSize, pos, 0.0f), color);
    }
}

void AGridActor::CreateAxisLines(float Length, const FVector& Origin)
{
    if (!LineComponent) return;
        
    // X축 - 빨강
    LineComponent->AddLine(Origin, 
                          Origin + FVector(Length * CellSize, 0.0f, 0.0f),
                          FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    
    // Y축 - 초록
    LineComponent->AddLine(Origin, 
                          Origin + FVector(0.0f, Length * CellSize, 0.0f),
                          FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    
    // Z축 - 파랑
    LineComponent->AddLine(Origin, 
                          Origin + FVector(0.0f, 0.0f, Length * CellSize),
                          FVector4(0.0f, 0.0f, 1.0f, 1.0f));
}

void AGridActor::ClearLines()
{
    if (LineComponent)
    {
        LineComponent->ClearLines();
    }
}

void AGridActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    //LineComponent = LineComponent->Duplicate();
}

void AGridActor::RegenerateGrid()
{
    // Clear existing lines
    ClearLines();

    // Generate new grid and axis lines with current settings
    if (bShowOriginAxis)
    {
        CreateAxisLines(AxisLength, FVector());
    }
    CreateGridLines(GridSize, CellSize, FVector());
}

