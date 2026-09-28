#include "app/Application.h"

namespace notepadx {

Application::Application()
    : app_(gtk_application_new("org.notepadx.editor", G_APPLICATION_HANDLES_OPEN)) {
    g_signal_connect(app_, "activate", G_CALLBACK(onActivate), this);
    g_signal_connect(app_, "open", G_CALLBACK(onOpen), this);
}

Application::~Application() {
    if (app_) {
        g_object_unref(app_);
    }
}

int Application::run(int argc, char* argv[]) {
    return g_application_run(G_APPLICATION(app_), argc, argv);
}

void Application::onActivate([[maybe_unused]] GtkApplication* app, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!self->mainWindow_) {
        self->mainWindow_ = std::make_unique<MainWindow>(self->app_);
    }
    self->mainWindow_->show();
}

void Application::onOpen([[maybe_unused]] GtkApplication* app,
                       GFile** files,
                       gint nFiles,
                       [[maybe_unused]] const gchar* hint,
                       gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!self->mainWindow_) {
        self->mainWindow_ = std::make_unique<MainWindow>(self->app_);
    }
    self->mainWindow_->show();

    if (files && nFiles > 0) {
        for (gint i = 0; i < nFiles; ++i) {
            char* path = g_file_get_path(files[i]);
            if (path) {
                self->mainWindow_->openFile(path);
                g_free(path);
            }
        }
    }
}

} // namespace notepadx
