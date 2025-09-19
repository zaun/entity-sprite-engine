#ifndef ESE_THREAD_H
#define ESE_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
#define ESE_THREAD_WIN32 1
#else
#define ESE_THREAD_POSIX 1
#endif

/* Opaque types */
typedef struct EseThread_s *EseThread;
typedef struct EseMutex_s  EseMutex;
typedef struct EseCond_s   EseCond;

/* Thread id type */
#if defined(ESE_THREAD_POSIX)
  #include <pthread.h>
  typedef pthread_t EseThreadId;
#elif defined(ESE_THREAD_WIN32)
  #include <windows.h>
  typedef DWORD EseThreadId;
#endif

/* Thread-local storage macro */
#if defined(_MSC_VER)
  #define ESE_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
  #define ESE_THREAD_LOCAL __thread
#else
  #define ESE_THREAD_LOCAL _Thread_local
#endif

/* Thread function signature */
typedef void *(*EseThreadFn)(void *ud);

/* Thread operations */
EseThread ese_thread_create(EseThreadFn fn, void *ud);
void *ese_thread_join(EseThread th); /* returns thread fn return value, or NULL */
void ese_thread_detach(EseThread th);
EseThreadId ese_thread_current_id(void);
bool ese_thread_id_equal(EseThreadId a, EseThreadId b);

/* Mutex */
EseMutex *ese_mutex_create(void);
void ese_mutex_destroy(EseMutex *m);
void ese_mutex_lock(EseMutex *m);
bool ese_mutex_trylock(EseMutex *m);
void ese_mutex_unlock(EseMutex *m);

/* Condition variable */
EseCond *ese_cond_create(void);
void ese_cond_destroy(EseCond *cv);
void ese_cond_wait(EseCond *cv, EseMutex *m);            /* wait until signaled */
int  ese_cond_wait_timeout(EseCond *cv, EseMutex *m, int ms); /* returns 0=signaled, 1=timeout, -1=error */
void ese_cond_signal(EseCond *cv);
void ese_cond_broadcast(EseCond *cv);

/* Helper: set/get thread-local current engine pointer (optional) */
void ese_thread_set_local_ptr(void *p);
void *ese_thread_get_local_ptr(void);

#ifdef __cplusplus
}
#endif

#endif /* ESE_THREAD_H */