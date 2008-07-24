#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "engine.h"
#include "settings.h"
#include "resource.h"

static char *bitrate_names[] = {
     "32 kbps",  "40 kbps",  "48 kbps",  "56 kbps",  "64 kbps", "80 kbps",  "96 kbps", "112 kbps",
     "128 kbps", "144 kbps", "160 kbps", "192 kbps", "224 kbps", "256 kbps", "320 kbps" };
static int bitrate_values[] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 192, 224, 256, 320 };
static unsigned bitrate_size = 15;

static char *channels_names[] = {
    "Mono", "Joint Stereo", "Full Stereo" };
static int channels_values[] = {
    CHANNELS_MONO, CHANNELS_JOINT, CHANNELS_STEREO };
static unsigned channels_size = 3;

// Module controller functions (called by Winamp)
static void *winamp_get_module(int which);
static void winamp_config( void *this_mod );
static int winamp_init( void *this_mod );
static int winamp_modify_samples( void *this_mod,
    short int *samples, int numsamples, int bps, int nch, int srate );
static void winamp_quit( void *this_mod );

// MiniCast module definition
static struct {
  char *description;
  HWND hwndParent;
  HINSTANCE hDllInstance;

  void (*Config)(void *this_mod);
  int (*Init)(void *this_mod);
  int (*ModifySamples)(void *this_mod, short int *samples, int numsamples, int bps, int nch, int srate);
  void (*Quit)(void *this_mod);

  void *userData;
  
} winamp_module_definition = {
    "MiniCast", NULL, NULL,
    &winamp_config, &winamp_init, &winamp_modify_samples, &winamp_quit
};

// MiniCast 

static struct {
  int version;
  char *description;
  void* (*getModule)(int);
} winamp_module_header = {
    0x20, "MiniCast 1.1", &winamp_get_module
};


__declspec( dllexport ) void *winampDSPGetHeader2()
{
	return &winamp_module_header;
}

BOOL WINAPI _mainCRTStartup( HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved )
{
	return TRUE;
}


static void *winamp_get_module(int which)
{
	switch (which)
	{
    case 0:
        return &winamp_module_definition;

    default:
        return NULL;
	}
}

static void winamp_config_from_dlg(HWND hwndDlg, engine_config_t *config)
{
    UINT value;
    
    GetDlgItemText( hwndDlg, IDC_STREAMNAME,
        config->stream_name, sizeof(config->stream_name) );
    
    value = (UINT)SendMessage(GetDlgItem(hwndDlg, IDC_BITRATE), CB_GETCURSEL, 0, 0);
    if(value < bitrate_size)
        config->encoder.bitrate = bitrate_values[value];
    
    value = (UINT)SendMessage(GetDlgItem(hwndDlg, IDC_CHANNELS), CB_GETCURSEL, 0, 0);
    if(value < channels_size)
        config->encoder.channels = channels_values[value];
    
    config->encoder.psycho = (short)
        SendMessage(GetDlgItem(hwndDlg, IDC_PSYCHO), BM_GETCHECK, 0, 0);

    config->encoder.lowpass = (short)
        SendMessage(GetDlgItem(hwndDlg, IDC_LOWPASS), BM_GETCHECK, 0, 0);

    value = GetDlgItemInt(hwndDlg, IDC_IPPORT, NULL, TRUE);
    if((value > 0) && (value < 65535))
        config->network.port = value;
    
    value = GetDlgItemInt(hwndDlg, IDC_CONNECTIONLIMIT, NULL, TRUE);
    if(value > 0)
        config->network.connection_limit = ((value < 1000) ?  value : 1000);
}

