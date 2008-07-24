/* Contains the implementation of the Minicast audio encoder. */

#include "engine_internal.h"

// Include Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Include standard library headers
#include <string.h>
#include <stdio.h>
#include <malloc.h>

// Include MP3 encoder header
#define _BLADEDLL
#include "BladeMP3EncDLL.h"


// Function prototypes

// API functions
int start_encoder_thread(const encoder_config_t *config);
int stop_encoder_thread();
int encoder_enqueue_raw_data( const short *samples, unsigned samples_size,
	                          unsigned channels, unsigned sampling_rate );

// Thread function
static DWORD WINAPI run_encoder(LPVOID unused);


// Encoder queue entries
typedef struct queue_entry {
	struct queue_entry *next;
	unsigned samples_size, sampling_rate, channels;
	// char samples[];  -- sample data follows!
} queue_entry_t;


// Global variables
static HANDLE encoder_thread,
			  shutdown_event,	// set when the encoder should shut down
			  queue_event;		// set when data is added to the encoder queue
								// (and when the encoder should exit)
static CRITICAL_SECTION queue_access; // controls access to the encoder queue

static queue_entry_t *volatile queue_first, *queue_last;
static encoder_config_t encoder_config;  // pointer to encoder configuration


static DWORD WINAPI run_encoder(LPVOID unused)
{
	unsigned sampling_rate = 44100, channels = 2;
	unsigned input_buffer_size,  input_buffer_pos,
		     output_buffer_size, output_buffer_pos;
	char *input_buffer, *output_buffer;
	BE_CONFIG config;
	HBE_STREAM stream;

	while(WaitForSingleObject(shutdown_event, 0) != WAIT_OBJECT_0)
	{
		// Set correct configuration
		memset(&config, 0, sizeof(config));
		config.dwConfig = BE_CONFIG_LAME;
		config.format.LHV1.dwStructVersion = CURRENT_STRUCT_VERSION;
		config.format.LHV1.dwStructSize = CURRENT_STRUCT_SIZE;
		config.format.LHV1.dwSampleRate = sampling_rate;
		config.format.LHV1.dwReSampleRate = 0;
		if(channels == 1)
			config.format.LHV1.nMode = BE_MP3_MODE_MONO;
		else
			switch(encoder_config.channels)
			{
			default:
			case CHANNELS_JOINT:	config.format.LHV1.nMode = BE_MP3_MODE_JSTEREO; break;
			case CHANNELS_STEREO:	config.format.LHV1.nMode = BE_MP3_MODE_STEREO;  break;
			}
		config.format.LHV1.dwBitrate = encoder_config.bitrate;
		config.format.LHV1.nPreset = LQP_HIGH_QUALITY;
		config.format.LHV1.bCRC = TRUE;
		config.format.LHV1.nVbrMethod = VBR_METHOD_NONE;
		
		// Initialize the stream
		if( beInitStream( &config, &input_buffer_size, &output_buffer_size,
			              &stream) != BE_ERR_SUCCESSFUL)
		{
			MessageBox(NULL, "Encoder initialization failed:\nunable to initialize audio stream.",
				"Minicast", MB_OK | MB_ICONERROR);
			return -1;
		}
		input_buffer_size *= 2; // 16-bit samples

		// Allocate buffers
		input_buffer = output_buffer = NULL;
		if( (input_buffer  = (char*)malloc(input_buffer_size )) == NULL ||
			(output_buffer = (char*)malloc(output_buffer_size)) == NULL )
		{
			MessageBox(NULL, "Encoder initialization failed:\nnot enough memory.",
				"Minicast", MB_OK | MB_ICONERROR);
			free(input_buffer);
			free(output_buffer);
		 	beCloseStream(stream);
			return -1;
		}

		// Process queued data
		input_buffer_pos = output_buffer_pos = 0;
		while(1) {
			queue_entry_t *entry;
			unsigned entry_pos;

			EnterCriticalSection(&queue_access);
			while(queue_first == NULL)
			{
				// No data available; wait for event.
				LeaveCriticalSection(&queue_access);
				WaitForSingleObject(queue_event, INFINITE);
				if(WaitForSingleObject(shutdown_event, 0) == WAIT_OBJECT_0)
					goto cleanup; // Shut down
				EnterCriticalSection(&queue_access);
			}
			// Peek at first entry at the queue
			entry = queue_first;
			if( entry->sampling_rate != sampling_rate ||
				entry->channels != channels )
			{
				// Incompatible format; restart encoder with new parameters
				sampling_rate = entry->sampling_rate;
				channels = entry->channels;
				LeaveCriticalSection(&queue_access);
				break;
			}
			// Take entry off the queue
			queue_first = entry->next;
			LeaveCriticalSection(&queue_access);

			// Add entry to input buffer
			entry_pos = 0;
			while(input_buffer_pos + entry->samples_size - entry_pos >= input_buffer_size)
			{
				// Complete input buffer
				memcpy( input_buffer + input_buffer_pos,
					    ((char*)entry) + sizeof(queue_entry_t) + entry_pos,
						input_buffer_size - input_buffer_pos );
				entry_pos += input_buffer_size - input_buffer_pos;

				// Encode buffer and send it to the network server
				if( beEncodeChunk( stream, input_buffer_size/2, (short*)input_buffer,
						output_buffer, &output_buffer_pos ) == BE_ERR_SUCCESSFUL &&
					output_buffer_pos > 0 )
				{
					server_enqueue_encoded_data(output_buffer, output_buffer_pos);
				}

				input_buffer_pos = 0;
			}

			// Put remaining data in input buffer
			input_buffer_pos = entry->samples_size - entry_pos;
			memcpy( input_buffer, ((char*)entry) + sizeof(queue_entry_t) + entry_pos,
				    input_buffer_pos );
		} 

		// Complete partial input data
		if( input_buffer_pos > 0 && 
			beEncodeChunk( stream, input_buffer_pos/2, (short*)input_buffer,
				output_buffer, &output_buffer_pos ) == BE_ERR_SUCCESSFUL &&
			output_buffer_pos > 0 )
		{
			server_enqueue_encoded_data(output_buffer, output_buffer_pos);
		}

		// Complete ouput data
		if( beDeinitStream(stream, output_buffer, &output_buffer_pos) ==
				BE_ERR_SUCCESSFUL && output_buffer_pos > 0 )
		{
			server_enqueue_encoded_data(output_buffer, output_buffer_pos);
		}

		// Clean up
	cleanup:
 		beCloseStream(stream);
		free(input_buffer);
		free(output_buffer);
	}
	
	return 0;
}

