#include <stdlib.h>
#include <string.h>

/* Define timeout constant for cross-platform compatibility */
#ifndef ETIMEDOUT
    #define ESE_THREAD_ETIMEDOUT 60
#else
    #define ESE_THREAD_ETIMEDOUT ETIMEDOUT
#endif

#if defined(ESE_THREAD_POSIX)
    #include <pthread.h>
    #include <errno.h>
    #include <time.h>
#elif defined(ESE_THREAD_WIN32)
    #include <windows.h>
    #include <process.h>
#endif
#include "thread.h"

/* ----------------------
 * Internal structures
 * ---------------------- */
struct EseThread_s {
#if defined(ESE_THREAD_POSIX)
    pthread_t tid;
#elif defined(ESE_THREAD_WIN32)
    HANDLE handle;
    unsigned thread_id;
#endif
    void *ret;
};

struct EseMutex_s {
#if defined(ESE_THREAD_POSIX)
    pthread_mutex_t mtx;
#elif defined(ESE_THREAD_WIN32)
    CRITICAL_SECTION cs;
#endif
};

struct EseCond_s {
#if defined(ESE_THREAD_POSIX)
    pthread_cond_t cv;
#elif defined(ESE_THREAD_WIN32)
    CONDITION_VARIABLE cv;
#endif
};

/* Thread-local pointer (simple helper for asserting owner) */
static ESE_THREAD_LOCAL void *tl_local_ptr = NULL;

void ese_thread_set_local_ptr(void *p) { tl_local_ptr = p; }
void *ese_thread_get_local_ptr(void) { return tl_local_ptr; }

/* ----------------------
 * Threads
 * ---------------------- */

#if defined(ESE_THREAD_POSIX)

static void *thread_start_trampoline(void *ud) {
    /* ud points to a small struct with fn and user data */
    typedef struct { EseThreadFn fn; void *ud; EseThread self; } StartArg;
    StartArg *arg = (StartArg *)ud;
    void *ret = arg->fn(arg->ud);
    arg->self->ret = ret;
    free(arg);
    return ret;
}

EseThread ese_thread_create(EseThreadFn fn, void *ud) {
    if (!fn) return NULL;
    EseThread th = (EseThread)malloc(sizeof(struct EseThread_s));
    if (!th) return NULL;
    memset(th, 0, sizeof(*th));
    typedef struct { EseThreadFn fn; void *ud; EseThread self; } StartArg;
    StartArg *arg = (StartArg*)malloc(sizeof(StartArg));
    arg->fn = fn; arg->ud = ud; arg->self = th;
    if (pthread_create(&th->tid, NULL, thread_start_trampoline, arg) != 0) {
        free(arg);
        free(th);
        return NULL;
    }
    return th;
}

void *ese_thread_join(EseThread th) {
    if (!th) return NULL;
    void *ret = NULL;
    pthread_join(th->tid, &ret);
    ret = th->ret ? th->ret : ret;
    free(th);
    return ret;
}

void ese_thread_detach(EseThread th) {
    if (!th) return;
    pthread_detach(th->tid);
    /* keep struct allocated? We free caller's pointer responsibility.
       For simplicity free structure here. The thread still runs detached. */
    free(th);
}

EseThreadId ese_thread_current_id(void) { return pthread_self(); }
bool ese_thread_id_equal(EseThreadId a, EseThreadId b) { return pthread_equal(a, b); }

#elif defined(ESE_THREAD_WIN32)

typedef struct {
    EseThreadFn fn;
    void *ud;
    EseThread self;
} WinStartArg;

static unsigned __stdcall win_thread_trampoline(void *argp) {
    WinStartArg *arg = (WinStartArg*)argp;
    void *ret = arg->fn(arg->ud);
    arg->self->ret = ret;
    free(arg);
    return 0;
}

EseThread ese_thread_create(EseThreadFn fn, void *ud) {
    if (!fn) return NULL;
    EseThread th = (EseThread*)malloc(sizeof(struct EseThread_s));
    if (!th) return NULL;
    memset(th, 0, sizeof(*th));
    WinStartArg *arg = (WinStartArg*)malloc(sizeof(WinStartArg));
    arg->fn = fn; arg->ud = ud; arg->self = th;
    unsigned thread_id;
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, win_thread_trampoline, arg, 0, &thread_id);
    if (!h) {
        free(arg);
        free(th);
        return NULL;
    }
    th->handle = h;
    th->thread_id = thread_id;
    return th;
}

