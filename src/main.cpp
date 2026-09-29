#include "app/Application.h"
#include "ipc/SingleInstance.h"

#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    g_set_prgname("notepadx");
    g_set_application_name("NotepadX");

    const auto startTime = std::chrono::steady_clock::now();
    bool forceNewWindow = false;
    bool benchStartup = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--new-window") {
            forceNewWindow = true;
        } else if (arg == "--bench-startup") {
            benchStartup = true;
            forceNewWindow = true;
        } else if (!arg.empty() && arg[0] != '-') {
            files.push_back(arg);
        }
    }

    if (!forceNewWindow) {
        notepadx::SingleInstance instance;
        if (instance.notifyRunningInstance(files)) {
            // Running instance notified and files forwarded, exit fast
            return 0;
        }
    }

    notepadx::Application app;
    if (benchStartup) {
        app.setBenchmarkMode(true, startTime);
    }

    std::vector<char*> cleanArgv;
    cleanArgv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg != "--bench-startup" && arg != "--new-window") {
            cleanArgv.push_back(argv[i]);
        }
    }
    cleanArgv.push_back(nullptr);
    int cleanArgc = static_cast<int>(cleanArgv.size()) - 1;

    return app.run(cleanArgc, cleanArgv.data());
}
