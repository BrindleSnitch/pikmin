/*
 * DVD reads, served from the extracted disc tree.
 *
 * The game asks for files by their path on the disc; tools/port/extract_disc.py
 * writes exactly that layout to a directory, so this maps one onto the other
 * and reads with pread(). No emulation of the drive, its queue, or its seek
 * behaviour -- reads complete synchronously and callbacks fire immediately.
 *
 * Set PIKMIN_DATA to the directory containing the extracted `files` tree.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Dolphin/dvd.h"
#include "types.h"

#define DVD_DEFAULT_ROOT "/root/pikmin-assets/files"

/* fileInfo->startAddr is a u32 the original used as a physical disc offset.
 * Nothing above this layer interprets it, so it carries the file descriptor.
 * Guarded by a sentinel so a stale or never-opened DVDFileInfo cannot be
 * mistaken for a live handle. */
#define DVD_FD_TAG   0x0FD00000u
#define DVD_FD_MASK  0x0000FFFFu
#define DVD_TAG_OF(x)  ((x) & ~DVD_FD_MASK)
#define DVD_FD_OF(x)   ((int)((x) & DVD_FD_MASK))

static char s_root[1024];
static int s_initialised;

static const char* dvd_root(void)
{
	if (!s_root[0]) {
		const char* env = getenv("PIKMIN_DATA");
		snprintf(s_root, sizeof(s_root), "%s", env && *env ? env : DVD_DEFAULT_ROOT);
	}
	return s_root;
}

void DVDInit(void)
{
	if (s_initialised) {
		return;
	}
	s_initialised = 1;

	/* Fail loudly and early. A missing asset tree otherwise surfaces much later
	 * as an unexplained failure to load something. */
	if (access(dvd_root(), R_OK | X_OK) != 0) {
		fprintf(stderr,
		        "DVDInit: cannot read asset tree at '%s' (%s)\n"
		        "  extract one with tools/port/extract_disc.py, or set PIKMIN_DATA\n",
		        dvd_root(), strerror(errno));
	}
}

BOOL DVDOpen(const char* filename, DVDFileInfo* fileInfo)
{
	char path[2048];
	const char* rel;
	int fd;
	off_t end;

	if (!filename || !fileInfo) {
		return FALSE;
	}
	if (!s_initialised) {
		DVDInit();
	}

	rel = filename;
	while (*rel == '/') { /* disc paths are absolute; the tree root is not */
		rel++;
	}
	snprintf(path, sizeof(path), "%s/%s", dvd_root(), rel);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "DVDOpen: %s (%s)\n", filename, strerror(errno));
		return FALSE;
	}
	if (fd > (int)DVD_FD_MASK) { /* would collide with the tag */
		fprintf(stderr, "DVDOpen: descriptor %d out of range\n", fd);
		close(fd);
		return FALSE;
	}

	end = lseek(fd, 0, SEEK_END);
	memset(fileInfo, 0, sizeof(*fileInfo));
	fileInfo->startAddr = DVD_FD_TAG | (u32)fd;
	fileInfo->length    = (u32)(end < 0 ? 0 : end);
	fileInfo->callback  = NULL;
	return TRUE;
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio)
{
	ssize_t got;

	(void)prio; /* no drive queue to prioritise against */

	if (!fileInfo || !addr || length < 0 || offset < 0) {
		return -1;
	}
	if (DVD_TAG_OF(fileInfo->startAddr) != DVD_FD_TAG) {
		return -1; /* not an open handle */
	}

	got = pread(DVD_FD_OF(fileInfo->startAddr), addr, (size_t)length, (off_t)offset);
	if (got < 0) {
		return -1;
	}

	/* The drive transfers whole 32-byte blocks and callers rely on the tail of a
	 * short final read being defined rather than stale. */
	if (got < length) {
		memset((char*)addr + got, 0, (size_t)(length - got));
	}

	if (fileInfo->callback) {
		fileInfo->callback((s32)got, fileInfo);
	}
	return (s32)got;
}

BOOL DVDClose(DVDFileInfo* fileInfo)
{
	if (!fileInfo || DVD_TAG_OF(fileInfo->startAddr) != DVD_FD_TAG) {
		return FALSE;
	}
	close(DVD_FD_OF(fileInfo->startAddr));
	fileInfo->startAddr = 0;
	fileInfo->length    = 0;
	return TRUE;
}

s32 DVDGetDriveStatus(void)
{
	return 0; /* ready; there is no drive to be otherwise */
}

DVDDiskID* DVDGetCurrentDiskID(void)
{
	/* GPIE01 -- Pikmin, USA, revision 1. Matches the version this port targets;
	 * the game reads this to key region and revision behaviour. */
	static DVDDiskID id;
	static int filled;

	if (!filled) {
		filled = 1;
		memset(&id, 0, sizeof(id));
		memcpy(id.gameName, "GPIE", 4);
		memcpy(id.company, "01", 2);
		id.diskNumber  = 0;
		id.gameVersion = 1;
		id.streaming   = 0;
	}
	return &id;
}
