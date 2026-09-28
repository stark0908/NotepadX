#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace notepadx {

class Document {
public:
    Document();
    explicit Document(std::string id, std::string title = "", std::string filePath = "");

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& title() const noexcept { return title_; }
    void setTitle(std::string title) { title_ = std::move(title); }

    [[nodiscard]] const std::string& filePath() const noexcept { return filePath_; }
    void setFilePath(std::string filePath);

    [[nodiscard]] bool isModified() const noexcept { return isModified_; }
    void setModified(bool modified) noexcept { isModified_ = modified; }

    [[nodiscard]] bool isUnnamed() const noexcept { return filePath_.empty(); }

    [[nodiscard]] int64_t cursorPosition() const noexcept { return cursorPosition_; }
    void setCursorPosition(int64_t pos) noexcept { cursorPosition_ = pos; }

    [[nodiscard]] int64_t scrollLine() const noexcept { return scrollLine_; }
    void setScrollLine(int64_t line) noexcept { scrollLine_ = line; }

    [[nodiscard]] const std::string& content() const noexcept { return content_; }
    void setContent(std::string content) { content_ = std::move(content); }

    [[nodiscard]] const std::string& encoding() const noexcept { return encoding_; }
    void setEncoding(std::string enc) { encoding_ = std::move(enc); }

    [[nodiscard]] std::string displayName() const;

    static std::string generateUuid();

private:
    std::string id_;
    std::string title_;
    std::string filePath_;
    bool isModified_{false};
    int64_t cursorPosition_{0};
    int64_t scrollLine_{0};
    std::string content_;
    std::string encoding_{"UTF-8"};
};

} // namespace notepadx
