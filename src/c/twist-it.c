#include <pebble.h>
#include "sound.h"

#define VOLUME_KEY 1
#define HIGH_SCORE_KEY 2

#define MAX_SCORE 100

#define BLITZ_MAX_SPEED 1.5
#define BLITZ_SPEED_STEP 10 // apply speed rate every 10 points
#define BLITZ_SPEED_RATE (BLITZ_MAX_SPEED - 1.0) / (BLITZ_SPEED_STEP - 1)

#define ACCEL_SAMPLE_RATE ACCEL_SAMPLING_10HZ
#define ACCEL_SAMPLES_PER_UPDATE 1
#define TWIST_THRESHOLD 1400

#define SWIPE_THRESHOLD 16

static Window *s_window;
static TextLayer *score_text_layer;
static TextLayer *high_score_text_layer;

static BitmapLayer *bopit_layer;
static GBitmap *bopit_bitmap;

/* game state */
static size_t blitz_size;
static uint8_t volume;
static bool game_running;
static bool lock_controls;
static bool lock_accel;
static float game_speed;
static bool said_action;
static uint16_t high_score;
static uint16_t score;
static uint8_t user_action;
static uint8_t called_action;

char score_buffer[20];
char high_score_buffer[20];

typedef enum {
  BOP_IT,
  TWIST_IT,
  PULL_IT,
} input_type;

input_type input;

static void unlock_controls_callback(void *data) {
  lock_controls = false;
}

static bool speaker_muted() {
  if (speaker_is_muted()) {
    vibes_long_pulse();
    text_layer_set_text(high_score_text_layer, "Your speaker is muted!");
    layer_set_hidden(text_layer_get_layer(high_score_text_layer), false);
    layer_set_hidden(text_layer_get_layer(score_text_layer), true);
  }

  return speaker_is_muted();
}

static void save_score() {
  if (score > high_score) {
    high_score = score;
    persist_write_int(HIGH_SCORE_KEY, high_score);
  }
}

static void write_high_score_buffer(char *buffer, int size, bool add_star) {
  snprintf(buffer, size, add_star ? "High Score: %d ⭐" : "High Score: %d", high_score);
}

static void update_score() {  
  if (score > high_score && score < MAX_SCORE) {
    snprintf(score_buffer, sizeof(score_buffer), "New High Score!");
  } else if (score >= MAX_SCORE) {
    snprintf(score_buffer, sizeof(score_buffer), "You Won!");
  } else {
    snprintf(score_buffer, sizeof(score_buffer), "Score: %d", score);
  }

  save_score();
  write_high_score_buffer(high_score_buffer, sizeof(high_score_buffer), false);

  if (score > 0) {
    layer_set_hidden(text_layer_get_layer(score_text_layer), false);
  }
  
  if (high_score > 0) {
    layer_set_hidden(text_layer_get_layer(high_score_text_layer), false);
  }
    
  text_layer_set_text(score_text_layer, score_buffer);
  text_layer_set_text(high_score_text_layer, high_score_buffer);
}

static void end_game() {
  stop_blitz_loop();
  user_action = 0;
  called_action = 0;
  said_action = false;
  game_running = false;
  lock_accel = false;
  lock_controls = true;
  app_timer_register(1500, unlock_controls_callback, NULL);
}

static void start_game() {
  if (speaker_muted()) return;

  layer_set_hidden(text_layer_get_layer(score_text_layer), true);
  layer_set_hidden(text_layer_get_layer(high_score_text_layer), true);
  vibes_cancel();
  lock_accel = true;
  game_running = true;
  game_speed = 1.0;
  score = 0;
  speaker_set_volume(volume);
  start_blitz_loop();
}

static void game_done() {
  update_score(); 
  start_voice_sfx(TYPE_VOICE, rand() / ((RAND_MAX + 1u) / 3) + 4);
  end_game();
}

static void game_over() {
  update_score();
  vibes_long_pulse();
  start_voice_sfx(TYPE_VOICE, 3);
  end_game();
}

static void timeout_callback(void) {
  if (!game_running) return;
  
  if (said_action) {
    game_over();
  }
}

static void bopit_callback(void) {
  if (!game_running) return;

  if (score >= MAX_SCORE) {
    game_done();
    return;
  }

  if (score < 3) {
    called_action = score;
  } else {
    called_action = rand() / ((RAND_MAX + 1u) / 3);
  }
  
  if (called_action != TWIST_IT) {
    lock_accel = true;
  } else {
    lock_accel = false;
  }

  start_voice_sfx(TYPE_VOICE, called_action);
  said_action = true;
}

static void bopit_action(int action) {
  if (!game_running || lock_controls) return;

  if (score >= MAX_SCORE) return;

  size_t vsfx_offset = *get_vsfx_offset();
  size_t blitz_offset = *get_blitz_offset();
   
  if (said_action && vsfx_offset != 0) {
    game_over();
    return;
  } else if (blitz_offset < blitz_size - 800 && blitz_offset > TIMEOUT_OFFSET) {
    game_over();
    return;
  }

  if (said_action && action == called_action) {

    if (score >= BLITZ_SPEED_STEP && score % BLITZ_SPEED_STEP == 0) {
      if (game_speed < BLITZ_MAX_SPEED) {
        game_speed += BLITZ_SPEED_RATE;
      } else {
        game_speed = BLITZ_MAX_SPEED;
      }
      set_blitz_speed(game_speed);
    }

    score++;
    said_action = false;
    called_action = 0;
    start_voice_sfx(TYPE_SFX, action);

  } else {
    game_over();
  }
}

static void bopit_button() {
  if (lock_controls || speaker_muted()) return;

  if (game_running) {
    bopit_action(input = BOP_IT);
  } else {
    start_voice_sfx(TYPE_SFX, BOP_IT);
    start_game();
  }
}

