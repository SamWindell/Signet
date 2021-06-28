#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

#include "common.h"

static void HandleTestArgs(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--silent") == 0) {
            g_messages_enabled = false;
            return;
        }
    }
}

int main(int argc, char **argv) {
    HandleTestArgs(argc, argv);
    return doctest::Context(argc, argv).run();
}
