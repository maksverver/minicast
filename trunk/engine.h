#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

/* This header file containes the API for the Minicast audio encoding and
   broadcasting engine.

   Application code (plug-ins) should call the functions in this file only.

   NB. These functions are not reentrant; it is assumed all functions are
	   called from the same thread!
*/


/* Handle to an instance of the engine; should be considered transparent
   by the calling application. */
struct engine_instance_t;
#define ENGINE_HANDLE volatile struct engine_instance *


// Constants to control the output channels configuration for the encoded data.
#define CHANNELS_STEREO (1)
#define CHANNELS_JOINT  (2)

// Names used
#define MINICAST_NAME      "Minicast"
#define MINICAST_FULL_NAME "Minicast 1.5"


// Configuration for the data encoder.
typedef struct encoder_config
{
    short
        bitrate,        /* 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,
                           112, 128, 144, 160, 192, 224, 256, 320 */
        channels;       /* 0 (mono), 1 (stereo), 2 (joint) */
} encoder_config_t;


// Configuration for the (internet) network server.
typedef struct network_config
{
    unsigned long  address;     /* in native order; 0 for any address */
    unsigned short port;        /* in native order */
    unsigned short connection_limit;	/* maximum number of connected
										   clients allowed */
    char           stream_name[64];		/* stream name */
} network_config_t;


/* Engine configuration; consists of a stream name and configurations for the
   data encoder and network server. */
typedef struct engine_config
{
    encoder_config_t encoder;
    network_config_t network;
} engine_config_t;


/* Initializes the engine. 'config' must be a valid engine configuration,
   which may be obtained by calling engine_get_default_config() first.

   config may be NULL, in which case the default configuration is used.
   
   If initialization failed, non-zero is returned and *engine is set to zero.
   Otherwise, an engine handle is returned and engine_cleanup() must be called
   eventually. */
int engine_initialize(const engine_config_t *config, ENGINE_HANDLE *engine);

/* Enqueues a block of raw audio data for processing and returns immediately.
   Samples should be 16-bit signed values. Returns zero if samples where
   succesfully queued. */
int engine_encode( ENGINE_HANDLE engine,
				   const short *samples, unsigned num_samples,
				   unsigned channels, unsigned sampling_rate );

/* Returns the default engine configuration. */
void engine_get_default_config(engine_config_t *config);

/* Returns the current engine configuration. */
void engine_get_current_config(ENGINE_HANDLE engine, engine_config_t *config);

/* Updates the current engine configuration.
   The engine must be initialized when calling this function. */
int engine_set_current_config(ENGINE_HANDLE engine, engine_config_t *config);

/* Updates the title for the current audio stream. Note that this does not
   change the stream title, but only the title of the currently playing item.
   The engine must be initialized when calling this function. */
int engine_update_title(ENGINE_HANDLE engine, const char *title );

/* Returns the number of clients currently connected to the audio stream. */
unsigned engine_connections(ENGINE_HANDLE engine);

/* Shuts down the engine. */
int engine_cleanup(ENGINE_HANDLE engine);

#endif //ndef ENGINE_H_INCLUDED
