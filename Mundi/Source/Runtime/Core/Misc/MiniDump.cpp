#include "pch.h"
#include "MiniDump.h"
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	// 1. 고유한 덤프 파일 이름 생성 (예: "CrashDump_2025-11-15_12-30-05.dmp")
	auto Now = std::chrono::system_clock::now();
	auto InTimeT = std::chrono::system_clock::to_time_t(Now);

	std::wstringstream ss;
	struct tm TimeInfo; // 시간을 저장할 구조체를 스택에 생성
	errno_t err = localtime_s(&TimeInfo, &InTimeT); // _s 함수 호출

	if (err == 0) // 변환 성공 시
	{
		ss << L"Mundi_CrashDump_";
		ss << std::put_time(&TimeInfo, L"%Y-%m-%d_%H-%M-%S");
		ss << L".dmp";
	}
	else // 혹시 모를 실패 시 (거의 발생 안 함)
	{
		ss << L"CrashDump_UnknownTime.dmp";
	}

	std::wstring DumpFileName = ss.str();
	// 2. 덤프 파일 생성
	HANDLE hFile = CreateFileW(DumpFileName.c_str(),
		GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
	);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// 3. MiniDumpWriteDump 호출
	_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
	ExInfo.ThreadId = GetCurrentThreadId();
	ExInfo.ExceptionPointers = ExceptionInfo;
	ExInfo.ClientPointers = FALSE;

	// 덤프에 포함될 정보 수준 (필요에 따라 MiniDumpWithFullMemory 등 사용)
	MINIDUMP_TYPE DumpType = MiniDumpNormal;

	BOOL bResult = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
		hFile, DumpType, &ExInfo, nullptr, nullptr);

	CloseHandle(hFile);

	return EXCEPTION_EXECUTE_HANDLER;
}

void InitializeMiniDump()
{
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

	_set_abort_behavior(0, _WRITE_ABORT_MSG);

	SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}

static bool bIsCrashInitialized = false;

void CauseCrash()
{
	bIsCrashInitialized = true;
}

#pragma optimize("", off)
void CrashLoop()
{
	if (!bIsCrashInitialized) { return; }

	uint32 ObjCount = GUObjectArray.Num();
	uint32 RandomIdx = std::rand() % ObjCount;
	UObject* Target = GUObjectArray[RandomIdx];

	if (Target)
	{
		Target->DeleteObjectDirty();
	}
}
#pragma optimize("", on)
