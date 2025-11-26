#pragma once
#include <string>
#include <algorithm>
#include <windows.h>
#include <filesystem>

#include "UEContainer.h"

namespace fs = std::filesystem;

extern const FString GDataDir;
extern const FString GCacheDir;

// ============================================================================
// 경로 정규화 유틸리티 함수
// ============================================================================

/**
 * @brief 경로 문자열의 디렉토리 구분자를 모두 '/'로 통일합니다.
 * @details Windows/Unix 간 경로 구분자 차이로 인한 중복 리소스 로드를 방지합니다.
 *          예: "Data\\Textures\\image.png" -> "Data/Textures/image.png"
 * @param InPath 정규화할 경로 문자열
 * @return 정규화된 경로 문자열 ('/' 구분자 사용)
 */
inline FString NormalizePath(const FString& InPath)
{
	FString Result = InPath;
	std::ranges::replace(Result, '\\', '/');
	return Result;
}
inline FWideString NormalizePath(const FWideString& InPath)
{
	FWideString Result = InPath;
	std::ranges::replace(Result, '\\', '/');
	return Result;
}

// ============================================================================
// 문자열 인코딩 변환 유틸리티 함수
// ============================================================================

/**
 * @brief UTF-8 문자열을 UTF-16 와이드 문자열로 변환합니다.
 * @details 한글 경로 등 비ASCII 문자를 포함한 경로를 Windows API에서 사용할 수 있도록 변환합니다.
 * @param InUtf8Str UTF-8 인코딩된 입력 문자열
 * @return UTF-16 인코딩된 와이드 문자열 (변환 실패 시 빈 문자열)
 */
inline FWideString UTF8ToWide(const FString& InUtf8Str)
{
	if (InUtf8Str.empty()) return FWideString();

	int needed = ::MultiByteToWideChar(CP_UTF8, 0, InUtf8Str.c_str(), -1, nullptr, 0);
	if (needed <= 0)
	{
		// UTF-8 변환 실패 시 ANSI 시도
		needed = ::MultiByteToWideChar(CP_ACP, 0, InUtf8Str.c_str(), -1, nullptr, 0);
		if (needed <= 0) return FWideString();

		FWideString result(needed - 1, L'\0');
		::MultiByteToWideChar(CP_ACP, 0, InUtf8Str.c_str(), -1, result.data(), needed);
		return result;
	}

	FWideString result(needed - 1, L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, InUtf8Str.c_str(), -1, result.data(), needed);
	return result;
}

/**
 * @brief UTF-16 와이드 문자열을 UTF-8 문자열로 변환합니다.
 * @details Windows API에서 반환된 와이드 문자열을 UTF-8로 변환하여 프로젝트 전역에서 사용합니다.
 * @param InWideStr UTF-16 인코딩된 입력 와이드 문자열
 * @return UTF-8 인코딩된 문자열 (변환 실패 시 빈 문자열)
 */
inline FString WideToUTF8(const FWideString& InWideStr)
{
	if (InWideStr.empty()) return FString();

	int size_needed = ::WideCharToMultiByte(
		CP_UTF8,                            // UTF-8 코드 페이지
		0,                                  // 플래그
		InWideStr.c_str(),                  // 입력 wstring
		static_cast<int>(InWideStr.size()), // 입력 문자 수
		nullptr,                            // 출력 버퍼 (nullptr이면 크기만 계산)
		0,                                  // 출력 버퍼 크기
		nullptr,                            // 기본 문자
		nullptr                             // 기본 문자 사용 여부
	);

	if (size_needed <= 0)
	{
		return FString(); // 변환 실패
	}

	// 실제 변환 수행
	FString result(size_needed, 0);
	::WideCharToMultiByte(
		CP_UTF8,
		0,
		InWideStr.c_str(),
		static_cast<int>(InWideStr.size()),
		&result[0],                         // 출력 버퍼
		size_needed,
		nullptr,
		nullptr
	);

	return result;
}

