/* Contains the implementation of the Minicast network server. */

#include "engine_internal.h"

// Include Windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

// Include standard library headers
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>


// Definitions
#define METADATA_SIZE			  (4081)	// max length of metadata packet;
#define METADATA_INTERVAL		 (16384)	//  16 kb ==  1 second @ 128 kbps
#define BUFFER_SIZE             (131072)	// 128 kb == 16 seconds @ 128 kbp;


// Function prototypes 
int start_server_thread(const network_config_t *config);
int stop_server_thread();
void server_update_title(const char *title);
unsigned server_get_connected_clients();
void server_enqueue_encoded_data(const char *data, unsigned length);

static DWORD WINAPI run_server(LPVOID unused);
static DWORD WINAPI run_client(LPVOID client_socket);


// Global variables
static network_config_t server_config;
static SOCKET server_socket;
static HANDLE server_thread, shutdown_event;

static CRITICAL_SECTION metadata_access;
static char volatile metadata_packet[METADATA_SIZE];
static unsigned volatile metadata_size;

static CRITICAL_SECTION clients_access;
static unsigned volatile clients_size;

static CRITICAL_SECTION buffer_access;
static HANDLE buffer_semaphore;
static unsigned volatile server_buffer_pos;
static volatile char server_buffer[BUFFER_SIZE];


