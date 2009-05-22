/*****************************************************************************
 * w32thread.c : Win32 back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "libvlc.h"
#include <stdarg.h>
#include <assert.h>

static vlc_threadvar_t cancel_key;

/**
 * Per-thread cancellation data
 */
typedef struct vlc_cancel_t
{
    vlc_cleanup_t *cleaners;
#ifdef UNDER_CE
    HANDLE         cancel_event;
#endif
    bool           killable;
    bool           killed;
} vlc_cancel_t;

#ifndef UNDER_CE
# define VLC_CANCEL_INIT { NULL, true, false }
#else
# define VLC_CANCEL_INIT { NULL, NULL; true, false }
#endif

#ifdef UNDER_CE
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy);

static DWORD vlc_cancelable_wait (DWORD count, const HANDLE *handles,
                                  DWORD delay)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
    {
        /* Main thread - cannot be cancelled anyway */
        return WaitForMultipleObjects (count, handles, FALSE, delay);
    }
    HANDLE new_handles[count + 1];
    memcpy(new_handles, handles, count * sizeof(HANDLE));
    new_handles[count] = nfo->cancel_event;
    DWORD result = WaitForMultipleObjects (count + 1, new_handles, FALSE,
                                           delay);
    if (result == WAIT_OBJECT_0 + count)
    {
        vlc_cancel_self (NULL);
        return WAIT_IO_COMPLETION;
    }
    else
    {
        return result;
    }
}

DWORD SleepEx (DWORD dwMilliseconds, BOOL bAlertable)
{
    if (bAlertable)
    {
        DWORD result = vlc_cancelable_wait (0, NULL, dwMilliseconds);
        return (result == WAIT_TIMEOUT) ? 0 : WAIT_IO_COMPLETION;
    }
    else
    {
        Sleep(dwMilliseconds);
        return 0;
    }
}

DWORD WaitForSingleObjectEx (HANDLE hHandle, DWORD dwMilliseconds,
                             BOOL bAlertable)
{
    if (bAlertable)
    {
        /* The MSDN documentation specifies different return codes,
         * but in practice they are the same. We just check that it
         * remains so. */
#if WAIT_ABANDONED != WAIT_ABANDONED_0
# error Windows headers changed, code needs to be rewritten!
#endif
        return vlc_cancelable_wait (1, &hHandle, dwMilliseconds);
    }
    else
    {
        return WaitForSingleObject (hHandle, dwMilliseconds);
    }
}

DWORD WaitForMultipleObjectsEx (DWORD nCount, const HANDLE *lpHandles,
                                BOOL bWaitAll, DWORD dwMilliseconds,
                                BOOL bAlertable)
{
    if (bAlertable)
    {
        /* We do not support the bWaitAll case */
        assert (! bWaitAll);
        return vlc_cancelable_wait (nCount, lpHandles, dwMilliseconds);
    }
    else
    {
        return WaitForMultipleObjects (nCount, lpHandles, bWaitAll,
                                       dwMilliseconds);
    }
}
#endif

static vlc_mutex_t super_mutex;

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) hinstDll;
    (void) lpvReserved;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            vlc_mutex_init (&super_mutex);
            vlc_threadvar_create (&cancel_key, free);
            break;

        case DLL_PROCESS_DETACH:
            vlc_threadvar_delete( &cancel_key );
            vlc_mutex_destroy (&super_mutex);
            break;
    }
    return TRUE;
}

/*** Mutexes ***/
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    /* This creates a recursive mutex. This is OK as fast mutexes have
     * no defined behavior in case of recursive locking. */
    InitializeCriticalSection (&p_mutex->mutex);
    p_mutex->initialized = 1;
    return 0;
}

void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    InitializeCriticalSection( &p_mutex->mutex );
    p_mutex->initialized = 1;
    return 0;
}


void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    assert (InterlockedExchange (&p_mutex->initialized, -1) == 1);
    DeleteCriticalSection (&p_mutex->mutex);
}

void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
    if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
    { /* ^^ We could also lock super_mutex all the time... sluggish */
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
            vlc_mutex_init (p_mutex);
        /* FIXME: destroy the mutex some time... */
        vlc_mutex_unlock (&super_mutex);
    }
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    EnterCriticalSection (&p_mutex->mutex);
}

int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
    { /* ^^ We could also lock super_mutex all the time... sluggish */
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        if (InterlockedCompareExchange (&p_mutex->initialized, 0, 0) == 0)
            vlc_mutex_init (p_mutex);
        /* FIXME: destroy the mutex some time... */
        vlc_mutex_unlock (&super_mutex);
    }
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    return TryEnterCriticalSection (&p_mutex->mutex) ? 0 : EBUSY;
}

void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    assert (InterlockedExchange (&p_mutex->initialized, 1) == 1);
    LeaveCriticalSection (&p_mutex->mutex);
}

/*** Condition variables ***/
void vlc_cond_init( vlc_cond_t *p_condvar )
{
    /* Create a manual-reset event (manual reset is needed for broadcast). */
    *p_condvar = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (!*p_condvar)
        abort();
}

void vlc_cond_destroy (vlc_cond_t *p_condvar)
{
    CloseHandle (*p_condvar);
}

void vlc_cond_signal (vlc_cond_t *p_condvar)
{
    /* NOTE: This will cause a broadcast, that is wrong.
     * This will also wake up the next waiting thread if no threads are yet
     * waiting, which is also wrong. However both of these issues are allowed
     * by the provision for spurious wakeups. Better have too many wakeups
     * than too few (= deadlocks). */
    SetEvent (*p_condvar);
}

