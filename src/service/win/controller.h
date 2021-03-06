/**
 * Serverpp Controller
 *
 * Description: Defines the control flow for the long-running service.
 * Author: Mayank Sindwani
 * Date: 2015-07-31
 */

#ifndef __CONTROLLER_SVC_SPP_H__
#define __CONTROLLER_SVC_SPP_H__

#include "installer.h"
#include "wtsapi32.h"
#include <ShlObj.h>
#include <iostream>
#include <sstream>
#include <string>

#define SPP_SVC_LOG   "Serverpp/log.txt"
#define SPP_MIME_FILE "Serverpp/static/mime.json"
#define SPP_CONF_FILE "Serverpp/config/http.json"

// Initialization function.
DWORD spp_svc_init(void);

#endif