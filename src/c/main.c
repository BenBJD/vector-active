#include <pebble.h>

// Vector Active — Pixel Watch "Active" watchface for color Pebble watches (gabbro, chalk, emery, basalt).
// Supports the Pebble Complications / Plugins SDK (preferences/complications + custom slot mapping),
// 8 color themes, font selection, and Pebble Health / Battery / Date built-in slots.

#define TEXT_MAX 12
#define LABEL_MAX 14
#define IMAGE_SIZE_MAX 48
#define NO_FILL -1

#define SLOT_RING_TOP 0
#define SLOT_RING_MID 1
#define SLOT_RING_BOT 2
#define SLOT_BADGE_TOP 3
#define SLOT_BADGE_BOT 4
#define SLOT_COUNT 5

#define PERSIST_SLOTS 1
#define PERSIST_SETTINGS 2

typedef enum {
  ICON_NONE = 0,
  ICON_STEPS,
  ICON_HEART,
  ICON_SUN,
  ICON_CLOUD,
  ICON_RAIN,
  ICON_BATTERY,
  ICON_FLAME,
  ICON_PIN,
  ICON_CALENDAR
} IconHint;

typedef struct {
  bool is_image;
  char text[TEXT_MAX];
  char label[LABEL_MAX];
  int8_t fill;       // -1 = no gauge, 0..100 = percentage
  uint8_t icon_hint; // IconHint
} Slot;

typedef struct {
  uint8_t theme;
  bool show_dot;
  bool leading_zero;
  uint8_t time_font; // 0 = LECO (Geometric), 1 = Gothic (Rounded)
  bool stacked_time; // false = single line (HH:MM), true = two lines (HH\nMM)
} Settings;

_Static_assert(sizeof(Slot) * SLOT_COUNT <= PERSIST_DATA_MAX_LENGTH, "slots must fit persist");
_Static_assert(sizeof(Settings) <= PERSIST_DATA_MAX_LENGTH, "settings must fit persist");

typedef struct {
  GColor bg;
  GColor surface;
  GColor track;
  GColor accent1;
  GColor accent2;
  GColor accent3;
  GColor text;
} ThemePalette;

static ThemePalette get_theme(uint8_t idx) {
  switch (idx) {
    case 1: // Sand
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorWindsorTan, .track = GColorDarkGray,
        .accent1 = GColorMelon, .accent2 = GColorPastelYellow, .accent3 = GColorWhite, .text = GColorWhite
      };
    case 2: // Ash
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorDarkGray, .track = GColorDarkGray,
        .accent1 = GColorLightGray, .accent2 = GColorWhite, .accent3 = GColorLightGray, .text = GColorWhite
      };
    case 3: // Sage
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorDarkGreen, .track = GColorDarkGreen,
        .accent1 = GColorMintGreen, .accent2 = GColorMayGreen, .accent3 = GColorPastelYellow, .text = GColorWhite
      };
    case 4: // Limoncello
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorArmyGreen, .track = GColorArmyGreen,
        .accent1 = GColorPastelYellow, .accent2 = GColorYellow, .accent3 = GColorWhite, .text = GColorWhite
      };
    case 5: // Sky
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorOxfordBlue, .track = GColorOxfordBlue,
        .accent1 = GColorVividCerulean, .accent2 = GColorPictonBlue, .accent3 = GColorCeleste, .text = GColorWhite
      };
    case 6: // Periwinkle
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorOxfordBlue, .track = GColorOxfordBlue,
        .accent1 = GColorBabyBlueEyes, .accent2 = GColorLavenderIndigo, .accent3 = GColorRichBrilliantLavender, .text = GColorWhite
      };
    case 7: // Flamingo
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorImperialPurple, .track = GColorImperialPurple,
        .accent1 = GColorShockingPink, .accent2 = GColorMagenta, .accent3 = GColorRichBrilliantLavender, .text = GColorWhite
      };
    case 0: // Campari (default)
    default:
      return (ThemePalette){
        .bg = GColorBlack, .surface = GColorBulgarianRose, .track = GColorBulgarianRose,
        .accent1 = GColorFolly, .accent2 = GColorSunsetOrange, .accent3 = GColorSpringBud, .text = GColorMelon
      };
  }
}

