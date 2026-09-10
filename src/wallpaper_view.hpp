#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string>
#include <memory>

namespace miqubg {

enum class WallpaperMode {
    Cover,   // Scales to fill entire bounds, cropped (default)
    Fit,     // Scales to fit inside bounds, preserving aspect ratio, letterboxed
    Stretch, // Stretches width & height to fill bounds
    Center,  // Centered at 1:1 scale
    Tile,    // Repeated pattern
};

class WallpaperView : public miqu::View {
public:
    WallpaperView(std::string image_path, WallpaperMode mode, miqu::Color bg_color);
    ~WallpaperView() override;

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;

    void set_image_path(std::string path);
    void set_mode(WallpaperMode mode) { m_mode = mode; }
    void set_bg_color(miqu::Color color) { m_bg_color = color; }

    static WallpaperMode parse_mode(const std::string& mode_str);

private:
    void reload_surface();

    std::string m_image_path;
    WallpaperMode m_mode = WallpaperMode::Cover;
    miqu::Color m_bg_color = miqu::Color::rgb(0.0f, 0.0f, 0.0f);

    cairo_surface_t* m_surface = nullptr;
};

} // namespace miqubg
