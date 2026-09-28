#pragma once

#include <gtk/gtk.h>
#include <Scintilla.h>
#include <ScintillaWidget.h>

#include <functional>
#include <string>
#include <string_view>

namespace notepadx {

class ScintillaAdapter {
public:
    using NotificationCallback = std::function<void(const SCNotification*)>;

    ScintillaAdapter();
    ~ScintillaAdapter();

    ScintillaAdapter(const ScintillaAdapter&) = delete;
    ScintillaAdapter& operator=(const ScintillaAdapter&) = delete;

    ScintillaAdapter(ScintillaAdapter&&) noexcept = default;
    ScintillaAdapter& operator=(ScintillaAdapter&&) noexcept = default;

    [[nodiscard]] GtkWidget* widget() const noexcept { return widget_; }

    sptr_t send(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0) const;

    void setText(std::string_view text);
    [[nodiscard]] std::string getText() const;
    [[nodiscard]] size_t getLength() const;

    void setLineNumbers(bool show);
    void updateLineNumberWidth();
    void setWordWrap(bool enable);
    void setTabWidth(int spaces);
    void setUseTabs(bool useTabs);
    void setFont(const std::string& fontName, int sizePt);
    void setEolMode(int eolMode);

    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    [[nodiscard]] sptr_t getCurrentPos() const;
    void setCurrentPos(sptr_t pos);
    [[nodiscard]] sptr_t getFirstVisibleLine() const;
    void setFirstVisibleLine(sptr_t line);

    void setNotificationCallback(NotificationCallback cb);

private:
    static void onNotification(GtkWidget* widget, gint id, SCNotification* scn, gpointer userData);

    GtkWidget* widget_{nullptr};
    NotificationCallback notificationCb_{nullptr};
};

} // namespace notepadx
