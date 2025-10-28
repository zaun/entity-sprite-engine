#include <stdlib.h>
#include <string.h>

/* thread support */
#if defined(ESE_THREAD_POSIX)
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#elif defined(ESE_THREAD_WIN32)
#include <intrin.h>
#include <process.h>
#include <windows.h>
#endif

#include "utility/log.h"

/* Define timeout constant for cross-platform compatibility */
#ifndef ETIMEDOUT
#define ESE_THREAD_ETIMEDOUT 60
#else
#define ESE_THREAD_ETIMEDOUT ETIMEDOUT
#endif

/* atomics support */
#if defined(__has_include)
#if __has_include(<stdatomic.h>)
#define ESE_HAVE_C11_ATOMICS 1
#include <stdatomic.h>
#endif
#endif
#ifndef ESE_HAVE_C11_ATOMICS
#if defined(_MSC_VER) || defined(ESE_THREAD_WIN32)
#include <Windows.h>
#else
#define ESE_HAVE_GNU_ATOMICS 1
#endif
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
  int32_t thread_number;
  void *ret;
  int detached; /* 0=false, 1=true */
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
 * Thread slots
 * ---------------------- */

// Thread-local slot (uninitialized = -1)
static ESE_THREAD_LOCAL int32_t tl_slot = -1;

// Global dynamic counter for slots, backed by your atomic API
static EseAtomicInt *g_next_slot_atomic = NULL;

#if defined(ESE_THREAD_POSIX)
/* once-only init */
static pthread_once_t g_slot_once = PTHREAD_ONCE_INIT;

static void ese_thread_init_slot_counter(void) {
  /* start at 1 to reserve 0 for the main thread */
  g_next_slot_atomic = ese_atomic_int_create(1);
}

static int ese_thread_ensure_slot_counter(void) {
  pthread_once(&g_slot_once, ese_thread_init_slot_counter);
  return g_next_slot_atomic != NULL;
}

#elif defined(ESE_THREAD_WIN32)
/* once-only init */
static INIT_ONCE g_slot_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK ese_thread_init_slot_counter(PINIT_ONCE once, PVOID param,
                                                  PVOID *ctx) {
  (void)once;
  (void)param;
  (void)ctx;
  /* start at 1 to reserve 0 for the main thread */
  g_next_slot_atomic = ese_atomic_int_create(1);
  return g_next_slot_atomic != NULL;
}

static int ese_thread_ensure_slot_counter(void) {
  BOOL ok = InitOnceExecuteOnce(&g_slot_once, ese_thread_init_slot_counter,
                                NULL, NULL);
  return ok && g_next_slot_atomic != NULL;
}
#endif

/* Get per-thread slot */
static int32_t _ese_thread_get_number(void) {
  if (tl_slot != -1) {
    return (int32_t)tl_slot;
  }

  if (!ese_thread_ensure_slot_counter()) {
    log_verbose("THREAD", "Failed to allocate atomic counter");
    // failed to allocate atomic counter
    tl_slot = -1;
    return (int32_t)-1;
  }

  // Fetch-and-increment returns the previous value
  int32_t slot = ese_atomic_int_fetch_add(g_next_slot_atomic, (int32_t)1);
  tl_slot = slot;
  return slot;
}

/* ----------------------
 * Threads
 * ---------------------- */

#if defined(ESE_THREAD_POSIX)

static void *thread_start_trampoline(void *ud) {
  /* ud points to a small struct with fn and user data */
  typedef struct {
    EseThreadFn fn;
    void *ud;
    EseThread self;
  } StartArg;
  StartArg *arg = (StartArg *)ud;

  /* Set thread-local pointer so ese_thread_get_number() can find this thread */
  ese_thread_set_local_ptr(arg->self);

  arg->self->thread_number = _ese_thread_get_number();
  log_verbose("THREAD", "Created thread %d", arg->self->thread_number);
  void *ret = arg->fn(arg->ud);
  arg->self->ret = ret;
  int should_free_self = arg->self->detached;
  free(arg);
  if (should_free_self) {
    free(arg->self);
  }
  return ret;
}

