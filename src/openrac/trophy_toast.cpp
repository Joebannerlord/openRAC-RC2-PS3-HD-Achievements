#include "trophy_toast.hpp"

#include <achievements/core.hpp>
#include <openrac/openrac_mod.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "toast_font.hpp"
#endif

namespace achievements::runtime {
namespace {

constexpr float kPictureWidth = 512.0F;
constexpr float kToastHeight = 40.0F;
constexpr float kToastRestY = 14.0F;
constexpr unsigned kIconPixels = 72;
constexpr float kIconHeight = 25.0F;
constexpr float kRasterScale = 3.0F;
constexpr float kHeadingSize = 8.5F;
constexpr float kTitleSize = 11.5F;
constexpr float kTextLimit = 214.0F;
constexpr char kCaption[] = u8"TROPHY EARNED  \u00b7";
constexpr float kSlideSeconds = 0.24F;
constexpr float kHoldSeconds = 2.30F;
constexpr float kFadeSeconds = 0.42F;
constexpr float kToastSeconds = kSlideSeconds + kHoldSeconds + kFadeSeconds;
constexpr float kMaxFrameAdvance = 0.10F;

struct Colour {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

// RAC2's launcher palette in launcher/src/lib/palettes.json.
constexpr Colour kPanel{29, 35, 39};
constexpr Colour kPanelHi{36, 43, 48};
constexpr Colour kAmber{49, 182, 232};
constexpr Colour kGold{163, 222, 245};

struct TierStyle {
    const char* label;
    Colour accent;
    Colour medallion;
};

TierStyle tier_style(Grade grade) {
    // Match the launcher collection's --tone values in ui/achievements.css.
    switch (grade) {
        case Grade::bronze: return {"BRONZE", {206, 147, 103}, {151, 99, 62}};
        case Grade::silver: return {"SILVER", {206, 208, 214}, {167, 178, 188}};
        case Grade::gold: return {"GOLD", {245, 192, 64}, {205, 161, 66}};
        case Grade::platinum: return {"PLATINUM", {169, 218, 213}, {153, 123, 190}};
    }
    return {"TROPHY", kAmber, {167, 178, 188}};
}

Colour tile_fill(Colour tier) {
    // Blend 40% of the tier colour into charcoal to keep the artwork legible.
    const auto shade = [](unsigned channel) { return static_cast<std::uint8_t>((3 * 24 + 2 * channel + 2) / 5); };
    return {shade(tier.red), shade(tier.green), shade(tier.blue)};
}

std::uint32_t rgba(Colour colour, float opacity) {
    const auto alpha = static_cast<std::uint8_t>(std::clamp(opacity, 0.0F, 1.0F) * 255.0F);
    return (static_cast<std::uint32_t>(colour.red) << 24U)
        | (static_cast<std::uint32_t>(colour.green) << 16U)
        | (static_cast<std::uint32_t>(colour.blue) << 8U)
        | alpha;
}

std::uint32_t rgba(std::uint32_t colour, float opacity) {
    const auto alpha = static_cast<std::uint32_t>(std::clamp(opacity, 0.0F, 1.0F) * 255.0F);
    return (colour & 0xffffff00U) | alpha;
}

float ease_out(float progress) {
    const float inverse = 1.0F - std::clamp(progress, 0.0F, 1.0F);
    return 1.0F - inverse * inverse * inverse;
}

float text_width(const openrac_overlay& overlay, float height, const char* text) {
    if (overlay.text_width != nullptr) return overlay.text_width(overlay.context, height, text);
    return static_cast<float>(std::char_traits<char>::length(text)) * height * 0.56F;
}

std::string fitted_text(const openrac_overlay& overlay, float height, const char* text, float limit) {
    std::string fitted(text);
    if (text_width(overlay, height, fitted.c_str()) <= limit) return fitted;
    constexpr char ellipsis[] = "...";
    while (!fitted.empty() && text_width(overlay, height, (fitted + ellipsis).c_str()) > limit) {
        fitted.pop_back();
    }
    return fitted.empty() ? std::string(ellipsis) : fitted + ellipsis;
}

struct PixelRun {
    std::uint8_t x;
    std::uint8_t y;
    std::uint8_t width;
    std::uint32_t colour;
};

struct Icon {
    std::vector<PixelRun> runs;
    bool attempted = false;
};

struct GlyphRun {
    unsigned x;
    unsigned y;
    unsigned width;
    std::uint8_t alpha;
};

struct TextRaster {
    std::vector<GlyphRun> runs;
    float width = 0.0F;
    float height = 0.0F;
};

#ifdef _WIN32
struct ComScope {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope() {
        if (SUCCEEDED(initialized)) CoUninitialize();
    }
};

TextRaster raster_text(const char* text, float size, DWRITE_FONT_WEIGHT weight,
                       const ToastFont* brand_font = nullptr) {
    using Microsoft::WRL::ComPtr;
    const ComScope com;
    if (FAILED(com.initialized) && com.initialized != RPC_E_CHANGED_MODE) return {};
    ComPtr<IWICImagingFactory> wic;
    ComPtr<ID2D1Factory> d2d;
    ComPtr<IDWriteFactory> dwrite;
    ComPtr<IDWriteTextFormat> format;
    ComPtr<IDWriteTextLayout> layout;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring wide(count, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide.data(), count)) return {};

