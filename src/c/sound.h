// Audio constants — must match the raw PCM file format
// Expected format: 8-bit signed mono at 8kHz

#define SAMPLE_RATE 8000
#define TIMER_MS 50
#define SAMPLES_PER_CHUNK (SAMPLE_RATE * TIMER_MS / 1000) // 400
#define BYTES_PER_CHUNK (SAMPLES_PER_CHUNK * 1)           // 400

#if defined(PBL_PLATFORM_FLINT)
#define MIN_VOLUME 50
#define MAX_VOLUME 100
#elif defined(PBL_PLATFORM_EMERY)
#define MIN_VOLUME 5
#define MAX_VOLUME 35
#endif

#define CALL_ACTION_OFFSET 7000
#define TIMEOUT_OFFSET 2000

#define VOICE_PCM_COUNT 7
#define SFX_PCM_COUNT 3

static const uint32_t blitz_pcm = RESOURCE_ID_PCM_DATA_BLITZ;

static const uint32_t voice_pcms[VOICE_PCM_COUNT] = {
  RESOURCE_ID_PCM_DATA_BOP_IT,
  RESOURCE_ID_PCM_DATA_TWIST_IT,
  RESOURCE_ID_PCM_DATA_PULL_IT,
  RESOURCE_ID_PCM_DATA_DIE,
  RESOURCE_ID_PCM_DATA_WIN_1,
  RESOURCE_ID_PCM_DATA_WIN_2,
  RESOURCE_ID_PCM_DATA_WIN_3
};

static const uint32_t sfx_pcms[SFX_PCM_COUNT] = {
  RESOURCE_ID_PCM_DATA_SFX_BOP,
  RESOURCE_ID_PCM_DATA_SFX_TWIST,
  RESOURCE_ID_PCM_DATA_SFX_PULL
};

typedef enum {
  TYPE_SFX,
  TYPE_VOICE
} sound_type;

typedef void (*bopit_callback_t)(void);

size_t *get_vsfx_offset(void);
size_t *get_blitz_offset(void);
size_t *get_blitz_size(void);
void start_voice_sfx(sound_type type, int index);
void set_blitz_speed(float speed);
void start_blitz_loop(void);
void stop_blitz_loop(void);
void register_bopit_handler(bopit_callback_t cb);
void register_timeout_handler(bopit_callback_t cb);
bool start_stream(int volume);
void end_stream(void);
void init_tracks(void);
