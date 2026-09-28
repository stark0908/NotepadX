#include "search/Regex.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace notepadx {

struct RegexEngine::Impl {
    pcre2_code* code{nullptr};
    pcre2_match_data* matchData{nullptr};
    uint32_t captureCount{0};

    ~Impl() {
        if (matchData) {
            pcre2_match_data_free(matchData);
        }
        if (code) {
            pcre2_code_free(code);
        }
    }
};

RegexEngine::RegexEngine() = default;

RegexEngine::~RegexEngine() = default;

RegexEngine::RegexEngine(RegexEngine&& other) noexcept = default;
RegexEngine& RegexEngine::operator=(RegexEngine&& other) noexcept = default;

void RegexEngine::cleanup() {
    impl_.reset();
}

bool RegexEngine::isCompiled() const noexcept {
    return impl_ != nullptr && impl_->code != nullptr;
}

std::string RegexEngine::escapePattern(std::string_view literal) {
    static const std::string_view specialChars = R"(\.^$*+?()[]{}|)";
    std::string escaped;
    escaped.reserve(literal.size() * 2);

    for (char c : literal) {
        if (specialChars.find(c) != std::string_view::npos) {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

bool RegexEngine::compile(std::string_view query,
                          bool caseSensitive,
                          bool wholeWord,
                          bool isRegex,
                          std::string* errorMsg) {
    cleanup();

    if (query.empty()) {
        if (errorMsg) *errorMsg = "Empty search pattern";
        return false;
    }

    std::string patternStr = isRegex ? std::string(query) : escapePattern(query);
    if (wholeWord) {
        patternStr = "\\b(?:" + patternStr + ")\\b";
    }

    uint32_t compileFlags = PCRE2_UTF | PCRE2_MULTILINE;
    if (!caseSensitive) {
        compileFlags |= PCRE2_CASELESS;
    }

    int errCode = 0;
    PCRE2_SIZE errOffset = 0;
    pcre2_code* code = pcre2_compile(
        reinterpret_cast<PCRE2_SPTR>(patternStr.c_str()),
        patternStr.size(),
        compileFlags,
        &errCode,
        &errOffset,
        nullptr
    );

    if (!code) {
        if (errorMsg) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errCode, buffer, sizeof(buffer));
            *errorMsg = reinterpret_cast<char*>(buffer);
        }
        return false;
    }

    pcre2_match_data* matchData = pcre2_match_data_create_from_pattern(code, nullptr);
    if (!matchData) {
        pcre2_code_free(code);
        if (errorMsg) *errorMsg = "Failed to create match data";
        return false;
    }

    uint32_t captureCount = 0;
    pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &captureCount);

    impl_ = std::make_unique<Impl>();
    impl_->code = code;
    impl_->matchData = matchData;
    impl_->captureCount = captureCount;
    return true;
}

bool RegexEngine::match(std::string_view subject, size_t startOffset, MatchResult* result) const {
    if (!isCompiled() || startOffset > subject.size()) {
        return false;
    }

    const int rc = pcre2_match(
        impl_->code,
        reinterpret_cast<PCRE2_SPTR>(subject.data()),
        subject.size(),
        startOffset,
        0,
        impl_->matchData,
        nullptr
    );

    if (rc < 0) {
        return false;
    }

    if (result) {
        PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(impl_->matchData);
        result->start = ovector[0];
        result->end = ovector[1];
        result->captureGroups.clear();

        const uint32_t totalGroups = static_cast<uint32_t>(rc);
        result->captureGroups.reserve(totalGroups);
        for (uint32_t i = 0; i < totalGroups; ++i) {
            result->captureGroups.emplace_back(ovector[2 * i], ovector[2 * i + 1]);
        }
    }
    return true;
}

std::vector<MatchResult> RegexEngine::matchAll(std::string_view subject) const {
    std::vector<MatchResult> matches;
    if (!isCompiled()) {
        return matches;
    }

    size_t offset = 0;
    MatchResult res;
    while (match(subject, offset, &res)) {
        matches.push_back(res);
        if (res.end == offset) {
            // Avoid infinite loop on zero-width match
            offset++;
            if (offset > subject.size()) {
                break;
            }
        } else {
            offset = res.end;
        }
    }
    return matches;
}

std::string RegexEngine::expandReplacement(std::string_view subject,
                                          const MatchResult& match,
                                          std::string_view replacement) const {
    std::string result;
    result.reserve(replacement.size() + 16);

    for (size_t i = 0; i < replacement.size(); ++i) {
        const char c = replacement[i];
        if ((c == '$' || c == '\\') && i + 1 < replacement.size()) {
            const char next = replacement[i + 1];
            if (next >= '1' && next <= '9') {
                const size_t groupIdx = static_cast<size_t>(next - '0');
                if (groupIdx < match.captureGroups.size()) {
                    const auto& [gStart, gEnd] = match.captureGroups[groupIdx];
                    if (gStart != PCRE2_UNSET && gEnd != PCRE2_UNSET && gEnd <= subject.size()) {
                        result.append(subject.substr(gStart, gEnd - gStart));
                    }
                }
                ++i;
                continue;
            }
            if (next == '0' || next == '&') {
                if (match.end <= subject.size()) {
                    result.append(subject.substr(match.start, match.end - match.start));
                }
                ++i;
                continue;
            }
            if (next == '$' || next == '\\') {
                result.push_back(next);
                ++i;
                continue;
            }
        }
        result.push_back(c);
    }
    return result;
}

} // namespace notepadx
