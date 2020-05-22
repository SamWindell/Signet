#include "fader.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    Fader fader {};
    AudioUtilInterface util(fader);
    return util.Main(argc, argv);
}
