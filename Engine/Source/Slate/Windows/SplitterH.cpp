#include "pch.h"
#include "SplitterH.h"

SSplitterH::~SSplitterH()
{
}

void SSplitterH::UpdateDrag(FVector2D MousePos)
{
    if (!bIsDragging) return;

    float NewSplitX = MousePos.X;
    float MinX = Rect.Min.X + SplitterThickness;
    float MaxX = Rect.Max.X - SplitterThickness;

    NewSplitX = FMath::Clamp(NewSplitX, MinX, MaxX);

    float NewRatio = (NewSplitX - Rect.Min.X) / GetWidth();
    SetSplitRatio(NewRatio);
}

void SSplitterH::UpdateChildRects()
{
    if (!SideLT || !SideRB) return;

    float TotalWidth = GetWidth();
    float SplitX = Rect.Min.X + (TotalWidth * SplitRatio);

    // MinChildSize enforcement for animation
    float LeftWidth = SplitX - Rect.Min.X - SplitterThickness / 2;
    float RightWidth = Rect.Max.X - SplitX - SplitterThickness / 2;

    // Clamp to ensure minimum size (0 allowed during animation)
    if (MinChildSize > 0)
    {
        if (LeftWidth < MinChildSize)
        {
            SplitX = Rect.Min.X + MinChildSize + SplitterThickness / 2;
        }
        if (RightWidth < MinChildSize)
        {
            SplitX = Rect.Max.X - MinChildSize - SplitterThickness / 2;
        }
    }

    // Left area
    SideLT->SetRect(
        Rect.Min.X, Rect.Min.Y,
        SplitX - SplitterThickness / 2, Rect.Max.Y
    );

    // Right area
    SideRB->SetRect(
        SplitX + SplitterThickness / 2, Rect.Min.Y,
        Rect.Max.X, Rect.Max.Y
    );
}

FRect SSplitterH::GetSplitterRect() const
{
    float SplitX = Rect.Min.X + (GetWidth() * SplitRatio);
    return FRect(
        SplitX - SplitterThickness / 2, Rect.Min.Y,
        SplitX + SplitterThickness / 2, Rect.Max.Y
    );
}
