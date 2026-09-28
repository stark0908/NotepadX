#pragma once

#include <gtk/gtk.h>
#include "app/MainWindow.h"
#include "ipc/SingleInstance.h"

#include <memory>
#include <vector>
#include <string>

namespace notepadx {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run(int argc, char* argv[]);

    [[nodiscard]] MainWindow* mainWindow() const noexcept { return mainWindow_.get(); }

private:
    void ensureInitialized();

    static void onActivate(GtkApplication* app, gpointer userData);
    static void onOpen(GtkApplication* app, GFile** files, gint nFiles, const gchar* hint, gpointer userData);

    GtkApplication* app_{nullptr};
    std::unique_ptr<MainWindow> mainWindow_;
    SingleInstance singleInstance_;
};

} // namespace notepadx
