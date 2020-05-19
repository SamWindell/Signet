#include "zcross_offsetter.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    ZeroCrossingOffseter offsetter {};
    AudioUtilInterface util(offsetter);
    return util.Main(argc, argv);
}
