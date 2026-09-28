#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace notepadx {

class SingleInstance {
public:
    using FilesReceivedCallback = std::function<void(const std::vector<std::string>& files)>;

    explicit SingleInstance(std::filesystem::path socketPath = defaultSocketPath());
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    SingleInstance(SingleInstance&& other) noexcept;
    SingleInstance& operator=(SingleInstance&& other) noexcept;

    // Attempts to notify an existing running instance.
    // Relative paths in `files` are converted to absolute paths relative to current_path.
    // Returns true if a running instance handled the request, false otherwise.
    bool notifyRunningInstance(const std::vector<std::string>& files);

    // Starts listening for incoming file-open requests from secondary instances.
    bool startListening(FilesReceivedCallback cb);

    // Synchronously checks and processes one pending connection (useful for unit tests without GLib).
    bool processPendingConnection(int timeoutMs = 0);

    // Stops listening and unlinks the socket file.
    void stop();

    [[nodiscard]] bool isListening() const noexcept { return serverFd_ >= 0; }
    [[nodiscard]] const std::filesystem::path& socketPath() const noexcept { return socketPath_; }
    [[nodiscard]] int serverFd() const noexcept { return serverFd_; }

    static std::filesystem::path defaultSocketPath();
    static std::string serializePayload(const std::vector<std::string>& files);
    static std::vector<std::string> deserializePayload(const std::string& jsonStr);

private:
    std::filesystem::path socketPath_;
    int serverFd_{-1};
    unsigned int watchId_{0};
    FilesReceivedCallback filesCb_{nullptr};
};

} // namespace notepadx
