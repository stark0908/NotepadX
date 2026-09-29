#include "platform/FileWatcher.h"

#include <glib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <climits>
#include <vector>

namespace notepadx {

FileWatcher::FileWatcher() {
    initInotify();
}

FileWatcher::~FileWatcher() {
    unwatchAll();
    if (watchSourceId_ != 0) {
        g_source_remove(watchSourceId_);
        watchSourceId_ = 0;
    }
    if (ioChannel_) {
        g_io_channel_unref(static_cast<GIOChannel*>(ioChannel_));
        ioChannel_ = nullptr;
    }
    if (inotifyFd_ >= 0) {
        close(inotifyFd_);
        inotifyFd_ = -1;
    }
}

FileWatcher::FileWatcher(FileWatcher&& other) noexcept
    : inotifyFd_(other.inotifyFd_),
      ioChannel_(other.ioChannel_),
      watchSourceId_(other.watchSourceId_),
      pathToWd_(std::move(other.pathToWd_)),
      wdToPath_(std::move(other.wdToPath_)),
      callbacks_(std::move(other.callbacks_)),
      ignoredPaths_(std::move(other.ignoredPaths_)) {
    other.inotifyFd_ = -1;
    other.ioChannel_ = nullptr;
    other.watchSourceId_ = 0;
}

FileWatcher& FileWatcher::operator=(FileWatcher&& other) noexcept {
    if (this != &other) {
        unwatchAll();
        if (watchSourceId_ != 0) {
            g_source_remove(watchSourceId_);
        }
        if (ioChannel_) {
            g_io_channel_unref(ioChannel_);
        }
        if (inotifyFd_ >= 0) {
            close(inotifyFd_);
        }

        inotifyFd_ = other.inotifyFd_;
        ioChannel_ = other.ioChannel_;
        watchSourceId_ = other.watchSourceId_;
        pathToWd_ = std::move(other.pathToWd_);
        wdToPath_ = std::move(other.wdToPath_);
        callbacks_ = std::move(other.callbacks_);
        ignoredPaths_ = std::move(other.ignoredPaths_);

        other.inotifyFd_ = -1;
        other.ioChannel_ = nullptr;
        other.watchSourceId_ = 0;
    }
    return *this;
}

void FileWatcher::initInotify() {
    inotifyFd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyFd_ < 0) {
        return;
    }

    auto* channel = g_io_channel_unix_new(inotifyFd_);
    g_io_channel_set_close_on_unref(channel, FALSE);
    g_io_channel_set_encoding(channel, nullptr, nullptr);
    ioChannel_ = channel;

    watchSourceId_ = g_io_add_watch(
        channel,
        static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
        reinterpret_cast<GIOFunc>(onIoCallback),
        this
    );
}

bool FileWatcher::watch(const std::string& filePath, ChangeCallback cb) {
    if (inotifyFd_ < 0 || filePath.empty() || !cb) {
        return false;
    }

    auto it = pathToWd_.find(filePath);
    if (it != pathToWd_.end()) {
        const int oldWd = it->second;
        if (inotifyFd_ >= 0) {
            inotify_rm_watch(inotifyFd_, oldWd);
        }
        wdToPath_.erase(oldWd);
        pathToWd_.erase(it);
    }

    const int wd = inotify_add_watch(
        inotifyFd_,
        filePath.c_str(),
        IN_CLOSE_WRITE | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF
    );

    if (wd < 0) {
        return false;
    }

    pathToWd_[filePath] = wd;
    wdToPath_[wd] = filePath;
    callbacks_[filePath] = std::move(cb);
    return true;
}

bool FileWatcher::unwatch(const std::string& filePath) {
    auto it = pathToWd_.find(filePath);
    if (it == pathToWd_.end()) {
        return false;
    }

    const int wd = it->second;
    if (inotifyFd_ >= 0) {
        inotify_rm_watch(inotifyFd_, wd);
    }

    pathToWd_.erase(it);
    wdToPath_.erase(wd);
    callbacks_.erase(filePath);
    ignoredPaths_.erase(filePath);
    return true;
}

void FileWatcher::unwatchAll() {
    if (inotifyFd_ >= 0) {
        for (const auto& [path, wd] : pathToWd_) {
            inotify_rm_watch(inotifyFd_, wd);
        }
    }
    pathToWd_.clear();
    wdToPath_.clear();
    callbacks_.clear();
    ignoredPaths_.clear();
}

void FileWatcher::ignoreNextChange(const std::string& filePath) {
    ignoredPaths_.insert(filePath);
}

bool FileWatcher::isWatching(const std::string& filePath) const {
    return pathToWd_.contains(filePath);
}

gboolean FileWatcher::onIoCallback([[maybe_unused]] GIOChannel* channel,
                                  int condition,
                                  void* userData) {
    auto* self = static_cast<FileWatcher*>(userData);
    if (!self) {
        return G_SOURCE_REMOVE;
    }

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        return G_SOURCE_REMOVE;
    }

    if (condition & G_IO_IN) {
        self->processInotifyEvents();
    }
    return G_SOURCE_CONTINUE;
}

void FileWatcher::processInotifyEvents() {
    constexpr size_t kBufSize = 4096;
    alignas(struct inotify_event) char buffer[kBufSize];

    while (true) {
        const ssize_t len = read(inotifyFd_, buffer, sizeof(buffer));
        if (len <= 0) {
            break;
        }

        ssize_t i = 0;
        while (i < len) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            auto it = wdToPath_.find(event->wd);
            if (it != wdToPath_.end()) {
                const std::string path = it->second;

                if (event->mask & IN_IGNORED) {
                    pathToWd_.erase(path);
                    wdToPath_.erase(event->wd);
                }

                if (ignoredPaths_.contains(path)) {
                    ignoredPaths_.erase(path);
                } else if (!(event->mask & IN_IGNORED)) {
                    auto cbIt = callbacks_.find(path);
                    if (cbIt != callbacks_.end() && cbIt->second) {
                        cbIt->second(path);
                    }
                }
            }
            i += static_cast<ssize_t>(sizeof(struct inotify_event)) + event->len;
        }
    }
}

} // namespace notepadx
