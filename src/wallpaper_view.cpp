#include "wallpaper_view.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqubg {

static cairo_surface_t* create_surface_from_pixbuf(GdkPixbuf* pixbuf) {
    if (!pixbuf) return nullptr;
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    const guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    unsigned char* c_data = cairo_image_surface_get_data(surface);
    int c_stride = cairo_image_surface_get_stride(surface);

    cairo_surface_flush(surface);
    for (int y = 0; y < height; ++y) {
        const guchar* src = pixels + y * stride;
        auto* dst = reinterpret_cast<uint32_t*>(c_data + y * c_stride);
        for (int x = 0; x < width; ++x) {
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];
            uint8_t a = (n_channels == 4) ? src[3] : 255;
            uint8_t pr = (r * a) / 255;
            uint8_t pg = (g * a) / 255;
            uint8_t pb = (b * a) / 255;
            dst[x] = (static_cast<uint32_t>(a) << 24) |
                     (static_cast<uint32_t>(pr) << 16) |
                     (static_cast<uint32_t>(pg) << 8) |
                     static_cast<uint32_t>(pb);
            src += n_channels;
        }
    }
    cairo_surface_mark_dirty(surface);
    return surface;
}

WallpaperView::WallpaperView(std::string image_path, WallpaperMode mode, miqu::Color bg_color)
    : m_image_path(std::move(image_path)), m_mode(mode), m_bg_color(bg_color)
{
    reload_surface();
}

WallpaperView::~WallpaperView() {
    if (m_surface) {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }
}

void WallpaperView::set_image_path(std::string path) {
    if (m_image_path != path) {
        m_image_path = std::move(path);
        reload_surface();
    }
}

void WallpaperView::reload_surface() {
    if (m_surface) {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }

    if (m_image_path.empty()) {
        return;
    }

    if (!fs::exists(m_image_path)) {
        std::cerr << "[miqubg] Warning: Wallpaper file does not exist: " << m_image_path << std::endl;
        return;
    }

    GError* err = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(m_image_path.c_str(), &err);
    if (!pixbuf) {
        std::cerr << "[miqubg] Failed to load image: " << m_image_path;
        if (err && err->message) {
            std::cerr << " (" << err->message << ")";
            g_error_free(err);
        }
        std::cerr << std::endl;
        return;
    }

    m_surface = create_surface_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
}

WallpaperMode WallpaperView::parse_mode(const std::string& mode_str) {
    std::string lower = mode_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "fit" || lower == "contain") return WallpaperMode::Fit;
    if (lower == "fill" || lower == "cover") return WallpaperMode::Cover;
    if (lower == "stretch") return WallpaperMode::Stretch;
    if (lower == "center") return WallpaperMode::Center;
    if (lower == "tile") return WallpaperMode::Tile;
    return WallpaperMode::Cover; // default
}

void WallpaperView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr || bounds.width <= 0 || bounds.height <= 0) return;

    // 1. Draw solid background color
    cairo_save(cr);
    cairo_set_source_rgba(cr, m_bg_color.r, m_bg_color.g, m_bg_color.b, m_bg_color.a);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_fill(cr);
    cairo_restore(cr);

    // 2. If no image loaded, solid fill is sufficient
    if (!m_surface) return;

    int img_w = cairo_image_surface_get_width(m_surface);
    int img_h = cairo_image_surface_get_height(m_surface);
    if (img_w <= 0 || img_h <= 0) return;

    cairo_save(cr);

    switch (m_mode) {
    case WallpaperMode::Cover: {
        double scale = std::max(static_cast<double>(bounds.width) / img_w,
                                static_cast<double>(bounds.height) / img_h);
        double dx = bounds.x + (bounds.width - img_w * scale) / 2.0;
        double dy = bounds.y + (bounds.height - img_h * scale) / 2.0;

        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_clip(cr);

        cairo_translate(cr, dx, dy);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, m_surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr);
        break;
    }
    case WallpaperMode::Fit: {
        double scale = std::min(static_cast<double>(bounds.width) / img_w,
                                static_cast<double>(bounds.height) / img_h);
        double dx = bounds.x + (bounds.width - img_w * scale) / 2.0;
        double dy = bounds.y + (bounds.height - img_h * scale) / 2.0;

        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_clip(cr);

        cairo_translate(cr, dx, dy);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, m_surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr);
        break;
    }
    case WallpaperMode::Stretch: {
        double sx = static_cast<double>(bounds.width) / img_w;
        double sy = static_cast<double>(bounds.height) / img_h;

        cairo_translate(cr, bounds.x, bounds.y);
        cairo_scale(cr, sx, sy);
        cairo_set_source_surface(cr, m_surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
        cairo_paint(cr);
        break;
    }
    case WallpaperMode::Center: {
        double dx = bounds.x + (bounds.width - img_w) / 2.0;
        double dy = bounds.y + (bounds.height - img_h) / 2.0;

        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_clip(cr);

        cairo_set_source_surface(cr, m_surface, dx, dy);
        cairo_paint(cr);
        break;
    }
    case WallpaperMode::Tile: {
        cairo_pattern_t* pattern = cairo_pattern_create_for_surface(m_surface);
        cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
        cairo_set_source(cr, pattern);
        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_fill(cr);
        cairo_pattern_destroy(pattern);
        break;
    }
    }

    cairo_restore(cr);
}

} // namespace miqubg
