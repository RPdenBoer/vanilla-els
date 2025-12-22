#ifndef LV_CONF_H
#define LV_CONF_H

/* Basic LVGL setup for embedded */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)

#define LV_TICK_CUSTOM 0

#define LV_USE_LABEL 1
#define LV_USE_BTN   1
#define LV_USE_LINE  1

/* Enable text selection in labels/textareas */
#define LV_LABEL_TEXT_SELECTION 1

/* Fonts */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_28  1
#define LV_FONT_MONTSERRAT_40  1
#define LV_FONT_MONTSERRAT_44  1
#define LV_FONT_MONTSERRAT_48  1


/* Optional: reduce features to keep compile fast */
#define LV_USE_LOG 0

#endif /*LV_CONF_H*/