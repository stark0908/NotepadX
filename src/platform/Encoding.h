#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace notepadx {

enum class EncodingType {
    Utf8,
    Utf8Bom,
    Utf16Le,
    Utf16Be,
    Latin1
};

struct DetectedEncoding {
    EncodingType type{EncodingType::Utf8};
    std::string name{"UTF-8"};
    size_t bomLength{0};
};

class Encoding {
public:
    static DetectedEncoding detect(std::string_view rawData);
    static std::string toUtf8(std::string_view rawData, EncodingType type);
    static std::string fromUtf8(std::string_view utf8Data, EncodingType type);
    static bool isValidUtf8(std::string_view data);
};

} // namespace notepadx
