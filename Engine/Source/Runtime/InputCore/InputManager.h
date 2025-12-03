#pragma once

#include <windows.h>
#include <cmath>

#include "Object.h"
#include "Vector.h"
#include "ImGui/imgui.h"

// 마우스 버튼 상수
enum EMouseButton
{
    LeftButton = 0,
    RightButton = 1,
    MiddleButton = 2,
    XButton1 = 3,
    XButton2 = 4,
    MaxMouseButtons = 5
};

class UInputManager : public UObject
{
public:
    DECLARE_CLASS(UInputManager, UObject)

    // 생성자/소멸자 (싱글톤)
    UInputManager();
protected:
    ~UInputManager() override;

    // 복사 방지
    UInputManager(const UInputManager&) = delete;
    UInputManager& operator=(const UInputManager&) = delete;

public:
    // 싱글톤 접근자
    static UInputManager& GetInstance();

    // 생명주기
    void Initialize(HWND hWindow);
    void Update(); // 매 프레임 호출
    void ProcessMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 마우스 함수들
    FVector2D GetMousePosition() const { return MousePosition; }
    FVector2D GetMouseDelta() const { return MousePosition - PreviousMousePosition; }
    // 화면 크기 (픽셀) - 매 호출 시 동적 조회
    FVector2D GetScreenSize() const;

	void SetLastMousePosition(const FVector2D& Pos) { PreviousMousePosition = Pos; }

    bool IsMouseButtonDown(EMouseButton Button) const;
    bool IsMouseButtonPressed(EMouseButton Button) const; // 이번 프레임에 눌림
    bool IsMouseButtonReleased(EMouseButton Button) const; // 이번 프레임에 떼짐

    // 키보드 함수들 (게임 입력, bGameInputEnabled 체크)
    bool IsKeyDown(int KeyCode) const;
    bool IsKeyPressed(int KeyCode) const; // 이번 프레임에 눌림
    bool IsKeyReleased(int KeyCode) const; // 이번 프레임에 떼짐

    // Raw 입력 함수들 (시스템용으로 항상 실제 상태 반환, Shift + F1 등 시스템 키 감지용)
    bool IsKeyDownRaw(int KeyCode) const;
    bool IsKeyPressedRaw(int KeyCode) const;

    // 마우스 휠 함수들
    float GetMouseWheelDelta() const { return MouseWheelDelta; }
    // 디버그 로그 토글
    void SetDebugLoggingEnabled(bool bEnabled) { bEnableDebugLogging = bEnabled; }
    bool IsDebugLoggingEnabled() const { return bEnableDebugLogging; }

    bool GetIsGizmoDragging() const { return bIsGizmoDragging; }
    void SetIsGizmoDragging(bool bInGizmoDragging) { bIsGizmoDragging = bInGizmoDragging; }

    uint32 GetDraggingAxis() const { return DraggingAxis; }
    void SetDraggingAxis(uint32 Axis) { DraggingAxis = Axis; }

    // 커서 제어 함수
    void SetCursorVisible(bool bVisible);
    void LockCursor();
    void ReleaseCursor();
    bool IsCursorLocked() const { return bIsCursorLocked; }

    // 게임 입력 활성화/비활성화 (PIE Shift+F1 용)
    // false로 설정하면 모든 키보드/마우스 입력 함수가 false 반환
    void SetGameInputEnabled(bool bEnabled) { bGameInputEnabled = bEnabled; }
    bool IsGameInputEnabled() const { return bGameInputEnabled; }

private:
    // 내부 헬퍼 함수들
    void UpdateMousePosition(int X, int Y);
    void UpdateMouseButton(EMouseButton Button, bool bPressed);
    void UpdateKeyState(int KeyCode, bool bPressed);

    // 윈도우 핸들
    HWND WindowHandle;

    // 마우스 상태
    FVector2D MousePosition;
    FVector2D PreviousMousePosition;
    // 스크린/뷰포트 사이즈 (클라이언트 영역 픽셀)
    FVector2D ScreenSize;
    bool MouseButtons[MaxMouseButtons];
    bool PreviousMouseButtons[MaxMouseButtons];

    // 마우스 휠 상태
    float MouseWheelDelta;

    // 키보드 상태 (Virtual Key Code 기준)
    bool KeyStates[256];
    bool PreviousKeyStates[256];

    // 마스터 디버그 로그 온/오프
    bool bEnableDebugLogging = false;

    bool bIsGizmoDragging = false;
    uint32 DraggingAxis = 0;

    // 커서 잠금 상태
    bool bIsCursorLocked = false;
    FVector2D LockedCursorPosition;    // 잠금 위치 (중앙)
    FVector2D PreLockCursorPosition;   // 잠금 전 원래 커서 위치 (복원용)

    // 게임 입력 활성화 상태 (PIE Shift+F1 용)
    bool bGameInputEnabled = true;
};
