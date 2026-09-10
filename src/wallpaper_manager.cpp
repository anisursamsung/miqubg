#include "wallpaper_manager.hpp"
#include <iostream>
#include <set>

namespace miqubg {

void WallpaperManager::init(miqu::AppEngine* engine) {
    m_engine = engine;

    miqu::OutputManager::get()->on_outputs_changed([this]() {
        sync_outputs();
    });

    sync_outputs();
}

OutputConfig WallpaperManager::get_config_for_output(const std::string& name) const {
    auto it = m_output_configs.find(name);
    if (it != m_output_configs.end()) {
        return it->second;
    }
    return m_default_config;
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

            auto view = std::make_shared<WallpaperView>(cfg.image_path, cfg.mode, cfg.bg_color);

            auto window = miqu::WindowBuilder::create()
                ->role(miqu::WindowRole::LayerBackground)
                ->output(out.wl_output)
                ->layerNamespace("wallpaper")
                ->keyboardInteractive(false)
                ->exclusiveZone(-1)
                ->contentView(view)
                ->build();

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