void *ese_thread_join(EseThread th) {
    if (!th) return NULL;
    WaitForSingleObject(th->handle, INFINITE);
    CloseHandle(th->handle);
    void *ret = th->ret;
    free(th);
    return ret;
}

void ese_thread_detach(EseThread th) {
    if (!th) return;
    CloseHandle(th->handle);
    free(th);
}

EseThreadId ese_thread_current_id(void) { return GetCurrentThreadId(); }
bool ese_thread_id_equal(EseThreadId a, EseThreadId b) { return a == b; }

#endif

/* ----------------------
 * Mutex
 * ---------------------- */
EseMutex *ese_mutex_create(void) {
    EseMutex *m = (EseMutex*)malloc(sizeof(EseMutex));
    if (!m) return NULL;
#if defined(ESE_THREAD_POSIX)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m->mtx, &attr);
    pthread_mutexattr_destroy(&attr);
#elif defined(ESE_THREAD_WIN32)
    InitializeCriticalSection(&m->cs);
#endif
    return m;
}

void ese_mutex_destroy(EseMutex *m) {
    if (!m) return;
#if defined(ESE_THREAD_POSIX)
    pthread_mutex_destroy(&m->mtx);
#elif defined(ESE_THREAD_WIN32)
    DeleteCriticalSection(&m->cs);
#endif
    free(m);
}

void ese_mutex_lock(EseMutex *m) {
    if (!m) return;
#if defined(ESE_THREAD_POSIX)
    pthread_mutex_lock(&m->mtx);
#elif defined(ESE_THREAD_WIN32)
    EnterCriticalSection(&m->cs);
#endif
}

bool ese_mutex_trylock(EseMutex *m) {
    if (!m) return false;
#if defined(ESE_THREAD_POSIX)
    return (pthread_mutex_trylock(&m->mtx) == 0);
#elif defined(ESE_THREAD_WIN32)
    return TryEnterCriticalSection(&m->cs) != 0;
#endif
}

void ese_mutex_unlock(EseMutex *m) {
    if (!m) return;
#if defined(ESE_THREAD_POSIX)
    pthread_mutex_unlock(&m->mtx);
#elif defined(ESE_THREAD_WIN32)
    LeaveCriticalSection(&m->cs);
#endif
}

/* ----------------------
 * Condition variable
 * ---------------------- */
EseCond *ese_cond_create(void) {
    EseCond *cv = (EseCond*)malloc(sizeof(EseCond));
    if (!cv) return NULL;
#if defined(ESE_THREAD_POSIX)
    pthread_cond_init(&cv->cv, NULL);
#elif defined(ESE_THREAD_WIN32)
    InitializeConditionVariable(&cv->cv);
#endif
    return cv;
}

void ese_cond_destroy(EseCond *cv) {
    if (!cv) return;
#if defined(ESE_THREAD_POSIX)
    pthread_cond_destroy(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
    /* CONDITION_VARIABLE doesn't need explicit destroy */
    (void)cv;
#endif
    free(cv);
}

void ese_cond_wait(EseCond *cv, EseMutex *m) {
    if (!cv || !m) return;
#if defined(ESE_THREAD_POSIX)
    pthread_cond_wait(&cv->cv, &m->mtx);
#elif defined(ESE_THREAD_WIN32)
    SleepConditionVariableCS(&cv->cv, &m->cs, INFINITE);
#endif
}

/* returns 0 on signaled, 1 on timeout, -1 on error */
int ese_cond_wait_timeout(EseCond *cv, EseMutex *m, int ms) {
    if (!cv || !m) return -1;
#if defined(ESE_THREAD_POSIX)
    if (ms < 0) { pthread_cond_wait(&cv->cv, &m->mtx); return 0; }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
    int rc = pthread_cond_timedwait(&cv->cv, &m->mtx, &ts);
    if (rc == 0) return 0;
    if (rc == ESE_THREAD_ETIMEDOUT) return 1;
    return -1;
#elif defined(ESE_THREAD_WIN32)
    BOOL ok = SleepConditionVariableCS(&cv->cv, &m->cs, (DWORD)ms);
    if (ok) return 0;
    if (GetLastError() == ERROR_TIMEOUT) return 1;
    return -1;
#endif
}

void ese_cond_signal(EseCond *cv) {
    if (!cv) return;
#if defined(ESE_THREAD_POSIX)
    pthread_cond_signal(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
    WakeConditionVariable(&cv->cv);
#endif
}

void ese_cond_broadcast(EseCond *cv) {
    if (!cv) return;
#if defined(ESE_THREAD_POSIX)
    pthread_cond_broadcast(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
    WakeAllConditionVariable(&cv->cv);
#endif
}