    const float limit = kTextLimit * kRasterScale;
    const float max_height = 24 * kRasterScale;
    IDWriteFontCollection* collection = brand_font ? brand_font->collection() : nullptr;
    if (collection) {
        dwrite = brand_font->factory();
    } else if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                         reinterpret_cast<IUnknown**>(dwrite.GetAddressOf())))) {
        return {};
    }
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(wic.GetAddressOf())))
        || FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf()))
        || FAILED(dwrite->CreateTextFormat(collection ? L"Audiowide" : L"Segoe UI", collection,
                                           collection ? DWRITE_FONT_WEIGHT_NORMAL : weight, DWRITE_FONT_STYLE_NORMAL,
                                           DWRITE_FONT_STRETCH_NORMAL, size * kRasterScale, L"en-us",
                                           format.GetAddressOf()))
        || FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP))
        || FAILED(dwrite->CreateTextLayout(wide.c_str(), static_cast<UINT32>(count - 1), format.Get(),
                                           limit, max_height, layout.GetAddressOf()))) return {};

    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout->GetMetrics(&metrics))) return {};
    if (metrics.width > limit) {
        const float fit = size * kRasterScale * (limit - 2) / metrics.width;
        if (FAILED(layout->SetFontSize(fit, {0, static_cast<UINT32>(count - 1)}))
            || FAILED(layout->GetMetrics(&metrics))) return {};
    }
    constexpr UINT pad = 2;
    const auto width = static_cast<UINT>(std::ceil(metrics.width)) + 2 * pad;
    const auto height = static_cast<UINT>(std::ceil(metrics.height)) + 2 * pad;
    ComPtr<IWICBitmap> bitmap;
    ComPtr<ID2D1RenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> white;
    const auto params = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    if (FAILED(wic->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA,
                                 WICBitmapCacheOnLoad, bitmap.GetAddressOf()))
        || FAILED(d2d->CreateWicBitmapRenderTarget(bitmap.Get(), params, target.GetAddressOf()))
        || FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white.GetAddressOf()))) return {};

    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    target->DrawTextLayout(D2D1::Point2F(pad, pad), layout.Get(), white.Get(),
                           D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
    if (FAILED(target->EndDraw())) return {};
    white.Reset();
    target.Reset();
    std::vector<std::uint8_t> pixels(width * height * 4U);
    if (FAILED(bitmap->CopyPixels(nullptr, width * 4U, static_cast<UINT>(pixels.size()), pixels.data()))) return {};

    TextRaster raster;
    raster.width = static_cast<float>(width) / kRasterScale;
    raster.height = static_cast<float>(height) / kRasterScale;
    for (UINT y = 0; y < height; ++y) {
        const auto alpha_at = [&](UINT x) { return pixels[(width * y + x) * 4U + 3U]; };
        for (UINT x = 0; x < width;) {
            const auto alpha = alpha_at(x);
            if (alpha == 0) {
                ++x;
                continue;
            }
            UINT end = x + 1;
            while (end < width && alpha_at(end) == alpha) ++end;
            raster.runs.push_back({x, y, end - x, alpha});
            x = end;
        }
    }
    return raster;
}

