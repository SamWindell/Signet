#include "normaliser.h"

int main(const int argc, const char *argv[]) {
    Normaliser normaliser {};
    AudioUtilInterface util(normaliser);
    return util.Main(argc, argv);
}
