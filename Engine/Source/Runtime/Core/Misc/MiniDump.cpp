#include "pch.h"
#include "MiniDump.h"
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <new.h> // _set_new_handler 용

// ============================================================================
// 1. 덤프 생성 로직을 함수 하나로 분리 (재사용 위해)
// ============================================================================
void CreateMiniDump(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    auto Now = std::chrono::system_clock::now();
    auto InTimeT = std::chrono::system_clock::to_time_t(Now);

    std::wstringstream ss;
    struct tm TimeInfo;
    errno_t err = localtime_s(&TimeInfo, &InTimeT);

    if (err == 0)
    {
        ss << L"Engine_CrashDump_";
        ss << std::put_time(&TimeInfo, L"%Y-%m-%d_%H-%M-%S");
        ss << L".dmp";
    }
    else
    {
        ss << L"CrashDump_UnknownTime.dmp";
    }

    std::wstring DumpFileName = ss.str();
    HANDLE hFile = CreateFileW(DumpFileName.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );

    if (hFile != INVALID_HANDLE_VALUE)
    {
        _MINIDUMP_EXCEPTION_INFORMATION ExInfo;
        ExInfo.ThreadId = GetCurrentThreadId();
        ExInfo.ExceptionPointers = ExceptionInfo;
        ExInfo.ClientPointers = FALSE;

        // 힙 상태까지 보고 싶다면 MiniDumpWithFullMemory 권장 (용량 큼)
        // 힙 오염 추적용이므로 MiniDumpWithDataSegs, MiniDumpWithHandleData 등을 섞는게 좋음
        MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithHandleData);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
            hFile, DumpType, ExceptionInfo ? &ExInfo : nullptr, nullptr, nullptr);

        CloseHandle(hFile);
    }
}

// ============================================================================
// 2. 각종 핸들러 구현
// ============================================================================

// [A] SEH 핸들러 (기존꺼)
LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    CreateMiniDump(ExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

// [B] VEH 핸들러 (우선순위 1등) - 여기서 잡힐 확률이 높음
LONG WINAPI VectoredExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    // 0xC0000005 (Access Violation) 등 치명적인 것만 캡처
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        ExceptionInfo->ExceptionRecord->ExceptionCode == 0xC0000374 /*STATUS_HEAP_CORRUPTION*/)
    {
        CreateMiniDump(ExceptionInfo);

        // 덤프 떴으면 강제 종료 (계속 실행하면 무한 루프 돔)
        TerminateProcess(GetCurrentProcess(), ExceptionInfo->ExceptionRecord->ExceptionCode);
    }

    // 다른 예외는 통과시켜서 다음 핸들러가 처리하게 함
    return EXCEPTION_CONTINUE_SEARCH;
}

// [C] CRT 잘못된 파라미터 핸들러 (delete에 쓰레기값 넣었을 때 호출)
void InvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
{
    // 여기서 덤프 생성 (ExceptionInfo가 없으므로 nullptr 전달하거나 가짜 생성 가능)
    // 보통은 여기서 강제로 예외를 발생시켜 VEH가 잡게 하거나 직접 덤프를 뜸

    // 간단히 덤프만 남기고 종료
    CreateMiniDump(nullptr);
    TerminateProcess(GetCurrentProcess(), 0xC000000D); // STATUS_INVALID_PARAMETER
}

// [D] Pure Virtual Function Call 핸들러
void PureCallHandler()
{
    CreateMiniDump(nullptr);
    TerminateProcess(GetCurrentProcess(), 0xC0000025); // STATUS_NONCONTINUABLE_EXCEPTION
}


// ============================================================================
// 3. 초기화 함수 (모든 안전장치 해제 및 훅 설치)
// ============================================================================
void InitializeMiniDump()
{
    // 1. CRT 에러 메시지 박스 억제
    _CrtSetReportMode(_CRT_WARN, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    // 2. CRT 핸들러 등록 (Heap 관련 에러 가로채기)
    _set_invalid_parameter_handler(InvalidParameterHandler);
    _set_purecall_handler(PureCallHandler);

    // 3. SEH 핸들러 등록
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    // 4. VEH 핸들러 등록
    // 첫 번째 인자 1: 제일 먼저 호출해달라
    AddVectoredExceptionHandler(1, VectoredExceptionHandler);
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
    if (ObjCount == 0) return;

    uint32 RandomIdx = std::rand() % ObjCount;
    UObject* Target = GUObjectArray[RandomIdx];

    // Release 모드에서도 이 코드가 실행되어
    // 1. Access Violation이 나면 VEH가 잡고
    // 2. CRT 검사(유효하지 않은 힙 포인터)에 걸리면 InvalidParameterHandler가 잡습니다.
    if (Target)
    {
       Target->DeleteObjectDirty();
    }
}
#pragma optimize("", on)
