#pragma once
#include "UIWindow.h"
#include <wrl/client.h>

struct ID3D11ShaderResourceView;

/**
 * @brief Main Menu Bar Window (Borderless Window Title Bar)
 * @details 커스텀 타이틀바 역할을 하는 상단 메뉴바 윈도우
 *          윈도우 컨트롤 버튼 (최소화, 최대화, 종료) 포함
 *
 * @param MenuBarHeight 메뉴바 높이 (픽셀)
 * @param bIsMenuBarVisible 메뉴바 표시 여부
 * @param AppIcon 앱 아이콘 텍스처
 */
class UMainMenuBarWindow : public UUIWindow
{
public:
    DECLARE_CLASS(UMainMenuBarWindow, UUIWindow)

    UMainMenuBarWindow();
    ~UMainMenuBarWindow() override = default;

    void Initialize() override;
    void RenderMenuBar();

    float GetMenuBarHeight() const { return MenuBarHeight; }
    bool IsMenuBarVisible() const { return bIsMenuBarVisible; }
    void SetMenuBarVisible(bool bVisible) { bIsMenuBarVisible = bVisible; }

private:
    void LoadIcons();
    void RenderAppIcon();
    void RenderFileMenu();
    void RenderViewMenu();
    void RenderWindowsMenu();
    void RenderHelpMenu();
    void RenderWindowControls();

    float MenuBarHeight = 54.0f;  // 패딩(4) + 메뉴바(22) + 레벨탭(28)
    float LeftPadding = 60.0f;   // 아이콘(10+42) + 여백(8)
    bool bIsMenuBarVisible = true;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AppIconSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MinimizeIconSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> MaximizeIconSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CloseIconSRV;
};
