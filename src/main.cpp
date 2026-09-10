#include "wallpaper_manager.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <csignal>

using namespace miqubg;

static volatile sig_atomic_t g_stop = 0;
static void sig_handler(int) {
    g_stop = 1;
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options...] [image_path]\n\n"
              << "Options:\n"
              << "  -i, --image <path>    Path to the wallpaper image\n"
              << "  -o, --output <name>   Target specific output name (e.g. HDMI-A-1, eDP-1, or * for all)\n"
              << "  -m, --mode <mode>     Scaling mode: fill/cover, fit, stretch, center, tile (default: fill)\n"
              << "  -c, --color <#hex>    Solid background color in hex (e.g. #1e1e2e, default: #000000)\n"
              << "  -v, --version         Show version number and quit\n"
              << "  -h, --help            Show this help message\n\n"
              << "Scaling Modes:\n"
              << "  fill / cover   Scale to fill output, cropping if necessary (default)\n"
              << "  fit / contain  Scale to fit inside output, letterboxing with background color\n"
              << "  stretch        Stretch image to fill output (ignores aspect ratio)\n"
              << "  center         Center image at 1:1 scale without scaling\n"
              << "  tile           Tile the image repeatedly across output\n\n"
              << "Examples:\n"
              << "  " << prog << " -i /path/to/wallpaper.jpg\n"
              << "  " << prog << " /path/to/wallpaper.jpg\n"
              << "  " << prog << " -i /path/to/wallpaper.jpg -m fit -c '#1e1e2e'\n"
              << "  " << prog << " -i default.png -o HDMI-A-1 -i monitor.png -m stretch\n";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);

    OutputConfig default_cfg;
    default_cfg.mode = WallpaperMode::Cover;
    default_cfg.bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);

    std::map<std::string, OutputConfig> output_cfgs;
    std::string current_output = ""; // empty means modifying default_cfg

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "miqubg version 1.0.0\n";
            return 0;
        } else if (arg == "-i" || arg == "--image") {
            if (i + 1 < argc) {
                std::string path = argv[++i];
                if (current_output.empty()) {
                    default_cfg.image_path = path;
                } else {
                    output_cfgs[current_output].image_path = path;
                }
            } else {
                std::cerr << "Error: " << arg << " requires a file path\n";
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                current_output = argv[++i];
                if (current_output == "*") {
                    current_output = "";
                } else if (output_cfgs.find(current_output) == output_cfgs.end()) {
                    output_cfgs[current_output] = default_cfg;
                }
            } else {
                std::cerr << "Error: " << arg << " requires an output name\n";
                return 1;
            }
        } else if (arg == "-m" || arg == "--mode") {
            if (i + 1 < argc) {
                WallpaperMode mode = WallpaperView::parse_mode(argv[++i]);
                if (current_output.empty()) {
                    default_cfg.mode = mode;
                } else {
                    output_cfgs[current_output].mode = mode;
                }
            } else {
                std::cerr << "Error: " << arg << " requires a mode name\n";
                return 1;
            }
        } else if (arg == "-c" || arg == "--color") {
            if (i + 1 < argc) {
                miqu::Color col = miqu::Color::from_hex(argv[++i], miqu::Color::rgb(0.0f, 0.0f, 0.0f));
                if (current_output.empty()) {
                    default_cfg.bg_color = col;
                } else {
                    output_cfgs[current_output].bg_color = col;
                }
            } else {
                std::cerr << "Error: " << arg << " requires a color string (e.g. #000000)\n";
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            // Positional image path shorthand
            if (current_output.empty()) {
                default_cfg.image_path = arg;
            } else {
                output_cfgs[current_output].image_path = arg;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    auto engine = miqu::AppEngine::create();
    if (!engine) {
        std::cerr << "[miqubg] Failed to create AppEngine. Is a Wayland compositor running?\n";
        return 1;
    }

    // Perform an initial roundtrip to ensure registry globals (outputs) are announced
    wl_display_roundtrip(engine->get_display());

    WallpaperManager manager;
    manager.set_default_config(default_cfg);
    for (const auto& pair : output_cfgs) {
        manager.set_output_config(pair.first, pair.second);
    }

    manager.init(engine.get());

    std::cout << "[miqubg] Daemon started successfully.\n";

    while (!g_stop) {
        if (wl_display_dispatch(engine->get_display()) < 0) {
            break;
        }
    }

    std::cout << "[miqubg] Shutting down.\n";
    return 0;
}
