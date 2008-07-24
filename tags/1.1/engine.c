#include "engine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "musenc.h"

// Default configuration
#define DEFAULT_STREAMNAME      "Minicast live MP3 stream"
#define DEFAULT_BITRATE         (96)
#define DEFAULT_CHANNELS        (CHANNELS_JOINT)
#define DEFAULT_LPF             (1)
#define DEFAULT_PSY             (1)
#define DEFAULT_ADDRESS         (INADDR_ANY)
#define DEFAULT_PORT            (8000)
#define DEFAULT_CONNECTIONLIMIT (5)

#define METADATA_INTERVAL		(4096)

// Encoder variables
static HANDLE buffer_requested = NULL;  // manual event
static HANDLE buffer_provided  = NULL;  // manual event
static HANDLE buffer_flush     = NULL;  // manual event
static HANDLE encoder_thread   = NULL;  // encoder thread
static char *buffer;
static unsigned buffer_size, buffer_pos;

// Client connection states
#define CLIENT_CONNECTED    (1)		/* connected, but HTTP request not yet finished */
#define CLIENT_STREAMING    (2)		/* client is listening to the MP3 stream */

// Server variables
static SOCKET server_socket = INVALID_SOCKET;   // http server socket
static HANDLE server_thread = NULL;             // http server thread
static HANDLE clients_mutex = NULL;          // client connection mutex
static struct client_list {
    struct client_list *next;

	SOCKET socket;
	int    status;

	char     buffer[512];
	unsigned buffer_size;

    char     resource[64];
	int		 wants_metadata;
	unsigned non_metadata_sent, metadata_revision;

} *clients = NULL;								// clients connection linked list
static unsigned clients_size = 0;

// Metadata
static CRITICAL_SECTION access_metadata;
static char metadata_title[128] = "";
static unsigned metadata_revision = 0;

// Configuration
static CRITICAL_SECTION access_config;
static const engine_config_t default_config = {
    DEFAULT_STREAMNAME,
    { DEFAULT_BITRATE, DEFAULT_CHANNELS, DEFAULT_LPF, DEFAULT_PSY },
    { DEFAULT_ADDRESS, DEFAULT_PORT, DEFAULT_CONNECTIONLIMIT }
};
static engine_config_t current_config = {
    DEFAULT_STREAMNAME,
    { DEFAULT_BITRATE, DEFAULT_CHANNELS, DEFAULT_LPF, DEFAULT_PSY },
    { DEFAULT_ADDRESS, DEFAULT_PORT, DEFAULT_CONNECTIONLIMIT }
};

#include <crtdbg.h>
void TRACE(const char *str)
{
    static HANDLE trace_mutex = NULL;
    if(trace_mutex == NULL)
        trace_mutex = CreateMutex(NULL, TRUE, NULL);
    else
        WaitForSingleObject(trace_mutex, INFINITE);
    _RPT0(_CRT_WARN, str);
    ReleaseMutex(trace_mutex);
}

// Appends the provided data to the encoding buffer
static void put_buffer(char *buf, unsigned long length)
{
    if(encoder_thread)
    {
        int clients;
        WaitForSingleObject(clients_mutex, INFINITE);
        clients = clients_size;
        ReleaseMutex(clients_mutex);
        if(clients > 0)
        {
            WaitForSingleObject(buffer_requested, INFINITE);
            buffer      = buf;
            buffer_size = length;
            buffer_pos  = 0;
            ResetEvent(buffer_requested);
            SetEvent(buffer_provided);
            WaitForSingleObject(buffer_requested, INFINITE);
        }
    }
}

// Removes data of size upto the requested size from the encoding buffer
static unsigned get_buffer(char *buf, unsigned length)
{
    HANDLE handles[2] = { buffer_provided, buffer_flush };
    unsigned read = 0;
    while(read < length)
    {
        switch(WaitForMultipleObjects(2, handles, FALSE, INFINITE))
        {
        case (WAIT_OBJECT_0 + 0):   // buffer_provided
            {
                if(buffer_size > length - read)
                {
                    memcpy(buf + read, buffer + buffer_pos, length - read);
                    buffer_pos  += length - read;
                    buffer_size -= length - read;
                    read = length;
                }
                else
                {
                    memcpy(buf + read, buffer + buffer_pos, buffer_size);
                    read += buffer_size;
                    ResetEvent(buffer_provided);
                    SetEvent(buffer_requested);
                }
            } break;
            
        case (WAIT_OBJECT_0 + 1):   // buffer_flush
            {
                goto exit;
            } break;
        }
    }
exit:
    return read;
}

