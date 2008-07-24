#ifndef ENGINE_INTERNAL_H_INCLUDED
#define ENGINE_INTERNAL_H_INCLUDED

/* This header file contains declarations for internal use of the Minicast
   engine. It should not be included by application code. */

// Include public API declarations
#include "engine.h"


// Engine state
typedef struct engine_instance
{
	int dummy;
	// TODO: move all global state into this structure!
} engine_instance_t;


// Encoder specific functions
int start_encoder_thread(const encoder_config_t *config);
int stop_encoder_thread();
int encoder_enqueue_raw_data( const short *samples, unsigned num_samples,
	                          unsigned channels, unsigned sampling_rate );


// Server specific functions
int start_server_thread(const network_config_t *config);
int stop_server_thread();
void server_update_title(const char *title);
unsigned server_get_connected_clients();
void server_enqueue_encoded_data(const char *data, unsigned length);


#endif //ndef ENGINE_INTERNAL_H_INCLUDED
