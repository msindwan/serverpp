/**
 * Serverpp process implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-10
 */

#include <spp/process.h>

using namespace spp;

/**
 * Lock Constructor
 */
Lock::Lock(void)
{
#if defined(_WIN32) || defined(WIN32)
    InitializeCriticalSection(&m_mtx);
#endif
}

/**
 * Lock Destructor
 */
Lock::~Lock(void)
{
#if defined(_WIN32) || defined(WIN32)
    DeleteCriticalSection(&m_mtx);
#endif
}

/**
 * Lock::aquire
 *
 * @description Aquires the lock.
 */
void Lock::aquire(void)
{
#if defined(_WIN32) || defined(WIN32)
    EnterCriticalSection(&m_mtx);
#else
    m_mtx.lock();
#endif
}

/**
* Lock::release
*
* @description Releases the lock.
*/
void Lock::release(void)
{
#if defined(_WIN32) || defined(WIN32)
    LeaveCriticalSection(&m_mtx);
#else
    m_mtx.unlock();
#endif
}