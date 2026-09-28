#include "platform/Encoding.h"

#include <cstdint>
#include <vector>

namespace notepadx {

bool Encoding::isValidUtf8(std::string_view data) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    size_t i = 0;
    const size_t len = data.size();

    while (i < len) {
        const uint8_t b = bytes[i];
        if (b <= 0x7F) {
            i += 1;
        } else if ((b & 0xE0) == 0xC0) {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            if (i + 1 >= len) return false;
            const uint8_t b2 = bytes[i + 1];
            if ((b2 & 0xC0) != 0x80) return false;
            if (b < 0xC2) return false; // Overlong encoding
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= len) return false;
            const uint8_t b2 = bytes[i + 1];
            const uint8_t b3 = bytes[i + 2];
            if ((b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
            if (b == 0xE0 && b2 < 0xA0) return false; // Overlong
            if (b == 0xED && b2 >= 0xA0) return false; // Surrogate pairs U+D800..U+DFFF
            i += 3;
        } else if ((b & 0xF8) == 0xF0) {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (i + 3 >= len) return false;
            const uint8_t b2 = bytes[i + 1];
            const uint8_t b3 = bytes[i + 2];
            const uint8_t b4 = bytes[i + 3];
            if ((b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80 || (b4 & 0xC0) != 0x80) return false;
            if (b == 0xF0 && b2 < 0x90) return false; // Overlong
            if (b == 0xF4 && b2 > 0x8F) return false; // > U+10FFFF
            if (b > 0xF4) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

DetectedEncoding Encoding::detect(std::string_view rawData) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(rawData.data());
    const size_t len = rawData.size();

    if (len >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return {EncodingType::Utf8Bom, "UTF-8 BOM", 3};
    }
    if (len >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        return {EncodingType::Utf16Le, "UTF-16 LE", 2};
    }
    if (len >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        return {EncodingType::Utf16Be, "UTF-16 BE", 2};
    }

    if (isValidUtf8(rawData)) {
        return {EncodingType::Utf8, "UTF-8", 0};
    }

    return {EncodingType::Latin1, "ISO-8859-1", 0};
}

std::string Encoding::toUtf8(std::string_view rawData, EncodingType type) {
    switch (type) {
        case EncodingType::Utf8Bom:
            return (rawData.size() >= 3) ? std::string(rawData.substr(3)) : std::string();

        case EncodingType::Utf8:
            return std::string(rawData);

        case EncodingType::Latin1: {
            std::string out;
            out.reserve(rawData.size() * 3 / 2);
            for (const char c : rawData) {
                const auto uc = static_cast<uint8_t>(c);
                if (uc < 0x80) {
                    out.push_back(c);
                } else {
                    out.push_back(static_cast<char>(0xC0 | (uc >> 6)));
                    out.push_back(static_cast<char>(0x80 | (uc & 0x3F)));
                }
            }
            return out;
        }

        case EncodingType::Utf16Le: {
            const auto* bytes = reinterpret_cast<const uint8_t*>(rawData.data());
            size_t start = (rawData.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) ? 2 : 0;
            std::string out;

            for (size_t i = start; i + 1 < rawData.size(); i += 2) {
                uint32_t cp = bytes[i] | (static_cast<uint32_t>(bytes[i + 1]) << 8);

                // Surrogate pair handling
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < rawData.size()) {
                    uint32_t low = bytes[i + 2] | (static_cast<uint32_t>(bytes[i + 3]) << 8);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + (((cp & 0x3FF) << 10) | (low & 0x3FF));
                        i += 2;
                    }
                }

                if (cp <= 0x7F) {
                    out.push_back(static_cast<char>(cp));
                } else if (cp <= 0x7FF) {
                    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp <= 0xFFFF) {
                    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp <= 0x10FFFF) {
                    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
            }
            return out;
        }

        case EncodingType::Utf16Be: {
            const auto* bytes = reinterpret_cast<const uint8_t*>(rawData.data());
            size_t start = (rawData.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) ? 2 : 0;
            std::string out;

            for (size_t i = start; i + 1 < rawData.size(); i += 2) {
                uint32_t cp = (static_cast<uint32_t>(bytes[i]) << 8) | bytes[i + 1];

                if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < rawData.size()) {
                    uint32_t low = (static_cast<uint32_t>(bytes[i + 2]) << 8) | bytes[i + 3];
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + (((cp & 0x3FF) << 10) | (low & 0x3FF));
                        i += 2;
                    }
                }

                if (cp <= 0x7F) {
                    out.push_back(static_cast<char>(cp));
                } else if (cp <= 0x7FF) {
                    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp <= 0xFFFF) {
                    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp <= 0x10FFFF) {
                    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
            }
            return out;
        }
    }

    return std::string(rawData);
}

std::string Encoding::fromUtf8(std::string_view utf8Data, EncodingType type) {
    if (type == EncodingType::Utf8Bom) {
        std::string out = "\xEF\xBB\xBF";
        out.append(utf8Data);
        return out;
    }
    if (type == EncodingType::Utf8) {
        return std::string(utf8Data);
    }
    if (type == EncodingType::Latin1) {
        std::string out;
        out.reserve(utf8Data.size());
        const auto* bytes = reinterpret_cast<const uint8_t*>(utf8Data.data());
        size_t i = 0;
        while (i < utf8Data.size()) {
            if (bytes[i] < 0x80) {
                out.push_back(static_cast<char>(bytes[i++]));
            } else if ((bytes[i] & 0xE0) == 0xC0 && i + 1 < utf8Data.size()) {
                uint32_t cp = ((bytes[i] & 0x1F) << 6) | (bytes[i + 1] & 0x3F);
                out.push_back(cp <= 0xFF ? static_cast<char>(cp) : '?');
                i += 2;
            } else {
                out.push_back('?');
                i++;
            }
        }
        return out;
    }

    // Default fallback
    return std::string(utf8Data);
}

} // namespace notepadx
