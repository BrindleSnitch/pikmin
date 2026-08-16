/*
 * Call counters for the platform layer.
 *
 * gdb cannot resolve symbols when attaching to a running process under proot,
 * and strace goes through emulated ptrace and produces nothing, so the usual
 * ways of finding out what a spinning process is doing are unavailable here.
 * These counters are the substitute: cheap, always available, and reported by a
 * thread of our own rather than by an external tracer.
 *
 * Enabled by setting PIKMIN_TRACE in the environment.
 */

#ifndef PORT_PLATFORM_TRACE_H
#define PORT_PLATFORM_TRACE_H

enum TraceCounter {
	TR_VI_WAIT,      /* VIWaitForRetrace calls */
	TR_VI_SLEPT,     /* ... that actually slept to the deadline */
	TR_VI_BEHIND,    /* ... that found the field already missed */
	TR_DVD_OPEN,
	TR_DVD_OPEN_FAIL,
	TR_DVD_READ,
	TR_OS_LOCK,
	TR_OS_UNLOCK,
	TR_OS_WAITCOND,
	TR_OS_SIGNALCOND,
	TR_OS_SEND,
	TR_OS_SEND_BLOCK,
	TR_OS_RECV,
	TR_OS_RECV_BLOCK,
	TR_OS_YIELD,
	TR_OS_INTR_OFF,
	TR_OS_THREAD_NEW,
	TR_OS_THREAD_RESUME,
	TR_PAD_READ,
	TR_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long g_trace[TR_COUNT];
extern int g_trace_on;

void traceStart(void);

/*
 * Register a table of call counters for the reporter to print.
 *
 * Generated stub files use this to surface which entry points the game actually
 * touches. Printed periodically rather than at exit because runs here are ended
 * with SIGKILL, which no destructor or atexit handler survives.
 */
void traceRegisterTable(const char* title, unsigned long* hits, const char* const* names, int count);

#define TRACE_HIT(c)                     \
	do {                                 \
		if (g_trace_on) {                \
			__sync_fetch_and_add(&g_trace[(c)], 1UL); \
		}                                \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif
