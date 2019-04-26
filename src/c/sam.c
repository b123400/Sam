#include <pebble.h>

#define SETTINGS_KEY 1

static Window *s_window;
static Layer *bitmap_layer;

static GColor background_color;
static GColor hour_color;
static GColor min_color;
// static int shape_size;
static GPath *filling_path = NULL;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor HourColor;
  GColor MinColor;
  // int ShapeSize;
} ClaySettings;

static ClaySettings settings;

static void draw_shape(GContext *ctx, GPoint point, int diameter, GColor color, int edgeCount) {
  graphics_context_set_fill_color(ctx, color);
  if (edgeCount <= 0) return;
  if (edgeCount == 1) {
    graphics_fill_circle(ctx, point, diameter / 2);
  } else if (edgeCount == 2) {
    graphics_fill_circle(ctx, point, diameter / 2);
    graphics_context_set_fill_color(ctx, background_color);
    graphics_fill_circle(ctx, point, diameter / 2 - 6);
  } else {
    int angle_per_corner = TRIG_MAX_ANGLE / edgeCount;
    int radius = diameter / 2;
    GPoint points[12];
    for (int i = 0; i < edgeCount; i++) {
      points[i] = GPoint(
        point.x + sin_lookup(angle_per_corner * i) * radius / TRIG_MAX_RATIO,
        point.y - cos_lookup(angle_per_corner * i) * radius / TRIG_MAX_RATIO
      );
    }
    GPathInfo pathInfo = {
      .num_points = edgeCount,
      .points = points
    };
    if (filling_path != NULL) {
      gpath_destroy(filling_path);
    }
    filling_path = gpath_create(&pathInfo);
    gpath_draw_filled(ctx, filling_path);
  }
}

// takes a number from 0 - 11
// output 2 int, <= 6, with sum of the input
static void shape_numbers_for_number(int in, int *out1, int *out2) {
  if (in == 0) {
    *out1 = 0;
    *out2 = 0;
    return;
  }
  srand(time(NULL));
  int ran = rand();
  if (in == 1) {
    if (ran % 2 == 0) {
      *out1 = 1;
      *out2 = 0;
    } else {
      *out1 = 0;
      *out2 = 1;
    }
    return;
  }

  if (in > 6) {
    int from = MIN(6, in - 6);
    int to = MAX(6, in - 6);
    if (from == to) {
      *out1 = from;
      *out2 = from;
    } else {
      // 10 should be sth like 4:6 / 5:5 / 6:4
      // 4-5-6
      int first = from + (ran % (to - from));
      int second = in - first;
      *out1 = first;
      *out2 = second;
    }
    return;
  }
  int first1 = ran % (in + 1);
  int second1 = in - first1;
  *out1 = first1;
  *out2 = second1;
}

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int minute = (*t).tm_min;
  int hour = (*t).tm_hour;

  int five_minute = minute / 5;

  GRect bounds = layer_get_bounds(layer);
  // GPoint center = grect_center_point(&bounds);

  // how big is each shape
  int diameter = MIN(bounds.size.w, bounds.size.h) / 5;

  // start drawing
  // background color
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // draw hour
  int first, second;
  shape_numbers_for_number(hour % 12, &first, &second);
  draw_shape(ctx, GPoint(
    (int)(bounds.size.w * 0.35f),
    (int)(bounds.size.h * 0.35f)
  ), diameter, hour_color, first);

  draw_shape(ctx, GPoint(
    (int)(bounds.size.w * 0.65f),
    (int)(bounds.size.h * 0.35f)
  ), diameter, hour_color, second);

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "hour first: %d second %d", first, second);

  // draw minute
  shape_numbers_for_number(five_minute, &first, &second);
  draw_shape(ctx, GPoint(
    (int)(bounds.size.w * 0.35f),
    (int)(bounds.size.h * 0.65f)
  ), diameter, min_color, first);

  draw_shape(ctx, GPoint(
    (int)(bounds.size.w * 0.65f),
    (int)(bounds.size.h * 0.65f)
  ), diameter, min_color, second);

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "min first: %d second %d", first, second);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

    bitmap_layer = layer_create(bounds);
  layer_set_update_proc(bitmap_layer, bitmap_layer_update_proc);
  layer_add_child(window_layer, bitmap_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(bitmap_layer);
  if (filling_path != NULL) {
    gpath_destroy(filling_path);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  int minute = (*tick_time).tm_min;
  int hour = (*tick_time).tm_hour;
  if ((minute + hour * 60) % 5 == 0) {
    layer_mark_dirty(bitmap_layer);
  }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read color preferences
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_background_color);
  if(bg_color_t) {
    background_color = GColorFromHEX(bg_color_t->value->int32);
  }
  Tuple *hour_color_t = dict_find(iter, MESSAGE_KEY_hour_color);
  if(hour_color_t) {
    hour_color = GColorFromHEX(hour_color_t->value->int32);
  }
  Tuple *min_color_t = dict_find(iter, MESSAGE_KEY_min_color);
  if(min_color_t) {
    min_color = GColorFromHEX(min_color_t->value->int32);
  }
  // Tuple *shape_size_t = dict_find(iter, MESSAGE_KEY_shape_size);
  // if (shape_size_t) {
  //   shape_size = shape_size_t->value->int32;
  // }

  layer_mark_dirty(bitmap_layer);

  settings.BackgroundColor = background_color;
  settings.HourColor = hour_color;
  settings.MinColor = min_color;
  // settings.ShapeSize = shape_size;

  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_init(void) {
    // default settings
  settings.BackgroundColor = GColorWhite;
  settings.HourColor = GColorFromRGBA(205, 34, 49, 255);
  settings.MinColor = GColorFromRGBA(49, 34, 205, 255);
  // settings.ShapeSize = 30;

  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
  // apply saved data
  background_color = settings.BackgroundColor;
  hour_color = settings.HourColor;
  min_color = settings.MinColor;
  // shape_size = settings.ShapeSize;

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