void decode_png_icon(const std::filesystem::path& path, Icon& icon) {
    using Microsoft::WRL::ComPtr;
    const ComScope com;
    if (FAILED(com.initialized) && com.initialized != RPC_E_CHANGED_MODE) return;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;
    ComPtr<IWICFormatConverter> converter;

    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(factory.GetAddressOf())))
        && SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf()))
        && SUCCEEDED(decoder->GetFrame(0, frame.GetAddressOf()))
        && SUCCEEDED(factory->CreateBitmapScaler(scaler.GetAddressOf()))
        && SUCCEEDED(scaler->Initialize(frame.Get(), kIconPixels, kIconPixels, WICBitmapInterpolationModeFant))
        && SUCCEEDED(factory->CreateFormatConverter(converter.GetAddressOf()))
        && SUCCEEDED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA,
                                           WICBitmapDitherTypeNone, nullptr, 0.0,
                                           WICBitmapPaletteTypeCustom))) {
        std::array<std::uint8_t, kIconPixels * kIconPixels * 4U> pixels{};
        if (SUCCEEDED(converter->CopyPixels(nullptr, kIconPixels * 4U,
                                            static_cast<UINT>(pixels.size()), pixels.data()))) {
            for (std::uint8_t y = 0; y < kIconPixels; ++y) {
                for (std::uint8_t x = 0; x < kIconPixels;) {
                    const std::size_t at = (static_cast<std::size_t>(y) * kIconPixels + x) * 4U;
                    const std::uint8_t alpha = pixels[at + 3U];
                    if (alpha == 0) {
                        ++x;
                        continue;
                    }
                    const std::uint32_t colour = (static_cast<std::uint32_t>(pixels[at]) << 24U)
                        | (static_cast<std::uint32_t>(pixels[at + 1U]) << 16U)
                        | (static_cast<std::uint32_t>(pixels[at + 2U]) << 8U)
                        | alpha;
                    std::uint8_t end = static_cast<std::uint8_t>(x + 1U);
                    while (end < kIconPixels) {
                        const std::size_t next = (static_cast<std::size_t>(y) * kIconPixels + end) * 4U;
                        const std::uint32_t next_colour =
                            (static_cast<std::uint32_t>(pixels[next]) << 24U)
                            | (static_cast<std::uint32_t>(pixels[next + 1U]) << 16U)
                            | (static_cast<std::uint32_t>(pixels[next + 2U]) << 8U)
                            | pixels[next + 3U];
                        if (next_colour != colour) break;
                        ++end;
                    }
                    icon.runs.push_back({x, y, static_cast<std::uint8_t>(end - x), colour});
                    x = end;
                }
            }
        }
    }

}
#endif

void draw_fallback_medallion(const openrac_overlay& overlay, float x, float y, float scale,
                             const TierStyle& tier, float opacity) {
    const auto disk = [&](float radius, Colour colour) {
        const float middle = kIconHeight / 2.0F;
        constexpr float step = 1.0F / kRasterScale;
        for (float row = -radius; row < radius; row += step) {
            const float height = std::min(step, radius - row);
            const float offset = row + height / 2.0F;
            const float half = std::sqrt(std::max(0.0F, radius * radius - offset * offset));
            overlay.rect(overlay.context, x + (middle - half) * scale, y + middle + row,
                         2 * half * scale, height, rgba(colour, opacity));
        }
    };
    disk(10.0F, tier.accent);
    disk(8.7F, {26, 30, 38});
    disk(6.5F, tier.medallion);
}

void rounded_panel(const openrac_overlay& overlay, float x, float y, float width, float height,
                   float scale, float radius, std::uint32_t colour) {
    // A continuous body prevents gaps at output pixel boundaries.
    constexpr float seam = 0.002F;
    const auto rows = static_cast<int>(std::ceil(radius * kRasterScale));
    const float step = radius / static_cast<float>(rows);
    if (height > 2 * radius) {
        overlay.rect(overlay.context, x, y + radius, width, height - 2 * radius, colour);
    }
    for (int row = 0; row < rows; ++row) {
        const float top = static_cast<float>(row) * step;
        const float corner = radius - (top + step * 0.5F);
        const float inset = (radius - std::sqrt(radius * radius - corner * corner)) * scale;
        const float band = step + seam;
        overlay.rect(overlay.context, x + inset, y + top, width - 2 * inset, band, colour);
        overlay.rect(overlay.context, x + inset, y + height - top - band, width - 2 * inset, band, colour);
    }
}

} // namespace