static void twist_it_button() {
  if (lock_controls || speaker_muted()) return;

  if (game_running) {
    bopit_action(input = TWIST_IT);
  } else {
    start_voice_sfx(TYPE_SFX, TWIST_IT);
  }
}

static void pull_it_button() {
  if (lock_controls || speaker_muted()) return;

  if (game_running) {
    bopit_action(input = PULL_IT);
  } else {
    start_voice_sfx(TYPE_SFX, PULL_IT);
  }
}

static void unlock_accel(void *data) {
  lock_accel = false;
}

static void on_accel() {
  if (lock_accel) return;

  twist_it_button();
  lock_accel = true;
  if (!game_running) {
    app_timer_register(1000, unlock_accel, NULL);
  }
}

static void accel_data_handler(AccelData *data, uint32_t num_samples) {
  if (lock_controls) return;

  for (uint32_t i = 0; i < num_samples; i++) {
    int16_t y = abs(data[i].y);

    if (y > TWIST_THRESHOLD) {
      on_accel();
    }
  }
}

#if defined(PBL_TOUCH)

static bool lock_touch;
static bool s_is_tracking = false;
static int16_t s_start_y = 0;

static void unlock_touch(void *data) {
  lock_touch = false;
}

static void touch_handler(const TouchEvent *event, void *context) {
  if (lock_controls || speaker_muted()) return;
   
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_start_y = event->y;
      s_is_tracking = true;
      break;

    case TouchEvent_PositionUpdate:
      break;

    case TouchEvent_Liftoff:
      if (s_is_tracking) {
        int16_t delta_y = event->y - s_start_y;
        
        if (!lock_touch) {
          if (delta_y > SWIPE_THRESHOLD) {
            pull_it_button();
          } else {
            bopit_button();
          }
          lock_touch = true;
          app_timer_register(500, unlock_touch, NULL);
        }

        s_is_tracking = false;
      }
      break;
  }
}

#endif

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  bopit_button();
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  twist_it_button();
 
  if (volume >= MAX_VOLUME || game_running || lock_controls) return;
  volume = volume + 2;
  speaker_set_volume(volume);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  pull_it_button();

  if (volume <= MIN_VOLUME || game_running || lock_controls) return;
  volume = volume - 2;
  speaker_set_volume(volume);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  bopit_bitmap = gbitmap_create_with_resource(PBL_IF_COLOR_ELSE(RESOURCE_ID_IMAGE_BOP_IT_COLOR, 
                                                                RESOURCE_ID_IMAGE_BOP_IT_BW));
  bopit_layer = bitmap_layer_create(bounds);

  bitmap_layer_set_bitmap(bopit_layer, bopit_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(bopit_layer));

  score_text_layer = text_layer_create(GRect(0, bounds.size.h / 20, bounds.size.w, 40));
  layer_set_hidden(text_layer_get_layer(score_text_layer), true);
  text_layer_set_text(score_text_layer, "TOP TEXT");
  text_layer_set_text_alignment(score_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(score_text_layer));
  
  high_score_text_layer = text_layer_create(GRect(0, bounds.size.h / 1.2, bounds.size.w, 40));
  layer_set_hidden(text_layer_get_layer(high_score_text_layer), true);
  text_layer_set_text(high_score_text_layer, "BOTTOM TEXT");
  text_layer_set_text_alignment(high_score_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(high_score_text_layer));
  
  if (!start_stream(volume)) {
    text_layer_set_overflow_mode(score_text_layer, GTextOverflowModeWordWrap);
    text_layer_set_text(score_text_layer, "Failed to open speaker stream!");
    layer_set_hidden(text_layer_get_layer(score_text_layer), false);
    layer_set_hidden(text_layer_get_layer(high_score_text_layer), true);
    lock_controls = true;
  }

  speaker_set_volume(volume);
}

static void prv_window_unload(Window *window) {
  end_stream();

  if (persist_exists(VOLUME_KEY)) {
    if (volume != persist_read_int(VOLUME_KEY)) {
      persist_write_int(VOLUME_KEY, volume);
    }
  } else {
    persist_write_int(VOLUME_KEY, volume);
  }
  
  text_layer_destroy(high_score_text_layer);
  text_layer_destroy(score_text_layer);
}

static void prv_update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) return;
  
  char high_score_glance[20];

  write_high_score_buffer(high_score_glance, 
                          sizeof(high_score_glance), 
                          high_score >= MAX_SCORE ? true : false);

  const AppGlanceSlice slice = (AppGlanceSlice) {
    .layout = {
      .subtitle_template_string = high_score_glance
    },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION
  };

  AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error adding AppGlanceSlice: %d", result);
  }
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = true;
 
  accel_service_set_sampling_rate(ACCEL_SAMPLE_RATE);
  accel_service_set_samples_per_update(ACCEL_SAMPLES_PER_UPDATE);
  accel_data_service_subscribe(ACCEL_SAMPLES_PER_UPDATE, accel_data_handler);

  register_bopit_handler(bopit_callback);
  register_timeout_handler(timeout_callback);

  #if defined(PBL_TOUCH)
  if (touch_service_is_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
  #endif

  if (persist_exists(VOLUME_KEY)) {
    volume = persist_read_int(VOLUME_KEY);
  } else {
    volume = MAX_VOLUME;
  }

  if (persist_exists(HIGH_SCORE_KEY)) {
    high_score = persist_read_int(HIGH_SCORE_KEY);
  } else {
    high_score = 0;
  }
   
  init_tracks();
  blitz_size = *get_blitz_size();
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  if (high_score > 0) {
    app_glance_reload(prv_update_app_glance, NULL);
  }
  
  vibes_cancel();
  accel_data_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