int start_server_thread(const network_config_t *config)
{
	struct sockaddr_in server_addr;
	const int true = 1;

	// Initialize global variables
	memcpy(&server_config, config, sizeof(server_config));
	metadata_size = 0;
	clients_size = 0;
	server_socket = INVALID_SOCKET;
	server_thread = NULL;
	shutdown_event = NULL;
	buffer_semaphore = NULL;
	server_buffer_pos = 0;

	// Initialize synchronization objects
	InitializeCriticalSection(&metadata_access);
	InitializeCriticalSection(&clients_access);
	InitializeCriticalSection(&buffer_access);
	if((buffer_semaphore = CreateSemaphore(NULL, 0, config->connection_limit, NULL)) == NULL)
		goto cleanup;

	// Initialize server socket
    if((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        MessageBox(NULL, "Server initialization failed:\nunable to create server socket.",
            "Minicast", MB_OK | MB_ICONERROR);
		goto cleanup;
    }    

    // HACK: allows the server socket to be bound immediately to a port that has just been closed.
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&true, sizeof(true));

	// Bind server socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(config->address);
    server_addr.sin_port = htons(config->port);
    if( bind( server_socket, (const struct sockaddr*)&server_addr,
			  sizeof(server_addr)) == SOCKET_ERROR ||
        listen(server_socket, SOMAXCONN) == SOCKET_ERROR )
    {
        MessageBox(NULL, "Server initialization failed:\n"
            "unable to bind server socket to TCP port.", "Minicast", MB_OK | MB_ICONERROR);
		goto cleanup;
    }

	// Initialize synchronization objects
	if((shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		goto cleanup;

	// Initialize thread
    if((server_thread = CreateThread(NULL, 0, run_server, NULL, 0, NULL)) == NULL)
    {
        MessageBox(NULL, "Server initialization failed:\nunable to create server thread.",
            "Minicast", MB_OK | MB_ICONERROR);
		goto cleanup;
    }

	return 0;

cleanup:
	if(server_socket != INVALID_SOCKET)
		closesocket(server_socket);
	if(server_thread != NULL)
		CloseHandle(server_thread);
	if(shutdown_event != NULL)
		CloseHandle(shutdown_event);
	if(buffer_semaphore != NULL)
		CloseHandle(buffer_semaphore);
	DeleteCriticalSection(&metadata_access);
	DeleteCriticalSection(&clients_access);
	DeleteCriticalSection(&buffer_access);
	return -1;
}

int stop_server_thread()
{
	SetEvent(shutdown_event);

	// Make the server thread shutdown
	closesocket(server_socket);
	WaitForSingleObject(server_thread, INFINITE);

	// Make the client threads shutdown
	EnterCriticalSection(&clients_access);
	ReleaseSemaphore(buffer_semaphore, server_config.connection_limit, NULL);
	LeaveCriticalSection(&clients_access);
	while(server_get_connected_clients() > 0)
		Sleep(100);
	ResetEvent(shutdown_event);

	// Clean up synchronization objects
	CloseHandle(shutdown_event);
	CloseHandle(server_thread);
	CloseHandle(buffer_semaphore);
	DeleteCriticalSection(&metadata_access);
	DeleteCriticalSection(&clients_access);
	DeleteCriticalSection(&buffer_access);

	return 0;
}

void server_update_title(const char *title)
{
	EnterCriticalSection(&metadata_access);
	memset((char*)metadata_packet, 0, METADATA_SIZE);
	sprintf((char*)metadata_packet + 1, "StreamTitle='%.4064s';", title);
	*metadata_packet = (char)(unsigned char)
		((strlen((char*)metadata_packet + 1) + 16)/16);
	LeaveCriticalSection(&metadata_access);
}

unsigned server_get_connected_clients()
{
	unsigned result;
	EnterCriticalSection(&clients_access);
	result = clients_size;
	LeaveCriticalSection(&clients_access);
	return result;
}

void server_enqueue_encoded_data(const char *data, unsigned length)
{
	while(length > BUFFER_SIZE)
	{
		data   += BUFFER_SIZE;
		length -= BUFFER_SIZE;
	}

	EnterCriticalSection(&buffer_access);
	if(server_buffer_pos + length <= BUFFER_SIZE)
	{
		memcpy((char*)server_buffer + server_buffer_pos, data, length);
		server_buffer_pos += length;
	}
	else
	{
		unsigned first_part_size  = BUFFER_SIZE - server_buffer_pos,
				 second_part_size = length - first_part_size;
		memcpy((char*)server_buffer + server_buffer_pos, data, first_part_size);
		memcpy((char*)server_buffer, data + first_part_size, second_part_size);
		server_buffer_pos = second_part_size;
	}
	LeaveCriticalSection(&buffer_access);

	EnterCriticalSection(&clients_access);
	ReleaseSemaphore(buffer_semaphore, server_config.connection_limit, NULL);
	LeaveCriticalSection(&clients_access);
}

static DWORD WINAPI run_server(LPVOID unused)
{
	while(1)
	{
		struct sockaddr client_addr;
		int client_addr_len = sizeof(client_addr);
		SOCKET client_socket = accept(server_socket, &client_addr, &client_addr_len);

		if(client_socket == INVALID_SOCKET)
		{
			if(WaitForSingleObject(shutdown_event, 0) == WAIT_OBJECT_0)
				break;
		}
		else
		{
			HANDLE client_thread = CreateThread(
				NULL, 0, run_client, (LPVOID)client_socket, 0, NULL );
			if(client_thread == NULL)
				closesocket(client_socket);
			else
				CloseHandle(client_thread);
		}
	}

	return 0;
}

static DWORD WINAPI run_client(LPVOID client_socket)
{
	// FIXME: reading/writing should time out!

	SOCKET socket = (SOCKET)client_socket;
	char buffer[8192],		// HTTP request buffer
		 *line, *eol,		// used for parsing request buffer
		 resource[64],		// requested HTTP resource
		 response[256];		// HTTP response buffer
	int metadata = 0,		// indicates if the client wants metadata
		streaming = 0;		// indicates if the client wants audio data
	char last_metadata[METADATA_SIZE] = { 0 };	// holds last metadata packet sent
	int buffer_size = 0, received;

	// Read request (into fixed-size buffer)
	while((received = recv( socket, buffer + buffer_size,
		                    sizeof(buffer) - buffer_size - 1, 0 )) >= 0)
	{
		buffer_size += received;
		buffer[buffer_size] = '\0';

		if(strstr(buffer, "\r\n\r\n") != NULL)
			break;

		if(received == 0)
		{
			// shutdown occured before request was completed.
			received = SOCKET_ERROR;
			break;
		}
	}

	// Socket prematurely closed.
	if(received == SOCKET_ERROR)
		return 0;

	// Parse HTTP request and formulate response
	line = buffer; eol = strstr(line, "\r\n"); *eol = '\0';
	if(sscanf(line, "GET %64s", resource) < 1)
	{
		// Unsupported HTTP method used
		strcpy(response, "HTTP/1.0 501 Not Implemented\r\n\r\n");
	}	
	else
	{
		if(strcmp(resource, "/") != 0)
		{
			// Unknown resource requested
			strcpy(response, "HTTP/1.0 404 Not Found\r\n\r\n");
		}
		else
		{
			// Register client
			EnterCriticalSection(&clients_access);
			if(clients_size < server_config.connection_limit)
			{
				++clients_size;
				streaming = 1;
			}
			LeaveCriticalSection(&clients_access);

			if(!streaming)
			{
				// Server is full.
				sprintf(response, "ICY 503 Service Unavailable\015\012\015\012");
			}
			else
			{
				sprintf(response, "ICY 200 OK\r\nicy-name: %s\r\n", server_config.stream_name);

				// Parse HTTP headers
				while(1)
				{
					char *key, *value, *p;
					line = eol + 2; eol = strstr(line, "\r\n"); *eol = '\0';
					if(line == eol)
						break;

					// Separate header into key and value.
					if((value = strchr(key = line, ':')) == NULL)
						continue;
					*(value++) = '\0';
					
					// Convert key to lower case
					for(p = line; *p; ++p)
						*p = tolower(*p);

					// Parse icy-metadata header
					if(strcmp(key, "icy-metadata") == 0)
					{
						int i;
						if(sscanf(value, "%d", &i) == 1)
							metadata = i;
					}

				}

				// Add metadata interval header to response
				if(metadata)
				{
					sprintf( response + strlen(response), "icy-metaint: %d\r\n",
							METADATA_INTERVAL );
				}
			
				strcat(response, "\r\n");
			}
		}
	}

	// Disable further reading.
	shutdown(socket, SD_RECEIVE);

	// Send HTTP response
	send(socket, response, (int)strlen(response), 0);

	if(streaming)
	{
		unsigned bytes_before_metadata = METADATA_INTERVAL;
		unsigned client_buffer_pos;
		char client_buffer[BUFFER_SIZE];

		// Set client position
		EnterCriticalSection(&buffer_access);
		client_buffer_pos = server_buffer_pos;
		LeaveCriticalSection(&buffer_access);

		while(1)
		{
			unsigned bytes_available;

			// Wait for data to become available
			EnterCriticalSection(&buffer_access);
			while(server_buffer_pos == client_buffer_pos)
			{
				LeaveCriticalSection(&buffer_access);
				WaitForSingleObject(buffer_semaphore, INFINITE);
				if(WaitForSingleObject(shutdown_event, 0) == WAIT_OBJECT_0)
					goto cleanup;
				EnterCriticalSection(&buffer_access);
			}

			// Calculate the number of bytes to copy
			bytes_available = ((server_buffer_pos - client_buffer_pos) + BUFFER_SIZE)
				% BUFFER_SIZE;
			if(bytes_available > bytes_before_metadata)
				bytes_available = bytes_before_metadata;

			if(client_buffer_pos + bytes_available <= BUFFER_SIZE)
			{
				// Copy a single chunk of data
				memcpy( client_buffer, (char*)server_buffer + client_buffer_pos,
					    bytes_available );
			}
			else
			{
				// Copy a two chunks of data
				unsigned first_part_size  = BUFFER_SIZE - client_buffer_pos,
					     second_part_size = bytes_available - first_part_size;
				memcpy( client_buffer, (char*)server_buffer + client_buffer_pos,
					    first_part_size );
				memcpy( client_buffer + first_part_size, (char*)server_buffer,
					    second_part_size);
			}
			LeaveCriticalSection(&buffer_access);

			// Send data packet
			if(send(socket, client_buffer, bytes_available, 0) <= 0)
				break;

			client_buffer_pos = (client_buffer_pos + bytes_available) % BUFFER_SIZE;
			bytes_before_metadata -= bytes_available;
			if(bytes_before_metadata == 0)
			{
				EnterCriticalSection(&metadata_access);
				if(memcmp( (char*)metadata_packet,
					       last_metadata, METADATA_SIZE ) == 0)
				{
					// Metadata unchanged; send empy metadata packet
					static const char empty_metadata_packet[1] = { 0 };
					LeaveCriticalSection(&metadata_access);
					send(socket, empty_metadata_packet, sizeof(empty_metadata_packet), 0);
				}
				else
				{
					// Send updated metadata packet
					memcpy(last_metadata, (char*)metadata_packet, METADATA_SIZE);
					LeaveCriticalSection(&metadata_access);
					send(socket, last_metadata, 1 + 16*((int)*last_metadata), 0);
				}
				bytes_before_metadata = METADATA_INTERVAL;
			}
		}
	cleanup:

		// Unregister client
		EnterCriticalSection(&clients_access);
		--clients_size;
		LeaveCriticalSection(&clients_access);

	}

	closesocket(socket);

	return 0;
}

