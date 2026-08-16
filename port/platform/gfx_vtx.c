/*
 * Vertex pipeline: GX display lists to GLES3 draws.
 *
 * The measured workload says this is where the drawing is. Over 363 frames the
 * game made 438,744 GXCallDisplayList calls against 3,147 GXBegin, so geometry
 * arrives almost entirely as pre-recorded command streams handed over as a
 * pointer and a length, not as immediate-mode writes into the write-gather
 * FIFO. A buffer we can parse deterministically is a far better starting point
 * than a register firehose.
 *
 * A GX display list is a byte stream of:
 *
 *   opcode                       one byte
 *   vertex count                 two bytes, big-endian, for draw opcodes
 *   vertex data                  count * (size implied by the current
 *                                vertex descriptor and attribute formats)
 *
 * The vertex size is not in the stream. It has to be computed from the state
 * the game set up earlier with GXSetVtxDesc and GXSetVtxAttrFmt: which
 * attributes are present, whether each is inline or an index into an array, and
 * how many components of what type each has. Get that wrong and the parser
 * walks off into noise, so this stops on anything it does not understand rather
 * than guessing.
 *
 * Only positions are consumed so far, drawn flat. Normals, colours, texture
 * coordinates and the TEV pipeline come later; the point of this stage is to
 * prove the stream is being walked correctly.
 */

#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Dolphin/gx.h"
#include "types.h"

#define MAX_ATTR    26
#define MAX_VTXFMT  8

/* Per-attribute descriptor: GX_NONE / GX_DIRECT / GX_INDEX8 / GX_INDEX16. */
static u8 s_desc[MAX_ATTR];

typedef struct {
	u8 cnt;  /* GXCompCnt  */
	u8 type; /* GXCompType */
	u8 frac; /* fixed-point shift */
} AttrFmt;

static AttrFmt s_fmt[MAX_VTXFMT][MAX_ATTR];

typedef struct {
	const u8* base;
	u8 stride;
} ArrayRef;

static ArrayRef s_array[MAX_ATTR];

/* Geometry accumulated this frame, in the order submitted. */
static float* s_verts;
static size_t s_vert_count, s_vert_cap;

static int s_unsupported; /* stop spamming once the parser gives up */

void GXSetVtxDesc(GXAttr attr, GXAttrType type)
{
	if ((unsigned)attr < MAX_ATTR) {
		s_desc[attr] = (u8)type;
	}
}

void GXSetVtxDescv(GXVtxDescList* list)
{
	/* Terminated by GX_VA_NULL rather than a count. */
	while (list && list->attr != GX_VA_NULL) {
		GXSetVtxDesc(list->attr, list->type);
		list++;
	}
}

void GXClearVtxDesc(void)
{
	memset(s_desc, GX_NONE, sizeof(s_desc));
}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac)
{
	if ((unsigned)vtxfmt < MAX_VTXFMT && (unsigned)attr < MAX_ATTR) {
		s_fmt[vtxfmt][attr].cnt  = (u8)cnt;
		s_fmt[vtxfmt][attr].type = (u8)type;
		s_fmt[vtxfmt][attr].frac = frac;
	}
}

void GXSetArray(GXAttr attr, void* base, u8 stride)
{
	if ((unsigned)attr < MAX_ATTR) {
		s_array[attr].base   = (const u8*)base;
		s_array[attr].stride = stride;
	}
}

/* ------------------------------------------------------------- decoding --*/
static int comp_size(u8 type)
{
	switch (type) {
	case GX_U8:
	case GX_S8:
		return 1;
	case GX_U16:
	case GX_S16:
		return 2;
	case GX_F32:
		return 4;
	default:
		return -1;
	}
}

static int pos_components(u8 cnt) { return cnt == GX_POS_XYZ ? 3 : 2; }

