#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

#include "common.h"
#include "tests_config.h"

static void HandleTestArgs(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--silent") == 0) {
            g_messages_enabled = false;
            return;
        }
    }
}

int main(int argc, char **argv) {
    fs::current_path(BUILD_DIRECTORY);
    HandleTestArgs(argc, argv);
    return doctest::Context(argc, argv).run();
}
