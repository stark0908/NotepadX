#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace notepadx {

struct MatchResult {
    size_t start{0};
    size_t end{0};
    std::vector<std::pair<size_t, size_t>> captureGroups;

    [[nodiscard]] size_t length() const noexcept { return end >= start ? end - start : 0; }
    [[nodiscard]] bool empty() const noexcept { return start == end; }
};

class RegexEngine {
public:
    RegexEngine();
    ~RegexEngine();

    RegexEngine(const RegexEngine&) = delete;
    RegexEngine& operator=(const RegexEngine&) = delete;

    RegexEngine(RegexEngine&& other) noexcept;
    RegexEngine& operator=(RegexEngine&& other) noexcept;

    bool compile(std::string_view query,
                 bool caseSensitive,
                 bool wholeWord,
                 bool isRegex,
                 std::string* errorMsg = nullptr);

    [[nodiscard]] bool isCompiled() const noexcept;

    bool match(std::string_view subject, size_t startOffset, MatchResult* result) const;
    [[nodiscard]] std::vector<MatchResult> matchAll(std::string_view subject) const;

    [[nodiscard]] std::string expandReplacement(std::string_view subject,
                                               const MatchResult& match,
                                               std::string_view replacement) const;

    static std::string escapePattern(std::string_view literal);

private:
    void cleanup();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace notepadx