void winamp_config_to_dlg(HWND hwndDlg, engine_config_t *config)
{
    HWND hwndControl;
    unsigned n;

    SetDlgItemText(hwndDlg, IDC_STREAMNAME, config->stream_name);
    
    hwndControl = GetDlgItem(hwndDlg, IDC_BITRATE);
    for(n = 0; n < bitrate_size; ++n)
    {
        SendMessage(hwndControl, CB_ADDSTRING, 0, (LPARAM)bitrate_names[n]);
        if(bitrate_values[n] == config->encoder.bitrate)
            SendMessage(hwndControl, CB_SETCURSEL, n, 0);;
    }
    
    hwndControl = GetDlgItem(hwndDlg, IDC_CHANNELS);
    for(n = 0; n < channels_size; ++n)
    {
        SendMessage(hwndControl, CB_ADDSTRING, 0, (LPARAM)channels_names[n]);
        if(channels_values[n] == config->encoder.channels)
            SendMessage(hwndControl, CB_SETCURSEL, n, 0);
    }
    
    SendMessage(GetDlgItem(hwndDlg, IDC_PSYCHO), BM_SETCHECK,
        config->encoder.psycho ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessage(GetDlgItem(hwndDlg, IDC_LOWPASS), BM_SETCHECK,
        config->encoder.lowpass ? BST_CHECKED : BST_UNCHECKED, 0);

    SetDlgItemInt(hwndDlg, IDC_IPPORT, config->network.port, FALSE);

    SetDlgItemInt(hwndDlg, IDC_CONNECTIONLIMIT,  config->network.connection_limit, FALSE);
}

LRESULT CALLBACK winamp_config_dlgproc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch(uMsg)
    {
    case WM_INITDIALOG:
        {
            engine_config_t config;

            engine_get_current_config(&config);
            winamp_config_to_dlg(hwndDlg, &config);            
            
            SetDlgItemInt(hwndDlg, IDC_CONNECTIONS, engine_connections(), FALSE);
            SetTimer(hwndDlg, 1, 250, (TIMERPROC)NULL);
        } break;
        
    case WM_TIMER:
        {
            if(wParam == 1)
                SetDlgItemInt(hwndDlg, IDC_CONNECTIONS, engine_connections(), FALSE);

        } break;
    
    case WM_COMMAND:
        switch(wParam)
        {
        case IDC_APPLY:
            {
                engine_config_t config;
                engine_get_current_config(&config);
                winamp_config_from_dlg(hwndDlg, &config);
                engine_set_current_config(&config);

                engine_get_current_config(&config);
                winamp_config_to_dlg(hwndDlg, &config);
                write_config(&config);
            } break;

        case IDC_DEFAULTS:
            {
                engine_config_t config;
                engine_get_default_config(&config);
                winamp_config_to_dlg(hwndDlg, &config);
            } break;

        case IDOK:
            {
                engine_config_t config;
                engine_get_current_config(&config);
                winamp_config_from_dlg(hwndDlg, &config);
                engine_set_current_config(&config);

                engine_get_current_config(&config);
                write_config(&config);
                EndDialog(hwndDlg, 0);
            } break;

        case IDCANCEL:
            {
                EndDialog(hwndDlg, 0);
            } break;
            
        } break;
        
    case WM_DESTROY:
        {
            KillTimer(hwndDlg, 1);
        } break;
    }
    
    return 0;
}

static void winamp_config( void *this_mod )
{
    DialogBox(winamp_module_definition.hDllInstance, MAKEINTRESOURCE(IDD_CONFIG), 
        winamp_module_definition.hwndParent, winamp_config_dlgproc);
}

static void CALLBACK winamp_update_title(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	char title[512], *begin, *p, *end;

	// Check if current song title has changed.
	GetWindowText(winamp_module_definition.hwndParent, title, sizeof(title));
	if(begin = strstr(title, ". "))
	{
		begin += 2; end = begin;
		while(p = strstr(end + 1, " - Winamp"))
			end = p;
		if(begin != end)
		{
			*end = '\0';
			engine_update_title(begin);
			return;
		}
	}

	engine_update_title("");
}

static int winamp_init( void *this_mod )
{
    engine_config_t config;
	int result;

	result = engine_initialize( (read_config(&config)==0) ? &config : NULL );

	if(result == 0)
		SetTimer(NULL, 0, 200, winamp_update_title);

	return result;
}

static int winamp_modify_samples( void *this_mod,
    short int *samples, int numsamples, int bps, int nch, int srate )
{
	engine_encode((char*)samples, numsamples, nch, bps, srate);

    return numsamples;
}
static void winamp_quit( void *this_mod )
{
    engine_cleanup();
}

// header version: 0x20 == 0.20 == winamp 2.0
#define DSP_HDRVER 0x20