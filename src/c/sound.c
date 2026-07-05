#include <pebble.h>
#include "sound.h"

static AppTimer *speaker_write_timer;
static AppTimer *track_1_timer;
static AppTimer *track_2_timer;

static ResHandle blitz_handle;
static size_t blitz_size;

static ResHandle voice_handles[VOICE_PCM_COUNT];
static size_t voice_sizes[VOICE_PCM_COUNT];

static ResHandle sfx_handles[SFX_PCM_COUNT];
static size_t sfx_sizes[SFX_PCM_COUNT];

static ResHandle current_vsfx_handle;
static size_t current_vsfx_size;

static size_t blitz_offset;
static size_t vsfx_offset;

static float blitz_speed;

static uint8_t s_buffer_1[SAMPLES_PER_CHUNK];
static uint8_t s_buffer_2[SAMPLES_PER_CHUNK];

static bool track_2_active;
static bool track_1_active;

static bool has_called_bopit_cb;
static bool enable_timeout;
static bopit_callback_t bopit_cb = NULL;
static bopit_callback_t timeout_cb = NULL;

static void speaker_write_callback(void *data) { 
  speaker_write_timer = NULL;

  int16_t track_1;
  int16_t track_2;

  uint8_t final_buffer[SAMPLES_PER_CHUNK];

  for (size_t i = 0; i < SAMPLES_PER_CHUNK; i++) {
    if (track_1_active) {
      track_1 = (int8_t)s_buffer_1[i];
    } else {
      track_1 = 0;
    }
    if (track_2_active) {
      track_2 = (int8_t)s_buffer_2[i];
    } else {
      track_2 = 0;
    }

    int16_t combined_tracks = (track_1 + track_2);

    if (combined_tracks > 127) {
      combined_tracks = 127;
    } else if (combined_tracks < -128) {
      combined_tracks = -128;
    }

    final_buffer[i] = (uint8_t)(int8_t)combined_tracks;
  }

  if (track_1_active || track_2_active) {
    speaker_stream_write(final_buffer, SAMPLES_PER_CHUNK);
  }
  
  speaker_write_timer = app_timer_register(TIMER_MS, speaker_write_callback, NULL);
}

static void blitz_loop_callback(void *data) {
  track_1_timer = NULL;

  size_t remaining = blitz_size - blitz_offset;

  size_t to_read_track_1 = (remaining < BYTES_PER_CHUNK) ? remaining : BYTES_PER_CHUNK;

  if (remaining > 0) {
    track_1_active = true;
    resource_load_byte_range(blitz_handle, blitz_offset, s_buffer_1, to_read_track_1);
    blitz_offset += to_read_track_1 * blitz_speed;
    track_1_timer = app_timer_register(TIMER_MS, blitz_loop_callback, NULL);
  }

  if (blitz_offset > CALL_ACTION_OFFSET && !has_called_bopit_cb) {
    if (bopit_cb != NULL) {
      bopit_cb();
    }
    has_called_bopit_cb = true;
  }
  
  if (blitz_offset > TIMEOUT_OFFSET && enable_timeout) {
    if (timeout_cb != NULL) {
      timeout_cb();
    }
    enable_timeout = false;
  }

  if (blitz_offset >= blitz_size) {
    has_called_bopit_cb = false;
    enable_timeout = true;
    blitz_offset = 0;
  }
}

static void track_2_callback(void *data) {
  track_2_timer = NULL;

  size_t remaining = current_vsfx_size - vsfx_offset;

  size_t to_read_track_2 = (remaining < BYTES_PER_CHUNK) ? remaining : BYTES_PER_CHUNK;

  if (remaining > BYTES_PER_CHUNK / 2) {
    track_2_active = true;
    resource_load_byte_range(current_vsfx_handle, vsfx_offset, s_buffer_2, to_read_track_2);
    vsfx_offset += to_read_track_2;
    track_2_timer = app_timer_register(TIMER_MS, track_2_callback, NULL);
  } else {
    track_2_active = false;
    vsfx_offset = 0;
    return;
  }
}

size_t *get_vsfx_offset(void) {
  return &vsfx_offset;
}

size_t *get_blitz_offset(void) {
  return &blitz_offset;
}

size_t *get_blitz_size(void) {
  return &blitz_size;
}

void start_voice_sfx(sound_type type, int index) {
  switch (type) {
    case TYPE_SFX:
    current_vsfx_handle = sfx_handles[index];
    current_vsfx_size = sfx_sizes[index];
    break;
    case TYPE_VOICE:
    current_vsfx_handle = voice_handles[index];
    current_vsfx_size = voice_sizes[index];
    break;
  }
  
  if (track_2_timer != NULL) {
    app_timer_cancel(track_2_timer);
    track_2_timer = NULL;
  }

  if (track_2_timer == NULL) {
    vsfx_offset = 0;
    track_2_callback(NULL);
  }
}

void set_blitz_speed(float speed) {
  blitz_speed = speed;
}

void start_blitz_loop(void) {
  has_called_bopit_cb = false;
  enable_timeout = false;
  set_blitz_speed(1.0);
  if (track_1_timer == NULL) {
    blitz_offset = 0;
    blitz_loop_callback(NULL);
  }
}

void stop_blitz_loop(void) {
  if (track_1_timer != NULL) {
    app_timer_cancel(track_1_timer);
    track_1_timer = NULL;
  }

  track_1_active = false;
}

void register_bopit_handler(bopit_callback_t cb) {
  bopit_cb = cb;
}

void register_timeout_handler(bopit_callback_t cb) {
  timeout_cb = cb;
}

bool start_stream(int volume) {
  if (!speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, volume)) {
    return false;
  }

  speaker_write_callback(NULL);
  return true;
}

void end_stream(void) {
  if (speaker_write_timer != NULL) {
    app_timer_cancel(speaker_write_timer);
    speaker_write_timer = NULL;
  }

  if (track_1_timer != NULL) {
    app_timer_cancel(track_1_timer);
    track_1_timer = NULL;
  }

  if (track_2_timer != NULL) {
    app_timer_cancel(track_2_timer);
    track_2_timer = NULL;
  }

  speaker_stop();
  speaker_stream_close();
}

void init_tracks(void) {
  blitz_handle = resource_get_handle(blitz_pcm);
  blitz_size = resource_size(blitz_handle);
  
  if (blitz_size == 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "blitz loop resource empty");
  }

  int voice_count = sizeof(voice_pcms) / sizeof(voice_pcms[0]);
  int sfx_count = sizeof(sfx_pcms) / sizeof(sfx_pcms[0]);

  for (int i = 0; i < voice_count; i++) {
    voice_handles[i] = resource_get_handle(voice_pcms[i]);
    voice_sizes[i] = resource_size(voice_handles[i]);

    if (voice_sizes[i] == 0) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "bop it pcm voice part %d resource is empty", i);
    }
  }

  for (int i = 0; i < sfx_count; i++) {
    sfx_handles[i] = resource_get_handle(sfx_pcms[i]);
    sfx_sizes[i] = resource_size(sfx_handles[i]);

    if (sfx_sizes[i] == 0) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "bop it pcm sfx part %d resource is empty", i);
    }
  }
}
