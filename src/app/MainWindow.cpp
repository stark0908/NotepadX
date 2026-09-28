#include "app/MainWindow.h"

namespace notepadx {

MainWindow::MainWindow(GtkApplication* app)
    : window_(gtk_application_window_new(app)),
      mainBox_(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)),
      editor_(std::make_unique<Editor>()) {
    gtk_window_set_title(GTK_WINDOW(window_), "NotepadX");
    gtk_window_set_default_size(GTK_WINDOW(window_), 900, 600);

    gtk_container_add(GTK_CONTAINER(window_), mainBox_);
    gtk_box_pack_start(GTK_BOX(mainBox_), editor_->widget(), TRUE, TRUE, 0);

    editor_->setModifiedChangedCallback([this](bool modified) {
        std::string title = modified ? "• NotepadX" : "NotepadX";
        gtk_window_set_title(GTK_WINDOW(window_), title.c_str());
    });
}

void MainWindow::show() {
    gtk_widget_show_all(window_);
}

} // namespace notepadx
