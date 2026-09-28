#pragma once

#include <gtk/gtk.h>
#include "editor/Editor.h"

#include <memory>

namespace notepadx {

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow() = default;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    MainWindow(MainWindow&&) noexcept = default;
    MainWindow& operator=(MainWindow&&) noexcept = default;

    [[nodiscard]] GtkWidget* window() const noexcept { return window_; }
    [[nodiscard]] Editor* activeEditor() const noexcept { return editor_.get(); }

    void show();

private:
    GtkWidget* window_{nullptr};
    GtkWidget* mainBox_{nullptr};
    std::unique_ptr<Editor> editor_;
};

} // namespace notepadx
