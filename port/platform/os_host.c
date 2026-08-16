/*
 * OS: threads, synchronisation, time and the arena, on POSIX.
 *
 * The GameCube's OSThread, OSMutex, OSCond and OSMessageQueue structs have
 * fixed layouts that the original code and this port both see. Rather than
 * overlay pthread handles onto their fields -- which would depend on those
 * layouts staying put and on a pthread_t fitting where a pointer used to -- the
 * host state lives in side tables keyed by the struct's address. The game's
 * structs are then never written to by this layer at all.
 *
 * Threads are the part most likely to need revisiting. GameCube threads are
 * cooperatively scheduled and start suspended; pthreads are preemptive and
 * start running. The suspend-on-create behaviour is reproduced with a gate, but
 * code that relied on cooperative scheduling for mutual exclusion will race
 * here where it did not on hardware.
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Dolphin/os.h"
#include "types.h"

/* GameCube bus clock. The timebase runs at a quarter of it. */
u32 __OSBusClock = 162000000u;
#define OS_TIMER_CLOCK (__OSBusClock / 4u)

/* NAN as the maths headers expect to find it. */
const f32 __float_nan[1] = { 0.0f / 0.0f };

/* ------------------------------------------------------------ side table --
 * Small open-addressed map from a game struct's address to host state. Fixed
 * capacity: the game creates a handful of each, and a fixed table keeps this
 * allocation-free on paths that may run early in startup.
 */
#define TBL_CAP 512

typedef struct {
	const void* key;
	void* val;
} Slot;

typedef struct {
	Slot slots[TBL_CAP];
	pthread_mutex_t lock;
} Table;

#define TABLE_INIT { { { 0, 0 } }, PTHREAD_MUTEX_INITIALIZER }

static size_t tbl_hash(const void* key)
{
	return ((size_t)key >> 4) % TBL_CAP;
}

static void* tbl_get(Table* t, const void* key)
{
	size_t i, h = tbl_hash(key);
	void* found = NULL;
	pthread_mutex_lock(&t->lock);
	for (i = 0; i < TBL_CAP; i++) {
		Slot* s = &t->slots[(h + i) % TBL_CAP];
		if (!s->key) {
			break;
		}
		if (s->key == key) {
			found = s->val;
			break;
		}
	}
	pthread_mutex_unlock(&t->lock);
	return found;
}

static void tbl_put(Table* t, const void* key, void* val)
{
	size_t i, h = tbl_hash(key);
	pthread_mutex_lock(&t->lock);
	for (i = 0; i < TBL_CAP; i++) {
		Slot* s = &t->slots[(h + i) % TBL_CAP];
		if (!s->key || s->key == key) {
			s->key = key;
			s->val = val;
			pthread_mutex_unlock(&t->lock);
			return;
		}
	}
	pthread_mutex_unlock(&t->lock);
	fprintf(stderr, "os_host: side table full (cap %d)\n", TBL_CAP);
	abort();
}

/* --------------------------------------------------------------- arena --
 * One block standing in for the GameCube's 24MB of main memory. The game
 * carves its own heaps out of this.
 *
 * It has to live below 4GB. The heap layer stores addresses as u32 --
 * System::mHeapStart, System::mHeapEnd and AyuHeap::init(u32, u32) all do --
 * which was lossless on a 32-bit console and silently truncating here. malloc
 * on a 64-bit host returns something far above 4GB, so the heap bounds came
 * back as garbage and the first allocation wrote into nowhere.
 *
 * Mapping the arena at a fixed low address fixes every one of those sites at
 * once, rather than widening u32 address fields across the codebase and
 * changing struct layouts the matching build depends on. 0x80000000 is where
 * the GameCube's own RAM was mapped, so addresses here even resemble the
 * originals, which helps when comparing against a debugger or Dolphin.
 */
#define ARENA_SIZE (24u * 1024u * 1024u)
#define ARENA_ADDR ((void*)(uintptr_t)0x80000000u)

static char* s_arena_lo;
static char* s_arena_hi;

