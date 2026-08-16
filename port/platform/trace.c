#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "trace.h"

unsigned long g_trace[TR_COUNT];
int g_trace_on;

static const char* const kNames[TR_COUNT] = {
	"VIWaitForRetrace", "  ...slept",       "  ...behind",     "DVDOpen",
	"  ...failed",      "DVDReadPrio",      "OSLockMutex",     "OSUnlockMutex",
	"OSWaitCond",       "OSSignalCond",     "OSSendMessage",   "  ...blocked",
	"OSReceiveMessage", "  ...blocked",     "OSYieldThread",   "OSDisableInterrupts",
	"OSCreateThread",   "OSResumeThread",   "PADRead",
};

/* Counter tables registered by generated stub files. */
#define MAX_TABLES 4

typedef struct {
	const char* title;
	unsigned long* hits;
	const char* const* names;
	int count;
} Table;

static Table s_tables[MAX_TABLES];
static int s_table_count;

void traceRegisterTable(const char* title, unsigned long* hits, const char* const* names, int count)
{
	if (s_table_count < MAX_TABLES) {
		s_tables[s_table_count].title = title;
		s_tables[s_table_count].hits  = hits;
		s_tables[s_table_count].names = names;
		s_tables[s_table_count].count = count;
		s_table_count++;
	}
}

static void report_tables(void)
{
	int t, i;
	for (t = 0; t < s_table_count; t++) {
		Table* tb = &s_tables[t];
		int used  = 0;
		for (i = 0; i < tb->count; i++) {
			if (tb->hits[i]) {
				used++;
			}
		}
		fprintf(stderr, "  [%s] %d of %d entry points used\n", tb->title, used, tb->count);
		for (i = 0; i < tb->count; i++) {
			if (tb->hits[i]) {
				fprintf(stderr, "      %-28s %lu\n", tb->names[i], tb->hits[i]);
			}
		}
	}
}

static void* reporter(void* unused)
{
	unsigned long prev[TR_COUNT];
	struct timespec iv;
	int round = 0;

	(void)unused;
	memset(prev, 0, sizeof(prev));
	iv.tv_sec  = 2;
	iv.tv_nsec = 0;

	for (;;) {
		int i, any = 0;
		nanosleep(&iv, NULL);
		round++;

		fprintf(stderr, "\n--- trace round %d (per 2s) ---\n", round);
		for (i = 0; i < TR_COUNT; i++) {
			unsigned long now = g_trace[i];
			unsigned long d   = now - prev[i];
			prev[i]           = now;
			if (d) {
				fprintf(stderr, "  %-20s %10lu/s   total %lu\n", kNames[i], d / 2, now);
				any = 1;
			}
		}
		if (!any) {
			fprintf(stderr, "  (nothing called -- fully blocked, not spinning)\n");
		}
		report_tables();
		fflush(stderr);
	}
	return NULL;
}

void traceStart(void)
{
	static int started;
	pthread_t t;

	if (started) {
		return;
	}
	started    = 1;
	g_trace_on = getenv("PIKMIN_TRACE") != NULL;
	if (!g_trace_on) {
		return;
	}
	if (pthread_create(&t, NULL, reporter, NULL) == 0) {
		pthread_detach(t);
		fprintf(stderr, "trace: reporting every 2s\n");
	}
}
