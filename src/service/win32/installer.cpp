/**
 * Serverpp installer implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-06-03
 */

#include "installer.h"

/**
 * Install Service
 *
 * @description Uses the service manager to install serverpp.
 * @returns // The resulting error code. '0' if successful
 */
DWORD install_service()
{
    SC_HANDLE manager = NULL, service = NULL;
    SERVICE_DESCRIPTION desc;
    char path[MAX_PATH + 2];
    DWORD rtn = 0;

    // Get the path to the binary.
    if (!(rtn = GetModuleFileName(NULL, path + 1, MAX_PATH)))
    {
        printf(
            "ERROR: Failed to get module file name {%d}.",
            rtn = GetLastError()
        );
        return rtn;
    }

    // Enclose the path in quotes.
    path[0] = path[++rtn] = '\"';
    path[rtn + 1] = '\0';

    // Attempt to open the manager.
    if (!(manager = OpenSCManager(NULL, NULL,
        SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE)))
    {
        printf(
            "ERROR: Failed to open the service manager {%d}.",
            rtn = GetLastError()
        );
        return rtn;
    }

    // Create the service.
    service = CreateService(
        manager,
        SERVICE_NAME,
        DISPLAY_NAME,
        SERVICE_QUERY_STATUS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL,
        NULL,
        DEPENDENCIES,
        SERVICE_ACCOUNT,
        SERVICE_PASSWORD
    );

    // Error handling.
    if (!service)
    {
        printf(
            "ERROR: Failed to create the serverpp service {%d}.",
            rtn = GetLastError()
        );
    }
    else
    {
        printf(
            "INFO: Successfully installed serverpp.",
            rtn = GetLastError()
        );

        service = OpenService(
            manager,
            SERVICE_NAME,
            SERVICE_CHANGE_CONFIG
        );

        // Modify the description.
        desc.lpDescription = (char*)SERVICE_DESC;

        if (!ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &desc))
        {
            printf(
                "WARNING: Failed to modify the serverpp service properties {%d}.",
                rtn = GetLastError()
            );
        }

        CloseServiceHandle(service);
    }

    CloseServiceHandle(manager);
    return rtn;
}

/**
 * Install Service
 *
 * @description Uses the service manager to remove serverpp.
 * @returns // The resulting error code. '0' if successful
 */
DWORD uninstall_service()
{
    SC_HANDLE manager = NULL, service = NULL;
    SERVICE_STATUS status;
    DWORD err = 0;

    // Attempt to open the manager.
    if (!(manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT)))
    {
        printf(
            "ERROR: Failed to open the service manager {%d}.",
            err = GetLastError()
        );
        return err;
    }

    // Attempt to open the service.
    if (!(service = OpenService(manager, SERVICE_NAME, SERVICE_STOP |
        SERVICE_QUERY_STATUS | DELETE)))
    {
        printf(
            "ERROR: Failed to open the serverpp service {%d}.",
            err = GetLastError()
        );
        return err;
    }

    // If the service is running, attempt to stop it first.
    if (ControlService(service, SERVICE_CONTROL_STOP, &status))
    {
        printf(
            "WARNING: %s is running. Please stop the service before uninstalling.",
            SERVICE_NAME);

        return SERVICE_CONTROL_STOP;
    }

    // Delete the service.
    if (!DeleteService(service))
    {
        printf(
            "ERROR: Failed to delete the serverpp service {%d}.",
            (err = GetLastError())
        );
    }
    else
    {
        printf(
            "INFO: %s was successfully uninstalled.",
            SERVICE_NAME
        );
    }

    CloseServiceHandle(manager);
    CloseServiceHandle(service);
    return 0;
}