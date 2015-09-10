/**
 * Serverpp controller implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-07-31
 */

#include "controller.h"
#include "Wtsapi32.h"

using namespace spp;
using namespace std;

static SERVICE_STATUS_HANDLE hstatus;

/**
 * Serverpp report service event.
 *
 * @description Reports a service event using windows event logs.
 * @param[out] {type}     // The event type.
 * @param[out] {message}  // The error message.
 */
static void report_svc_event(WORD type, char* message)
{
    LPCSTR strs[2] = { SERVICE_NAME, message };
    HANDLE eventsrc;

    eventsrc = RegisterEventSource(NULL, SERVICE_NAME);
    if (eventsrc)
    {
    ReportEvent(eventsrc, type, 0, 0, NULL, 2, 0, strs, NULL);
    DeregisterEventSource(eventsrc);
    }
}

/**
 * Serverpp set service status.
 *
 * @description Updates the service status code.
 * @param[in] {state} // The service state.
 * @param[in] {exit}  // The service exit code.
 * @returns           // true if successful, false otherwise
 */
static BOOL spp_svc_set_status(DWORD state, DWORD exit)
{
    static DWORD                 checkpoint = 1;
    static SERVICE_STATUS        status;

    // Set the main service information.
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwServiceSpecificExitCode = 0;

    // Update the exit status.
    status.dwCurrentState = state;
    status.dwWin32ExitCode = exit;
    status.dwWaitHint = 0;

    // Set accepted controls.
    status.dwControlsAccepted =
        state == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP;

    // Set check point.
    status.dwCheckPoint =
        (state == SERVICE_RUNNING) || (state == SERVICE_STOPPED) ? 0 : checkpoint++;

    return SetServiceStatus(hstatus, &status);
}

/**
 * Serverpp service handler.
 *
 * @description Handles service control signals.
 * @param[in] {ctrl} // The control signal.
 */
static void WINAPI spp_svc_handler(DWORD ctrl)
{
    TCPServerManager* manager;
    manager = TCPServerManager::get_manager();

    if (ctrl == SERVICE_CONTROL_STOP)
    {
        spp_svc_set_status(SERVICE_STOP_PENDING, NO_ERROR);
        manager->stop_servers();
    }
}

/**
 * Serverpp load configuration.
 *
 * @description Loads the configuration file.
 * @returns // True if successful, false otherwise.
 */
errno_t spp_svc_load_config(void)
{
    TCPServerManager* manager;

    char *conf_file,
         *mime_file;

    size_t conf_file_size,
        mime_file_size;

    jNode *temp;
    jToken *config,
        *servers,
        *server,
        *mimes,
        *mime;

    jMap* mime_map;
    jArray* server_array;
    WSADATA wsaData;
    jArgs args;

    DWORD dwSessionID;
    HANDLE hToken;

    TCHAR path[MAX_PATH];
    errno_t err;
    int i;
    
    // TODO: Define error codes.
    manager = TCPServerManager::get_manager();
    err = NO_ERROR;

    // Get session ID.
    if ((dwSessionID = WTSGetActiveConsoleSessionId()) == 0xFFFFFFFF)
    {
        return GetLastError();
    }

    // Get user token.
    if (!WTSQueryUserToken(dwSessionID, &hToken))
    {
        return GetLastError();
    }
            
    // Get the config file.
    SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, hToken, 0, path);

    CloseHandle(hToken);

    if (!SetCurrentDirectory(path))
    {
        return GetLastError();
    }

    // Initialize Winsock
    if ((err = WSAStartup(MAKEWORD(2, 2), &wsaData)) != NO_ERROR)
    {
        return err;
    }
    
    mime_file = read_file(SPP_MIME_FILE, &mime_file_size);
    if (mime_file == NULL)
    {
        return 1;
    }
        
    // Load the mime file.
    if ((mimes = jconf_json2c(mime_file, mime_file_size, &args)) == NULL)
    {
        err = 1;
        goto cleanup;
    }

    if (mimes->type != JCONF_OBJECT)
    {
        err = 1;
        goto cleanup;
    }

    mime_map = (jMap*)mimes->data;

    // Add the mime types.
    for (i = 0; i < JCONF_BUCKET_SIZE; i++)
    {
        temp = (jNode*)mime_map->buckets[i];

        while (temp)
        {
            mime = (jToken*)temp->value;
            
            if (mime->type != JCONF_STRING)
            {
                err = 1;
                goto cleanup;
            }

            manager->add_type(
                string(temp->key, temp->len),
                string((char*)mime->data, jconf_strlen((char*)mime->data, "\""))
            );
            temp = temp->next;
        }
    }

    conf_file = read_file(SPP_CONF_FILE, &conf_file_size);
    if (conf_file == NULL)
    {
        err = 1;
        goto cleanup;
    }

    // Load the configuration file.
    if ((config = jconf_json2c(conf_file, conf_file_size, &args)) == NULL)
    {
        err = 1;
        goto cleanup;
    }

    if (config->type != JCONF_OBJECT)
    {
        err = 1;
        goto cleanup;
    }

    // Get the servers token.
    if ((servers = jconf_get(config, "o", "servers")) == NULL)
    {
        err = 1;
        goto cleanup;
    }

    if (servers->type != JCONF_ARRAY)
    {
        err = 1;
        goto cleanup;
    }

    server_array = (jArray*)servers->data;

    // Populate the manager.
    try
    {
        for (i = 0; i < server_array->end; i++)
        {
            server = jconf_get(servers, "a", i);
            manager->add_server(new TCPServer(server));
        }
    }
    catch (TCPServer::TCPException e)
    {
        err = e.get_errcode();
        goto cleanup;
    }

