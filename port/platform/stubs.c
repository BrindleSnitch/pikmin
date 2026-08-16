/*
 * Subsystems that have no meaning off-GameCube.
 *
 * Cache maintenance and host debug I/O. Each is either a
 * genuine no-op on this target or something the port does not need. Kept
 * together because none of them will ever grow a real implementation; anything
 * that will belongs in its own file -- ARAM did, and now lives in aram.c.
 */

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
