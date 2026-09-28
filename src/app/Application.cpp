#include "app/Application.h"

namespace notepadx {

Application::Application()
    : app_(gtk_application_new("org.notepadx.editor",
                               static_cast<GApplicationFlags>(G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE))) {
    g_signal_connect(app_, "activate", G_CALLBACK(onActivate), this);
    g_signal_connect(app_, "open", G_CALLBACK(onOpen), this);
}

Application::~Application() {
    if (app_) {
        g_object_unref(app_);
    }
}

void Application::ensureInitialized() {
    if (!mainWindow_) {
        mainWindow_ = std::make_unique<MainWindow>(app_);
        singleInstance_.startListening([this](const std::vector<std::string>& files) {
            if (mainWindow_) {
                for (const auto& file : files) {
                    mainWindow_->openFile(file);
                }
                mainWindow_->present();
            }
        });
    }
    mainWindow_->show();
}

int Application::run(int argc, char* argv[]) {
    return g_application_run(G_APPLICATION(app_), argc, argv);
}

void Application::onActivate([[maybe_unused]] GtkApplication* app, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    self->ensureInitialized();
}

void Application::onOpen([[maybe_unused]] GtkApplication* app,
                       GFile** files,
                       gint nFiles,
                       [[maybe_unused]] const gchar* hint,
                       gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    self->ensureInitialized();

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
