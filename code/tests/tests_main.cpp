#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

#include "common.h"

static void SetSilent(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--silent") == 0) {
            SetMessagesEnabled(false);
        }
    }
}

int main(int argc, char **argv) {
    SetSilent(argc, argv);
    return doctest::Context(argc, argv).run();
}
