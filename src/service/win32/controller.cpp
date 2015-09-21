/**
 * Serverpp controller implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-07-31
 */

#include "controller.h"

using namespace spp;
using namespace std;

static SERVICE_STATUS_HANDLE hstatus;

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
int spp_svc_load_config(void)
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
    jArgs args;
    WSADATA wsaData;
    int i, rtn;

    manager = TCPServerManager::get_manager();

    // Initialize Winsock and SSL.
    if ((rtn = WSAStartup(MAKEWORD(2, 2), &wsaData)) != NO_ERROR)
    {
        manager->log(Logger::ERR, SPP_SVC_LOG, "WSAStartup failed. {%d}.", rtn);
        return rtn;
    }

    // Read the mime file.
    if ((mime_file = read_file(SPP_MIME_FILE, &mime_file_size)) == NULL)
    {
        manager->log(Logger::ERR, SPP_SVC_LOG, "Failed to read %s.", SPP_MIME_FILE);
        return ERROR_FILE_NOT_FOUND;
    }
    
    // Load the mime file.
    mimes = jconf_json2c(mime_file, mime_file_size, &args);
    free(mime_file);
    
    // Validation.
    if (mimes == NULL)
    {
        manager->log(
            Logger::ERR, SPP_SVC_LOG,
            "JSON Parse error in %s: Error %d Line %d, Position %d",
            SPP_MIME_FILE, args.e, args.line, args.pos);
        
        return ERROR_BAD_CONFIGURATION;
    }

    if (mimes->type != JCONF_OBJECT)
    {
        jconf_free_token(mimes);
        manager->log(Logger::ERR, SPP_SVC_LOG, "%s: Expected an object", SPP_MIME_FILE);
        
        return ERROR_BAD_CONFIGURATION;
    }

    mime_map = (jMap*)mimes->data;

    // Add the mime types.
    for (i = 0; i < JCONF_BUCKET_SIZE; i++)
    {
        temp = (jNode*)mime_map->buckets[i];

        while (temp)
        {
            mime = (jToken*)temp->value;

            // Expect a string.
            if (mime->type != JCONF_STRING)
            {
                manager->log(
                    Logger::ERR,
                    SPP_SVC_LOG, "%s: Mime type %s is not a string",
                    SPP_MIME_FILE, temp->key);
                
                jconf_free_token(mimes);
                return ERROR_BAD_CONFIGURATION;
            }

            manager->add_type(string(temp->key, temp->len), string((char*)mime->data));
            temp = temp->next;
        }
    }

    jconf_free_token(mimes);

    // Read the configuration file.
    if ((conf_file = read_file(SPP_CONF_FILE, &conf_file_size)) == NULL)
    {
        manager->log(Logger::ERR, SPP_SVC_LOG, "Failed to read %s.", SPP_CONF_FILE);
        return ERROR_FILE_NOT_FOUND;
    }

    // Load the configuration file.
    config = jconf_json2c(conf_file, conf_file_size, &args);
    free(conf_file);
    
    if (config == NULL)
    {
        manager->log(
            Logger::ERR, SPP_SVC_LOG,
            "JSON Parse error in %s: Error %d Line %d, Position %d",
            SPP_CONF_FILE, args.e, args.line, args.pos);

        return ERROR_BAD_CONFIGURATION;
    }

    if (config->type != JCONF_OBJECT)
    {
        jconf_free_token(config);
        manager->log(Logger::ERR, SPP_SVC_LOG, "%s: Expected configuration to be an object.", SPP_CONF_FILE);

        return ERROR_BAD_CONFIGURATION;
    }

    // Get the servers token.
    if ((servers = jconf_get(config, "o", "servers")) == NULL)
    {
        jconf_free_token(config);
        manager->log(Logger::ERR, SPP_SVC_LOG, "%s: Servers not found.", SPP_CONF_FILE);

        return ERROR_BAD_CONFIGURATION;
    }

    if (servers->type != JCONF_ARRAY)
    {
        manager->log(Logger::ERR, SPP_SVC_LOG, "%s: Expected an array for servers", SPP_CONF_FILE);
        return ERROR_BAD_CONFIGURATION;
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
        jconf_free_token(config);
        manager->log(
            Logger::ERR,
            SPP_SVC_LOG,
            "%s: Server index %d - %s",
            SPP_CONF_FILE, i, e.get_error_msg());
        
        return ERROR_BAD_CONFIGURATION;
    }
    
    jconf_free_token(config);
    return ERROR_SUCCESS;
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
    int rtn;

    manager = TCPServerManager::get_manager();

    // Try to register the service control handler.
    if (!(hstatus = RegisterServiceCtrlHandler(SERVICE_NAME, spp_svc_handler)))
    {
        manager->log(
            Logger::ERR,
            SPP_SVC_LOG,
            "RegisterServiceCtrlHandler failed. {%d}.",
            GetLastError());

        return;
    }

    /* Service main body */

    spp_svc_set_status(SERVICE_START_PENDING, NO_ERROR);
    init_ssl();
    
    // Attempt to load the configuration file.
    if ((rtn = spp_svc_load_config()) != 0)
    {
        spp_svc_set_status(SERVICE_STOPPED, rtn);
        goto cleanup;
    }

    // Set the status to service running.
    spp_svc_set_status(SERVICE_RUNNING, NO_ERROR);

    // Dispatch servers and block.
    try
    {
        manager->start_servers();
        manager->wait_for_servers();

        // Set the status to stopped.
        spp_svc_set_status(SERVICE_STOPPED, NO_ERROR);
    }
    catch (TCPServer::TCPException e)
    {
        manager->log(Logger::ERR, SPP_SVC_LOG, "%s {%d}", e.get_error_msg(), e.get_errcode());
        spp_svc_set_status(SERVICE_STOPPED, e.get_errcode());
    }

    // Remove servers.
    servers = manager->get_servers();
    for (it = servers.begin(); it != servers.end(); it++)
        delete (*it);

cleanup:
    WSACleanup();
    destroy_ssl();
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
    TCHAR path[MAX_PATH];
    DWORD session_id;
    HANDLE h_token;

    manager = TCPServerManager::get_manager();

    // Initialize a service table entry.
    SERVICE_TABLE_ENTRY serverpp[] =
    {
        { (char*)SERVICE_NAME, &spp_svc_main },
        { NULL, NULL }
    };

    // Switch to the serverpp local app directory directory.
    if ((session_id = WTSGetActiveConsoleSessionId()) == 0xFFFFFFFF)
        return GetLastError();

    if (!WTSQueryUserToken(session_id, &h_token))
        return GetLastError();

    SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, h_token, 0, path);
    CloseHandle(h_token);

    if (!SetCurrentDirectory(path))
        return GetLastError();

    // Start the service control dispatcher.
    if (!StartServiceCtrlDispatcher(serverpp))
    {
        manager->log(
            TCPServerManager::ERR,
            SPP_SVC_LOG,
            "(Win32) Failed to start the service control dispatcher. {%d}",
            GetLastError());

        return GetLastError();
    }

    return 0;
}