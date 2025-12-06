#include "pch.h"
#include "EditorEngine.h"
#include "USlateManager.h"
#include "SelectionManager.h"
#include "FAudioDevice.h"
#include "FbxLoader.h"
#include <ObjManager.h>
#include "ClothManager.h"
#include "AsyncLoader.h"

#include "MiniDump.h"

float UEditorEngine::ClientWidth = 1024.0f;
float UEditorEngine::ClientHeight = 1024.0f;

static void LoadIniFile()
{
    std::ifstream infile("editor.ini");
    if (!infile.is_open()) return;

    std::string line;
    while (std::getline(infile, line))
    {
        if (line.empty() || line[0] == ';') continue;
        size_t delimiterPos = line.find('=');
        if (delimiterPos != FString::npos)
        {
            FString key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            EditorINI[key] = value;
        }
    }
}

static void SaveIniFile()
{
    std::ofstream outfile("editor.ini");
    for (const auto& pair : EditorINI)
        outfile << pair.first << " = " << pair.second << std::endl;
}

UEditorEngine::UEditorEngine()
{

}

UEditorEngine::~UEditorEngine()
{
    // Cleanup is now handled in Shutdown()
    // Do not call FObjManager::Clear() here due to static destruction order
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void UEditorEngine::GetViewportSize(HWND hWnd)
{
    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    ClientWidth = static_cast<float>(clientRect.right - clientRect.left);
    ClientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    if (ClientWidth <= 0) ClientWidth = 1;
    if (ClientHeight <= 0) ClientHeight = 1;

    //레거시
    extern float CLIENTWIDTH;
    extern float CLIENTHEIGHT;

    CLIENTWIDTH = ClientWidth;
    CLIENTHEIGHT = ClientHeight;
}

LRESULT CALLBACK UEditorEngine::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // WM_NCHITTEST는 ImGui보다 먼저 처리되어야 함 (윈도우 리사이징 커서 우선)
    if (message == WM_NCHITTEST)
    {
        // 클라이언트 좌표로 변환
        POINT ScreenPoint;
        ScreenPoint.x = static_cast<int16>(LOWORD(lParam));
        ScreenPoint.y = static_cast<int16>(HIWORD(lParam));
        ScreenToClient(hWnd, &ScreenPoint);

        // 윈도우 크기 가져오기
        RECT WindowRect;
        GetClientRect(hWnd, &WindowRect);

        // 리사이징 가능한 가장자리 크기 (픽셀)
        const int BorderWidth = 8;

        // 가장자리 영역 체크 (리사이징 우선순위가 가장 높음)
        bool bOnLeft = ScreenPoint.x < BorderWidth;
        bool bOnRight = ScreenPoint.x >= WindowRect.right - BorderWidth;
        bool bOnTop = ScreenPoint.y < BorderWidth;
        bool bOnBottom = ScreenPoint.y >= WindowRect.bottom - BorderWidth;

        // 모서리 우선 처리
        if (bOnTop && bOnLeft) return HTTOPLEFT;
        if (bOnTop && bOnRight) return HTTOPRIGHT;
        if (bOnBottom && bOnLeft) return HTBOTTOMLEFT;
        if (bOnBottom && bOnRight) return HTBOTTOMRIGHT;

        // 가장자리 처리
        if (bOnLeft) return HTLEFT;
        if (bOnRight) return HTRIGHT;
        if (bOnTop) return HTTOP;
        if (bOnBottom) return HTBOTTOM;

        // 상단 메뉴바 영역이면 드래그 가능하도록 설정 (30픽셀)
        if (ScreenPoint.y >= 0 && ScreenPoint.y <= 30)
        {
            if (ImGui::GetIO().WantCaptureMouse && ImGui::IsAnyItemHovered())
            {
                // ImGui 요소 위에 있으면 클라이언트 영역으로 처리
                return HTCLIENT;
            }
            // 빈 공간이면 타이틀바처럼 동작
            return HTCAPTION;
        }

        // 나머지는 클라이언트 영역
        return HTCLIENT;
    }

    // Input first
    INPUT.ProcessMessage(hWnd, message, wParam, lParam);

    // ImGui next
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        if (ImGui::GetIO().WantCaptureMouse)
        {
            return true;
        }
    }

    switch (message)
    {
    case WM_NCCALCSIZE:
        // non-client 영역을 완전히 제거 (borderless window)
        if (wParam == TRUE)
        {
            return 0;
        }
        break;
    case WM_SIZE:
    {
        WPARAM SizeType = wParam;
        if (SizeType != SIZE_MINIMIZED)
        {
            GetViewportSize(hWnd);

            UINT NewWidth = static_cast<UINT>(ClientWidth);
            UINT NewHeight = static_cast<UINT>(ClientHeight);
            GEngine.GetRHIDevice()-> OnResize(NewWidth, NewHeight);

            // Save CLIENT AREA size (will be converted back to window size on load)
            EditorINI["WindowWidth"] = std::to_string(NewWidth);
            EditorINI["WindowHeight"] = std::to_string(NewHeight);

            if (ImGui::GetCurrentContext() != nullptr)
            {
                ImGuiIO& io = ImGui::GetIO();
                if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0)
                {
                    UI.RepositionImGuiWindows();
                }
            }
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

UWorld* UEditorEngine::GetDefaultWorld()
{
    if (!WorldContexts.IsEmpty() && WorldContexts[0].World)
    {
        return WorldContexts[0].World;
    }
    return nullptr;
}

bool UEditorEngine::CreateMainWindow(HINSTANCE hInstance)
{
    // 윈도우 생성
    WCHAR WindowClass[] = L"JungleWindowClass";
    WCHAR Title[] = L"FutureEngine";
    HICON hIcon = (HICON)LoadImageW(NULL, L"Data\\Default\\Icon\\Future.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);

    WNDCLASSW wndclass = {};
    wndclass.style = 0;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = hIcon;
    wndclass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wndclass.hbrBackground = nullptr;
    wndclass.lpszMenuName = nullptr;
    wndclass.lpszClassName = WindowClass;
    RegisterClassW(&wndclass);

    // Load client area size from INI
    int clientWidth = 1920, clientHeight = 1080;
    if (EditorINI.count("WindowWidth"))
    {
        try { clientWidth = stoi(EditorINI["WindowWidth"]); } catch (...) {}
    }
    if (EditorINI.count("WindowHeight"))
    {
        try { clientHeight = stoi(EditorINI["WindowHeight"]); } catch (...) {}
    }

    // Validate minimum window size to prevent unusable windows
    if (clientWidth < 800) clientWidth = 1920;
    if (clientHeight < 600) clientHeight = 1080;

    // Borderless window: WS_POPUP (타이틀바 제거) + WS_THICKFRAME (리사이징)
    DWORD windowStyle = WS_POPUP | WS_VISIBLE | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    HWnd = CreateWindowExW(0, WindowClass, Title, windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, clientWidth, clientHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!HWnd)
    {
        return false;
    }

    // DWM을 사용하여 클라이언트 영역을 non-client 영역까지 확장 (Borderless window)
    MARGINS Margins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(HWnd, &Margins);

    // 아이콘 설정
    if (hIcon)
    {
        SendMessageW(HWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        SendMessageW(HWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
    }

    ShowWindow(HWnd, SW_SHOW);
    UpdateWindow(HWnd);
    SetWindowTextW(HWnd, Title);

    // 종횡비 계산
    GetViewportSize(HWnd);
    return true;
}

bool UEditorEngine::Startup(HINSTANCE hInstance)
{
    LoadIniFile();

    if (!CreateMainWindow(hInstance))
        return false;

    //디바이스 리소스 및 렌더러 생성
    RHIDevice.Initialize(HWnd);
    Renderer = std::make_unique<URenderer>(&RHIDevice);

    // Audio Device 초기화
    FAudioDevice::Initialize();

    //매니저 초기화
    UI.Initialize(HWnd, RHIDevice.GetDevice(), RHIDevice.GetDeviceContext());
    INPUT.Initialize(HWnd);

    // PhysX: initialize SDK/scene/material before world initialization
    // PhysXGlobals::InitializePhysX(false);

    // 통합 에셋 프리로드
    UResourceManager::GetInstance().PreloadAllAssets();

    ///////////////////////////////////
    WorldContexts.Add(FWorldContext(NewObject<UWorld>(), EWorldType::Editor));
    GWorld = WorldContexts[0].World;
    WorldContexts[0].World->Initialize();
    ///////////////////////////////////

    // 슬레이트 매니저 (singleton)
    FRect ScreenRect(0, 0, ClientWidth, ClientHeight);
    SLATE.Initialize(RHIDevice.GetDevice(), GWorld, ScreenRect);

    bRunning = true;
    return true;
}

void UEditorEngine::Tick(float DeltaSeconds)
{
    if (bPIEActive && INPUT.IsKeyDownRaw(VK_SHIFT) && INPUT.IsKeyPressedRaw(VK_F1))
    {
        TogglePIEInputCapture();
    }

    // 비동기 로딩 큐 처리
    UResourceManager::GetInstance().ProcessLoadQueue(5.0f);

    //@TODO UV 스크롤 입력 처리 로직 이동
    HandleUVInput(DeltaSeconds);

    //@TODO: Delta Time 계산 + EditorActor Tick은 어떻게 할 것인가
    for (auto& WorldContext : WorldContexts)
    {
        WorldContext.World->Tick(DeltaSeconds);
    }

    SLATE.Update(DeltaSeconds);
    UI.Update(DeltaSeconds);
    INPUT.Update();
}

void UEditorEngine::Render()
{
    Renderer->BeginFrame();

    UI.Render();
    SLATE.Render();
    UI.EndFrame();
    SLATE.RenderAfterUI();

    Renderer->EndFrame();
}

void UEditorEngine::HandleUVInput(float DeltaSeconds)
{
    UInputManager& InputMgr = UInputManager::GetInstance();
    if (InputMgr.IsKeyPressed('T'))
    {
        bUVScrollPaused = !bUVScrollPaused;
        if (bUVScrollPaused)
        {
            UVScrollTime = 0.0f;
            if (Renderer) Renderer->GetRHIDevice()->UpdateUVScrollConstantBuffers(UVScrollSpeed, UVScrollTime);
        }
    }
    if (!bUVScrollPaused)
    {
        UVScrollTime += DeltaSeconds;
        if (Renderer) Renderer->GetRHIDevice()->UpdateUVScrollConstantBuffers(UVScrollSpeed, UVScrollTime);
    }

}


void UEditorEngine::MainLoop()
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    LARGE_INTEGER PrevTime, CurrTime;
    QueryPerformanceCounter(&PrevTime);

    MSG msg;

    while (bRunning)
    {
        QueryPerformanceCounter(&CurrTime);
        float DeltaSeconds = static_cast<float>((CurrTime.QuadPart - PrevTime.QuadPart) / double(Frequency.QuadPart));
        PrevTime = CurrTime;

        // 처리할 메시지가 더 이상 없을때 까지 수행
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                bRunning = false;
                break;
            }
        }

        if (!bRunning) break;

        if (bChangedPieToEditor)
        {
            if (GWorld && bPIEActive)
            {
                WorldContexts.pop_back();
                ObjectFactory::DeleteObject(GWorld);
            }

            GWorld = WorldContexts[0].World;
            GWorld->GetSelectionManager()->ClearSelection();
            GWorld->GetLightManager()->SetDirtyFlag();
            SLATE.SetPIEWorld(GWorld);

            bPIEActive = false;
            bPIEInputCaptured = true;  // 상태 리셋

            // PIE 종료 시 입력 및 커서 복원
            INPUT.SetGameInputEnabled(true);
            INPUT.SetCursorVisible(true);
            if (INPUT.IsCursorLocked())
            {
                INPUT.ReleaseCursor();
            }

            UE_LOG("Editor: EndPIE");

            bChangedPieToEditor = false;
        }



		// GWorld에 PhysX가 활성화되어있고, simulate, fetchResult가 분리되어있다면,
		// 여기서 fetchResult를 실행해준다.
		if (GWorld && GWorld->GetPhysicsSceneHandle().IsValid())
		{
			if (PHYSICS.GetPipelineMode() == EPhysicsPipelineMode::FetchAfterRender)
			{
				// Tick에서 돌린 simulate 결과 받기
				PHYSICS.EndSimulate(GWorld->GetPhysicsSceneHandle(), true);
			}
		}
		FClothManager::GetInstance().ClothSimulation(DeltaSeconds);

        Tick(DeltaSeconds);
		// Physics simulation is now handled per-World in UWorld::Tick
        Render();

        // Shader Hot Reloading - Call AFTER render to avoid mid-frame resource conflicts
        // This ensures all GPU commands are submitted before we check for shader updates
        UResourceManager::GetInstance().CheckAndReloadShaders(DeltaSeconds);
    	CrashLoop();
    }
}

void UEditorEngine::Shutdown()
{
    // 비동기 로더를 먼저 종료해야 워커 스레드가 리소스 접근 중 크래시 방지
    UResourceManager::GetInstance().Clear();

    // 월드부터 삭제해야 DeleteAll 때 문제가 없음
    for (FWorldContext WorldContext : WorldContexts)
    {
        ObjectFactory::DeleteObject(WorldContext.World);
    }
    WorldContexts.clear();

    // Release ImGui first (it may hold D3D11 resources)
    UUIManager::GetInstance().Release();

    USlateManager::GetInstance().Shutdown();
    // Delete all UObjects (Components, Actors, Resources)
    // Resource destructors will properly release D3D resources
    ObjectFactory::DeleteAll(true);

    // Clear FObjManager's static map BEFORE static destruction
    // This must be done in Shutdown() (before main() exits) rather than ~UEditorEngine()
    // because ObjStaticMeshMap is a static member variable that may be destroyed
    // before the global GEngine variable's destructor runs
    FObjManager::Clear();

    // AudioDevice 종료
    FAudioDevice::Shutdown();

    // IMPORTANT: Explicitly release Renderer before RHIDevice destructor runs
    // Renderer may hold references to D3D resources
    Renderer.reset();

    // Explicitly release D3D11RHI resources before global destruction
    RHIDevice.Release();

    // PhysX shutdown (release scene/SDK)
    // PhysXGlobals::ShutdownPhysX();

    SaveIniFile();
}


void UEditorEngine::StartPIE()
{
    UE_LOG("Editor: StartPIE");

    // 비동기 로딩 완료 대기
    FAsyncLoader& AsyncLoader = FAsyncLoader::Get();
    if (AsyncLoader.IsLoading())
    {
        UE_LOG("EditorEngine: StartPIE: Waiting for async loading");

        // 로딩이 완료될 때까지 대기 (메시지 펌프 유지)
        MSG msg;
        while (AsyncLoader.IsLoading())
        {
            // 완료된 리소스 처리 (메인 스레드에서)
            AsyncLoader.ProcessCompletedResources();

            // 메시지 펌프 유지 (UI 응답성)
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                {
                    return;
                }
            }

            // CPU 과부하 방지
            Sleep(1);
        }

        // 마지막으로 완료된 리소스 처리
        AsyncLoader.ProcessCompletedResources();
        UE_LOG("EditorEngine: StartPIE: Async loading completed");
    }

    UWorld* EditorWorld = WorldContexts[0].World;
    UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorld);

    GWorld = PIEWorld;
    SLATE.SetPIEWorld(GWorld);  // SLATE의 카메라를 가져와서 설정, TODO: 추후 월드의 카메라 컴포넌트를 가져와서 설정하도록 변경 필요

    bPIEActive = true;
    bPIEInputCaptured = false;  // PIE 시작 시 Detach 상태 (에디터 UI 조작 가능)

    // PIE 시작 시 Detach 상태 - 커서 표시, 게임 입력 비활성화
    INPUT.SetGameInputEnabled(false);
    INPUT.SetCursorVisible(true);

    // BeginPlay 중에 새로운 actor가 추가될 수도 있어서 복사 후 호출
    TArray<AActor*> LevelActors = GWorld->GetLevel()->GetActors();
    for (AActor* Actor : LevelActors)
    {
        // NOTE: PIE 시작 후에는 액터 생성 시 직접 불러줌
        Actor->BeginPlay();
    }

    // NOTE: BeginPlay 중에 삭제된 액터 삭제 후 Tick 시작
    GWorld->ProcessPendingKillActors();
}

