// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <thread>
#include <iostream>

// This needs to be included before getopt.h because the latter #defines symbols used by it
#include "common/microprofile.h"

#ifdef _MSC_VER
#include <getopt.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

#include "common/logging/log.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"

#include "core/settings.h"
#include "core/system.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/loader/loader.h"

#include "citra/config.h"
#include "citra/emu_window/emu_window_glfw.h"

#include "video_core/video_core.h"


static void PrintHelp()
{
    std::cout << "Usage: citra [options] <filename>" << std::endl;
    std::cout << "--help, -h            Display this information" << std::endl;
    std::cout << "--gdbport, -g number  Enable gdb stub on port number" << std::endl;
}

/// Application entry point
int main(int argc, char **argv) {
    int option_index = 0;
    u32 gdb_port = 0;
    char *endarg;
    std::string boot_filename;

    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "gdbport", required_argument, 0, 'g' },
        { 0, 0, 0, 0 }
    };

    while (optind < argc) {
        char arg = getopt_long(argc, argv, ":hg:", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'h':
                PrintHelp();
                return 0;
            case 'g':
                errno = 0;
                gdb_port = strtoul(optarg, &endarg, 0);
                if (endarg == optarg) errno = EINVAL;
                if (errno != 0) {
                    perror("--gdbport");
                    exit(1);
                }
                break;
            }
        } else {
            boot_filename = argv[optind];
            optind++;
        }
    }

    Log::Filter log_filter(Log::Level::Debug);
    Log::SetFilter(&log_filter);

    MicroProfileOnThreadCreate("EmuThread");

    if (boot_filename.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return -1;
    }

    Config config;
    log_filter.ParseFilterString(Settings::values.log_filter);

    if (gdb_port != 0) {
        GDBStub::ToggleServer(true);
        GDBStub::SetServerPort(gdb_port);
    }

    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    VideoCore::g_hw_renderer_enabled = Settings::values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = Settings::values.use_shader_jit;

    System::Init(emu_window);

    Loader::ResultStatus load_result = Loader::LoadFile(boot_filename);
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Frontend, "Failed to load ROM (Error %i)!", load_result);
        return -1;
    }

    while (emu_window->IsOpen()) {
        Core::RunLoop();
    }

    System::Shutdown();

    delete emu_window;

    MicroProfileShutdown();

    return 0;
}