void OSInit(void)
{
	void* got;

	if (s_arena_lo) {
		return;
	}

	got = mmap(ARENA_ADDR, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (got == MAP_FAILED) {
		fprintf(stderr, "OSInit: could not map %u byte arena: %s\n", ARENA_SIZE, strerror(errno));
		abort();
	}

	/* Without MAP_FIXED the kernel may ignore the hint. Accept any placement
	 * that still fits in 32 bits; refuse anything that does not, because the
	 * failure mode otherwise is silent memory corruption rather than a crash. */
	if ((uintptr_t)got + ARENA_SIZE > 0xFFFFFFFFu) {
		fprintf(stderr,
		        "OSInit: arena mapped at %p, above the 4GB the heap layer can\n"
		        "  address (it stores addresses as u32). Cannot continue.\n",
		        got);
		munmap(got, ARENA_SIZE);
		abort();
	}

	s_arena_lo = (char*)got;
	s_arena_hi = s_arena_lo + ARENA_SIZE;
}

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps)
{
	(void)maxHeaps, (void)arenaEnd;
	return arenaStart;
}

void* OSGetArenaLo(void)
{
	if (!s_arena_lo) {
		OSInit();
	}
	return s_arena_lo;
}

void* OSGetArenaHi(void)
{
	if (!s_arena_lo) {
		OSInit();
	}
	return s_arena_hi;
}

/* ------------------------------------------------------------ interrupts --
 * On hardware these bracket short critical sections that could not be
 * interrupted. A no-op would leave those sections genuinely unguarded now that
 * threads are preemptive, so they take a global recursive lock instead --
 * closer to "nothing else runs", and recursive because nesting is common.
 *
 * This does assume no caller blocks while "interrupts are disabled", which
 * would have been a bug on hardware too.
 */
static pthread_mutex_t s_intr_lock;
static pthread_once_t s_intr_once = PTHREAD_ONCE_INIT;

static void intr_init(void)
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&s_intr_lock, &a);
	pthread_mutexattr_destroy(&a);
}

BOOL OSDisableInterrupts(void)
{
	pthread_once(&s_intr_once, intr_init);
	pthread_mutex_lock(&s_intr_lock);
	return TRUE;
}

BOOL OSRestoreInterrupts(BOOL enabled)
{
	pthread_once(&s_intr_once, intr_init);
	if (enabled) {
		pthread_mutex_unlock(&s_intr_lock);
	}
	return TRUE;
}

/* ----------------------------------------------------------------- mutex --*/
static Table s_mutexes = TABLE_INIT;

static pthread_mutex_t* host_mutex(OSMutex* m)
{
	pthread_mutex_t* h = (pthread_mutex_t*)tbl_get(&s_mutexes, m);
	if (!h) {
		OSInitMutex(m);
		h = (pthread_mutex_t*)tbl_get(&s_mutexes, m);
	}
	return h;
}

void OSInitMutex(OSMutex* mutex)
{
	pthread_mutexattr_t a;
	pthread_mutex_t* h;

	if (tbl_get(&s_mutexes, mutex)) {
		return;
	}
	h = (pthread_mutex_t*)malloc(sizeof(*h));
	/* GameCube mutexes are recursive: a thread already holding one may lock it
	 * again without deadlocking. */
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(h, &a);
	pthread_mutexattr_destroy(&a);
	tbl_put(&s_mutexes, mutex, h);
}

void OSLockMutex(OSMutex* mutex) { pthread_mutex_lock(host_mutex(mutex)); }
void OSUnlockMutex(OSMutex* mutex) { pthread_mutex_unlock(host_mutex(mutex)); }

/* ------------------------------------------------------------------ cond --*/
static Table s_conds = TABLE_INIT;

static pthread_cond_t* host_cond(OSCond* c)
{
	pthread_cond_t* h = (pthread_cond_t*)tbl_get(&s_conds, c);
	if (!h) {
		OSInitCond(c);
		h = (pthread_cond_t*)tbl_get(&s_conds, c);
	}
	return h;
}

void OSInitCond(OSCond* cond)
{
	pthread_cond_t* h;
	if (tbl_get(&s_conds, cond)) {
		return;
	}
	h = (pthread_cond_t*)malloc(sizeof(*h));
	pthread_cond_init(h, NULL);
	tbl_put(&s_conds, cond, h);
}

void OSSignalCond(OSCond* cond) { pthread_cond_broadcast(host_cond(cond)); }

void OSWaitCond(OSCond* cond, OSMutex* mutex) { pthread_cond_wait(host_cond(cond), host_mutex(mutex)); }

