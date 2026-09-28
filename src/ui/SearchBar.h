#pragma once

#include <gtk/gtk.h>
#include "search/Search.h"

#include <functional>
#include <string>

namespace notepadx {

class SearchBar {
public:
    using FindCallback = std::function<void(const SearchOptions& options, bool backward)>;
    using ReplaceCallback = std::function<void(const SearchOptions& options)>;
    using ReplaceAllCallback = std::function<void(const SearchOptions& options)>;
    using QueryChangedCallback = std::function<void(const SearchOptions& options)>;
    using CloseCallback = std::function<void()>;

    SearchBar();
    ~SearchBar() = default;

    SearchBar(const SearchBar&) = delete;
    SearchBar& operator=(const SearchBar&) = delete;

    [[nodiscard]] GtkWidget* widget() const noexcept { return container_; }

    void show(bool replaceMode = false);
    void hide();
    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] bool isReplaceMode() const noexcept { return replaceMode_; }

    void setStatusText(const std::string& text);
    [[nodiscard]] SearchOptions currentOptions() const;

    void setFindCallback(FindCallback cb) { findCb_ = std::move(cb); }
    void setReplaceCallback(ReplaceCallback cb) { replaceCb_ = std::move(cb); }
    void setReplaceAllCallback(ReplaceAllCallback cb) { replaceAllCb_ = std::move(cb); }
    void setQueryChangedCallback(QueryChangedCallback cb) { queryChangedCb_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCb_ = std::move(cb); }

    void focusFindEntry();

private:
    void notifyQueryChanged();
    static void onEntryChanged(GtkEditable* editable, gpointer userData);
    static void onEntryActivate(GtkEntry* entry, gpointer userData);
    static void onToggleChanged(GtkToggleButton* btn, gpointer userData);

    GtkWidget* container_{nullptr};
    GtkWidget* findRow_{nullptr};
    GtkWidget* replaceRow_{nullptr};

    GtkWidget* findEntry_{nullptr};
    GtkWidget* replaceEntry_{nullptr};

    GtkWidget* btnPrev_{nullptr};
    GtkWidget* btnNext_{nullptr};
    GtkWidget* btnClose_{nullptr};

    GtkWidget* chkCase_{nullptr};
    GtkWidget* chkWord_{nullptr};
    GtkWidget* chkRegex_{nullptr};

    GtkWidget* btnReplace_{nullptr};
    GtkWidget* btnReplaceAll_{nullptr};

    GtkWidget* statusLabel_{nullptr};

    FindCallback findCb_{nullptr};
    ReplaceCallback replaceCb_{nullptr};
    ReplaceAllCallback replaceAllCb_{nullptr};
    QueryChangedCallback queryChangedCb_{nullptr};
    CloseCallback closeCb_{nullptr};

    bool replaceMode_{false};
};

} // namespace notepadx
