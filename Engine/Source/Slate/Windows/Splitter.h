#pragma once
#include"Vector.h"
#include "Window.h"
#include <fstream>
#include <string>
#include <vector>
class SSplitter : public SWindow
{
public:
    SSplitter();
    virtual ~SSplitter();

    // Child window setup
    void SetLeftOrTop(SWindow* Window) { SideLT = Window; }
    void SetRightOrBottom(SWindow* Window) { SideRB = Window; }

    SWindow* GetLeftOrTop() const { return SideLT; }
    SWindow* GetRightOrBottom() const { return SideRB; }

    // Split ratio (0.0 ~ 1.0)
    void SetSplitRatio(float Ratio) { SplitRatio = FMath::Clamp(Ratio, 0.1f, 0.9f); }
    float GetSplitRatio() const { return SplitRatio; }

    // Animation ratio setting (allows full 0.0~1.0 range, bypassing clamp)
    void SetEffectiveRatio(float Ratio) { SplitRatio = FMath::Clamp(Ratio, 0.0f, 1.0f); }
    float GetEffectiveRatio() const { return SplitRatio; }

    // Drag related
    bool IsMouseOnSplitter(FVector2D MousePos) const;
    void StartDrag(FVector2D MousePos);
    virtual void UpdateDrag(FVector2D MousePos);
    void EndDrag();

    // Virtual functions
    void OnRender() override;
    void OnUpdate(float DeltaSeconds) override;
    void OnMouseMove(FVector2D MousePos) override;
    void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    // Config save/load
    virtual void SaveToConfig(const FString& SectionName) const;
    virtual void LoadFromConfig(const FString& SectionName);

    // Splitter rect (public for unified splitter control in USlateManager)
    virtual FRect GetSplitterRect() const = 0;

    SWindow* SideLT = nullptr;  // Left or Top
    SWindow* SideRB = nullptr;  // Right or Bottom
    float SplitRatio = 0.5f;    // Split ratio

    // Min child size (set to 0 during animation to allow extreme values)
    int32 MinChildSize = 4;

protected:
    int SplitterThickness = 12;  // Splitter thickness

    bool bIsDragging = false;
    FVector2D DragStartPos;
    float DragStartRatio;

    // Abstract functions - implemented by subclasses
    virtual void UpdateChildRects() = 0;
    virtual ImGuiMouseCursor GetMouseCursor() const = 0;
};
