#pragma once

#include "search/Regex.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace notepadx {

struct SearchOptions {
    std::string query;
    std::string replacement;
    bool caseSensitive{false};
    bool wholeWord{false};
    bool isRegex{false};
    bool wrapAround{true};
};

struct SearchResult {
    bool found{false};
    size_t matchIndex{0};  // 1-based, e.g. 3 of 17
    size_t totalMatches{0};
    size_t startPos{0};
    size_t endPos{0};
    std::string statusText;
};

class SearchEngine {
public:
    SearchEngine() = default;
    ~SearchEngine() = default;

    SearchResult findNext(std::string_view text,
                          size_t currentPos,
                          const SearchOptions& options);

    SearchResult findPrevious(std::string_view text,
                              size_t currentPos,
                              const SearchOptions& options);

    [[nodiscard]] std::vector<MatchResult> findAll(std::string_view text,
                                                  const SearchOptions& options);

    std::pair<std::string, size_t> replaceAll(std::string_view text,
                                             const SearchOptions& options);

private:
    RegexEngine regexEngine_;
};

} // namespace notepadx
