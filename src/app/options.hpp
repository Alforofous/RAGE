#pragma once

#include <cstring>
#include <filesystem>

namespace RAGE::App {
    /** @brief Command-line options for the test app. */
    struct Options {
        /// --profile: launch the Tracy GUI and wait for it before engine allocations.
        bool profile = false;
        /// --no-vsync clears this.
        bool vsync = true;
        /// --world=<dir>: persist streamed chunks here (empty = generate-only).
        std::filesystem::path worldDir;

        static Options parse(int argc, char **argv) {
            Options o;
            for (int i = 1; i < argc; ++i) {
                if (std::strcmp(argv[i], "--profile") == 0) {
                    o.profile = true;
                } else if (std::strcmp(argv[i], "--no-vsync") == 0) {
                    o.vsync = false;
                } else if (std::strncmp(argv[i], "--world=", 8) == 0) {
                    o.worldDir = argv[i] + 8;
                }
            }
            return o;
        }
    };
}
