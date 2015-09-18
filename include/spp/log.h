/**
 * Serverpp Logger
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 *
 * Description: Logs messages.
 */

#ifndef __LOG_SPP_H__
#define __LOG_SPP_H__

#include "process.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

namespace spp
{
    /**
     * Logger: Logger entity with criticality levels.
     */
    class Logger
    {
    public:
        // Constructor
        Logger() {};

    public:
        enum Level
        {
            DEBUG = 0,
            INFO,
            WARNING,
            ERR,
            CRITICAL
        };

    public:
        // Logging helper functions.
        virtual bool log(Level, const char*, const char*, ...);
        virtual void log(Level, FILE*, const char*, ...);

    private:
        // Private member variables.
        Lock m_mtx;
    };
}

#endif