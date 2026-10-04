#include "wallpaper_manager.hpp"
#include <iostream>
#include <set>
#include <algorithm>
#include <thread>
#include <chrono>

namespace miqubg {

miqu::FitMode WallpaperManager::parse_mode(const std::string& mode_str) {
    std::string s = mode_str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "fit" || s == "contain") return miqu::FitMode::Contain;
    if (s == "stretch" || s == "fill") return miqu::FitMode::Fill;
    if (s == "center") return miqu::FitMode::Center;
    if (s == "tile") return miqu::FitMode::Tile;
    return miqu::FitMode::Cover;
}


void WallpaperManager::init(miqu::AppEngine* engine) {
    m_engine = engine;

    miqu::OutputManager::get()->on_outputs_changed([this]() {
        sync_outputs();
    });

    sync_outputs();
}

OutputConfig WallpaperManager::get_config_for_output(const std::string& name) const {
    OutputConfig cfg = m_default_config;
    auto it = m_output_configs.find(name);
    if (it != m_output_configs.end()) {
        cfg = it->second;
    }
    return cfg;
}

static std::shared_ptr<miqu::Window> create_wallpaper_window(const miqu::OutputInfo& out, const OutputConfig& cfg) {
    auto imageView = miqu::ImageViewBuilder::create()
        ->source(cfg.image_path)
        ->fitMode(cfg.mode)
        ->backgroundColor(cfg.bg_color)
        ->build();

    return miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerBackground)
        ->output(out.wl_output)
        ->layerNamespace("wallpaper")
        ->keyboardInteractive(false)
        ->exclusiveZone(-1)
        ->contentView(imageView)
        ->build();
}

void WallpaperManager::update_wallpaper(const std::string& output_name, OutputConfig config) {
    if (output_name.empty() || output_name == "*") {
        m_default_config = config;
        m_output_configs.clear();
    } else {
        m_output_configs[output_name] = config;
    }

    auto outputs = miqu::OutputManager::get()->get_outputs();
    for (const auto& out : outputs) {
        if (!output_name.empty() && output_name != "*" && out.name != output_name) {
            continue;
        }

        OutputConfig cfg = get_config_for_output(out.name);
        std::cout << "[miqubg] Swapping wallpaper for output '"
                  << (out.name.empty() ? "(unknown)" : out.name) << "' [id=" << out.id << "]"
                  << " image: " << cfg.image_path << std::endl;

        auto new_window = create_wallpaper_window(out, cfg);
        if (!new_window) {
            std::cerr << "[miqubg] Failed to create new wallpaper surface for output " << out.id << std::endl;
            continue;
        }

        auto it_old = m_windows.find(out.id);
        std::shared_ptr<miqu::Window> old_window = (it_old != m_windows.end()) ? it_old->second : nullptr;

        m_windows[out.id] = new_window;

        if (old_window && m_engine) {
            // miquland's compositor engine smoothly animates the new surface into place.
            // After 380ms, cleanly tear down the old background window on the main thread.
            std::thread([engine = m_engine, old_window]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(380));
                engine->post([old_window]() {
                    if (old_window) {
                        old_window->close();
                    }
                });
            }).detach();
        }
    }
}

void WallpaperManager::sync_outputs() {
    if (!m_engine) return;

    auto outputs = miqu::OutputManager::get()->get_outputs();
    std::set<uint32_t> current_output_ids;

    for (const auto& out : outputs) {
        current_output_ids.insert(out.id);

        if (m_windows.find(out.id) == m_windows.end()) {
            OutputConfig cfg = get_config_for_output(out.name);

            std::cout << "[miqubg] Creating wallpaper surface for output '"
                      << (out.name.empty() ? "(unknown)" : out.name) << "' [id=" << out.id << "]"
                      << (cfg.image_path.empty() ? " (solid color)" : (" image: " + cfg.image_path))
                      << std::endl;

            auto window = create_wallpaper_window(out, cfg);
            if (window) {
                m_windows[out.id] = window;
            } else {
                std::cerr << "[miqubg] Failed to create layer window for output " << out.id << std::endl;
            }
        }
    }

    // Clean up surfaces for removed outputs
    for (auto it = m_windows.begin(); it != m_windows.end();) {
        if (current_output_ids.find(it->first) == current_output_ids.end()) {
            std::cout << "[miqubg] Output disconnected [id=" << it->first << "], removing wallpaper surface." << std::endl;
            if (it->second) {
                it->second->close();
            }
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace miqubg
