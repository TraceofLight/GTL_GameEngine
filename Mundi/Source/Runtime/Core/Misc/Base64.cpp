#include "pch.h"
#include "Base64.h"
#include <windows.h>
#include <wincrypt.h>

std::string FBase64::Encode(const uint8_t* Data, size_t Length)
{
    if (!Data || Length == 0)
    {
        return "";
    }

    DWORD EncodedLength = 0;
    if (!CryptBinaryToStringA(Data, static_cast<DWORD>(Length), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &EncodedLength))
    {
        return "";
    }

    std::string Result(EncodedLength, '\0');
    if (!CryptBinaryToStringA(Data, static_cast<DWORD>(Length), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &Result[0], &EncodedLength))
    {
        return "";
    }

    // null terminator 제거
    if (!Result.empty() && Result.back() == '\0')
    {
        Result.pop_back();
    }

    return Result;
}

std::vector<uint8_t> FBase64::Decode(const std::string& EncodedString)
{
    if (EncodedString.empty())
    {
        return {};
    }

    DWORD DecodedLength = 0;
    if (!CryptStringToBinaryA(EncodedString.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &DecodedLength, nullptr, nullptr))
    {
        return {};
    }

    std::vector<uint8_t> Result(DecodedLength);
    if (!CryptStringToBinaryA(EncodedString.c_str(), 0, CRYPT_STRING_BASE64, Result.data(), &DecodedLength, nullptr, nullptr))
    {
        return {};
    }

    return Result;
}
