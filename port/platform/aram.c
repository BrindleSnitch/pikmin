/*
 * ARAM: the GameCube's 16MB of auxiliary audio RAM, as a plain buffer.
 *
 * This is not just an audio concern, which is why it cannot stay stubbed until
 * audio is tackled. The game streams archives into ARAM and reads their
 * contents back out again: System::parseArchiveDirectory pushes blo_eng.arc
 * across with copyRamToCache, and AramStream::read pulls individual files back
 * with copyCacheToRam. A stub that reports transfers without performing them
 * therefore does not merely lose sound -- every file inside an archive reads
 * back as nothing, screens load with no panes, and P2DScreen::search returns a
 * null the caller does not check.
 *
 * The CPU cannot address ARAM directly on hardware; it is reached only through
 * DMA, and addresses in it are offsets from zero rather than pointers. That is
 * reproduced here: a flat buffer indexed by offset, with main-RAM addresses
 * converted to pointers (valid because the arena and the executable both live
 * below 4GB -- see OSInit and the -no-pie link).
 *
 * Completions still arrive on a worker thread. See the note in ARQPostRequest:
 * callers finish populating their SystemCache after posting and then busy-wait
 * for the callback, so it must not run inline or on the calling thread.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Dolphin/ar.h"
#include "types.h"

#define ARAM_SIZE (16u * 1024u * 1024u)

static u8* s_aram;

/* ------------------------------------------------------ completion queue --*/
#define ARQ_QUEUE_CAP 256

typedef struct {
	ARQCallback cb;
	u32 arg;
} ArqDone;

static ArqDone s_arq_q[ARQ_QUEUE_CAP];
static int s_arq_head, s_arq_count;
static pthread_mutex_t s_arq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_arq_wake  = PTHREAD_COND_INITIALIZER;
static int s_arq_started;

static void* arq_worker(void* unused)
{
	(void)unused;
	for (;;) {
		ArqDone job;
		pthread_mutex_lock(&s_arq_lock);
		while (s_arq_count == 0) {
			pthread_cond_wait(&s_arq_wake, &s_arq_lock);
		}
		job        = s_arq_q[s_arq_head];
		s_arq_head = (s_arq_head + 1) % ARQ_QUEUE_CAP;
		s_arq_count--;
		pthread_mutex_unlock(&s_arq_lock);

		if (job.cb) {
			job.cb(job.arg);
		}
	}
	return NULL;
}

/* ----------------------------------------------------------------- API --*/
u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
	(void)stack_index_addr, (void)num_entries;
	if (!s_aram) {
		s_aram = (u8*)calloc(1, ARAM_SIZE);
		if (!s_aram) {
			fprintf(stderr, "ARInit: could not reserve %u bytes of ARAM\n", ARAM_SIZE);
			abort();
		}
	}
	return 0;
}

void ARQInit(void)
{
	if (!s_aram) {
		ARInit(NULL, 0);
	}
}

static int aram_range_ok(const char* what, u32 offset, u32 length)
{
	if ((u64)offset + (u64)length > (u64)ARAM_SIZE) {
		fprintf(stderr, "ARQ: %s past end of ARAM (offset %u, length %u, size %u)\n", what, offset, length, ARAM_SIZE);
		return 0;
	}
	return 1;
}

void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback)
{
	(void)priority;

	if (!task) {
		return;
	}
	if (!s_aram) {
		ARInit(NULL, 0);
	}

	/* Fill the request in, as the real ARQ does. doneDMA is handed the request
	 * and reads task->owner back to find the SystemCache it belongs to --
	 * SystemCache derives from ARQRequest -- so leaving these unset makes the
	 * completion dereference whatever the struct happened to contain. */
	task->next     = NULL;
	task->owner    = owner;
	task->type     = type;
	task->priority = priority;
	task->source   = source;
	task->dest     = dest;
	task->length   = length;
	task->callback = callback;

	if (length) {
		if (type == ARQ_TYPE_MRAM_TO_ARAM) {
			if (aram_range_ok("write", dest, length)) {
				memcpy(s_aram + dest, (const void*)(uintptr_t)source, length);
			}
		} else if (type == ARQ_TYPE_ARAM_TO_MRAM) {
			if (aram_range_ok("read", source, length)) {
				memcpy((void*)(uintptr_t)dest, s_aram + source, length);
			}
		} else {
			fprintf(stderr, "ARQ: unknown transfer type %u\n", type);
		}
	}

	if (!callback) {
		return;
	}

	pthread_mutex_lock(&s_arq_lock);
	if (!s_arq_started) {
		pthread_t t;
		s_arq_started = 1;
		if (pthread_create(&t, NULL, arq_worker, NULL) == 0) {
			pthread_detach(t);
		}
	}
	if (s_arq_count < ARQ_QUEUE_CAP) {
		int slot          = (s_arq_head + s_arq_count) % ARQ_QUEUE_CAP;
		s_arq_q[slot].cb  = callback;
		s_arq_q[slot].arg = owner ? owner : (u32)(sptr)task;
		s_arq_count++;
		pthread_cond_signal(&s_arq_wake);
	} else {
		fprintf(stderr, "ARQPostRequest: completion queue full, dropping\n");
	}
	pthread_mutex_unlock(&s_arq_lock);
}
