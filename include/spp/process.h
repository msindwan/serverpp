/**
 * Serverpp process
 *
 * Description: A cross platform wrapper for threading primitives.
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#ifndef __PROCESS_SPP_H__
#define __PROCESS_SPP_H__

#if defined(SPP_WINDOWS)
    #include <Windows.h>
#elif defined(__unix__)
    #include <mutex>
#endif

namespace spp
{
    /**
     * Lock: Recursive mutex wrapper.
     */
	class Lock
	{
    public:
        // Constructor / Destructor
        Lock(void);
        ~Lock(void);

    public:
        // Release and aquire
        void release(void);
        void aquire(void);

    private:
        // Data members.
#if defined(SPP_WINDOWS)
        CRITICAL_SECTION m_mtx;
#else
        std::recursive_mutex m_mtx;
#endif
	};
}

#endif