/* --------------------------------------------------------- message queue --*/
typedef struct {
	OSMessage* buf;
	s32 cap;
	s32 head;
	s32 count;
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
} HostQueue;

static Table s_queues = TABLE_INIT;

void OSInitMessageQueue(OSMessageQueue* queue, OSMessage* msgArray, s32 msgCount)
{
	HostQueue* q = (HostQueue*)tbl_get(&s_queues, queue);
	if (!q) {
		q = (HostQueue*)malloc(sizeof(*q));
		pthread_mutex_init(&q->lock, NULL);
		pthread_cond_init(&q->not_empty, NULL);
		pthread_cond_init(&q->not_full, NULL);
		tbl_put(&s_queues, queue, q);
	}
	q->buf   = msgArray; /* the caller owns the storage, as on hardware */
	q->cap   = msgCount;
	q->head  = 0;
	q->count = 0;
}

BOOL OSSendMessage(OSMessageQueue* queue, OSMessage msg, s32 flags)
{
	HostQueue* q = (HostQueue*)tbl_get(&s_queues, queue);
	if (!q || q->cap <= 0) {
		return FALSE;
	}
	pthread_mutex_lock(&q->lock);
	while (q->count == q->cap) {
		if (!(flags & OS_MESSAGE_BLOCK)) {
			pthread_mutex_unlock(&q->lock);
			return FALSE;
		}
		pthread_cond_wait(&q->not_full, &q->lock);
	}
	q->buf[(q->head + q->count) % q->cap] = msg;
	q->count++;
	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->lock);
	return TRUE;
}

BOOL OSReceiveMessage(OSMessageQueue* queue, OSMessage* msgPtr, s32 flags)
{
	HostQueue* q = (HostQueue*)tbl_get(&s_queues, queue);
	if (!q || q->cap <= 0) {
		return FALSE;
	}
	pthread_mutex_lock(&q->lock);
	while (q->count == 0) {
		if (!(flags & OS_MESSAGE_BLOCK)) {
			pthread_mutex_unlock(&q->lock);
			return FALSE;
		}
		pthread_cond_wait(&q->not_empty, &q->lock);
	}
	if (msgPtr) {
		*msgPtr = q->buf[q->head];
	}
	q->head = (q->head + 1) % q->cap;
	q->count--;
	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->lock);
	return TRUE;
}

/* --------------------------------------------------------------- threads --*/
typedef struct {
	pthread_t tid;
	OSThreadStartFunction func;
	void* param;
	void* result;
	int started;   /* released from the suspend gate */
	int cancelled;
	int joined;
	pthread_mutex_t lock;
	pthread_cond_t gate;
} HostThread;

static Table s_threads = TABLE_INIT;
static pthread_key_t s_self_key;
static pthread_once_t s_self_once = PTHREAD_ONCE_INIT;

static void self_key_init(void) { pthread_key_create(&s_self_key, NULL); }

static void* thread_trampoline(void* arg)
{
	HostThread* h = (HostThread*)arg;

	pthread_once(&s_self_once, self_key_init);
	pthread_setspecific(s_self_key, h);

	/* GameCube threads are created suspended and only run once resumed. */
	pthread_mutex_lock(&h->lock);
	while (!h->started && !h->cancelled) {
		pthread_cond_wait(&h->gate, &h->lock);
	}
	pthread_mutex_unlock(&h->lock);

	if (h->cancelled) {
		return NULL;
	}
	h->result = h->func(h->param);
	return h->result;
}

BOOL OSCreateThread(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr)
{
	HostThread* h;
	pthread_attr_t a;

	(void)stack;    /* the host allocates its own stacks */
	(void)priority; /* GameCube priorities do not map onto host scheduling */
	(void)attr;

	h = (HostThread*)calloc(1, sizeof(*h));
	if (!h) {
		return FALSE;
	}
	h->func  = func;
	h->param = param;
	pthread_mutex_init(&h->lock, NULL);
	pthread_cond_init(&h->gate, NULL);

	pthread_attr_init(&a);
	if (stackSize) {
		/* honour the requested size where the host allows it */
		size_t want = stackSize < PTHREAD_STACK_MIN ? (size_t)PTHREAD_STACK_MIN : (size_t)stackSize;
		pthread_attr_setstacksize(&a, want);
	}
	if (pthread_create(&h->tid, &a, thread_trampoline, h) != 0) {
		pthread_attr_destroy(&a);
		free(h);
		return FALSE;
	}
	pthread_attr_destroy(&a);

	tbl_put(&s_threads, thread, h);
	return TRUE;
}

