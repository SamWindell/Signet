#include "signet_interface.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    SignetInterface signet;
    return signet.Main(argc, argv);
}
