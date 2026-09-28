#include <gtk/gtk.h>

static void on_activate(GtkApplication *app, gpointer) {
    auto *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "NotepadX");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    auto *app = gtk_application_new("org.notepadx.editor",
                                    G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
