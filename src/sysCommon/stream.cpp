#include "Stream.h"

#include "Common/String.h"
#include <string.h>

// operator new[] is used without this header being included.
#if defined(BUGFIX)
#include "sysNew.h"
#endif

/*
 * Disc data is big-endian, because the GameCube was.
 *
 * Every structured file the game reads -- textures, models, animations,
 * collision, layouts -- stores its multi-byte fields in that order, and the
 * original code simply read them straight into memory because no conversion was
 * needed. On a little-endian host every one of those fields comes back
 * byte-reversed: a 464x56 texture reads as 53249x14336, and the size computed
 * from it asks for 728MB.
 *
 * These four accessors are the choke point. Formats do not parse raw buffers;
 * they read field by field through readByte/readShort/readInt/readFloat (see
 * BtiHeader::read in Texture.h), so swapping here converts every one of them at
 * once rather than per format.
 *
 * Keyed on the host's byte order rather than on the compiler, so this is
 * inert wherever the host is already big-endian -- including the PowerPC
 * matching build, which is therefore untouched.
 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define STREAM_SWAP_FROM_DISC 1
#else
#define STREAM_SWAP_FROM_DISC 0
#endif

#if STREAM_SWAP_FROM_DISC
static inline short streamSwap16(short v) { return (short)__builtin_bswap16((unsigned short)v); }
static inline int streamSwap32(int v) { return (int)__builtin_bswap32((unsigned int)v); }
static inline f32 streamSwapF32(f32 v)
{
	// Reverse the bytes, not the value: punning through a union keeps the bit
	// pattern intact where a numeric conversion would not.
	union {
		f32 f;
		unsigned int u;
	} c;
	c.f = v;
	c.u = __builtin_bswap32(c.u);
	return c.f;
}
#else
static inline short streamSwap16(short v) { return v; }
static inline int streamSwap32(int v) { return v; }
static inline f32 streamSwapF32(f32 v) { return v; }
#endif

/**
 * @todo: Documentation
 */
int Stream::readInt()
{
	int i;
	read(&i, sizeof(int));
	return streamSwap32(i);
}

/**
 * @todo: Documentation
 */
u8 Stream::readByte()
{
	u8 c;
	read(&c, sizeof(u8));
	return c;
}

/**
 * @todo: Documentation
 */
short Stream::readShort()
{
	short s;
	read(&s, sizeof(short));
	return streamSwap16(s);
}

/**
 * @todo: Documentation
 */
f32 Stream::readFloat()
{
	f32 f;
	read(&f, sizeof(f32));
	return streamSwapF32(f);
}

/**
 * @todo: Documentation
 */
char* Stream::readString()
{
	int size = readInt();

	char* str = new char[size + 1];
	read(str, size);
	str[size] = '\0';
	return str;
}

/**
 * @todo: Documentation
 */
void Stream::readString(char* dest, int size)
{
	String str(dest, size);
	readString(str);
}

/**
 * @todo: Documentation
 */
void Stream::readString(String& str)
{
	int size = readInt();
	if (str.mLength < size) {
		str.init(size);
	}

	read(str.mString, size);
	str.mString[size] = '\0';
}

/**
 * @todo: Documentation
 */
void Stream::writeInt(int i)
{
	int result = i;
#ifdef WIN32
	result = (((result & 0xFF000000) >> 24) | ((result & 0xFF0000) >> 8) | ((result & 0xFF00) << 8) | (result << 24));
#endif
	write(&result, sizeof(result));
}

/**
 * @todo: Documentation
 */
void Stream::writeByte(u8 c)
{
	write(&c, sizeof(u8));
}

/**
 * @todo: Documentation
 */
void Stream::writeShort(short _s)
{
	short s = _s;
#ifdef WIN32
	s = (((s & 0xFF00) >> 8) | (s << 8));
#endif
	write(&s, sizeof(short));
}

/**
 * @todo: Documentation
 */
void Stream::writeFloat(f32 f)
{
	f32 result = f;
#ifdef WIN32
	int c  = reinterpret_cast<int&>(result);
	result = ((u8)c << 24) | ((c & 0xFF00) << 8) | ((c & 0xFF0000) >> 8) | ((c & 0xFF000000) >> 24);
#endif
	write(&result, sizeof(f32));
}

/**
 * @todo: Documentation
 */
void Stream::writeString(immut char* str)
{
	// `String` can't decide if it wants to be owning or non-owning.
	String s(const_cast<char*>(str), 0);
	writeString(s);
}

/**
 * @todo: Documentation
 */
void Stream::writeString(immut String& s)
{
	s32 length = ALIGN_NEXT(s.getLength(), 4);
	writeInt(length);
	write(s.mString, s.getLength());

	char c = 0;
	for (s32 i = 0; i < length - s.getLength(); i++) {
		write(&c, 1);
	}
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 0000C4 (Matching by size)
 */
void Stream::print(immut char* fmt, ...)
{
	char dest[1024];
	va_list args;
	va_start(args, fmt);
	vsprintf(dest, fmt, args);
	va_end(args);
	if (strlen(dest)) {
		write(dest, strlen(dest));
	}
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 000064 (Matching by size)
 */
void Stream::vPrintf(immut char* param_1, va_list args)
{
	char dest[1024];
	vsprintf(dest, param_1, args);
	if (strlen(dest) != 0) {
		write(dest, strlen(dest));
	}
}

/**
 * @todo: Documentation
 */
void Stream::read(void*, int)
{
}

/**
 * @todo: Documentation
 */
void Stream::write(immut void*, int)
{
}

/**
 * @todo: Documentation
 */
int Stream::getPending()
{
	return 0;
}

/**
 * @todo: Documentation
 */
int Stream::getAvailable()
{
	return 0;
}

/**
 * @todo: Documentation
 */
void Stream::close()
{
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 00006C
 */
void RandomAccessStream::writeTo(int position, immut void* buffer, int length)
{
	setPosition(position);
	write(buffer, length);
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 00006C (Matching by size)
 */
void RandomAccessStream::readFrom(int position, void* buffer, int length)
{
	setPosition(position);
	read(buffer, length);
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 00005C (Matching by size)
 */
void RandomAccessStream::writeIntTo(int position, int value)
{
	setPosition(position);
	writeInt(value);
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 00004C (Matching by size)
 */
int RandomAccessStream::readIntFrom(int position)
{
	setPosition(position);
	int value = readInt();
	return value;
}