void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    SetEvent (*p_condvar);
}

void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    DWORD result;

    do
    {
        vlc_testcancel ();
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (*p_condvar, INFINITE, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED); /* another thread failed to cleanup! */
    assert (result != WAIT_FAILED);
    ResetEvent (*p_condvar);
}

int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    DWORD result;

    do
    {
        vlc_testcancel ();

        mtime_t total = (deadline - mdate ())/1000;
        if( total < 0 )
            total = 0;

        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (*p_condvar, delay, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED);
    assert (result != WAIT_FAILED);
    ResetEvent (*p_condvar);

    return (result == WAIT_OBJECT_0) ? 0 : ETIMEDOUT;
}

/*** Thread-specific variables (TLS) ***/
int vlc_threadvar_create (vlc_threadvar_t *p_tls, void (*destr) (void *))
{
#warning FIXME: use destr() callback and stop leaking!
    *p_tls = TlsAlloc();
    return (*p_tls == TLS_OUT_OF_INDEXES) ? EAGAIN : 0;
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    TlsFree (*p_tls);
}

/**
 * Sets a thread-local variable.
 * @param key thread-local variable key (created with vlc_threadvar_create())
 * @param value new value for the variable for the calling thread
 * @return 0 on success, a system error code otherwise.
 */
int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return TlsSetValue (key, value) ? ENOMEM : 0;
}

/**
 * Gets the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 * @return the value associated with the given variable for the calling
 * or NULL if there is no value.
 */
void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return TlsGetValue (key);
}


/*** Threads ***/
static unsigned __stdcall vlc_entry (void *data)
{
    vlc_cancel_t cancel_data = VLC_CANCEL_INIT;
    vlc_thread_t self = data;
#ifdef UNDER_CE
    cancel_data.cancel_event = self->cancel_event;
#endif

    vlc_threadvar_set (cancel_key, &cancel_data);
    self->data = self->entry (self->data);
    return 0;
}

int vlc_clone (vlc_thread_t *p_handle, void * (*entry) (void *), void *data,
               int priority)
{
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    HANDLE hThread;
    vlc_thread_t th = malloc (sizeof (*th));

    if (th == NULL)
        return ENOMEM;

    th->data = data;
    th->entry = entry;
#if defined( UNDER_CE )
    th->cancel_event = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (th->cancel_event == NULL)
    {
        free(th);
        return errno;
    }
    hThread = CreateThread (NULL, 128*1024, vlc_entry, th, CREATE_SUSPENDED, NULL);
#else
    hThread = (HANDLE)(uintptr_t)
        _beginthreadex (NULL, 0, vlc_entry, th, CREATE_SUSPENDED, NULL);
#endif

    if (hThread)
    {
#ifndef UNDER_CE
        /* Thread closes the handle when exiting, duplicate it here
         * to be on the safe side when joining. */
        if (!DuplicateHandle (GetCurrentProcess (), hThread,
                              GetCurrentProcess (), &th->handle, 0, FALSE,
                              DUPLICATE_SAME_ACCESS))
        {
            CloseHandle (hThread);
            free (th);
            return ENOMEM;
        }
#else
        th->handle = hThread;
#endif

        ResumeThread (hThread);
        if (priority)
            SetThreadPriority (hThread, priority);

        *p_handle = th;
        return 0;
    }
    free (th);
    return errno;
}

void vlc_join (vlc_thread_t handle, void **result)
{
    do
        vlc_testcancel ();
    while (WaitForSingleObjectEx (handle->handle, INFINITE, TRUE)
                                                        == WAIT_IO_COMPLETION);

    CloseHandle (handle->handle);
    if (result)
        *result = handle->data;
#ifdef UNDER_CE
    CloseHandle (handle->cancel_event);
#endif
    free (handle);
}


/*** Thread cancellation ***/

/* APC procedure for thread cancellation */
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy)
{
    (void)dummy;
    vlc_control_cancel (VLC_DO_CANCEL);
}

void vlc_cancel (vlc_thread_t thread_id)
{
#ifndef UNDER_CE
    QueueUserAPC (vlc_cancel_self, thread_id->handle, 0);
#else
    SetEvent (thread_id->cancel_event);
#endif
}

int vlc_savecancel (void)
{
    int state;

    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    state = nfo->killable;
    nfo->killable = false;
    return state;
}

void vlc_restorecancel (int state)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    assert (state == false || state == true);

    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!nfo->killable);
    nfo->killable = state != 0;
}

void vlc_testcancel (void)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (nfo->killable && nfo->killed)
    {
        for (vlc_cleanup_t *p = nfo->cleaners; p != NULL; p = p->next)
             p->proc (p->data);
#ifndef UNDER_CE
        _endthread ();
#else
        ExitThread(0);
#endif
    }
}

void vlc_control_cancel (int cmd, ...)
{
    /* NOTE: This function only modifies thread-specific data, so there is no
     * need to lock anything. */
    va_list ap;

    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    va_start (ap, cmd);
    switch (cmd)
    {
        case VLC_DO_CANCEL:
            nfo->killed = true;
            break;

        case VLC_CLEANUP_PUSH:
        {
            /* cleaner is a pointer to the caller stack, no need to allocate
             * and copy anything. As a nice side effect, this cannot fail. */
            vlc_cleanup_t *cleaner = va_arg (ap, vlc_cleanup_t *);
            cleaner->next = nfo->cleaners;
            nfo->cleaners = cleaner;
            break;
        }

        case VLC_CLEANUP_POP:
        {
            nfo->cleaners = nfo->cleaners->next;
            break;
        }
    }
    va_end (ap);
}
