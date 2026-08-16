/*
 * VI: frame pacing, without a display.
 *
 * The video interface's real job here is timing. The game's main loop is driven
 * by VIWaitForRetrace, and everything downstream -- animation, physics, AI --
 * advances at whatever rate that returns. Getting the rate right matters even
 * with nothing on screen, because a wrong one makes the whole game run fast or
 * slow in a way that is easy to misread as a physics bug later.
 *
 * NTSC video is 59.94 fields per second, not 60. The game was tuned against
 * that, so this paces to the real figure.
 *
 * There is no window yet: GX is unimplemented, so nothing produces pixels. The
 * framebuffer pointer and blanking flag are recorded so a display backend can
 * pick them up without this file's callers changing.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "Dolphin/vi.h"
#include "trace.h"
#include "types.h"

/* NTSC field rate: 60000/1001 Hz. */
#define VI_FIELD_NS (1000000000LL * 1001LL / 60000LL)

static VIRetraceCallback s_post_retrace;
static void* s_next_fb;
static BOOL s_black;
static u32 s_retrace_count;
static long long s_next_deadline_ns;
static int s_initialised;

static long long now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void VIInit(void)
{
	if (s_initialised) {
		return;
	}
	s_initialised      = 1;
	s_retrace_count    = 0;
	s_next_deadline_ns = now_ns() + VI_FIELD_NS;
}

void VIConfigure(const GXRenderModeObj* obj)
{
	(void)obj; /* recorded by the display backend when there is one */
}

void VIFlush(void)
{
	/* On hardware this latches pending register writes at the next retrace.
	 * Nothing is deferred here, so there is nothing to latch. */
}

void VISetNextFrameBuffer(void* fb) { s_next_fb = fb; }

void VISetBlack(BOOL isBlack) { s_black = isBlack; }

u32 VIGetRetraceCount(void) { return s_retrace_count; }

u32 VIGetDTVStatus(void)
{
	/* No progressive-capable display attached. Reporting 0 keeps the game on
	 * its interlaced path, which is the one that matches how it was tuned. */
	return 0;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)
{
	VIRetraceCallback previous = s_post_retrace;
	s_post_retrace             = callback;
	return previous;
}

void VIWaitForRetrace(void)
{
	long long now;
	struct timespec deadline;

	TRACE_HIT(TR_VI_WAIT);
	if (!s_initialised) {
		VIInit();
	}

	now = now_ns();
	if (s_next_deadline_ns <= now) {
		/* Missed the field -- we are running behind. Resynchronise to the next
		 * boundary rather than trying to catch up by spinning through frames
		 * we have already lost. */
		long long behind  = now - s_next_deadline_ns;
		TRACE_HIT(TR_VI_BEHIND);
		long long skipped = behind / VI_FIELD_NS + 1;
		s_retrace_count += (u32)skipped;
		s_next_deadline_ns += skipped * VI_FIELD_NS;
	} else {
		deadline.tv_sec  = (time_t)(s_next_deadline_ns / 1000000000LL);
		deadline.tv_nsec = (long)(s_next_deadline_ns % 1000000000LL);
		while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL) == EINTR) {
			/* a signal cut the sleep short; wait out the rest */
		}
		TRACE_HIT(TR_VI_SLEPT);
		s_retrace_count++;
		s_next_deadline_ns += VI_FIELD_NS;
	}

	if (s_post_retrace) {
		s_post_retrace(s_retrace_count);
	}
}