static MERET __cdecl input(char *buf, unsigned long buf_size)
{
    unsigned read = get_buffer(buf, buf_size);
    if(read < buf_size)
    {
        memset(buf + read, 0, buf_size - read);
        return ME_EMPTYSTREAM;
    }
    return ME_NOERR;
}

static struct {
    MPGE_USERFUNC input_func;
    unsigned buffer_size, bits_per_channel, sampling_rate, channels;
} input_info = {
    input, MC_INPDEV_MEMORY_NOSIZE
};

static MERET __cdecl output(void *buf, unsigned long buf_size)
{
    struct client_list *s, *t = NULL;
    WaitForSingleObject(clients_mutex, INFINITE);
    s = clients;
    while(s)
    {
        int result = 0;

		if(s->status == CLIENT_STREAMING)
		{
			char     *local_buf = buf;
			unsigned local_buf_size = buf_size;
			while(s->wants_metadata && s->non_metadata_sent + local_buf_size >= METADATA_INTERVAL)
			{
				unsigned size;
				char metadata[32+sizeof(metadata_title)];
                
				// Send partial data.
				size = METADATA_INTERVAL - s->non_metadata_sent;
				send(s->socket, local_buf, size, 0);
				local_buf      += size;
				local_buf_size -= size;
                s->non_metadata_sent += size;

				EnterCriticalSection(&access_metadata);
				if(s->metadata_revision != metadata_revision)
				{
					s->metadata_revision = metadata_revision;

					// Build metadata packet.
					memset(metadata, 0, sizeof(metadata));
					EnterCriticalSection(&access_metadata);
					sprintf(metadata+1, "StreamTitle='%s';", metadata_title);
					LeaveCriticalSection(&access_metadata);
					size = ((unsigned)strlen(metadata+1)/16 + 1)*16;
				}
				else
				{
					// Skip metadata packet.
					size = 0;
				}
				LeaveCriticalSection(&access_metadata);

                // Send metadata packet.
				metadata[0] = (char)size/16;
				send(s->socket, metadata, size+1, 0);
				s->non_metadata_sent = 0;
			}

            result = send(s->socket, local_buf, local_buf_size, 0);
            s->non_metadata_sent += local_buf_size;
		}

		if(result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{
            // Remove malfunctioning client
			*((t == NULL) ? &clients : &(t->next)) = s->next;
			free(s);
			s = ((t == NULL) ? clients : (t->next));
			--clients_size;
		}
		else
		{
            // Get next client in list.
			t = s; s = s->next;
		}
    }
    ReleaseMutex(clients_mutex);
    return ME_NOERR;
}

static DWORD WINAPI run_encoder(LPVOID unused)
{
    while(MPGE_processFrame() == ME_NOERR) { };
    return 0;
}

static int start_encoder( int channels, int bits_per_channel, int sampling_rate )
{
    int result;

    if((result = MPGE_initializeWork()) != ME_NOERR)
    {
        MessageBox(NULL, "Encoder initialization failed:\nunable to initialize environment.",
            "Minicast", MB_OK | MB_ICONERROR);
        return result;
    }

    input_info.channels         = channels;
    input_info.bits_per_channel = bits_per_channel;
    input_info.sampling_rate    = sampling_rate;

    MPGE_setConfigure( MC_INPUTFILE, MC_INPDEV_USERFUNC, (UPARAM)&input_info );

    EnterCriticalSection(&access_config);
    MPGE_setConfigure( MC_BITRATE,    current_config.encoder.bitrate,  (UPARAM)0 );
    MPGE_setConfigure( MC_ENCODEMODE, channels==1 ? MC_MODE_MONO :
                                      current_config.encoder.channels, (UPARAM)0 );
    MPGE_setConfigure( MC_OUTFREQ,    sampling_rate,                   (UPARAM)0 );
    MPGE_setConfigure( MC_USEPSY,     current_config.encoder.psycho,   (UPARAM)0 );        
    MPGE_setConfigure( MC_USELPF16,   current_config.encoder.lowpass,  (UPARAM)0 );        
    MPGE_setConfigure( MC_OUTPUTFILE, MC_OUTDEV_USERFUNC,              (UPARAM)output );
    LeaveCriticalSection(&access_config);

    if((result = MPGE_detectConfigure()) != ME_NOERR)
    {
        MPGE_closeCoder();
        MPGE_endCoder();
        MessageBox(NULL, "Encoder initialization failed:\ninvalid configuration detected.",
            "Minicast", MB_OK | MB_ICONWARNING);
        return result;
    }

    if((encoder_thread = CreateThread(NULL, 0, run_encoder, NULL, 0, NULL)) == NULL)
    {
        MPGE_closeCoder();    
        MPGE_endCoder();
        MessageBox(NULL, "Encoder initialization failed:\nunable to create encoder thread.",
            "Minicast", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    return result;
}

static void stop_encoder()
{
    if(encoder_thread)
    {
        SetEvent(buffer_flush);
        WaitForSingleObject(encoder_thread, INFINITE);
        ResetEvent(buffer_flush);
        encoder_thread = NULL;
        MPGE_closeCoder();
        MPGE_endCoder();
    }
}

static DWORD WINAPI run_server(LPVOID unused)
{
    unsigned connection_limit;
	char stream_name[sizeof(current_config.stream_name)];

    EnterCriticalSection(&access_config);
    connection_limit = current_config.network.connection_limit;
	strcpy(stream_name, current_config.stream_name);
    LeaveCriticalSection(&access_config);

    while(TRUE)
    {
        SOCKET conn;
        struct client_list *entry, *prev_entry;
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(server_socket, &fds);
        WaitForSingleObject(clients_mutex, INFINITE);
		for(entry = clients; entry != NULL; entry = entry->next)
			if(entry->status == CLIENT_CONNECTED)
                FD_SET(entry->socket, &fds);
        ReleaseMutex(clients_mutex);

		// Wait for a new connection or data available from a connected client.
		select(0, &fds, NULL, NULL, NULL);

        if( FD_ISSET(server_socket, &fds) &&
			(conn = accept(server_socket, NULL, NULL)) != SOCKET_ERROR )
		{
			// Add newly connected client

			entry = (struct client_list*)malloc(sizeof(struct client_list));
			memset(entry, 0, sizeof(struct client_list));
			entry->socket = conn;

			WaitForSingleObject(clients_mutex, INFINITE);
			if(clients_size >= connection_limit)
			{
				LINGER linger = { TRUE, 1 };

				// Reject connected client.
				const char *msg = "ICY 503 Service Unavailable\015\012\015\012";
				setsockopt(conn, SOL_SOCKET, SO_LINGER, (const char*)&linger, sizeof(linger));
				send(conn, msg, (int)strlen(msg), 0);
				closesocket(conn);
			}
			else
			{
				ULONG true = TRUE;

				// Accept connected client.
				//ioctlsocket(entry->socket, FIONBIO, &true);
				entry->status = CLIENT_CONNECTED;
				entry->next = clients;
				clients     = entry;
				++clients_size;
			}
			ReleaseMutex(clients_mutex);
		}

        WaitForSingleObject(clients_mutex, INFINITE);
		entry = clients; prev_entry = NULL;
		while(entry)
		{
			if(entry->status == CLIENT_CONNECTED || FD_ISSET(entry->socket, &fds))
			{
				int result;

				// Read data from connected client.
				result = recv( entry->socket, entry->buffer + entry->buffer_size,
					sizeof(entry->buffer) - entry->buffer_size, 0 );

				if(result == 0 || (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK))
					goto close_connection;

				if(result > 0)
				{
					unsigned p;
					char line[sizeof(entry->buffer)];

	                // Update buffer.
					entry->buffer_size += result;
					if(entry->buffer_size == sizeof(entry->buffer))
						goto close_connection; // buffer full; client shouldn't send such long lines!

					// Parse lines from buffer.
					while(1)
					{
						for(p = 0; p + 1 < entry->buffer_size; ++p)
							if(entry->buffer[p] == '\r' && entry->buffer[p+1] == '\n')
								goto line_found;
						break;

					line_found:
						memcpy(line, entry->buffer, p); line[p] = '\0';
						entry->buffer_size -= (p + 2);
						memmove(entry->buffer, entry->buffer + (p + 2), entry->buffer_size);

                        if( !*entry->resource &&
							(sscanf(line, "GET %64s", entry->resource) < 1 || !*entry->resource) )
						{
							const char *response = "501 Not Implemented\r\n\r\n";

							send(entry->socket, response, (int)strlen(response), 0);
							goto close_connection;
						}
						else
						if(*line == '\0')
						{
							if(!*entry->resource)
							{
								const char *response = "400 Bad Request\r\n\r\n";

								send(entry->socket, response, (int)strlen(response), 0);
								goto close_connection;
							}

							// Request header received. Send response header.
							{
								char buffer[64 + sizeof(stream_name)];
								sprintf(buffer, "ICY 200 OK\r\nicy-name: %s\r\n", stream_name);
								send(entry->socket, buffer, (int)strlen(buffer), 0);
								if(entry->wants_metadata)
								{
									sprintf(buffer, "icy-metaint: %d\r\n", METADATA_INTERVAL);
									send(entry->socket, buffer, (int)strlen(buffer), 0);
								}
								sprintf(buffer, "\r\n");
								send(entry->socket, buffer, (int)strlen(buffer), 0);
							}

							entry->status = CLIENT_STREAMING;
						}
						else
						{
							// Parse HTTP header
							char *name, *value, *p;

							if((value = strchr(name = line, ':')) != NULL)
							{
								*(value++) = '\0';

								// Convert header name to lower case.
                                for(p = name; *p; ++p)
									if(*p >= 'A' && *p <= 'Z')
										(*p) += 'a' - 'A';
                                
								if(strcmp(name, "icy-metadata") == 0)
								{
									int arg;
									if(sscanf(value, " %d", &arg) == 1 && (arg == 1))
										entry->wants_metadata = 1;
								}
							}
						}
					}	// parsing lines from buffer
				}
			}

			prev_entry = entry; entry = entry->next;
			continue;

		close_connection:
			closesocket(entry->socket);
			*((prev_entry == NULL) ? &clients : &(prev_entry->next)) = entry->next;
			free(entry);
			entry = ((prev_entry == NULL) ? clients : (prev_entry->next));
			--clients_size;
		}

        ReleaseMutex(clients_mutex);

	}
    return 0;
}

static int start_server()
{
    struct sockaddr_in addr;
    ULONG true = TRUE;

    if((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        MessageBox(NULL, "Server initialization failed:\nunable to create server socket.",
            "Minicast", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // HACK: allows the server socket to be bound immediately to a port that has just been closed.
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&true, sizeof(true));

    addr.sin_family = AF_INET;
    EnterCriticalSection(&access_config);
    addr.sin_addr.s_addr = htonl(current_config.network.address);
    addr.sin_port = htons(current_config.network.port);
    LeaveCriticalSection(&access_config);
    if( bind(server_socket, (const struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(server_socket, SOMAXCONN) == SOCKET_ERROR )
    {
        closesocket(server_socket);
        server_socket = INVALID_SOCKET;
        MessageBox(NULL, "Server initialization failed:\n"
            "unable to bind server socket to TCP port.", "Minicast", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    if((server_thread = CreateThread(NULL, 0, run_server, NULL, 0, NULL)) == NULL)
    {
        MessageBox(NULL, "Server initialization failed:\nunable to create server thread.",
            "Minicast", MB_OK | MB_ICONERROR);
        return -1;
    }
        
    return 0;
}

static int stop_server()
{
    TerminateThread(server_thread, 0);  // FIXME: hack!
    server_thread = NULL;
    closesocket(server_socket);
    server_socket = INVALID_SOCKET;
    return 0;
}

void engine_get_default_config(
    engine_config_t *config )
{
    memcpy(config, &default_config, sizeof(*config));
}

void engine_get_current_config(
    engine_config_t *config )
{
    EnterCriticalSection(&access_config);
    memcpy(config, &current_config, sizeof(*config));
    LeaveCriticalSection(&access_config);
}

void engine_set_current_config(
    engine_config_t *config )
{
    BOOL restart_encoder, restart_server;
    EnterCriticalSection(&access_config);
    restart_encoder =
        config->encoder.bitrate  != current_config.encoder.bitrate  ||
        config->encoder.channels != current_config.encoder.channels ||
        config->encoder.lowpass  != current_config.encoder.lowpass  ||
        config->encoder.psycho   != current_config.encoder.psycho,
    restart_server =
        config->network.address          != current_config.network.address ||
        config->network.port             != current_config.network.port ||
        config->network.connection_limit != current_config.network.connection_limit ||
        config->stream_name              != current_config.stream_name;
    
    config->stream_name[sizeof(config->stream_name)-1] = '\0';
    memcpy(&current_config, config, sizeof(current_config));
    LeaveCriticalSection(&access_config);

    if(restart_encoder)
        stop_encoder();
    if(restart_server)
        stop_server();
    
    if(restart_server)
        start_server();
    if(restart_encoder)
        start_encoder(input_info.channels, input_info.bits_per_channel, input_info.sampling_rate);
}

unsigned engine_connections()
{
    unsigned result;
    WaitForSingleObject(clients_mutex, INFINITE);
    result = clients_size;
    ReleaseMutex(clients_mutex);
    return result;
}

void engine_update_title(
	const char *title )
{
	EnterCriticalSection(&access_metadata);
	if(strcmp(metadata_title, title) != 0)
	{
		strncpy(metadata_title, title, sizeof(metadata_title));
		metadata_title[sizeof(metadata_title)-1] = '\0';
		if(++metadata_revision == 0)
			metadata_revision = 1;
	}	
	LeaveCriticalSection(&access_metadata);
}


int engine_initialize(engine_config_t *config )
{
    int result = 0;
    WSADATA wsaData;
        
    if(WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR)
    {
        MessageBox(NULL, "Uable to initialize WinSock 2.2 API.",
            "Minicast", MB_OK | MB_ICONERROR);
        return -1;
    }

    // Initialize synchronization objects
    buffer_requested = CreateEvent(NULL, TRUE, TRUE, NULL);
    buffer_provided  = CreateEvent(NULL, TRUE, FALSE, NULL);
    buffer_flush     = CreateEvent(NULL, TRUE, FALSE, NULL);
    clients_mutex    = CreateMutex(NULL, FALSE, NULL);
    InitializeCriticalSection(&access_config);
    InitializeCriticalSection(&access_metadata);

    // Initialize shoutcast server
    if((result = start_server()) != 0)
        return result;

    // Initialize MP3 encoder
    if((result = start_encoder(2, 16, 44100)) != ME_NOERR)
        return result;
        
    // Update configuration, if neccessary
    if(config)
        engine_set_current_config(config);
        
    return 0;
}

int engine_encode( char *samples, int samples_size,
    int channels, int bits_per_channel, int sampling_rate )
{
    if( input_info.channels         != channels ||
        input_info.bits_per_channel != bits_per_channel ||
        input_info.sampling_rate    != sampling_rate )
    {
        stop_encoder();
        start_encoder(channels, bits_per_channel, sampling_rate);
    }

    // Put data in encoding buffer.
    put_buffer(samples, samples_size*(bits_per_channel/8)*channels );
    
    return ME_NOERR;    
}

int engine_cleanup()
{
    stop_encoder();
    stop_server();

    // Clean up synchronization objects
    CloseHandle(buffer_requested);
    CloseHandle(buffer_provided);
    CloseHandle(buffer_flush);
    CloseHandle(clients_mutex);
    DeleteCriticalSection(&access_config);
    DeleteCriticalSection(&access_metadata);
    
    WSACleanup();
   
    return 0;
}
