#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <string>

namespace notepadx {

class StatusBar {
public:
    using LanguageClickedCallback = std::function<void()>;
    using EolClickedCallback = std::function<void()>;

    StatusBar();
    ~StatusBar() = default;

    StatusBar(const StatusBar&) = delete;
    StatusBar& operator=(const StatusBar&) = delete;

    [[nodiscard]] GtkWidget* widget() const noexcept { return container_; }

    void updateCursor(int line, int col);
    void updateSelection(int selLength);
    void updateDocStats(int lineCount, size_t length);
    void updateEol(int eolMode);
    void updateEncoding(const std::string& enc);
    void updateLanguage(const std::string& lang);

    void setLanguageClickedCallback(LanguageClickedCallback cb) { langCb_ = std::move(cb); }
    void setEolClickedCallback(EolClickedCallback cb) { eolCb_ = std::move(cb); }

private:
    GtkWidget* container_{nullptr};
    GtkWidget* lblPos_{nullptr};
    GtkWidget* lblSel_{nullptr};
    GtkWidget* lblStats_{nullptr};
    GtkWidget* lblEol_{nullptr};
    GtkWidget* lblEncoding_{nullptr};
    GtkWidget* lblLang_{nullptr};

    LanguageClickedCallback langCb_{nullptr};
    EolClickedCallback eolCb_{nullptr};
};

} // namespace notepadx
