#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include"UEContainer.h"
// ──────────────────────────────
// FNameEntry & Pool
// ──────────────────────────────
struct FNameEntry
{
    FString Display;    // 원문
    FString Comparison; // lower-case
};

class FNamePool
{
public:
    static uint32 Add(const FString& InStr);
    static const FNameEntry& Get(uint32 Index);
};

// ──────────────────────────────
// FName
// ──────────────────────────────
struct FName
{
	static constexpr uint32 InvalidIndex = (std::numeric_limits<uint32>::max)();

    uint32 DisplayIndex = InvalidIndex;
    uint32 ComparisonIndex = InvalidIndex;

#if defined(DEBUG) || defined(_DEBUG)
	FString DebugString; // 디버거에서 해당 FName의 실제 문자열을 쉽게 확인하기 위한 멤버
#endif

    FName() = default;
    FName(const char* InStr) { Init(FString(InStr)); }
    FName(const FString& InStr) { Init(InStr); }

    void Init(const FString& InStr)
    {
        int32_t Index = FNamePool::Add(InStr);
        DisplayIndex = Index;
        ComparisonIndex = Index; // 필요시 다른 규칙 적용 가능

#if defined(DEBUG) || defined(_DEBUG)
		DebugString = InStr;
#endif
    }

	// 초기화 여부 확인 함수
	bool IsValid() const
    {
    	return ComparisonIndex != InvalidIndex;
    }

    bool operator==(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex; }
    FString ToString() const { return FNamePool::Get(DisplayIndex).Display; }

    friend FName operator+(const FName& A, const FName& B)
    {
        return FName(A.ToString() + B.ToString());
    }

    friend FName operator+(const FName& A, const FString& B)
    {
        return FName(A.ToString() + B);
    }

    friend FName operator+(const FString& A, const FName& B)
    {
        return FName(A + B.ToString());
    }
};

// --- FName을 위한 std::hash 특수화 ---
namespace std
{
    template<>
    struct hash<FName>
    {
        size_t operator()(const FName& Name) const noexcept
        {
            // FName의 비교 기준인 ComparisonIndex를 해시합니다.
            return hash<uint32>{}(Name.ComparisonIndex);
        }
    };
}
