#pragma once
#include <string>
#include <vector>

/**
 * @brief Base64 인코딩/디코딩 유틸리티
 */
class FBase64
{
public:
    // 바이너리 데이터를 Base64 문자열로 인코딩
    static std::string Encode(const uint8_t* Data, size_t Length);

    // Base64 문자열을 바이너리 데이터로 디코딩
    static std::vector<uint8_t> Decode(const std::string& EncodedString);
};
