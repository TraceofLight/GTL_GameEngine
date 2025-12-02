#include "pch.h"
#include "SplitterV.h"

SSplitterV::~SSplitterV()
{
}

void SSplitterV::UpdateDrag(FVector2D MousePos)
{
    if (!bIsDragging) return;

    float NewSplitY = MousePos.Y;
    float MinY = Rect.Min.Y + SplitterThickness;
    float MaxY = Rect.Max.Y - SplitterThickness;

    NewSplitY = FMath::Clamp(NewSplitY, MinY, MaxY);

    float NewRatio = (NewSplitY - Rect.Min.Y) / GetHeight();
    SetSplitRatio(NewRatio);
}

void SSplitterV::UpdateChildRects()
{
    if (!SideLT || !SideRB) return;

    float TotalHeight = GetHeight();
    float SplitY = Rect.Min.Y + (TotalHeight * SplitRatio);

    // MinChildSize enforcement for animation
    float TopHeight = SplitY - Rect.Min.Y - SplitterThickness / 2;
    float BottomHeight = Rect.Max.Y - SplitY - SplitterThickness / 2;

    // Clamp to ensure minimum size (0 allowed during animation)
    if (MinChildSize > 0)
    {
        if (TopHeight < MinChildSize)
        {
            SplitY = Rect.Min.Y + MinChildSize + SplitterThickness / 2;
        }
        if (BottomHeight < MinChildSize)
        {
            SplitY = Rect.Max.Y - MinChildSize - SplitterThickness / 2;
        }
    }

    // Top area
    SideLT->SetRect(
        Rect.Min.X, Rect.Min.Y,
        Rect.Max.X, SplitY - SplitterThickness / 2
    );

    // Bottom area
    SideRB->SetRect(
        Rect.Min.X, SplitY + SplitterThickness / 2,
        Rect.Max.X, Rect.Max.Y
    );
}

FRect SSplitterV::GetSplitterRect() const
{
    float SplitY = Rect.Min.Y + (GetHeight() * SplitRatio);
    return FRect(
        Rect.Min.X, SplitY - SplitterThickness / 2,
        Rect.Max.X, SplitY + SplitterThickness / 2
    );
}
