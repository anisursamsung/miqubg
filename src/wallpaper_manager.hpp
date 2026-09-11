#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <map>
#include <string>
#include <memory>
#include <cstdint>

namespace miqubg {

struct OutputConfig {
    std::string image_path;
    miqu::FitMode mode = miqu::FitMode::Cover;
    miqu::Color bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);
};

class WallpaperManager {
public:
    WallpaperManager() = default;
    ~WallpaperManager() = default;

    void set_default_config(OutputConfig config) { m_default_config = std::move(config); }
    void set_output_config(const std::string& output_name, OutputConfig config) {
        m_output_configs[output_name] = std::move(config);
    }

    void init(miqu::AppEngine* engine);
    void sync_outputs();

    static miqu::FitMode parse_mode(const std::string& mode_str);

private:
    OutputConfig get_config_for_output(const std::string& name) const;

    miqu::AppEngine* m_engine = nullptr;
    OutputConfig m_default_config;
    std::map<std::string, OutputConfig> m_output_configs;
    std::map<uint32_t, std::shared_ptr<miqu::Window>> m_windows;
};

} // namespace miqubg
