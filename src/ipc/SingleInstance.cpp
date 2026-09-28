#include "ipc/SingleInstance.h"

#include <glib.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>

namespace notepadx {

std::filesystem::path SingleInstance::defaultSocketPath() {
    const char* xdgRuntime = std::getenv("XDG_RUNTIME_DIR");
    if (xdgRuntime && *xdgRuntime) {
        return std::filesystem::path(xdgRuntime) / "notepadx.sock";
    }
    const uid_t uid = getuid();
    return "/tmp/notepadx-" + std::to_string(uid) + ".sock";
}

std::string SingleInstance::serializePayload(const std::vector<std::string>& files) {
    nlohmann::json j;
    j["action"] = "open";
    j["files"] = files;
    return j.dump();
}

std::vector<std::string> SingleInstance::deserializePayload(const std::string& jsonStr) {
    std::vector<std::string> result;
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        if (j.contains("files") && j["files"].is_array()) {
            for (const auto& item : j["files"]) {
                if (item.is_string()) {
                    result.push_back(item.get<std::string>());
                }
            }
        }
    } catch (...) {
        // Return empty on corrupted or invalid JSON
    }
    return result;
}

SingleInstance::SingleInstance(std::filesystem::path socketPath)
    : socketPath_(std::move(socketPath)) {
}

SingleInstance::~SingleInstance() {
    stop();
}

SingleInstance::SingleInstance(SingleInstance&& other) noexcept
    : socketPath_(std::move(other.socketPath_)),
      serverFd_(other.serverFd_),
      watchId_(other.watchId_),
      filesCb_(std::move(other.filesCb_)) {
    other.serverFd_ = -1;
    other.watchId_ = 0;
}

SingleInstance& SingleInstance::operator=(SingleInstance&& other) noexcept {
    if (this != &other) {
        stop();
        socketPath_ = std::move(other.socketPath_);
        serverFd_ = other.serverFd_;
        watchId_ = other.watchId_;
        filesCb_ = std::move(other.filesCb_);
        other.serverFd_ = -1;
        other.watchId_ = 0;
    }
    return *this;
}

void SingleInstance::stop() {
    if (watchId_ != 0) {
        g_source_remove(watchId_);
        watchId_ = 0;
    }
    if (serverFd_ >= 0) {
        close(serverFd_);
        serverFd_ = -1;
    }
    if (!socketPath_.empty()) {
        std::error_code ec;
        std::filesystem::remove(socketPath_, ec);
    }
}

bool SingleInstance::notifyRunningInstance(const std::vector<std::string>& files) {
    if (!std::filesystem::exists(socketPath_)) {
        return false;
    }

    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clientFd < 0) {
        return false;
    }

    // Set timeout to avoid hanging if the socket is unresponsive
    struct timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms
    setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    const std::string pathStr = socketPath_.string();
    if (pathStr.length() >= sizeof(addr.sun_path)) {
        close(clientFd);
        return false;
    }
    std::strncpy(addr.sun_path, pathStr.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(clientFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        // Socket file exists but no process is listening (stale socket)
        close(clientFd);
        std::error_code ec;
        std::filesystem::remove(socketPath_, ec);
        return false;
    }

    // Convert relative paths to canonical/absolute paths
    std::vector<std::string> absFiles;
    absFiles.reserve(files.size());
    for (const auto& f : files) {
        std::error_code ec;
        auto p = std::filesystem::absolute(f, ec);
        absFiles.push_back(ec ? f : p.string());
    }

    const std::string payload = serializePayload(absFiles);
    const uint32_t len = static_cast<uint32_t>(payload.size());

    if (write(clientFd, &len, sizeof(len)) != sizeof(len)) {
        close(clientFd);
        return false;
    }

    if (write(clientFd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
        close(clientFd);
        return false;
    }

    // Read 1-byte ACK from primary instance
    char ack = 0;
    const ssize_t bytesRead = read(clientFd, &ack, 1);
    close(clientFd);

    return (bytesRead == 1 && ack == 'K');
}

bool SingleInstance::startListening(FilesReceivedCallback cb) {
    stop();
    filesCb_ = std::move(cb);

    // Remove stale socket if present
    std::error_code ec;
    std::filesystem::remove(socketPath_, ec);

    serverFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (serverFd_ < 0) {
        return false;
    }

    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    const std::string pathStr = socketPath_.string();
    if (pathStr.length() >= sizeof(addr.sun_path)) {
        stop();
        return false;
    }
    std::strncpy(addr.sun_path, pathStr.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        stop();
        return false;
    }

    if (listen(serverFd_, 16) != 0) {
        stop();
        return false;
    }

    // Register with GLib event loop
    GIOChannel* channel = g_io_channel_unix_new(serverFd_);
    watchId_ = g_io_add_watch(
        channel,
        static_cast<GIOCondition>(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR),
        +[](GIOChannel* /*source*/, GIOCondition cond, gpointer data) -> gboolean {
            if (cond & (G_IO_IN | G_IO_PRI)) {
                auto* self = static_cast<SingleInstance*>(data);
                self->processPendingConnection(0);
            }
            return G_SOURCE_CONTINUE;
        },
        this
    );
    g_io_channel_unref(channel);

    return true;
}

bool SingleInstance::processPendingConnection(int timeoutMs) {
    if (serverFd_ < 0) {
        return false;
    }

    if (timeoutMs > 0) {
        struct pollfd pfd {};
        pfd.fd = serverFd_;
        pfd.events = POLLIN;
        const int ret = poll(&pfd, 1, timeoutMs);
        if (ret <= 0 || !(pfd.revents & POLLIN)) {
            return false;
        }
    }

    int clientFd = accept4(serverFd_, nullptr, nullptr, SOCK_CLOEXEC);
    if (clientFd < 0) {
        return false;
    }

    uint32_t len = 0;
    ssize_t bytesRead = read(clientFd, &len, sizeof(len));
    if (bytesRead != sizeof(len) || len > 10 * 1024 * 1024) { // 10MB sanity limit
        close(clientFd);
        return false;
    }

    std::string payload(len, '\0');
    size_t total = 0;
    while (total < len) {
        ssize_t r = read(clientFd, payload.data() + total, len - total);
        if (r <= 0) {
            break;
        }
        total += static_cast<size_t>(r);
    }

    if (total == len) {
        const char ack = 'K';
        [[maybe_unused]] auto w = write(clientFd, &ack, 1);
        close(clientFd);

        std::vector<std::string> files = deserializePayload(payload);
        if (filesCb_) {
            filesCb_(files);
        }
        return true;
    }

    close(clientFd);
    return false;
}

} // namespace notepadx