EseThread ese_thread_create(EseThreadFn fn, void *ud) {
  if (!fn)
    return NULL;
  EseThread th = (EseThread)malloc(sizeof(struct EseThread_s));
  if (!th)
    return NULL;
  memset(th, 0, sizeof(*th));
  th->detached = 0;
  typedef struct {
    EseThreadFn fn;
    void *ud;
    EseThread self;
  } StartArg;
  StartArg *arg = (StartArg *)malloc(sizeof(StartArg));
  arg->fn = fn;
  arg->ud = ud;
  arg->self = th;
  if (pthread_create(&th->tid, NULL, thread_start_trampoline, arg) != 0) {
    free(arg);
    free(th);
    return NULL;
  }
  return th;
}

void *ese_thread_join(EseThread th) {
  if (!th)
    return NULL;
  void *ret = NULL;
  pthread_join(th->tid, &ret);
  ret = th->ret ? th->ret : ret;
  free(th);
  return ret;
}

void ese_thread_detach(EseThread th) {
  if (!th)
    return;
  pthread_detach(th->tid);
  /* Mark as detached; the trampoline will free the struct when the thread exits
   */
  th->detached = 1;
}

EseThreadId ese_thread_current_id(void) { return pthread_self(); }
bool ese_thread_id_equal(EseThreadId a, EseThreadId b) {
  return pthread_equal(a, b);
}

#elif defined(ESE_THREAD_WIN32)

typedef struct {
  EseThreadFn fn;
  void *ud;
  EseThread self;
} WinStartArg;

static unsigned __stdcall win_thread_trampoline(void *argp) {
  WinStartArg *arg = (WinStartArg *)argp;

  /* Set thread-local pointer so ese_thread_get_number() can find this thread */
  ese_thread_set_local_ptr(arg->self);

  arg->self->thread_number = _ese_thread_get_number();
  log_verbose("THREAD", "Created thread %d", arg->self->thread_number);
  void *ret = arg->fn(arg->ud);
  arg->self->ret = ret;
  int should_free_self = arg->self->detached;
  free(arg);
  if (should_free_self) {
    free(arg->self);
  }
  return 0;
}

EseThread ese_thread_create(EseThreadFn fn, void *ud) {
  if (!fn)
    return NULL;
  EseThread th = (EseThread *)malloc(sizeof(struct EseThread_s));
  if (!th)
    return NULL;
  memset(th, 0, sizeof(*th));
  th->detached = 0;
  WinStartArg *arg = (WinStartArg *)malloc(sizeof(WinStartArg));
  arg->fn = fn;
  arg->ud = ud;
  arg->self = th;
  unsigned thread_id;
  HANDLE h = (HANDLE)_beginthreadex(NULL, 0, win_thread_trampoline, arg, 0,
                                    &thread_id);
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
  if (!th)
    return NULL;
  WaitForSingleObject(th->handle, INFINITE);
  CloseHandle(th->handle);
  void *ret = th->ret;
  free(th);
  return ret;
}

void ese_thread_detach(EseThread th) {
  if (!th)
    return;
  CloseHandle(th->handle);
  th->detached = 1;
}

EseThreadId ese_thread_current_id(void) { return GetCurrentThreadId(); }
bool ese_thread_id_equal(EseThreadId a, EseThreadId b) { return a == b; }

#endif

int32_t ese_thread_get_number() {
  void *p = ese_thread_get_local_ptr();
  if (!p)
    return 0; // main thread (no EseThread attached)

  EseThread th = (EseThread)p;
  if (!th)
    return 0;
  return (int32_t)th->thread_number;
}

/* ----------------------
 * Atomic int
 * ---------------------- */

struct EseAtomicInt_s {
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_int v;
#elif defined(ESE_THREAD_WIN32)
  volatile LONG v;
#elif defined(ESE_HAVE_GNU_ATOMICS)
  volatile int v;
#else
  volatile int v;
#endif
};