struct TrophyToast::State {
    struct Entry {
        std::size_t index;
        bool preview;
    };

    std::filesystem::path icon_directory;
    std::deque<Entry> pending;
    std::optional<Entry> active;
    std::unordered_map<std::size_t, Icon> icons;
    std::unordered_map<std::size_t, TextRaster> titles;
    std::optional<TextRaster> caption;
    std::unordered_map<Grade, TextRaster> tier_labels;
#ifdef _WIN32
    std::unique_ptr<ToastFont> brand_font;
#endif
    std::chrono::steady_clock::time_point last_draw{};
    float shown_seconds = 0.0F;
};

TrophyToast::TrophyToast(std::filesystem::path icon_directory)
    : state_(std::make_unique<State>()) {
    state_->icon_directory = std::move(icon_directory);
#ifdef _WIN32
    if (!state_->icon_directory.empty()) {
        state_->brand_font = std::make_unique<ToastFont>(
            state_->icon_directory.parent_path() / "audiowide-latin-400-normal.woff2");
    }
#endif
}

TrophyToast::~TrophyToast() = default;
TrophyToast::TrophyToast(TrophyToast&&) noexcept = default;
TrophyToast& TrophyToast::operator=(TrophyToast&&) noexcept = default;

void TrophyToast::enqueue(std::size_t trophy_index, bool preview) {
    const auto& trophies = catalog();
    if (trophy_index >= trophies.size()) return;

    const auto is_preview = [](const State::Entry& entry) { return entry.preview; };
    if (preview && ((state_->active && is_preview(*state_->active))
                    || std::any_of(state_->pending.begin(), state_->pending.end(), is_preview))) {
        return;
    }
    if (state_->pending.size() >= trophies.size()) return;
    state_->pending.push_back({trophy_index, preview});
}

void TrophyToast::clear() {
    state_->pending.clear();
    state_->active.reset();
    state_->shown_seconds = 0.0F;
}

bool TrophyToast::draw(const openrac_overlay& overlay, std::chrono::steady_clock::time_point now) {
    if (overlay.rect == nullptr || overlay.text == nullptr) return false;
    const bool started = !state_->active;
    if (started) {
        if (state_->pending.empty()) return false;
        state_->active = state_->pending.front();
        state_->pending.pop_front();
        state_->shown_seconds = 0.0F;
    } else {
        const float elapsed = std::chrono::duration<float>(now - state_->last_draw).count();
        state_->shown_seconds += std::clamp(elapsed, 0.0F, kMaxFrameAdvance);
    }
    state_->last_draw = now;
    if (state_->shown_seconds >= kToastSeconds) {
        state_->active.reset();
        return false;
    }

    const Trophy& trophy = catalog()[state_->active->index];
    const TierStyle tier = tier_style(trophy.grade);
    const float slide = ease_out(state_->shown_seconds / kSlideSeconds);
    const float y = kToastRestY - (1.0F - slide) * 28.0F;
    const float fade_start = kSlideSeconds + kHoldSeconds;
    const float opacity = state_->shown_seconds <= fade_start
        ? 1.0F
        : 1.0F - (state_->shown_seconds - fade_start) / kFadeSeconds;

    // The game scales logical X and Y independently. A cell in the
    // overlay's 6x8 font stays square, so it tells us the X compensation.
    const float scale = std::clamp(text_width(overlay, 8.0F, "M") / 6.0F, 0.25F, 1.2F);
    if (!state_->caption) {
        state_->caption.emplace();
#ifdef _WIN32
        *state_->caption = raster_text(kCaption, kHeadingSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);
#endif
    }
    auto [tier_it, new_tier] = state_->tier_labels.try_emplace(trophy.grade);
    auto [title_it, new_title] = state_->titles.try_emplace(state_->active->index);
#ifdef _WIN32
    if (new_tier) tier_it->second = raster_text(tier.label, kHeadingSize, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    if (new_title) {
        title_it->second = raster_text(trophy.name, kTitleSize, DWRITE_FONT_WEIGHT_SEMI_BOLD, state_->brand_font.get());
    }
#else
    (void)new_tier;
    (void)new_title;
#endif
    const auto& caption = *state_->caption;
    const auto& tier_label = tier_it->second;
    const auto& title = title_it->second;
    const auto measured_width = [&](const TextRaster& raster, float size, const char* content) {
        return raster.runs.empty() ? text_width(overlay, size, content) : raster.width * scale;
    };
    const float title_width = measured_width(title, kTitleSize, trophy.name);
    const float tier_offset = measured_width(caption, kHeadingSize, kCaption) + 3.0F * scale;
    const float caption_width = tier_offset + measured_width(tier_label, kHeadingSize, tier.label);
    constexpr float text_left = 46.0F;
    constexpr float padding = 57.0F;
    const float width = std::clamp(padding * scale + std::max(title_width, caption_width),
                                   163.0F * scale, (padding + kTextLimit) * scale);
    const float x = (kPictureWidth - width) / 2.0F;
    const auto rectangle = [&](float left, float top, float w, float h, std::uint32_t color) {
        overlay.rect(overlay.context, x + left, y + top, w, h, color);
    };
    constexpr float radius = 5.5F;
    constexpr float rim = .65F;
    rounded_panel(overlay, x, y + 1.4F, width, kToastHeight, scale, radius,
                  rgba({18, 18, 18}, opacity * .45F));
    rounded_panel(overlay, x, y, width, kToastHeight, scale, radius, rgba(kAmber, opacity * .58F));
    rounded_panel(overlay, x + rim * scale, y + rim, width - 2 * rim * scale, kToastHeight - 2 * rim,
                  scale, radius - rim, rgba(kPanel, opacity * .98F));

    constexpr float tile = 31.0F;
    constexpr float tile_left = 4.5F;
    constexpr float tile_top = (kToastHeight - tile) / 2.0F;
    rounded_panel(overlay, x + tile_left * scale, y + tile_top, tile * scale, tile, scale, 4.4F,
                  rgba(tier.accent, opacity * .82F));
    constexpr float tile_rim = .55F;
    rounded_panel(overlay, x + (tile_left + tile_rim) * scale, y + tile_top + tile_rim,
                  (tile - 2 * tile_rim) * scale, tile - 2 * tile_rim, scale, 4.4F - tile_rim,
                  rgba(tile_fill(tier.accent), opacity));
    rectangle(40.0F * scale, 9.0F, .4F * scale, 22.0F, rgba(kPanelHi, opacity));

    Icon& icon = state_->icons[state_->active->index];
    if (!icon.attempted) {
        icon.attempted = true;
#ifdef _WIN32
        if (!state_->icon_directory.empty()) {
            decode_png_icon(state_->icon_directory / (std::string(trophy.id) + ".png"), icon);
        }
#endif
    }
    const float icon_x = x + (tile_left + (tile - kIconHeight) / 2.0F) * scale;
    const float icon_y = y + (kToastHeight - kIconHeight) / 2.0F;
    if (icon.runs.empty()) {
        draw_fallback_medallion(overlay, icon_x, icon_y, scale, tier, opacity);
    } else {
        constexpr float pixel = kIconHeight / kIconPixels;
        for (const PixelRun& run : icon.runs) {
            overlay.rect(overlay.context, icon_x + run.x * pixel * scale,
                         icon_y + run.y * pixel, run.width * pixel * scale, pixel,
                         rgba(run.colour, opacity * (static_cast<float>(run.colour & 0xffU) / 255.0F)));
        }
    }

    const auto line = [&](const TextRaster& raster, float left, float center, float size, Colour color,
                          const char* content) {
        if (raster.runs.empty()) {
            const auto fitted = fitted_text(overlay, size, content, width - left - (padding - text_left) * scale);
            overlay.text(overlay.context, x + left, y + center - size / 2.0F, size,
                         rgba(color, opacity), fitted.c_str());
            return;
        }
        const float top = center - raster.height / 2.0F;
        for (const GlyphRun& run : raster.runs) {
            rectangle(left + run.x * scale / kRasterScale, top + run.y / kRasterScale,
                      run.width * scale / kRasterScale, 1.0F / kRasterScale,
                      rgba(color, opacity * (static_cast<float>(run.alpha) / 255.0F)));
        }
    };
    const float left = text_left * scale;
    line(caption, left, 12.5F, kHeadingSize, kAmber, kCaption);
    line(tier_label, left + tier_offset, 12.5F, kHeadingSize, tier.accent, tier.label);
    line(title, left, 26.0F, kTitleSize, kGold, trophy.name);
    return started;
}

} // namespace achievements::runtime
