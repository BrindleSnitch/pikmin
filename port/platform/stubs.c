/*
 * Subsystems that have no meaning off-GameCube.
 *
 * Cache maintenance, host debug I/O, and audio RAM DMA. Each is either a
 * genuine no-op on this target or something the port does not need. Kept
 * together because none of them will ever grow a real implementation; anything
 * that will belongs in its own file.
 */

#include <pthread.h>
#include <stdio.h>

#include "Dolphin/ar.h"
#include "Dolphin/hio.h"
#include "Dolphin/os.h"
#include "types.h"

/* ---------------------------------------------------------------- cache --
 * The GameCube has software-managed cache coherency between the CPU and the
 * hardware that reads its memory (GX FIFO, DVD DMA, audio DMA). Nothing on this
 * target reads our memory behind the CPU's back, so these are genuine no-ops
 * rather than unimplemented placeholders. If a future backend introduces real
 * DMA, that backend does its own synchronisation.
 */
void DCInvalidateRange(void* addr, u32 numBytes) { (void)addr, (void)numBytes; }
void DCFlushRange(void* addr, u32 numBytes) { (void)addr, (void)numBytes; }
void DCStoreRange(void* addr, u32 numBytes) { (void)addr, (void)numBytes; }

/* ------------------------------------------------------------------ HIO --
 * Host I/O talked to a development PC over the GameCube's parallel port. There
 * is no such link here. Reporting "no device" is the honest answer and is what
 * the calling code is written to cope with.
 */
BOOL HIOInit(s32 chan, HIOCallback callback)
{
	(void)chan, (void)callback;
	return FALSE;
}
BOOL HIOEnumDevices(HIOEnumCallback callback)
{
	(void)callback;
	return FALSE;
}
BOOL HIOReadMailbox(u32* word)
{
	(void)word;
	return FALSE;
}
BOOL HIOWriteMailbox(u32 word)
{
	(void)word;
	return FALSE;
}
BOOL HIOWrite(u32 addr, void* buffer, s32 size)
{
	(void)addr, (void)buffer, (void)size;
	return FALSE;
}

/* ------------------------------------------------------------------- AR --
 * Audio RAM is a separate 16MB pool the DSP could DMA from. Audio is the last
 * subsystem scheduled for this port, so these hold the interface open without
 * pretending to move data. ARInit reports zero available so nothing tries to
 * allocate out of a pool that does not exist.
 */
u32 ARInit(u32* stack_index_addr, u32 num_entries)
{
	(void)stack_index_addr, (void)num_entries;
	return 0;
}
void ARQInit(void) { }

/*
 * ARQ completions have to arrive asynchronously, on another thread.
 *
 * On hardware ARQPostRequest queues a transfer and returns; the callback runs
 * later from a DMA interrupt. Two things depend on that and both break if the
 * callback is invoked inline:
 *
 *   System::copyRamToCache finishes populating its SystemCache entry after
 *   posting, so a callback that runs inside the post reads a half-built entry
 *   -- doneDMA dereferences ->owner and crashes.
 *
 *   System::copyWaitUntilDone busy-waits on a flag that only the callback sets,
 *   with no yield in the loop, so a completion deferred to this same thread
 *   would never run and the wait would spin forever.
 *
 * A single worker thread satisfies both: the post returns immediately, the
 * caller finishes its setup and enters its spin, and the completion lands from
 * outside. No data is actually moved -- there is no ARAM -- so this reports
 * transfers it never performed, which is fine while audio is stubbed but is not
 * fine once anything reads back what it thought it cached.
 */
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
		job = s_arq_q[s_arq_head];
		s_arq_head = (s_arq_head + 1) % ARQ_QUEUE_CAP;
		s_arq_count--;
		pthread_mutex_unlock(&s_arq_lock);

		if (job.cb) {
			job.cb(job.arg);
		}
	}
	return NULL;
}

void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback)
{
	if (!task) {
		return;
	}

	/* Fill the request in, exactly as the real ARQ does. This is not
	 * bookkeeping: doneDMA is handed the request and immediately reads back
	 * task->owner to find the SystemCache it belongs to, so leaving these unset
	 * makes the completion dereference whatever happened to be in the struct.
	 * SystemCache derives from ARQRequest, which is what makes that work. */
	task->next     = NULL;
	task->owner    = owner;
	task->type     = type;
	task->priority = priority;
	task->source   = source;
	task->dest     = dest;
	task->length   = length;
	task->callback = callback;

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
		int slot        = (s_arq_head + s_arq_count) % ARQ_QUEUE_CAP;
		/* doneDMA is passed the request as its argument; the game hands the
		 * same pointer as `owner`, so prefer that and fall back to the task. */
		s_arq_q[slot].cb  = callback;
		s_arq_q[slot].arg = owner ? owner : (u32)(sptr)task;
		s_arq_count++;
		pthread_cond_signal(&s_arq_wake);
	} else {
		fprintf(stderr, "ARQPostRequest: completion queue full, dropping\n");
	}
	pthread_mutex_unlock(&s_arq_lock);
}
