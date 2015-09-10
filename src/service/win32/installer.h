/**
 * Serverpp Installer
 *
 * Description: Manages the installation of the service.
 * Author: Mayank Sindwani
 * Date: 2015-06-03
 */

#ifndef __INSTALLER_SVC_SPP_H__
#define __INSTALLER_SVC_SPP_H__

#include <spp\tcp.h>

// Macros.
#define SERVICE_NAME       "serverpp"
#define DISPLAY_NAME       "Server++"
#define SERVICE_DESC       "Processes and services client HTTP requests."
#define DEPENDENCIES       ""
#define SERVICE_ACCOUNT    NULL
#define SERVICE_PASSWORD   NULL

// Installer functions.
DWORD uninstall_service(void);
DWORD install_service(void);

#endif