/* Big-endian reads: this data came off a GameCube disc. */
static u16 rd16(const u8* p) { return (u16)((p[0] << 8) | p[1]); }
static u32 rd32(const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

static float rdf32(const u8* p)
{
	union {
		u32 u;
		float f;
	} c;
	c.u = rd32(p);
	return c.f;
}

/* One component of a position, honouring the fixed-point shift. */
static float read_pos_comp(const u8* p, u8 type, u8 frac)
{
	float scale = 1.0f / (float)(1u << frac);
	switch (type) {
	case GX_U8:
		return (float)p[0] * scale;
	case GX_S8:
		return (float)(signed char)p[0] * scale;
	case GX_U16:
		return (float)rd16(p) * scale;
	case GX_S16:
		return (float)(short)rd16(p) * scale;
	case GX_F32:
		return rdf32(p);
	default:
		return 0.0f;
	}
}

/*
 * Size in bytes of one attribute as it appears inline in the stream, or of its
 * index. Returns -1 for anything unrecognised so the caller can bail rather
 * than desynchronise.
 */
static int attr_stream_size(int vtxfmt, int attr)
{
	int cs, n;

	switch (s_desc[attr]) {
	case GX_NONE:
		return 0;
	case GX_INDEX8:
		return 1;
	case GX_INDEX16:
		return 2;
	case GX_DIRECT:
		break;
	default:
		return -1;
	}

	/* Matrix index attributes are a single byte when direct. */
	if (attr <= GX_VA_TEX7MTXIDX) {
		return 1;
	}

	cs = comp_size(s_fmt[vtxfmt][attr].type);
	if (cs < 0) {
		return -1;
	}

	switch (attr) {
	case GX_VA_POS:
		n = pos_components(s_fmt[vtxfmt][attr].cnt);
		break;
	case GX_VA_NRM:
		n = 3;
		break;
	case GX_VA_CLR0:
	case GX_VA_CLR1:
		/* Colours are packed formats, not component arrays. */
		switch (s_fmt[vtxfmt][attr].type) {
		case GX_RGB565:
		case GX_RGBA4:
			return 2;
		case GX_RGB8:
		case GX_RGBA6:
			return 3;
		case GX_RGBX8:
		case GX_RGBA8:
			return 4;
		default:
			return -1;
		}
	default: /* texture coordinates */
		n = (s_fmt[vtxfmt][attr].cnt == GX_TEX_ST) ? 2 : 1;
		break;
	}
	return cs * n;
}

static void push_vertex(float x, float y, float z)
{
	if (s_vert_count + 3 > s_vert_cap) {
		size_t cap = s_vert_cap ? s_vert_cap * 2 : 3 * 4096;
		float* g   = (float*)realloc(s_verts, cap * sizeof(float));
		if (!g) {
			return;
		}
		s_verts   = g;
		s_vert_cap = cap;
	}
	s_verts[s_vert_count++] = x;
	s_verts[s_vert_count++] = y;
	s_verts[s_vert_count++] = z;
}

/* Pull one vertex's position, advancing p past the whole vertex. */
static int read_vertex(const u8** pp, const u8* end, int vtxfmt)
{
	const u8* p = *pp;
	int attr;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	int have = 0;

	for (attr = 0; attr < MAX_ATTR; attr++) {
		int sz = attr_stream_size(vtxfmt, attr);
		if (sz < 0) {
			return 0;
		}
		if (!sz) {
			continue;
		}
		if (p + sz > end) {
			return 0;
		}

		if (attr == GX_VA_POS) {
			const u8* src = p;
			u8 type       = s_fmt[vtxfmt][attr].type;
			u8 frac       = s_fmt[vtxfmt][attr].frac;
			int n         = pos_components(s_fmt[vtxfmt][attr].cnt);
			int cs        = comp_size(type);

			if (s_desc[attr] == GX_INDEX8 || s_desc[attr] == GX_INDEX16) {
				u32 idx = (s_desc[attr] == GX_INDEX8) ? p[0] : rd16(p);
				if (!s_array[attr].base || !s_array[attr].stride) {
					return 0;
				}
				src = s_array[attr].base + (size_t)idx * s_array[attr].stride;
			}
			if (cs > 0) {
				x = read_pos_comp(src, type, frac);
				y = read_pos_comp(src + cs, type, frac);
				z = (n == 3) ? read_pos_comp(src + 2 * cs, type, frac) : 0.0f;
				have = 1;
			}
		}
		p += sz;
	}

	if (have) {
		push_vertex(x, y, z);
	}
	*pp = p;
	return 1;
}

/*
 * Walk a display list. Draw opcodes are 0x80 | (primitive << 3) | vtxfmt, so
 * the top bit distinguishes them; 0x00 is a no-op pad, which lists are padded
 * with to a 32-byte boundary.
 */
void GXCallDisplayList(void* list, u32 numBytes)
{
	const u8* p   = (const u8*)list;
	const u8* end = p + numBytes;

	if (!list || !numBytes || s_unsupported) {
		return;
	}

	{
		static unsigned long calls;
		if ((++calls % 20000) == 0) {
			fprintf(stderr, "gfx: %lu display lists parsed, %lu vertices decoded\n", calls,
			        (unsigned long)(s_vert_count / 3));
		}
	}

	while (p < end) {
		u8 op = *p++;
		u16 count;
		int vtxfmt, i;

		if (op == 0x00) {
			continue; /* padding */
		}
		if (!(op & 0x80)) {
			/* A register/CP command rather than a draw: cannot be skipped
			 * safely without knowing its length, so stop here. */
			s_unsupported = 1;
			fprintf(stderr, "gfx: display list opcode 0x%02x not handled; geometry disabled\n", op);
			return;
		}

		if (p + 2 > end) {
			return;
		}
		count = rd16(p);
		p += 2;
		vtxfmt = op & 0x07;

		for (i = 0; i < count; i++) {
			if (!read_vertex(&p, end, vtxfmt)) {
				s_unsupported = 1;
				fprintf(stderr, "gfx: could not decode vertex %d/%u of opcode 0x%02x; geometry disabled\n", i, count,
				        op);
				return;
			}
		}
	}
}

/* Accessors for the drawing stage. */
const float* gfxVertexData(size_t* count)
{
	*count = s_vert_count;
	return s_verts;
}

void gfxVertexReset(void) { s_vert_count = 0; }
