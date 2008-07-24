/* Contains the implementation of the Minicast engine public API. */

#include "engine_internal.h"
#include <string.h>
#include <stdlib.h>

// Function prototypes
int engine_initialize(const engine_config_t *config, ENGINE_HANDLE *engine);
int engine_encode( ENGINE_HANDLE engine,
				   const short *samples, unsigned num_samples,
				   unsigned channels, unsigned sampling_rate );
void engine_get_default_config(engine_config_t *config);
void engine_get_current_config(ENGINE_HANDLE engine, engine_config_t *config);
int engine_set_current_config(ENGINE_HANDLE engine, engine_config_t *config);
int engine_update_title(ENGINE_HANDLE engine, const char *title );
unsigned engine_connections(ENGINE_HANDLE engine);
int engine_cleanup(ENGINE_HANDLE engine);

// Default configuration
#define DEFAULT_STREAMNAME      "Minicast live MP3 stream"
#define DEFAULT_BITRATE         (96)
#define DEFAULT_CHANNELS        (CHANNELS_JOINT)
#define DEFAULT_ADDRESS         (0)
#define DEFAULT_PORT            (8000)
#define DEFAULT_CONNECTIONLIMIT (5)


// Global variables
static const engine_config_t default_config = {
    { DEFAULT_BITRATE, DEFAULT_CHANNELS },
    { DEFAULT_ADDRESS, DEFAULT_PORT, DEFAULT_CONNECTIONLIMIT, DEFAULT_STREAMNAME }
};

static engine_config_t current_config = {
    { DEFAULT_BITRATE, DEFAULT_CHANNELS },
    { DEFAULT_ADDRESS, DEFAULT_PORT, DEFAULT_CONNECTIONLIMIT, DEFAULT_STREAMNAME }
};

static void update_config(const engine_config_t *config)
{
    memcpy(&current_config, config, sizeof(current_config));
	current_config.network.stream_name[
		sizeof(current_config.network.stream_name)-1 ] = '\0';
}

int engine_initialize(const engine_config_t *config, ENGINE_HANDLE *engine)
{
	if(!config)
		config = &default_config;

	*engine = 0;

	update_config(config);

	if(start_encoder_thread(&config->encoder) != 0)
		return 1;

	if(start_server_thread(&config->network) != 0)
	{
		stop_encoder_thread(); 
		return 2;
	}

	*engine = (engine_instance_t*)malloc(sizeof(engine_instance_t));

	return 0;
}

int engine_encode( ENGINE_HANDLE engine,
				   const short *samples, unsigned num_samples,
				   unsigned channels, unsigned sampling_rate )
{
	return encoder_enqueue_raw_data( samples, num_samples, channels, sampling_rate );
}

void engine_get_default_config(engine_config_t *config)
{
    memcpy(config, &default_config, sizeof(*config));
}

void engine_get_current_config(ENGINE_HANDLE engine, engine_config_t *config)
{
	memcpy(config, &current_config, sizeof(*config));
}

int engine_set_current_config(ENGINE_HANDLE engine, engine_config_t *config)
{
	int restart_encoder =
        config->encoder.bitrate  != current_config.encoder.bitrate  ||
        config->encoder.channels != current_config.encoder.channels;
    int restart_server =
        config->network.address          != current_config.network.address ||
        config->network.port             != current_config.network.port ||
        config->network.connection_limit != current_config.network.connection_limit ||
        config->network.stream_name      != current_config.network.stream_name;

	// FIXME: return non-zero on error!

    if(restart_encoder)
		stop_encoder_thread();
    if(restart_server)
        stop_server_thread();

	update_config(config);
    
    if(restart_server)
        start_server_thread(&config->network);
    if(restart_encoder)
        start_encoder_thread(&config->encoder);

	return 0;
}

int engine_update_title(ENGINE_HANDLE engine, const char *title)
{
	server_update_title(title);
	return 0;
}

unsigned engine_connections(ENGINE_HANDLE engine)
{
	return server_get_connected_clients();
}

int engine_cleanup(ENGINE_HANDLE engine)
{
	stop_encoder_thread();
	stop_server_thread();
	free((void*)engine);
	return 0;
}
