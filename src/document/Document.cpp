#include "document/Document.h"

#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

namespace notepadx {

std::string Document::generateUuid() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());
    thread_local std::uniform_int_distribution<uint64_t> dist;

    uint64_t part1 = dist(gen);
    uint64_t part2 = dist(gen);

    // Set version 4 (bits 12-15 of time_hi_and_version to 0100)
    part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant (bits 6-7 of clk_seq_hi_res to 10)
    part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (part1 >> 32);
    oss << '-' << std::setw(4) << ((part1 >> 16) & 0xFFFF);
    oss << '-' << std::setw(4) << (part1 & 0xFFFF);
    oss << '-' << std::setw(4) << (part2 >> 48);
    oss << '-' << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

Document::Document()
    : id_(generateUuid()),
      title_("Untitled") {
}

Document::Document(std::string id, std::string title, std::string filePath)
    : id_(id.empty() ? generateUuid() : std::move(id)),
      title_(std::move(title)),
      filePath_(std::move(filePath)) {
    if (title_.empty()) {
        if (!filePath_.empty()) {
            title_ = std::filesystem::path(filePath_).filename().string();
        } else {
            title_ = "Untitled";
        }
    }
}

void Document::setFilePath(std::string filePath) {
    filePath_ = std::move(filePath);
    if (!filePath_.empty()) {
        title_ = std::filesystem::path(filePath_).filename().string();
    }
}

std::string Document::displayName() const {
    std::string name = title_.empty() ? "Untitled" : title_;
    if (isModified_) {
        return "• " + name;
    }
    return name;
}

} // namespace notepadx
