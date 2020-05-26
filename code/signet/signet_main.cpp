#include "audio_util_interface.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    AudioUtilInterface signet;
    return signet.Main(argc, argv);
}
