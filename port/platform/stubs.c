/*
 * Subsystems that have no meaning off-GameCube.
 *
 * Cache maintenance, host debug I/O, and audio RAM DMA. Each is either a
 * genuine no-op on this target or something the port does not need. Kept
 * together because none of them will ever grow a real implementation; anything
 * that will belongs in its own file.
 */

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
void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback)
{
	(void)owner, (void)type, (void)priority, (void)source, (void)dest, (void)length;
	/* Report completion immediately: callers block waiting for the callback,
	 * and never firing it would deadlock rather than merely lose audio. */
	if (callback) {
		callback((u32)task);
	}
}