void UEditorEngine::EndPIE()
{
    // 지연 종료 처리 (UEditorEngine::MainLoop에서 종료 처리됨)
    bChangedPieToEditor = true;
}

void UEditorEngine::SetPIEInputCaptured(bool bCaptured)
{
    if (!bPIEActive)
    {
        return;
    }

    bPIEInputCaptured = bCaptured;

    // InputManager의 게임 입력 활성화 상태 설정
    // false면 모든 게임 입력 함수가 차단됨
    INPUT.SetGameInputEnabled(bCaptured);

    if (bCaptured)
    {
        // 게임에 입력 캡처 (Attach) - 커서 숨김 + 잠금 (무한 드래그)
        INPUT.SetCursorVisible(false);
        INPUT.LockCursor();
        UE_LOG("EditorEngine: SetPIEInputCaptured: Input captured by game");
    }
    else
    {
        // 에디터에 입력 반환 - 커서 표시
        INPUT.SetCursorVisible(true);
        if (INPUT.IsCursorLocked())
        {
            INPUT.ReleaseCursor();
        }
        UE_LOG("EditorEngine: SetPIEInputCaptured: Input released to editor (Shift+F1)");
    }
}

void UEditorEngine::TogglePIEInputCapture()
{
    SetPIEInputCaptured(!bPIEInputCaptured);
}