s32 OSResumeThread(OSThread* thread)
{
	HostThread* h = (HostThread*)tbl_get(&s_threads, thread);
	if (!h) {
		return 0;
	}
	pthread_mutex_lock(&h->lock);
	h->started = 1;
	pthread_cond_broadcast(&h->gate);
	pthread_mutex_unlock(&h->lock);
	return 1;
}

BOOL OSJoinThread(OSThread* thread, void** val)
{
	HostThread* h = (HostThread*)tbl_get(&s_threads, thread);
	if (!h || h->joined) {
		return FALSE;
	}
	h->joined = 1;
	if (pthread_join(h->tid, NULL) != 0) {
		return FALSE;
	}
	if (val) {
		*val = h->result;
	}
	return TRUE;
}

void OSCancelThread(OSThread* thread)
{
	HostThread* h = (HostThread*)tbl_get(&s_threads, thread);
	if (!h) {
		return;
	}
	/* Only meaningful before the thread leaves the gate. Interrupting a running
	 * thread at an arbitrary point is not something pthreads can do safely, and
	 * the original relied on cooperative scheduling to make it safe. */
	pthread_mutex_lock(&h->lock);
	h->cancelled = 1;
	pthread_cond_broadcast(&h->gate);
	pthread_mutex_unlock(&h->lock);
}

void OSYieldThread(void) { sched_yield(); }

OSThread* OSGetCurrentThread(void)
{
	/* The game uses this for identity and logging, not to read thread fields. */
	pthread_once(&s_self_once, self_key_init);
	return (OSThread*)pthread_getspecific(s_self_key);
}

s32 OSCheckActiveThreads(void) { return 0; }

u32 OSGetStackPointer(void)
{
	int here;
	return (u32)(sptr)&here;
}

/* ------------------------------------------------------------------ time --*/
static OSTime now_ticks(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (OSTime)ts.tv_sec * OS_TIMER_CLOCK + (OSTime)ts.tv_nsec * OS_TIMER_CLOCK / 1000000000LL;
}

OSTime OSGetTime(void) { return now_ticks(); }
OSTick OSGetTick(void) { return (OSTick)now_ticks(); }

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td)
{
	time_t secs;
	struct tm tmv;
	OSTime rem;

	if (!td) {
		return;
	}
	secs = (time_t)(ticks / OS_TIMER_CLOCK);
	rem  = ticks % OS_TIMER_CLOCK;

	gmtime_r(&secs, &tmv);
	td->sec  = tmv.tm_sec;
	td->min  = tmv.tm_min;
	td->hour = tmv.tm_hour;
	td->mday = tmv.tm_mday;
	td->mon  = tmv.tm_mon;
	td->year = tmv.tm_year + 1900;
	td->wday = tmv.tm_wday;
	td->yday = tmv.tm_yday;
	td->msec = (int)(rem * 1000 / OS_TIMER_CLOCK);
	td->usec = (int)(rem * 1000000 / OS_TIMER_CLOCK % 1000);
}

/* --------------------------------------------------------------- console --*/
void OSReport(const char* message, ...)
{
	va_list ap;
	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
}

void OSPanic(const char* file, int line, const char* message, ...)
{
	va_list ap;
	fprintf(stderr, "OSPanic: %s:%d: ", file ? file : "?", line);
	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	fputc('\n', stderr);
	abort();
}

/* -------------------------------------------------------------- settings --*/
void OSResetSystem(int reset, u32 code, BOOL doForceMenu)
{
	(void)reset, (void)code, (void)doForceMenu;
	exit(0);
}

BOOL OSGetResetSwitchState(void) { return FALSE; }

/* Progressive scan and sound mode are console settings from the GameCube's own
 * setup screen. Report the defaults a stock console would: interlaced, stereo. */
static u32 s_progressive;
static u32 s_sound_mode = 1;

u32 OSGetProgressiveMode(void) { return s_progressive; }
void OSSetProgressiveMode(u32 on) { s_progressive = on; }
u32 OSGetSoundMode(void) { return s_sound_mode; }
void OSSetSoundMode(u32 mode) { s_sound_mode = mode; }
