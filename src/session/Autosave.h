#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>

namespace notepadx {

class Autosave {
public:
    using SaveCallback = std::function<void(const std::string& docId)>;
    using TimerScheduler = std::function<void(uint32_t delayMs, std::function<void()> onTimeout)>;

    explicit Autosave(SaveCallback saveCb,
                     uint32_t delayMs = 2000,
                     TimerScheduler timerScheduler = nullptr);
    ~Autosave() = default;

    Autosave(const Autosave&) = delete;
    Autosave& operator=(const Autosave&) = delete;

    Autosave(Autosave&&) noexcept = default;
    Autosave& operator=(Autosave&&) noexcept = default;

    void markDirty(const std::string& docId);
    void markClean(const std::string& docId);
    [[nodiscard]] bool isPending(const std::string& docId) const;
    [[nodiscard]] bool hasPending() const noexcept { return !pendingDocIds_.empty(); }

    void flushNow();
    void setTimerScheduler(TimerScheduler scheduler) { scheduler_ = std::move(scheduler); }
    [[nodiscard]] uint32_t delayMs() const noexcept { return delayMs_; }

private:
    void triggerTimer();
    void onTimeoutFired();

    SaveCallback saveCb_;
    uint32_t delayMs_{2000};
    TimerScheduler scheduler_{nullptr};
    std::unordered_set<std::string> pendingDocIds_;
    bool timerRunning_{false};
};

} // namespace notepadx
