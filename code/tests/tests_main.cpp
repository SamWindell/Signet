#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

#include "common.h"

int main(int argc, char **argv) {
    SetMessagesEnabled(false);
    return doctest::Context(argc, argv).run();
}
