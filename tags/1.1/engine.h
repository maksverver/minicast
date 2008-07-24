#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#define CHANNELS_MONO   (0)
#define CHANNELS_STEREO (1)
#define CHANNELS_JOINT  (2)

typedef struct encoder_config
{
    short
        bitrate,        /* 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,
                           112, 128, 144, 160, 192, 224, 256, 320 */
        channels,       /* 0 (mono), 1 (stereo), 2 (joint) */
        lowpass,        /* 0 (disable), 1 (enable) */
        psycho;         /* 0 (disable), 1 (enable) */
} encoder_config_t;

typedef struct network_config
{
    unsigned long  address;     /* in native order; 0 for any address */
    unsigned short port;        /* in native order */
    unsigned short connection_limit;
} network_config_t;

typedef struct engine_config
{
    char             stream_name[64];
    encoder_config_t encoder;
    network_config_t network;
} engine_config_t;

int engine_initialize( engine_config_t *config );

int engine_encode( char *samples, int samples_size,
    int channels, int bits_per_channel, int sampling_rate );

void engine_get_default_config(
    engine_config_t *config );

void engine_get_current_config(
    engine_config_t *config );

void engine_set_current_config(
    engine_config_t *config );

void engine_update_title(
	const char *title );

unsigned engine_connections();

int engine_cleanup( );

#endif //ndef ENGINE_H_INCLUDED
