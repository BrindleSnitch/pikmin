/*
 * Memory card, stubbed as "no card inserted".
 *
 * Saving is out of scope until the game runs, but the interface has to answer
 * consistently: every entry point reports CARD_RESULT_NOCARD, which is a state
 * the original game already handles gracefully because players genuinely do
 * play without a card in the slot. That makes this a real supported
 * configuration rather than a hole.
 *
 * When saving is implemented it should back onto a file per slot rather than
 * emulating the card's block structure; nothing above this layer cares how the
 * bytes are stored, only that read and write round-trip.
 */

#include "Dolphin/card.h"
#include "types.h"

void CARDInit(void) { }

BOOL CARDProbe(s32 channel)
{
	(void)channel;
	return FALSE;
}

s32 CARDCheck(s32 channel)
{
	(void)channel;
	return CARD_RESULT_NOCARD;
}

s32 CARDCheckAsync(s32 channel, CARDCallback callback)
{
	(void)channel;
	/* Async variants must still invoke their callback -- callers wait on it. */
	if (callback) {
		callback(channel, CARD_RESULT_NOCARD);
	}
	return CARD_RESULT_NOCARD;
}

s32 CARDMount(s32 channel, CARDMemoryCard* workArea, CARDCallback detachCallback)
{
	(void)channel, (void)workArea, (void)detachCallback;
	return CARD_RESULT_NOCARD;
}

s32 CARDMountAsync(s32 channel, CARDMemoryCard* workArea, CARDCallback detachCallback, CARDCallback attachCallback)
{
	(void)workArea, (void)detachCallback;
	if (attachCallback) {
		attachCallback(channel, CARD_RESULT_NOCARD);
	}
	return CARD_RESULT_NOCARD;
}

s32 CARDUnmount(s32 channel)
{
	(void)channel;
	return CARD_RESULT_NOCARD;
}

s32 CARDFormat(s32 channel)
{
	(void)channel;
	return CARD_RESULT_NOCARD;
}

s32 CARDFormatAsync(s32 channel, CARDCallback callback)
{
	if (callback) {
		callback(channel, CARD_RESULT_NOCARD);
	}
	return CARD_RESULT_NOCARD;
}

s32 CARDOpen(s32 channel, const char* fileName, CARDFileInfo* fileInfo)
{
	(void)channel, (void)fileName, (void)fileInfo;
	return CARD_RESULT_NOCARD;
}

s32 CARDFastOpen(s32 channel, s32 fileNo, CARDFileInfo* fileInfo)
{
	(void)channel, (void)fileNo, (void)fileInfo;
	return CARD_RESULT_NOCARD;
}

s32 CARDClose(CARDFileInfo* fileInfo)
{
	(void)fileInfo;
	return CARD_RESULT_NOCARD;
}

s32 CARDCreate(s32 channel, const char* fileName, u32 size, CARDFileInfo* fileInfo)
{
	(void)channel, (void)fileName, (void)size, (void)fileInfo;
	return CARD_RESULT_NOCARD;
}

s32 CARDRead(CARDFileInfo* fileInfo, void* addr, s32 length, s32 offset)
{
	(void)fileInfo, (void)addr, (void)length, (void)offset;
	return CARD_RESULT_NOCARD;
}

s32 CARDWrite(CARDFileInfo* fileInfo, void* addr, s32 length, s32 offset)
{
	(void)fileInfo, (void)addr, (void)length, (void)offset;
	return CARD_RESULT_NOCARD;
}

s32 CARDFastDelete(s32 channel, s32 fileNo)
{
	(void)channel, (void)fileNo;
	return CARD_RESULT_NOCARD;
}

s32 CARDRename(s32 channel, const char* oldName, const char* newName)
{
	(void)channel, (void)oldName, (void)newName;
	return CARD_RESULT_NOCARD;
}

s32 CARDGetStatus(s32 channel, s32 fileNo, CARDStat* state)
{
	(void)channel, (void)fileNo, (void)state;
	return CARD_RESULT_NOCARD;
}

s32 CARDSetStatus(s32 channel, s32 fileNo, CARDStat* state)
{
	(void)channel, (void)fileNo, (void)state;
	return CARD_RESULT_NOCARD;
}

s32 CARDGetResultCode(s32 channel)
{
	(void)channel;
	return CARD_RESULT_NOCARD;
}

s32 CARDFreeBlocks(s32 channel, s32* byteNotUsed, s32* filesNotUsed)
{
	(void)channel;
	if (byteNotUsed) {
		*byteNotUsed = 0;
	}
	if (filesNotUsed) {
		*filesNotUsed = 0;
	}
	return CARD_RESULT_NOCARD;
}

s32 CARDGetSectorSize(s32 channel, u32* size)
{
	(void)channel;
	if (size) {
		*size = 0;
	}
	return CARD_RESULT_NOCARD;
}

s32 CARDGetXferredBytes(s32 channel)
{
	(void)channel;
	return 0;
}
