#include "pch.h"
#include "EditorEngine.h"
#include "MiniDump.h"
#include "ClothManager.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#   define _CRTDBG_MAP_ALLOC
#   include <cstdlib>
#   include <crtdbg.h>
#endif

PxDefaultAllocator gAllocator;
PxDefaultErrorCallback gErrorCallback;

// Note: Old test helper using a "GameObject" struct has been removed
// because that struct is no longer defined. Physics bodies are now
// created and managed via FBodyInstance on components.

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    _CrtSetBreakAlloc(0);
#endif

	// COM 초기화
	HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hrCom))
	{
		UE_LOG("main: CoInitializeEx failed (0x%08X)", hrCom);
	}

	InitializeMiniDump();

	PHYSICS.Initialize();

	// Cloth Manager 초기화 (싱글톤)
	// 미안 일단 여기다가 둘게 나도알아 쓰레긴거 하지만 체출 7시간 전인걸...
	FClothManager::GetInstance().Initialize();

	if (!GEngine.Startup(hInstance))
        return -1;

    GEngine.MainLoop();
    GEngine.Shutdown();

	// COM 정리
	if (SUCCEEDED(hrCom))
	{
		CoUninitialize();
	}

    return 0;
}