EseAtomicInt *ese_atomic_int_create(int init) {
  EseAtomicInt *a = (EseAtomicInt *)malloc(sizeof(EseAtomicInt));
  if (!a)
    return NULL;
  ese_atomic_int_init(a, init);
  return a;
}

void ese_atomic_int_destroy(EseAtomicInt *a) {
  if (!a)
    return;
  free(a);
}

void ese_atomic_int_init(EseAtomicInt *a, int init) {
  if (!a)
    return;
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_init(&a->v, init);
#elif defined(ESE_THREAD_WIN32)
  InterlockedExchange(&a->v, (LONG)init);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  __atomic_store_n(&a->v, init, __ATOMIC_SEQ_CST);
#else
  a->v = init;
#endif
}

int ese_atomic_int_load(EseAtomicInt *a) {
  if (!a)
    return 0;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_load_explicit(&a->v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32)
  return (int)InterlockedCompareExchange(&a->v, 0, 0);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  return __atomic_load_n(&a->v, __ATOMIC_SEQ_CST);
#else
  return a->v;
#endif
}

void ese_atomic_int_store(EseAtomicInt *a, int v) {
  if (!a)
    return;
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_store_explicit(&a->v, v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32)
  InterlockedExchange(&a->v, (LONG)v);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  __atomic_store_n(&a->v, v, __ATOMIC_SEQ_CST);
#else
  a->v = v;
#endif
}

int ese_atomic_int_fetch_add(EseAtomicInt *a, int v) {
  if (!a)
    return 0;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_fetch_add_explicit(&a->v, v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32)
  return (int)InterlockedExchangeAdd(&a->v, (LONG)v);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  return __atomic_fetch_add(&a->v, v, __ATOMIC_SEQ_CST);
#else
  int old = a->v;
  a->v = old + v;
  return old;
#endif
}

