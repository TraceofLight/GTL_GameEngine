#pragma once
#include "Splitter.h"
/**
 * @brief 수평 스플리터 (좌/우 분할)
 */
class SSplitterH : public SSplitter
{
public:
    SSplitterH() = default;
    ~SSplitterH() override;
    void UpdateDrag(FVector2D MousePos) override;
    FRect GetSplitterRect() const override;

protected:
    void UpdateChildRects() override;
    ImGuiMouseCursor GetMouseCursor() const override { return ImGuiMouseCursor_ResizeEW; }
};
