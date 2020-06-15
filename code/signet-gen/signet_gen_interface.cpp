#include "signet_gen_interface.h"

#include "rang.hpp"

#include "sample_blender.h"

int SignetGenInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {"Signet-gen is the generator program for Signet. Rather than edit given files, this "
                  "program generates new files. It has a different set of subcommands to Signet."};

    app.require_subcommand();

    SampleBlender::Create(app);

    try {
        app.parse(argc, argv);
        return 0;
    } catch (const CLI::ParseError &e) {
        if (e.get_exit_code() != 0) {
            std::atexit([]() { std::cout << rang::style::reset; });
            std::cout << rang::fg::red;
        }
        return app.exit(e);
    }
}
