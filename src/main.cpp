#include "app/Application.h"
#include "ipc/SingleInstance.h"

#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    bool forceNewWindow = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--new-window") {
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
    return app.run(argc, argv);
}
