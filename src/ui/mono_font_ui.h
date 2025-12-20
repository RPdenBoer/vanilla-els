#pragma once

#include <lvgl.h>

// Returns a pointer to a "monospaced" numeric font based on Montserrat 44.
// This keeps the glyph shapes, but forces equal advance widths for digits and
// common numeric punctuation so readouts line up nicely.
const lv_font_t * get_mono_numeric_font_44();
