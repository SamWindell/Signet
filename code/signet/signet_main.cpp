#include "signet_interface.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

int main(const int argc, const char *argv[]) {
    doctest::Context context(0, nullptr);
    context.setAsDefaultForAssertsOutOfTestCases();

    SignetInterface signet;
    return signet.Main(argc, argv);
}
