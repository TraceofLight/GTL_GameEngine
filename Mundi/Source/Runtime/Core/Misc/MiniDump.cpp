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
	SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}

// 크래시 시스템 활성화 플래그
static volatile bool bCrashTestArmed = false;

// 크래시를 유발할 나머지 연산의 분모
static volatile int CrashModulo = 0;

// 크래시를 유발할 목표 나머지 값
static volatile int TargetModuloResult = 0;

// 총 함수 호출 횟수 (싱글 스레드이므로 __declspec(thread) 불필요)
static volatile LONG64 TotalCallCount = 0;

// [!!!] 재진입(무한 재귀) 방지를 위한 가드 변수
static bool bIsInsideHook = false;

void CrashNow()
{
	printf("!!! CRASH TRIGGERED at Total Call Count: %lld !!!\n", TotalCallCount);
	volatile int* pNull = nullptr;
	*pNull = 0;
}
// 랜덤 숫자 생성기
int GetRandomInt(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<> distrib(min, max - 1);
	return distrib(gen);
}

void CauseCrashRandom(int32 Modulo)
{
	if (Modulo <= 1) Modulo = 10000; // 0이나 1로 나누는 것 방지

	CrashModulo = Modulo;
	// 0 ~ (modulo-1) 사이의 랜덤한 값을 목표로 설정
	TargetModuloResult = GetRandomInt(0, CrashModulo);

	TotalCallCount = 0; // 카운터 리셋
	bCrashTestArmed = true; // "활성화"

	printf("[CrashTest] Armed. Will crash when (TotalCalls %% %d) == %d\n",
		   CrashModulo, TargetModuloResult);
}

// --- C 컴파일러 훅 구현 ---
extern "C" {

	// /Gh 플래그에 의해 모든 함수 "시작" 시 호출됨
	void __cdecl _penter(void)
	{
		if (bIsInsideHook) { return; }

		bIsInsideHook = true;

		// 1. 시스템이 활성화되지 않았으면 즉시 리턴
		if (bCrashTestArmed)
		{
			TotalCallCount++;
			if ((TotalCallCount % CrashModulo) == TargetModuloResult)
			{
				bCrashTestArmed = false;
				CrashNow();
			}
		}

		bIsInsideHook = false;
	}
}
