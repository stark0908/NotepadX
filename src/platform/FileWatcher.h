#pragma once

#include <glib.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace notepadx {

class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& path)>;

    FileWatcher();
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    FileWatcher(FileWatcher&&) noexcept;
    FileWatcher& operator=(FileWatcher&&) noexcept;

    bool watch(const std::string& filePath, ChangeCallback cb);
    bool unwatch(const std::string& filePath);
    void unwatchAll();

    void ignoreNextChange(const std::string& filePath);
    [[nodiscard]] bool isWatching(const std::string& filePath) const;

private:
    void initInotify();
    void processInotifyEvents();
    static gboolean onIoCallback(GIOChannel* channel, int condition, void* userData);

    int inotifyFd_{-1};
    GIOChannel* ioChannel_{nullptr};
    unsigned int watchSourceId_{0};

    std::unordered_map<std::string, int> pathToWd_;
    std::unordered_map<int, std::string> wdToPath_;
    std::unordered_map<std::string, ChangeCallback> callbacks_;
    std::unordered_set<std::string> ignoredPaths_;
};

} // namespace notepadx