int start_encoder_thread(const encoder_config_t *config)
{
	memcpy(&encoder_config, config, sizeof(encoder_config));

	// Initialize global variables
	queue_first = queue_last = NULL;
	shutdown_event = queue_event = NULL;

	// Create synchronization objects
	InitializeCriticalSection(&queue_access);

	if((shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		goto cleanup;

	if((queue_event = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
		goto cleanup;

	// Create encoder thread
    if((encoder_thread = CreateThread(NULL, 0, run_encoder, NULL, 0, NULL)) == NULL)
    {
        MessageBox(NULL, "Encoder initialization failed:\nunable to create encoder thread.",
            "Minicast", MB_OK | MB_ICONERROR);
		goto cleanup;
    }

    return 0;

cleanup:
	CloseHandle(shutdown_event);
	CloseHandle(encoder_thread);
	DeleteCriticalSection(&queue_access);	

	return -1;
}

int stop_encoder_thread()
{
	// Set shutdown events and wait for thread to exit
	SetEvent(shutdown_event);
	SetEvent(queue_event);
	WaitForSingleObject(encoder_thread, INFINITE);	
	
	// Clean up kernel objects
	ResetEvent(shutdown_event);
	CloseHandle(shutdown_event);
	CloseHandle(encoder_thread);
	DeleteCriticalSection(&queue_access);

	return 0;
}

int encoder_enqueue_raw_data( const short *samples, unsigned num_samples,
							  unsigned channels, unsigned sampling_rate )
{
	int result;
	unsigned samples_size;
	queue_entry_t *entry;

	// Check if input data format is supported.
	if(!( (channels == 1 || channels == 2) &&
		  ( sampling_rate ==  8000 || sampling_rate == 11025 || sampling_rate == 12000 || 
		    sampling_rate == 16000 || sampling_rate == 22050 || sampling_rate == 24000 ||
			sampling_rate == 32000 || sampling_rate == 44100 || sampling_rate == 48000 ) ))
		return -1;

	// Do not encode data when no clients are connected.
	if(server_get_connected_clients() == 0)
		return 0;

	result = 0;
	samples_size = num_samples * channels * 2;

	EnterCriticalSection(&queue_access);
	if((entry  = (queue_entry_t*)malloc(sizeof(queue_entry_t) + samples_size)) == NULL)
		result = -1;
	else
	{
		// Copy data to queue entry
		entry->samples_size = samples_size;
		entry->channels = channels;
		entry->sampling_rate = sampling_rate;
		entry->next = NULL;
		memcpy(((char*)entry) + sizeof(queue_entry_t), samples, samples_size);

		// Add entry to the queue
		if(!queue_first)
			queue_first = queue_last = entry;
		else
		{
			queue_last->next = entry;
			queue_last = entry;
		}

		// Signal that data is available
		SetEvent(queue_event);
	}

	LeaveCriticalSection(&queue_access);

	return result;
}
