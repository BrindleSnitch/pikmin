/*
 * Application-level entry points the decompilation declares but never defines.
 *
 * System::sleep is marked "unused/inlined" in system.h: the original compiler
 * inlined it everywhere it appeared, so no out-of-line body survived into the
 * decompilation to be recovered. It still has to exist for anything that calls
 * it through a non-inlined path to link.
 */

#include <time.h>

#include "system.h"
#include "types.h"

void System::sleep(f32 seconds)
{
	struct timespec deadline;
	long nsec;

	if (seconds <= 0.0f) {
		return;
	}

	/* Sleep to an absolute deadline rather than for a duration.
	 *
	 * This file compiles against include/stl, whose <errno.h> shim shadows the
	 * real one: errno resolves to a plain global rather than the thread-local
	 * the C library defines, and EINTR is not defined at all. Working to a
	 * deadline avoids needing either. A signal cutting the sleep short just
	 * means going round again against the same deadline, so the loop ends when
	 * the time genuinely arrives -- and the full interval is honoured, which
	 * matters because callers use this for pacing. */
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += (time_t)seconds;
	nsec = deadline.tv_nsec + (long)((seconds - (f32)(time_t)seconds) * 1000000000.0f);
	if (nsec >= 1000000000L) {
		nsec -= 1000000000L;
		deadline.tv_sec += 1;
	}
	deadline.tv_nsec = nsec;

	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL) != 0) {
	}
}
