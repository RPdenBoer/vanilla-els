#include "mono_font_ui.h"

#include <cstring>

namespace {

struct MonoFontCtx {
    const lv_font_t *base;
    uint16_t fixed_adv;
};

static MonoFontCtx g_ctx;
static lv_font_t g_mono_font;
static bool g_inited = false;

static bool is_numeric_mono_char(uint32_t letter) {
    // Keep decimals narrow: only force fixed width for digits and sign.
    return (letter >= '0' && letter <= '9') || letter == '-';
}

static bool mono_get_glyph_dsc(const lv_font_t *font,
                              lv_font_glyph_dsc_t *dsc_out,
                              uint32_t letter,
                              uint32_t /*letter_next*/) {
    const MonoFontCtx *ctx = static_cast<const MonoFontCtx *>(font->user_data);
    if(!ctx || !ctx->base) return false;

    // Disable kerning by always passing 0 for the next letter.
    if(!ctx->base->get_glyph_dsc(ctx->base, dsc_out, letter, 0)) return false;

    // Important: LVGL uses resolved_font for bitmap lookup.
    dsc_out->resolved_font = font;

    // Force fixed advance for numeric readouts and center the glyph within the block.
    if(is_numeric_mono_char(letter)) {
        const uint16_t original_adv = dsc_out->adv_w;
        const int16_t pad = (ctx->fixed_adv > original_adv) ? (int16_t)(ctx->fixed_adv - original_adv) : 0;

        dsc_out->adv_w = ctx->fixed_adv;
        dsc_out->ofs_x = (int16_t)(dsc_out->ofs_x + (pad / 2));
    }

    return true;
}

static uint16_t compute_fixed_adv(const lv_font_t *base) {
    uint16_t max_adv = 0;
    lv_font_glyph_dsc_t g;

    // Consider digits plus '-' to avoid jitter on sign changes.
    const char *chars = "0123456789-";
    for(const char *p = chars; *p; ++p) {
        if(base->get_glyph_dsc(base, &g, static_cast<uint32_t>(*p), 0)) {
            if(g.adv_w > max_adv) max_adv = g.adv_w;
        }
    }

    // Fallback: if something went wrong, keep the base font's typical digit width.
    if(max_adv == 0) {
        if(base->get_glyph_dsc(base, &g, static_cast<uint32_t>('0'), 0)) {
            max_adv = g.adv_w;
        }
    }

    return max_adv;
}

static void ensure_init() {
    if(g_inited) return;

    const lv_font_t *base = &lv_font_montserrat_44;
    g_ctx.base = base;
    g_ctx.fixed_adv = compute_fixed_adv(base);

    // Clone base font properties but override the glyph descriptor callback.
    g_mono_font = *base;
    g_mono_font.get_glyph_dsc = mono_get_glyph_dsc;

    // Ensure our font behaves consistently.
    g_mono_font.kerning = LV_FONT_KERNING_NONE;

    // The bitmap getter will use font->dsc; keep base->dsc.
    g_mono_font.dsc = base->dsc;

    // Store context so the callback can reach the base font + fixed advance.
    g_mono_font.user_data = &g_ctx;

    g_inited = true;
}

} // namespace

const lv_font_t * get_mono_numeric_font_44() {
    ensure_init();
    return &g_mono_font;
}