/**
 * @brief UTF-16 와이드 문자열을 현재 시스템 ANSI 코드 페이지(MBCS) 문자열로 변환합니다.
 * @details FBX SDK나 legacy Win32 API처럼 wchar_t 버전이 없는 API에 경로를 전달할 때 사용합니다.
 * @param InWideStr UTF-16 인코딩된 입력 와이드 문자열
 * @return ANSI(MBCS, CP_ACP) 인코딩된 문자열 (변환 실패 시 빈 문자열)
 */
inline FString WideToACP(const FWideString& InWideStr)
{
	if (InWideStr.empty()) return FString();

	int size_needed = ::WideCharToMultiByte(
		CP_ACP,                             // 시스템 기본 코드 페이지
		0,
		InWideStr.c_str(),
		-1,                                 // null-terminated 입력
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (size_needed <= 0)
	{
		return FString();
	}

	// null 문자 제외한 길이만큼 버퍼 할당
	FString result(size_needed - 1, 0);
	::WideCharToMultiByte(
		CP_ACP,
		0,
		InWideStr.c_str(),
		-1,
		&result[0],
		size_needed,
		nullptr,
		nullptr
	);

	return result;
}

/**
 * @brief 현재 시스템 ANSI 코드 페이지(MBCS) 문자열을 UTF-16 와이드 문자열로 변환합니다.
 * @details legacy Win32 API의 ANSI 버전에서 받은 문자열을 내부 FWideString으로 변환할 때 사용합니다.
 * @param InAcpStr ANSI(MBCS, CP_ACP) 인코딩된 입력 문자열
 * @return UTF-16 인코딩된 와이드 문자열 (변환 실패 시 빈 문자열)
 */
inline FWideString ACPToWide(const FString& InAcpStr)
{
	if (InAcpStr.empty()) return FWideString();

	// 1. 필요한 와이드 문자열의 길이 계산 (null 문자 포함)
	int size_needed = ::MultiByteToWideChar(
		CP_ACP,                             // 시스템 기본 코드 페이지
		0,
		InAcpStr.c_str(),
		-1,                                 // null-terminated 입력
		nullptr,
		0
	);

	if (size_needed <= 0)
	{
		return FWideString();
	}

	// 2. null 문자를 제외한 길이만큼 버퍼 할당
	FWideString result(size_needed - 1, 0);

	// 3. 실제 변환 수행
	::MultiByteToWideChar(
		CP_ACP,
		0,
		InAcpStr.c_str(),
		-1,
		&result[0],                         // FWideString의 내부 버퍼에 직접 쓰기
		size_needed
	);

	return result;
}

/**
 * @brief UTF-8 문자열을 현재 시스템 ANSI 코드 페이지(MBCS) 문자열로 변환합니다.
 * @details 내부에서는 UTF-8 → UTF-16 → ANSI 순으로 변환합니다.
 * @param InUtf8Str UTF-8 인코딩된 입력 문자열
 * @return ANSI(MBCS, CP_ACP) 인코딩된 문자열 (변환 실패 시 빈 문자열)
 */
inline FString UTF8ToACP(const FString& InUtf8Str)
{
	if (InUtf8Str.empty()) return FString();
	FWideString wide = UTF8ToWide(InUtf8Str);
	if (wide.empty()) return FString();
	return WideToACP(wide);
}

/**
 * @brief 현재 시스템 ANSI 코드 페이지(MBCS) 문자열을 UTF-8 문자열로 변환합니다.
 * @details 내부에서는 ANSI → UTF-16 → UTF-8 순으로 변환합니다.
 * @param InAcpStr ANSI(MBCS, CP_ACP) 인코딩된 입력 문자열
 * @return UTF-8 인코딩된 문자열 (변환 실패 시 빈 문자열)
 */
inline FString ACPToUTF8(const FString& InAcpStr)
{
	if (InAcpStr.empty()) return FString();

	// 1. ANSI(FString) -> UTF-16(FWideString)
	FWideString wide = ACPToWide(InAcpStr);
	if (wide.empty()) return FString();

	// 2. UTF-16(FWideString) -> UTF-8(FString)
	return WideToUTF8(wide);
}

// ============================================================================
// 에셋 경로 변환 유틸리티 (저장 / 로드용)
// ============================================================================

/**
 * @brief 절대 경로를 프로젝트 상대 경로 (Data/...)로 변환합니다.
 * @details 파티클 시스템, 씬 파일 등에서 에셋 경로를 저장할 때 사용합니다.
 *          절대 경로를 상대 경로로 변환하여 다른 PC에서도 동작하도록 합니다.
 *
 * 예: "C:/Users/Jungle/Projects/Mundi/Data/Model/Box.obj" → "Data/Model/Box.obj"
 * 예: "Data/Model/Box.obj" → "Data/Model/Box.obj" (이미 상대 경로면 그대로)
 *
 * @param InPath 변환할 경로 (절대 또는 상대)
 * @return 프로젝트 상대 경로 ("Data/..."로 시작)
 */
inline FString MakeAssetRelativePath(const FString& InPath)
{
	if (InPath.empty())
	{
		return "";
	}

	FString NormalizedPath = NormalizePath(InPath);

	// 이미 "Data/"로 시작하는 상대 경로인 경우 그대로 반환
	FString DataPrefix = GDataDir + "/";
	if (NormalizedPath.length() >= DataPrefix.length() &&
		_strnicmp(NormalizedPath.c_str(), DataPrefix.c_str(), DataPrefix.length()) == 0)
	{
		return NormalizedPath;
	}

	try
	{
		FWideString WPath = UTF8ToWide(NormalizedPath);
		fs::path AbsPath(WPath);

		// 상대 경로인 경우 현재 디렉토리 기준으로 절대 경로로 변환
		if (!AbsPath.is_absolute())
		{
			AbsPath = fs::absolute(AbsPath);
		}
		AbsPath = AbsPath.lexically_normal();

		// 현재 작업 디렉토리
		fs::path CurrentDir = fs::current_path();

		// "/Data/" 패턴을 찾아서 그 이후 부분만 추출
		FString AbsPathStr = NormalizePath(WideToUTF8(AbsPath.wstring()));

		// "Data/" 또는 "/Data/" 위치 찾기 (대소문자 무관)
		size_t DataPos = FString::npos;

		// 다양한 패턴 시도: "/Data/", "\\Data\\", "/data/", etc.
		for (const char* Pattern : { "/Data/", "/data/", "\\Data\\", "\\data\\" })
		{
			size_t Pos = AbsPathStr.find(Pattern);
			if (Pos != FString::npos)
			{
				DataPos = Pos + 1;  // '/' 다음부터
				break;
			}
		}

		if (DataPos != FString::npos)
		{
			// "Data/..." 형태로 반환
			return NormalizePath(AbsPathStr.substr(DataPos));
		}

		// "Data/" 패턴을 찾지 못한 경우, 현재 디렉토리 기준 상대 경로 시도
		std::error_code EC;
		fs::path RelPath = fs::relative(AbsPath, CurrentDir, EC);
		if (!EC && !RelPath.empty())
		{
			return NormalizePath(WideToUTF8(RelPath.wstring()));
		}

		// 변환 실패 시 원본 반환
		return NormalizedPath;
	}
	catch (const std::exception&)
	{
		return NormalizePath(InPath);
	}
}

/**
 * @brief 프로젝트 상대 경로를 현재 작업 디렉토리 기준 경로로 변환합니다.
 * @details 파티클 시스템, 씬 파일 등에서 에셋 경로를 로드할 때 사용합니다.
 *
 * 예: "Data/Model/Box.obj" → "Data/Model/Box.obj" (현재 디렉토리에서 접근 가능)
 *
 * @param InRelativePath 프로젝트 상대 경로 ("Data/..."로 시작)
 * @return 현재 작업 디렉토리 기준 접근 가능한 경로
 */
inline FString ResolveAssetPath(const FString& InRelativePath)
{
	if (InRelativePath.empty())
	{
		return "";
	}

	FString NormalizedPath = NormalizePath(InRelativePath);

	// 이미 존재하는 경로인 경우 그대로 반환
	FWideString WPath = UTF8ToWide(NormalizedPath);
	if (fs::exists(fs::path(WPath)))
	{
		return NormalizedPath;
	}

	// 현재 작업 디렉토리 기준으로 경로 구성
	try
	{
		fs::path CurrentDir = fs::current_path();
		fs::path FullPath = CurrentDir / fs::path(WPath);

		if (fs::exists(FullPath))
		{
			// 상대 경로로 반환 (ResourceManager가 정규화할 것임)
			std::error_code EC;
			fs::path RelPath = fs::relative(FullPath, CurrentDir, EC);
			if (!EC && !RelPath.empty())
			{
				return NormalizePath(WideToUTF8(RelPath.wstring()));
			}
		}
	}
	catch (const std::exception&)
	{
		// 무시
	}

	// 변환 실패 시 원본 반환
	return NormalizedPath;
}

inline FString ConvertDataPathToCachePath(const FString& InAssetPath)
{
	FString DataDirPrefix = GDataDir + "/";

	// GDataDir("Data")로 시작하는지 (대소문자 무관) 확인
	// _strnicmp는 C-스타일 문자열을 받으며, n 글자수만큼 대소문자 무관 비교
	if (InAssetPath.length() >= DataDirPrefix.length() &&
		_strnicmp(InAssetPath.c_str(), DataDirPrefix.c_str(), DataDirPrefix.length()) == 0)
	{
		// "GCacheDir/" 접두사를 제거하고 GCacheDir/"... " 접두사를 붙임
		return GCacheDir + "/" + InAssetPath.substr(DataDirPrefix.length());
	}

	// Data/로 시작하지 않는 경로 (예: 절대 경로, Data 외부의 상대 경로)
	// GetDDSCachePath의 기존 정책을 따라 파일명만 사용
	FWideString WPath = UTF8ToWide(InAssetPath);
	fs::path FileName = fs::path(WPath).filename();

	return GCacheDir + "/" + WideToUTF8(FileName.wstring());
}

/**
 * 에셋(예: mtl) 내부에서 참조된 경로를 해석하여
 * 엔진이 사용하는 (현재 작업 디렉토리 기준) 상대 경로로 변환합니다.
 */
inline FString ResolveAssetRelativePath(const FString& InAssetPath, const FString& InAssetBaseDir)
{
	if (InAssetPath.empty())
	{
		return "";
	}

	try
	{
		FString NormalizedAssetPath = NormalizePath(InAssetPath);

		FString DataDirPrefix = GDataDir + "/";
		if (NormalizedAssetPath.length() >= DataDirPrefix.length() &&
			_strnicmp(NormalizedAssetPath.c_str(), DataDirPrefix.c_str(), DataDirPrefix.length()) == 0)
		{
			return NormalizedAssetPath;
		}

		FWideString WTexPath = UTF8ToWide(NormalizedAssetPath);
		FWideString WBaseDir = UTF8ToWide(InAssetBaseDir);
		fs::path TexPath(WTexPath);
		fs::path BaseDir(WBaseDir);
		fs::path FinalPath;

		if (TexPath.is_absolute())
		{
			FinalPath = TexPath.lexically_normal();
		}
		else
		{
			FinalPath = (BaseDir / TexPath).lexically_normal();
		}

		// 현재 작업 디렉토리 기준 상대 경로로 변환 시도
		fs::path CurrentDir = fs::current_path();
		std::error_code ec;
		fs::path RelativePath = fs::relative(FinalPath, CurrentDir, ec);

		fs::path PathToUse = (ec || RelativePath.empty()) ? FinalPath : RelativePath;

		return NormalizePath(WideToUTF8(PathToUse.wstring()));
	}
	catch (const std::exception&)
	{
		// 예외 발생 시 원본 경로 유지
		return NormalizePath(InAssetPath);
	}
}