static Window *s_window;
static Layer *s_canvas;
static Slot s_slots[SLOT_COUNT];
static GBitmap *s_images[SLOT_COUNT];
static Settings s_settings = {
  .theme = 0,
  .show_dot = true,
  .leading_zero = true,
  .time_font = 0,
  .stacked_time = false,
};
static char s_time[8];
static char s_date_day[8];

// ---------------------------------------------------------------- layout

typedef struct {
  bool is_rect;
  GRect time_pill;
  int time_radius;
  GRect rings[3];
  int ring_thickness[3];
  GRect badges[2];
} Layout;

static Layout layout_for(GRect bounds) {
  Layout l;
  if (bounds.size.w >= 240) {
    // Gabbro (260x260 round)
    l.is_rect = false;
    l.time_pill = s_settings.stacked_time ? GRect(24, 72, 94, 116) : GRect(24, 108, 116, 44);
    l.time_radius = s_settings.stacked_time ? 28 : 22;
    l.rings[0] = GRect(123, 36, 64, 64);
    l.rings[1] = GRect(156, 94, 72, 72);
    l.rings[2] = GRect(123, 160, 64, 64);
    l.ring_thickness[0] = 6;
    l.ring_thickness[1] = 7;
    l.ring_thickness[2] = 6;
    l.badges[0] = GRect(62, 53, 42, 42);
    l.badges[1] = GRect(62, 165, 42, 42);
  } else if (bounds.size.w >= 195) {
    // Emery (200x228 rect)
    l.is_rect = true;
    l.time_pill = s_settings.stacked_time ? GRect(10, 34, 116, 160) : GRect(6, 84, 126, 58);
    l.time_radius = s_settings.stacked_time ? 28 : 24;
    l.rings[0] = GRect(138, 10, 58, 58);
    l.rings[1] = GRect(142, 87, 54, 54);
    l.rings[2] = GRect(138, 160, 58, 58);
    l.ring_thickness[0] = 4;
    l.ring_thickness[1] = 4;
    l.ring_thickness[2] = 4;
    l.badges[0] = GRect(48, 18, 42, 42);
    l.badges[1] = GRect(48, 168, 42, 42);
  } else if (bounds.size.w >= 175) {
    // Chalk (180x180 round)
    l.is_rect = false;
    l.time_pill = s_settings.stacked_time ? GRect(16, 48, 64, 84) : GRect(14, 74, 82, 32);
    l.time_radius = s_settings.stacked_time ? 20 : 16;
    l.rings[0] = GRect(84, 23, 44, 44);
    l.rings[1] = GRect(106, 64, 52, 52);
    l.rings[2] = GRect(84, 113, 44, 44);
    l.ring_thickness[0] = 4;
    l.ring_thickness[1] = 5;
    l.ring_thickness[2] = 4;
    l.badges[0] = GRect(40, 34, 30, 30);
    l.badges[1] = GRect(40, 116, 30, 30);
  } else {
    // Basalt (144x168 rect)
    l.is_rect = true;
    l.time_pill = s_settings.stacked_time ? GRect(6, 24, 84, 120) : GRect(4, 64, 90, 40);
    l.time_radius = s_settings.stacked_time ? 22 : 18;
    l.rings[0] = GRect(97, 8, 44, 44);
    l.rings[1] = GRect(101, 64, 40, 40);
    l.rings[2] = GRect(97, 116, 44, 44);
    l.ring_thickness[0] = 3;
    l.ring_thickness[1] = 3;
    l.ring_thickness[2] = 3;
    l.badges[0] = GRect(33, 12, 32, 32);
    l.badges[1] = GRect(33, 124, 32, 32);
  }
  return l;
}

static int ring_image_size(Layout *l) {
  int s = l->rings[0].size.w / 3;
  return s > IMAGE_SIZE_MAX ? IMAGE_SIZE_MAX : s;
}

// ---------------------------------------------------------------- vector icons