cleanup:
    
    jconf_free_token(mimes);
    jconf_free_token(config);

    if (mime_file != NULL) free(mime_file);
    if (conf_file != NULL) free(conf_file);

    return err;
}

/**
 * Serverpp service entry point.
 *
 * @description Service entry point.
 * @param[in] {argc} // The number of args.
 * @param[in] {argv} // The arguments.
 */
static void WINAPI spp_svc_main(DWORD argc, LPTSTR* argv)
{
    list<TCPServer*>::iterator it;
    TCPServerManager* manager;
    list<TCPServer*> servers;
    char message[200];
    errno_t rtn;

    manager = TCPServerManager::get_manager();

    // Try to register the service control handler.
    if (!(hstatus = RegisterServiceCtrlHandler(SERVICE_NAME, spp_svc_handler)))
    {
        sprintf(message, "RegisterServiceCtrlHandler failed. {%d}", GetLastError());
        report_svc_event(
            EVENTLOG_ERROR_TYPE,
            message);
        
        return;
    }
    
    /* Service main body */

    spp_svc_set_status(SERVICE_START_PENDING, NO_ERROR);

    // Attempt to load the configuration file.
    if ((rtn = spp_svc_load_config()) != 0)
    {
        sprintf(message, "Failed to load the configuration file. {%d}", rtn);
        report_svc_event(
            EVENTLOG_ERROR_TYPE,
            message);

        spp_svc_set_status(SERVICE_STOPPED, rtn);
    }

    // Set the status to service running.
    spp_svc_set_status(SERVICE_RUNNING, NO_ERROR);

    // Dispatch servers and block.
    try
    {
        manager->start_servers();
        manager->wait_for_servers();
    }
    catch (TCPServer::TCPException e)
    {
        spp_svc_set_status(SERVICE_STOPPED, e.get_errcode());
    }

    // Remove servers.
    servers = manager->get_servers();
    for (it = servers.begin(); it != servers.end(); it++)
        delete (*it);

    WSACleanup();

    // Set the status to stopped.
    spp_svc_set_status(SERVICE_STOPPED, NO_ERROR);
}

/**
 * Serverpp service initialization.
 *
 * @description Starts the service control dispatcher.
 * @returns // The resulting error code. '0' if successful.
 */
DWORD spp_svc_init()
{
    TCPServerManager* manager;
    manager = TCPServerManager::get_manager();
        
    // Initialize a service table entry.
    SERVICE_TABLE_ENTRY serverpp[] =
    {
        { (char*)SERVICE_NAME, &spp_svc_main },
        { NULL, NULL }
    };

    // Start the service control dispatcher.
    if (!StartServiceCtrlDispatcher(serverpp))
    {
        manager->log(
            TCPServerManager::ERR,
            stdout,
            "(Win32) Failed to start the service control dispatcher. {%d}",
            GetLastError());

        return GetLastError();
    }

    return 0;
}