#pragma once
#include "Window.h"

class UControlPanelWindow;
class USceneWindow;
class SControlPanel :
    public SWindow
{
public:
    SControlPanel();
    virtual ~SControlPanel();

    void Initialize();
    virtual void OnRender() override;
    virtual void OnUpdate(float deltaSecond) override;

private:
    UControlPanelWindow* ControlPanelWidget;
    USceneWindow* SceneWindow;
};