bool ese_atomic_int_compare_exchange(EseAtomicInt *a, int *expected,
                                     int desired) {
  if (!a || !expected)
    return false;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_compare_exchange_strong_explicit(
      &a->v, expected, desired, memory_order_seq_cst, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32)
  LONG exp = (LONG)*expected;
  LONG old = InterlockedCompareExchange(&a->v, (LONG)desired, exp);
  if (old == exp)
    return true;
  *expected = (int)old;
  return false;
#elif defined(ESE_HAVE_GNU_ATOMICS)
  return __atomic_compare_exchange_n(&a->v, expected, desired, false,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
  if (a->v == *expected) {
    a->v = desired;
    return true;
  }
  *expected = a->v;
  return false;
#endif
}

/* ----------------------
 * Atomic size_t
 * ---------------------- */

struct EseAtomicSizeT_s {
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_size_t v;
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  volatile LONGLONG v; /* 64-bit on Win64 */
#elif defined(ESE_THREAD_WIN32)
  volatile LONG v; /* 32-bit on Win32 x86 */
#elif defined(ESE_HAVE_GNU_ATOMICS)
  volatile unsigned long long v;
#else
  volatile unsigned long long v;
#endif
};

EseAtomicSizeT *ese_atomic_size_t_create(size_t init) {
  EseAtomicSizeT *a = (EseAtomicSizeT *)malloc(sizeof(EseAtomicSizeT));
  if (!a)
    return NULL;
  ese_atomic_size_t_init(a, init);
  return a;
}

void ese_atomic_size_t_destroy(EseAtomicSizeT *a) {
  if (!a)
    return;
  free(a);
}

void ese_atomic_size_t_init(EseAtomicSizeT *a, size_t init) {
  if (!a)
    return;
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_init(&a->v, init);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  InterlockedExchange64(&a->v, (LONGLONG)init);
#elif defined(ESE_THREAD_WIN32)
  InterlockedExchange((LONG *)&a->v, (LONG)init);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  __atomic_store_n(&a->v, (unsigned long long)init, __ATOMIC_SEQ_CST);
#else
  a->v = (unsigned long long)init;
#endif
}

size_t ese_atomic_size_t_load(EseAtomicSizeT *a) {
  if (!a)
    return 0;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_load_explicit(&a->v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  return (size_t)InterlockedCompareExchange64(&a->v, 0, 0);
#elif defined(ESE_THREAD_WIN32)
  return (size_t)InterlockedCompareExchange((LONG *)&a->v, 0, 0);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  return (size_t)__atomic_load_n(&a->v, __ATOMIC_SEQ_CST);
#else
  return (size_t)a->v;
#endif
}

void ese_atomic_size_t_store(EseAtomicSizeT *a, size_t v) {
  if (!a)
    return;
#if defined(ESE_HAVE_C11_ATOMICS)
  atomic_store_explicit(&a->v, v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  InterlockedExchange64(&a->v, (LONGLONG)v);
#elif defined(ESE_THREAD_WIN32)
  InterlockedExchange((LONG *)&a->v, (LONG)v);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  __atomic_store_n(&a->v, (unsigned long long)v, __ATOMIC_SEQ_CST);
#else
  a->v = (unsigned long long)v;
#endif
}

size_t ese_atomic_size_t_fetch_add(EseAtomicSizeT *a, size_t v) {
  if (!a)
    return 0;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_fetch_add_explicit(&a->v, v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  return (size_t)InterlockedExchangeAdd64(&a->v, (LONGLONG)v);
#elif defined(ESE_THREAD_WIN32)
  return (size_t)InterlockedExchangeAdd((LONG *)&a->v, (LONG)v);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  return (size_t)__atomic_fetch_add(&a->v, (unsigned long long)v,
                                    __ATOMIC_SEQ_CST);
#else
  unsigned long long old = a->v;
  a->v = old + (unsigned long long)v;
  return (size_t)old;
#endif
}

bool ese_atomic_size_t_compare_exchange(EseAtomicSizeT *a, size_t *expected,
                                        size_t desired) {
  if (!a || !expected)
    return false;
#if defined(ESE_HAVE_C11_ATOMICS)
  return atomic_compare_exchange_strong_explicit(
      &a->v, expected, desired, memory_order_seq_cst, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  LONGLONG exp = (LONGLONG)*expected;
  LONGLONG old = InterlockedCompareExchange64(&a->v, (LONGLONG)desired, exp);
  if (old == exp)
    return true;
  *expected = (size_t)old;
  return false;
#elif defined(ESE_THREAD_WIN32)
  LONG exp = (LONG)*expected;
  LONG old = InterlockedCompareExchange((LONG *)&a->v, (LONG)desired, exp);
  if (old == exp)
    return true;
  *expected = (size_t)old;
  return false;
#elif defined(ESE_HAVE_GNU_ATOMICS)
  unsigned long long exp = (unsigned long long)*expected;
  bool ok =
      __atomic_compare_exchange_n(&a->v, &exp, (unsigned long long)desired,
                                  false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  if (!ok)
    *expected = (size_t)exp;
  return ok;
#else
  unsigned long long exp = (unsigned long long)*expected;
  if (a->v == exp) {
    a->v = (unsigned long long)desired;
    return true;
  }
  *expected = (size_t)a->v;
  return false;
#endif
}

void ese_atomic_size_t_fetch_sub_inplace(EseAtomicSizeT *a, size_t v) {
  if (!a)
    return;
#if defined(ESE_HAVE_C11_ATOMICS)
  /* C11: second arg is the numeric value (size_t) */
  atomic_fetch_sub_explicit(&a->v, v, memory_order_seq_cst);
#elif defined(ESE_THREAD_WIN32) && defined(_WIN64)
  /* 64-bit Windows */
  InterlockedExchangeAdd64(&a->v, -(LONGLONG)v);
#elif defined(ESE_THREAD_WIN32)
  /* 32-bit Windows */
  InterlockedExchangeAdd((LONG *)&a->v, -(LONG)v);
#elif defined(ESE_HAVE_GNU_ATOMICS)
  /* GCC/Clang builtin */
  __atomic_fetch_sub(&a->v, (unsigned long long)v, __ATOMIC_SEQ_CST);
#else
  /* best-effort non-atomic fallback */
  a->v = (unsigned long long)((unsigned long long)a->v - (unsigned long long)v);
#endif
}

/* ----------------------
 * Mutex
 * ---------------------- */
EseMutex *ese_mutex_create(void) {
  EseMutex *m = (EseMutex *)malloc(sizeof(EseMutex));
  if (!m)
    return NULL;
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
  if (!m)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_mutex_destroy(&m->mtx);
#elif defined(ESE_THREAD_WIN32)
  DeleteCriticalSection(&m->cs);
#endif
  free(m);
}

void ese_mutex_lock(EseMutex *m) {
  if (!m)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_mutex_lock(&m->mtx);
#elif defined(ESE_THREAD_WIN32)
  EnterCriticalSection(&m->cs);
#endif
}

bool ese_mutex_trylock(EseMutex *m) {
  if (!m)
    return false;
#if defined(ESE_THREAD_POSIX)
  return (pthread_mutex_trylock(&m->mtx) == 0);
#elif defined(ESE_THREAD_WIN32)
  return TryEnterCriticalSection(&m->cs) != 0;
#endif
}

void ese_mutex_unlock(EseMutex *m) {
  if (!m)
    return;
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
  EseCond *cv = (EseCond *)malloc(sizeof(EseCond));
  if (!cv)
    return NULL;
#if defined(ESE_THREAD_POSIX)
  pthread_cond_init(&cv->cv, NULL);
#elif defined(ESE_THREAD_WIN32)
  InitializeConditionVariable(&cv->cv);
#endif
  return cv;
}

void ese_cond_destroy(EseCond *cv) {
  if (!cv)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_cond_destroy(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
  /* CONDITION_VARIABLE doesn't need explicit destroy */
  (void)cv;
#endif
  free(cv);
}

void ese_cond_wait(EseCond *cv, EseMutex *m) {
  if (!cv || !m)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_cond_wait(&cv->cv, &m->mtx);
#elif defined(ESE_THREAD_WIN32)
  SleepConditionVariableCS(&cv->cv, &m->cs, INFINITE);
#endif
}

/* returns 0 on signaled, 1 on timeout, -1 on error */
int ese_cond_wait_timeout(EseCond *cv, EseMutex *m, int ms) {
  if (!cv || !m)
    return -1;
#if defined(ESE_THREAD_POSIX)
  if (ms < 0) {
    pthread_cond_wait(&cv->cv, &m->mtx);
    return 0;
  }
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ms / 1000;
  ts.tv_nsec += (ms % 1000) * 1000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec += 1;
    ts.tv_nsec -= 1000000000;
  }
  int rc = pthread_cond_timedwait(&cv->cv, &m->mtx, &ts);
  if (rc == 0)
    return 0;
  if (rc == ESE_THREAD_ETIMEDOUT)
    return 1;
  return -1;
#elif defined(ESE_THREAD_WIN32)
  BOOL ok = SleepConditionVariableCS(&cv->cv, &m->cs, (DWORD)ms);
  if (ok)
    return 0;
  if (GetLastError() == ERROR_TIMEOUT)
    return 1;
  return -1;
#endif
}

void ese_cond_signal(EseCond *cv) {
  if (!cv)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_cond_signal(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
  WakeConditionVariable(&cv->cv);
#endif
}

void ese_cond_broadcast(EseCond *cv) {
  if (!cv)
    return;
#if defined(ESE_THREAD_POSIX)
  pthread_cond_broadcast(&cv->cv);
#elif defined(ESE_THREAD_WIN32)
  WakeAllConditionVariable(&cv->cv);
#endif
}
