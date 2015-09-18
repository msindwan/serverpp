/**
 * Serverpp process implementation
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#include <spp/process.h>

using namespace spp;

/**
 * Lock Constructor
 */
Lock::Lock(void)
{
#if defined(SPP_WINDOWS)
    InitializeCriticalSection(&m_mtx);
#endif
}

/**
 * Lock Destructor
 */
Lock::~Lock(void)
{
#if defined(SPP_WINDOWS)
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
#if defined(SPP_WINDOWS)
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
#if defined(SPP_WINDOWS)
    LeaveCriticalSection(&m_mtx);
#else
    m_mtx.unlock();
#endif
}