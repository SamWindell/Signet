#include "normaliser.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    Normaliser normaliser {};
    AudioUtilInterface util(normaliser);
    return util.Main(argc, argv);
}
