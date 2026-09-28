#include "session/Autosave.h"

namespace notepadx {

Autosave::Autosave(SaveCallback saveCb, uint32_t delayMs, TimerScheduler timerScheduler)
    : saveCb_(std::move(saveCb)),
      delayMs_(delayMs),
      scheduler_(std::move(timerScheduler)) {
}

void Autosave::markDirty(const std::string& docId) {
    if (docId.empty()) {
        return;
    }
    pendingDocIds_.insert(docId);
    triggerTimer();
}

void Autosave::markClean(const std::string& docId) {
    pendingDocIds_.erase(docId);
}

bool Autosave::isPending(const std::string& docId) const {
    return pendingDocIds_.contains(docId);
}

void Autosave::triggerTimer() {
    if (scheduler_) {
        timerRunning_ = true;
        scheduler_(delayMs_, [this]() {
            onTimeoutFired();
        });
    }
}

void Autosave::onTimeoutFired() {
    timerRunning_ = false;
    flushNow();
}

void Autosave::flushNow() {
    if (pendingDocIds_.empty() || !saveCb_) {
        return;
    }

    auto copy = std::move(pendingDocIds_);
    pendingDocIds_.clear();

    for (const auto& docId : copy) {
        saveCb_(docId);
    }
}

} // namespace notepadx