static void draw_vector_icon(GContext *ctx, GPoint c, int r, IconHint hint, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_width(ctx, r >= 7 ? 2 : 1);

  switch (hint) {
    case ICON_HEART: {
      int d = r / 2;
      graphics_fill_circle(ctx, GPoint(c.x - d / 2 - 1, c.y - 1), d / 2 + 1);
      graphics_fill_circle(ctx, GPoint(c.x + d / 2 + 1, c.y - 1), d / 2 + 1);
      GPoint pts[3] = {
        GPoint(c.x - d - 1, c.y),
        GPoint(c.x + d + 1, c.y),
        GPoint(c.x, c.y + d + 1)
      };
      GPathInfo info = { .num_points = 3, .points = pts };
      GPath *p = gpath_create(&info);
      gpath_draw_filled(ctx, p);
      gpath_destroy(p);
      break;
    }
    case ICON_SUN: {
      int sr = r / 2;
      graphics_draw_circle(ctx, c, sr);
      for (int i = 0; i < 8; i++) {
        int32_t a = TRIG_MAX_ANGLE * i / 8;
        GPoint p1 = GPoint(c.x + (sin_lookup(a) * (sr + 2)) / TRIG_MAX_RATIO,
                           c.y - (cos_lookup(a) * (sr + 2)) / TRIG_MAX_RATIO);
        GPoint p2 = GPoint(c.x + (sin_lookup(a) * (r)) / TRIG_MAX_RATIO,
                           c.y - (cos_lookup(a) * (r)) / TRIG_MAX_RATIO);
        graphics_draw_line(ctx, p1, p2);
      }
      break;
    }
    case ICON_CLOUD:
    case ICON_RAIN: {
      int d = r / 2;
      graphics_fill_circle(ctx, GPoint(c.x - d / 2, c.y), d / 2 + 1);
      graphics_fill_circle(ctx, GPoint(c.x + d / 3, c.y - 1), d / 2 + 2);
      graphics_fill_rect(ctx, GRect(c.x - d, c.y, d * 2, d / 2 + 1), 2, GCornersAll);
      if (hint == ICON_RAIN) {
        graphics_draw_line(ctx, GPoint(c.x - d / 2, c.y + d), GPoint(c.x - d / 2 - 1, c.y + d + 2));
        graphics_draw_line(ctx, GPoint(c.x + d / 2, c.y + d), GPoint(c.x + d / 2 - 1, c.y + d + 2));
      }
      break;
    }
    case ICON_STEPS: {
      int w = r >= 7 ? 3 : 2;
      graphics_fill_rect(ctx, GRect(c.x - w - 1, c.y - r / 2, w, r - 2), 1, GCornersAll);
      graphics_fill_rect(ctx, GRect(c.x + 1, c.y - r / 2 + 2, w, r - 2), 1, GCornersAll);
      break;
    }
    case ICON_BATTERY: {
      int bw = r + 2;
      int bh = r;
      graphics_draw_rect(ctx, GRect(c.x - bw / 2, c.y - bh / 2, bw, bh));
      graphics_fill_rect(ctx, GRect(c.x + bw / 2, c.y - bh / 4, 2, bh / 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(c.x - bw / 2 + 2, c.y - bh / 2 + 2, bw - 4, bh - 4), 0, GCornerNone);
      break;
    }
    case ICON_FLAME: {
      int d = r / 2;
      graphics_fill_circle(ctx, GPoint(c.x, c.y + 1), d);
      GPoint pts[3] = {
        GPoint(c.x - d, c.y + 1),
        GPoint(c.x + d, c.y + 1),
        GPoint(c.x, c.y - d - 2)
      };
      GPathInfo info = { .num_points = 3, .points = pts };
      GPath *p = gpath_create(&info);
      gpath_draw_filled(ctx, p);
      gpath_destroy(p);
      break;
    }
    case ICON_PIN: {
      int d = r / 2;
      graphics_draw_circle(ctx, GPoint(c.x, c.y - 1), d);
      graphics_draw_line(ctx, GPoint(c.x - d + 1, c.y + 1), GPoint(c.x, c.y + d + 2));
      graphics_draw_line(ctx, GPoint(c.x + d - 1, c.y + 1), GPoint(c.x, c.y + d + 2));
      break;
    }
    case ICON_CALENDAR: {
      int s = r >= 6 ? r + 2 : r + 1;
      graphics_draw_rect(ctx, GRect(c.x - s / 2, c.y - s / 2, s, s));
      graphics_fill_rect(ctx, GRect(c.x - s / 2, c.y - s / 2, s, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(c.x - 1, c.y, 2, 2), 0, GCornerNone);
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------- drawing

static GFont font_for_ring(int w, bool is_rect, int *out_h) {
  if (is_rect) {
    if (w >= 52) { *out_h = 26; return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD); }
    *out_h = 20;
    return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  }
  if (w >= 68) { *out_h = 28; return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD); }
  if (w >= 56) { *out_h = 22; return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD); }
  *out_h = 18;
  return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
}

static bool is_slot_empty(const Slot *slot) {
  return !slot->is_image && slot->text[0] == '\0' && slot->icon_hint == ICON_NONE;
}

static void draw_ring_slot(GContext *ctx, GRect rect, int thickness, const Slot *slot,
                           GBitmap *image, const ThemePalette *pal, bool is_mid, bool is_rect) {
  if (is_slot_empty(slot)) return;
  // 300-degree arc open at the bottom (from 210 deg clockwise through 0 to 150 deg)
  int32_t start_angle = DEG_TO_TRIGANGLE(210);
  int32_t span_angle = DEG_TO_TRIGANGLE(300);

  // Track
  graphics_context_set_fill_color(ctx, pal->track);
  graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle, thickness,
                       start_angle, start_angle + span_angle);

  // Progress fill
  int fill = slot->fill;
  if (fill > 100) fill = 100;
  if (fill > 0) {
    int32_t fill_span = span_angle * fill / 100;
    graphics_context_set_fill_color(ctx, pal->accent1);
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle, thickness,
                         start_angle, start_angle + fill_span);

    if (s_settings.show_dot && fill < 100) {
      int32_t end_angle = start_angle + fill_span;
      GPoint center = grect_center_point(&rect);
      int r = rect.size.w / 2 - thickness / 2;
      GPoint dot = GPoint(center.x + (sin_lookup(end_angle) * r) / TRIG_MAX_RATIO,
                          center.y - (cos_lookup(end_angle) * r) / TRIG_MAX_RATIO);
      graphics_context_set_fill_color(ctx, pal->accent2);
      graphics_fill_circle(ctx, dot, thickness / 2 + 1);
    }
  } else if (fill == NO_FILL) {
    // Subtle partial highlight if no range is provided
    graphics_context_set_fill_color(ctx, pal->accent1);
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle, thickness,
                         start_angle, start_angle + span_angle);
  }

  GPoint center = grect_center_point(&rect);
  if (slot->is_image && image) {
    GSize sz = gbitmap_get_bounds(image).size;
    GRect frame = GRect(center.x - sz.w / 2, center.y - sz.h / 2 - 2, sz.w, sz.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, image, frame);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  } else {
    int text_h = 18;
    GFont font = font_for_ring(rect.size.w, is_rect, &text_h);
    int pad = is_rect ? 2 : thickness;
    graphics_context_set_text_color(ctx, is_mid ? pal->accent2 : pal->text);
    graphics_draw_text(ctx, slot->text, font,
                       GRect(rect.origin.x + pad, center.y - text_h / 2 - 3,
                             rect.size.w - pad * 2, text_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Bottom gap icon or short label
  GPoint icon_pos = GPoint(center.x, rect.origin.y + rect.size.h - thickness / 2 - 1);
  if (slot->icon_hint != ICON_NONE) {
    int icon_r = rect.size.w >= 60 ? 6 : 4;
    draw_vector_icon(ctx, icon_pos, icon_r, (IconHint)slot->icon_hint, pal->accent2);
  } else if (slot->label[0] != '\0') {
    graphics_context_set_text_color(ctx, pal->accent2);
    graphics_draw_text(ctx, slot->label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(rect.origin.x, icon_pos.y - 9, rect.size.w, 14),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void draw_badge_slot(GContext *ctx, GRect rect, const Slot *slot,
                            GBitmap *image, const ThemePalette *pal) {
  if (is_slot_empty(slot)) return;
  GPoint center = grect_center_point(&rect);
  graphics_context_set_fill_color(ctx, pal->surface);
  graphics_fill_circle(ctx, center, rect.size.w / 2);

  if (slot->is_image && image) {
    GSize sz = gbitmap_get_bounds(image).size;
    GRect frame = GRect(center.x - sz.w / 2, center.y - sz.h / 2, sz.w, sz.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, image, frame);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  } else if (slot->text[0] != '\0') {
    graphics_context_set_text_color(ctx, pal->text);
    graphics_draw_text(ctx, slot->text,
                       fonts_get_system_font(rect.size.w >= 40 ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_14),
                       GRect(rect.origin.x + 2, center.y - 9, rect.size.w - 4, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else if (slot->icon_hint != ICON_NONE) {
    draw_vector_icon(ctx, center, rect.size.w / 4, (IconHint)slot->icon_hint, pal->accent2);
  }
}

static void update_face(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  Layout l = layout_for(bounds);
  ThemePalette pal = get_theme(s_settings.theme);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, pal.bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Time pill
  graphics_context_set_fill_color(ctx, pal.surface);
  graphics_fill_rect(ctx, l.time_pill, l.time_radius, GCornersAll);
  graphics_context_set_text_color(ctx, pal.accent2);
  GFont time_font;
  int line_h;
  int time_y_off;
  if (s_settings.stacked_time) {
    if (s_settings.time_font == 1) {
      if (l.time_pill.size.h >= 110) {
        time_font = fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
        line_h = 48;
      } else {
        time_font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
        line_h = 30;
      }
      time_y_off = -4;
    } else {
#ifdef FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM
      if (l.time_pill.size.h >= 150) {
        time_font = fonts_get_system_font(FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM);
        line_h = 60;
      } else
#endif
      if (l.time_pill.size.h >= 110) {
        time_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
        line_h = 42;
      } else {
        time_font = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
        line_h = 32;
      }
      time_y_off = -3;
    }

    char hr[4] = {0};
    char mn[4] = {0};
    const char *colon = strchr(s_time, ':');
    if (colon) {
      size_t hlen = (size_t)(colon - s_time);
      if (hlen > 3) hlen = 3;
      memcpy(hr, s_time, hlen);
      snprintf(mn, sizeof(mn), "%s", colon + 1);
    } else {
      snprintf(hr, sizeof(hr), "%.3s", s_time);
    }
    int total_h = line_h * 2 - 4;
    int top_y = l.time_pill.origin.y + (l.time_pill.size.h - total_h) / 2 + time_y_off;
    graphics_draw_text(ctx, hr, time_font,
                       GRect(l.time_pill.origin.x + 1, top_y, l.time_pill.size.w - 2, line_h + 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, mn, time_font,
                       GRect(l.time_pill.origin.x + 1, top_y + line_h - 4, l.time_pill.size.w - 2, line_h + 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else {
    if (s_settings.time_font == 1) {
      // Gothic (Rounded)
      bool small_pill = l.time_pill.size.w < 80;
      time_font = fonts_get_system_font(small_pill ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_28_BOLD);
      line_h = small_pill ? 24 : 28;
      time_y_off = -3;
    } else {
      // LECO (Geometric)
      if (l.time_pill.size.w >= 110) {
        time_font = fonts_get_system_font(l.is_rect ? FONT_KEY_LECO_36_BOLD_NUMBERS : FONT_KEY_LECO_32_BOLD_NUMBERS);
        line_h = l.is_rect ? 38 : 34;
      } else if (l.time_pill.size.w >= 80) {
        time_font = fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM);
        line_h = 26;
      } else {
        time_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
        line_h = 21;
      }
      time_y_off = -2;
    }
    graphics_draw_text(ctx, s_time, time_font,
                       GRect(l.time_pill.origin.x + 4,
                             l.time_pill.origin.y + (l.time_pill.size.h - line_h) / 2 + time_y_off,
                             l.time_pill.size.w - 8, line_h + 2),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  // Two small circular badges (hidden in two-line time mode)
  if (!s_settings.stacked_time) {
    draw_badge_slot(ctx, l.badges[0], &s_slots[SLOT_BADGE_TOP], s_images[SLOT_BADGE_TOP], &pal);
    draw_badge_slot(ctx, l.badges[1], &s_slots[SLOT_BADGE_BOT], s_images[SLOT_BADGE_BOT], &pal);
  }

  // Three main ring complications
  for (int i = 0; i < 3; i++) {
    draw_ring_slot(ctx, l.rings[i], l.ring_thickness[i], &s_slots[i], s_images[i], &pal, i == 1, l.is_rect);
  }
}

// ---------------------------------------------------------------- health & defaults

static void init_default_slots(void) {
  for (int i = 0; i < SLOT_COUNT; i++) {
    s_slots[i] = (Slot){ .is_image = false, .text = "", .label = "", .fill = NO_FILL, .icon_hint = ICON_NONE };
  }
}

static void refresh_local_fallbacks(void) {
  // Only populate slots that were explicitly set to local built-in mode (via label tag)
  BatteryChargeState bat = battery_state_service_peek();
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (strcmp(s_slots[i].label, "BAT") == 0) {
      snprintf(s_slots[i].text, TEXT_MAX, "%d%%", bat.charge_percent);
      s_slots[i].fill = bat.charge_percent;
      s_slots[i].icon_hint = ICON_BATTERY;
    } else if (strcmp(s_slots[i].label, "DATE") == 0) {
      snprintf(s_slots[i].text, TEXT_MAX, "%s", s_date_day);
      s_slots[i].icon_hint = ICON_CALENDAR;
    }
#if defined(PBL_HEALTH)
    else if (strcmp(s_slots[i].label, "STEPS") == 0) {
      HealthServiceAccessibilityMask mask =
          health_service_metric_accessible(HealthMetricStepCount, time_start_of_today(), time(NULL));
      if (mask & HealthServiceAccessibilityMaskAvailable) {
        HealthValue steps = health_service_sum_today(HealthMetricStepCount);
        if (steps >= 1000) {
          snprintf(s_slots[i].text, TEXT_MAX, "%d,%03d", (int)(steps / 1000), (int)(steps % 1000));
        } else {
          snprintf(s_slots[i].text, TEXT_MAX, "%d", (int)steps);
        }
        int pct = (int)(steps * 100 / 8000);
        s_slots[i].fill = pct > 100 ? 100 : pct;
        s_slots[i].icon_hint = ICON_STEPS;
      }
    }
#endif
  }
}

// ---------------------------------------------------------------- messages

static void clear_image(int slot) {
  if (slot >= 0 && slot < SLOT_COUNT && s_images[slot]) {
    gbitmap_destroy(s_images[slot]);
    s_images[slot] = NULL;
  }
}

static void set_image(int slot, const Tuple *width, const Tuple *height, const Tuple *palette,
                      const Tuple *pixels) {
  if (slot < 0 || slot >= SLOT_COUNT) return;
  GSize size = GSize(width->value->int32, height->value->int32);
  if (size.w <= 0 || size.h <= 0 || size.w > IMAGE_SIZE_MAX || size.h > IMAGE_SIZE_MAX) return;
  clear_image(slot);
  uint8_t *owned_palette = malloc(palette->length);
  if (!owned_palette) return;
  memcpy(owned_palette, palette->value->data, palette->length);
  GBitmap *bitmap = gbitmap_create_blank_with_palette(size, GBitmapFormat4BitPalette,
                                                      (GColor *)owned_palette, true);
  if (!bitmap) {
    free(owned_palette);
    return;
  }
  uint16_t wire_stride = (size.w + 1) / 2;
  uint16_t stride = gbitmap_get_bytes_per_row(bitmap);
  uint8_t *data = gbitmap_get_data(bitmap);
  for (int row = 0; row < size.h; row++) {
    uint32_t offset = row * wire_stride;
    if (offset + wire_stride > pixels->length) break;
    memcpy(data + row * stride, pixels->value->data + offset, wire_stride);
  }
  s_images[slot] = bitmap;
}

static void send_image_size(void) {
  if (!s_canvas) return;
  Layout l = layout_for(layer_get_bounds(s_canvas));
  int size = ring_image_size(&l);
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int(out, MESSAGE_KEY_imageSize, &size, sizeof(int), true);
  app_message_outbox_send();
}

static void copy_field(DictionaryIterator *iter, uint32_t key, char *out, size_t size) {
  Tuple *tuple = dict_find(iter, key);
  if (tuple) snprintf(out, size, "%s", tuple->value->cstring);
}

static void update_time_str(struct tm *now) {
  if (clock_is_24h_style()) {
    strftime(s_time, sizeof(s_time), s_settings.leading_zero ? "%H:%M" : "%k:%M", now);
  } else {
    strftime(s_time, sizeof(s_time), s_settings.leading_zero ? "%I:%M" : "%l:%M", now);
  }
  // Trim leading space if %k/%l produced one
  if (s_time[0] == ' ') {
    memmove(s_time, s_time + 1, strlen(s_time));
  }
  strftime(s_date_day, sizeof(s_date_day), "%d %b", now);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_hello)) {
    send_image_size();
    return;
  }

  bool settings_changed = false;
  Tuple *t_theme = dict_find(iter, MESSAGE_KEY_theme);
  if (t_theme) {
    s_settings.theme = (uint8_t)t_theme->value->int32;
    settings_changed = true;
  }
  Tuple *t_dot = dict_find(iter, MESSAGE_KEY_showDot);
  if (t_dot) {
    s_settings.show_dot = t_dot->value->int32 != 0;
    settings_changed = true;
  }
  Tuple *t_lz = dict_find(iter, MESSAGE_KEY_leadingZero);
  if (t_lz) {
    s_settings.leading_zero = t_lz->value->int32 != 0;
    time_t now = time(NULL);
    update_time_str(localtime(&now));
    settings_changed = true;
  }
  Tuple *t_font = dict_find(iter, MESSAGE_KEY_timeFont);
  if (t_font) {
    s_settings.time_font = (uint8_t)t_font->value->int32;
    settings_changed = true;
  }
  Tuple *t_stacked = dict_find(iter, MESSAGE_KEY_stackedTime);
  if (t_stacked) {
    s_settings.stacked_time = t_stacked->value->int32 != 0;
    settings_changed = true;
  }
  if (settings_changed) {
    persist_write_data(PERSIST_SETTINGS, &s_settings, sizeof(s_settings));
  }

  Tuple *slot_tuple = dict_find(iter, MESSAGE_KEY_slot);
  if (slot_tuple) {
    int index = slot_tuple->value->int32;
    if (index >= 0 && index < SLOT_COUNT) {
      Slot *slot = &s_slots[index];
      Tuple *pixels = dict_find(iter, MESSAGE_KEY_iconPixels);
      if (pixels) {
        Tuple *width = dict_find(iter, MESSAGE_KEY_iconWidth);
        Tuple *height = dict_find(iter, MESSAGE_KEY_iconHeight);
        Tuple *palette = dict_find(iter, MESSAGE_KEY_iconPalette);
        if (width && height && palette) set_image(index, width, height, palette, pixels);
        slot->is_image = true;
        copy_field(iter, MESSAGE_KEY_imgLabel, slot->label, sizeof(slot->label));
      } else {
        clear_image(index);
        slot->is_image = false;
        Tuple *fill = dict_find(iter, MESSAGE_KEY_numFill);
        if (fill) slot->fill = (int8_t)fill->value->int32;
        Tuple *hint = dict_find(iter, MESSAGE_KEY_iconHint);
        if (hint) slot->icon_hint = (uint8_t)hint->value->int32;
        copy_field(iter, MESSAGE_KEY_numText, slot->text, sizeof(slot->text));
        copy_field(iter, MESSAGE_KEY_numLabel, slot->label, sizeof(slot->label));
      }
      persist_write_data(PERSIST_SLOTS, s_slots, sizeof(s_slots));
    }
  }

  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------- app lifecycle

static void tick(struct tm *now, TimeUnits units_changed) {
  update_time_str(now);
  refresh_local_fallbacks();
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  refresh_local_fallbacks();
  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, update_face);
  layer_add_child(root, s_canvas);
  send_image_size();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
  for (int i = 0; i < SLOT_COUNT; i++) clear_image(i);
}

static void init(void) {
  init_default_slots();
  if (persist_exists(PERSIST_SLOTS) &&
      persist_get_size(PERSIST_SLOTS) == (int)sizeof(s_slots)) {
    persist_read_data(PERSIST_SLOTS, s_slots, sizeof(s_slots));
  }
  if (persist_exists(PERSIST_SETTINGS) &&
      persist_get_size(PERSIST_SETTINGS) == (int)sizeof(Settings)) {
    persist_read_data(PERSIST_SETTINGS, &s_settings, sizeof(Settings));
  }

  refresh_local_fallbacks();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });

  app_message_register_inbox_received(inbox_received);
  app_message_open(app_message_inbox_size_maximum(), 64);
  tick_timer_service_subscribe(MINUTE_UNIT, tick);
  battery_state_service_subscribe(battery_handler);

  time_t now = time(NULL);
  update_time_str(localtime(&now));
  refresh_local_fallbacks();
  window_stack_push(s_window, true);
}

static void deinit(void) {
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
