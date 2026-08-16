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

/* Geometry accumulated this frame, already in clip space. */
static float* s_verts;
static size_t s_vert_count, s_vert_cap;

/*
 * Transforms are applied on the CPU as each list is parsed, rather than being
 * carried through to the draw as per-object uniforms.
 *
 * That is deliberate for a first pass: the game changes the position matrix
 * roughly as often as it draws (224,946 GXLoadPosMtxImm against 438,744
 * GXCallDisplayList), so preserving the association between geometry and its
 * matrix would otherwise mean splitting every frame into a quarter of a million
 * separate draws. Baking the transform in lets the whole frame go as one buffer
 * while still putting each object where it belongs. Batching by state comes
 * later, once there is something correct to optimise.
 */
#define MAX_PNMTX 64

static float s_proj[4][4];
static int s_have_proj;
static float s_posmtx[MAX_PNMTX][3][4];
static u32 s_cur_mtx; /* index into GX matrix memory */

void GXSetProjection(const Mtx44 mtx, GXProjectionType type)
{
	(void)type;
	memcpy(s_proj, mtx, sizeof(s_proj));
	s_have_proj = 1;
}

void GXLoadPosMtxImm(const Mtx mtx, u32 id)
{
	{
		static int shown;
		static unsigned long total, bad;
		int r, c, isnan = 0;
		for (r = 0; r < 3; r++) {
			for (c = 0; c < 4; c++) {
				if (mtx[r][c] != mtx[r][c]) {
					isnan = 1;
				}
			}
		}
		total++;
		if (isnan) {
			bad++;
		}
		if (shown < 4) {
			shown++;
			fprintf(stderr, "gfx: LoadPosMtx id=%u nan=%d  row0: %g %g %g %g\n", id, isnan, mtx[0][0], mtx[0][1],
			        mtx[0][2], mtx[0][3]);
		}
		if ((total % 50000) == 0) {
			fprintf(stderr, "gfx: LoadPosMtx %lu calls, %lu with NaN\n", total, bad);
		}
	}
	if (id < MAX_PNMTX) {
		memcpy(s_posmtx[id], mtx, sizeof(s_posmtx[0]));
	}
}

void GXSetCurrentMtx(u32 id)
{
	if (id < MAX_PNMTX) {
		s_cur_mtx = id;
	}
}

/* model -> clip, via the current position matrix and the projection. */
static void transform(float x, float y, float z, u32 mtx, float out[4])
{
	const float(*m)[4] = s_posmtx[mtx < MAX_PNMTX ? mtx : 0];
	float vx           = m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3];
	float vy           = m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3];
	float vz           = m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3];
	int i;

	if (!s_have_proj) {
		out[0] = vx;
		out[1] = vy;
		out[2] = vz;
		out[3] = 1.0f;
		return;
	}
	for (i = 0; i < 4; i++) {
		out[i] = s_proj[i][0] * vx + s_proj[i][1] * vy + s_proj[i][2] * vz + s_proj[i][3];
	}
}

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

/*
 * One component of a position, honouring the fixed-point shift.
 *
 * `host` selects the byte order, and the two cases have genuinely different
 * provenance rather than being a guess:
 *
 *   Display list bytes are read straight off the disc as an opaque blob and
 *   never pass through Stream, so they are still big-endian. The vertex indices
 *   in them decode correctly that way -- 8957, 9028, 9320 for a model of about
 *   ten thousand vertices.
 *
 *   Vertex arrays reach memory through the model loader, which reads them field
 *   by field through Stream::readFloat and therefore has already converted them
 *   to host order. Swapping again produced 3.19e-10 and 7.28e+22 where the same
 *   bytes read natively give 54.67 and 90.73.
 */
static float read_pos_comp(const u8* p, u8 type, u8 frac, int host)
{
	float scale = 1.0f / (float)(1u << frac);
	u16 h16     = host ? *(const u16*)p : rd16(p);

	switch (type) {
	case GX_U8:
		return (float)p[0] * scale;
	case GX_S8:
		return (float)(signed char)p[0] * scale;
	case GX_U16:
		return (float)h16 * scale;
	case GX_S16:
		return (float)(short)h16 * scale;
	case GX_F32:
		if (host) {
			float f;
			memcpy(&f, p, sizeof(f));
			return f;
		}
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

static void push_vertex(const float v[4])
{
	int i;
	if (s_vert_count + 4 > s_vert_cap) {
		size_t cap = s_vert_cap ? s_vert_cap * 2 : 4 * 8192;
		float* g   = (float*)realloc(s_verts, cap * sizeof(float));
		if (!g) {
			return;
		}
		s_verts    = g;
		s_vert_cap = cap;
	}
	for (i = 0; i < 4; i++) {
		s_verts[s_vert_count++] = v[i];
	}
}

/* Vertices of the primitive currently being assembled. */
static float* s_prim;
static size_t s_prim_cap;

static float* prim_slot(size_t i)
{
	if (i + 1 > s_prim_cap) {
		size_t cap = s_prim_cap ? s_prim_cap * 2 : 1024;
		float* g;
		while (cap < i + 1) {
			cap *= 2;
		}
		g = (float*)realloc(s_prim, cap * 4 * sizeof(float));
		if (!g) {
			return NULL;
		}
		s_prim     = g;
		s_prim_cap = cap;
	}
	return s_prim + i * 4;
}

static void emit_tri(size_t a, size_t b, size_t c)
{
	push_vertex(s_prim + a * 4);
	push_vertex(s_prim + b * 4);
	push_vertex(s_prim + c * 4);
}

/* GX primitives, all reduced to a triangle list. */
static void emit_primitive(u8 prim, size_t n)
{
	size_t i;
	switch (prim) {
	case 0x90: /* triangles */
		for (i = 0; i + 2 < n; i += 3) {
			emit_tri(i, i + 1, i + 2);
		}
		break;
	case 0x98: /* triangle strip -- winding alternates */
		for (i = 0; i + 2 < n; i++) {
			if (i & 1) {
				emit_tri(i + 1, i, i + 2);
			} else {
				emit_tri(i, i + 1, i + 2);
			}
		}
		break;
	case 0xA0: /* triangle fan */
		for (i = 1; i + 1 < n; i++) {
			emit_tri(0, i, i + 1);
		}
		break;
	case 0x80: /* quads */
		for (i = 0; i + 3 < n; i += 4) {
			emit_tri(i, i + 1, i + 2);
			emit_tri(i, i + 2, i + 3);
		}
		break;
	default:
		/* points and lines contribute no triangles */
		break;
	}
}

/* Pull one vertex's position, advancing p past the whole vertex. */
static int read_vertex(const u8** pp, const u8* end, int vtxfmt, float out[4])
{
	const u8* p = *pp;
	int attr;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	int have = 0;
	/* Each vertex may name its own transform. GXSetCurrentMtx was called 4,995
	 * times against 224,946 GXLoadPosMtxImm, so the matrix is chosen per-vertex
	 * through this attribute far more often than by the global setter; ignoring
	 * it leaves nearly everything transformed by whatever was last set. */
	u32 mtx = s_cur_mtx;

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

		if (attr == GX_VA_PNMTXIDX && s_desc[attr] == GX_DIRECT) {
			mtx = p[0];
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
				{
					static int shown;
					if (shown < 4) {
						union {
							u32 u;
							float f;
						} be, le;
						shown++;
						be.u = rd32(src);
						le.u = *(const u32*)src;
						fprintf(stderr,
						        "gfx: POS idx=%u src=%p raw=%02x%02x%02x%02x  BE=%g  LE=%g\n", idx,
						        (const void*)src, src[0], src[1], src[2], src[3], be.f, le.f);
					}
				}
			}
			if (cs > 0) {
				/* Indexed data was converted by the loader; inline data is raw
				 * disc bytes and still big-endian. */
				int host = (s_desc[attr] == GX_INDEX8 || s_desc[attr] == GX_INDEX16);
				x        = read_pos_comp(src, type, frac, host);
				y        = read_pos_comp(src + cs, type, frac, host);
				z        = (n == 3) ? read_pos_comp(src + 2 * cs, type, frac, host) : 0.0f;
				have     = 1;
			}
		}
		p += sz;
	}

	if (have) {
		transform(x, y, z, mtx, out);
		/* About 8% of the matrices the game hands to GXLoadPosMtxImm contain
		 * NaN. Where that comes from is a separate question, but a single one
		 * poisons every triangle drawn through it and a NaN vertex takes the
		 * whole primitive with it, so non-finite results are dropped rather
		 * than submitted. */
		if (out[0] != out[0] || out[1] != out[1] || out[2] != out[2] || out[3] != out[3]) {
			have = 0;
		}
	}

	if (have) {
		{
			static int shown;
			if (shown < 2) {
				int r;
				shown++;
				fprintf(stderr, "gfx: model(%.2f,%.2f,%.2f) mtx=%u -> clip(%g,%g,%g,%g)\n", x, y, z, mtx, out[0],
				        out[1], out[2], out[3]);
				for (r = 0; r < 3; r++) {
					fprintf(stderr, "   posmtx[%u] row%d: %g %g %g %g\n", mtx, r, s_posmtx[mtx][r][0],
					        s_posmtx[mtx][r][1], s_posmtx[mtx][r][2], s_posmtx[mtx][r][3]);
				}
				for (r = 0; r < 4; r++) {
					fprintf(stderr, "   proj row%d: %g %g %g %g\n", r, s_proj[r][0], s_proj[r][1], s_proj[r][2],
					        s_proj[r][3]);
				}
			}
		}
	} else {
		/* w = 0 makes the vertex degenerate, so any triangle using it is
		 * discarded. Collapsing to (0,0,0,1) instead would put it at the centre
		 * of the screen and fan visible artefacts out of it. */
		out[0] = out[1] = out[2] = out[3] = 0.0f;
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
		static int dumped;
		if (!dumped) {
			static const char* const kind[] = { "NONE", "DIRECT", "IDX8", "IDX16" };
			int a;
			dumped = 1;
			fprintf(stderr, "gfx: vertex descriptor at first list (op 0x%02x, %u bytes):\n", ((const u8*)list)[0],
			        numBytes);
			for (a = 0; a < MAX_ATTR; a++) {
				if (s_desc[a] != GX_NONE) {
					fprintf(stderr, "   attr %2d %-6s cnt=%u type=%u frac=%u arraybase=%p stride=%u size=%d\n", a,
					        s_desc[a] < 4 ? kind[s_desc[a]] : "?", s_fmt[0][a].cnt, s_fmt[0][a].type, s_fmt[0][a].frac,
					        (const void*)s_array[a].base, s_array[a].stride, attr_stream_size(0, a));
				}
			}
		}
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
			float* slot = prim_slot((size_t)i);
			if (!slot || !read_vertex(&p, end, vtxfmt, slot)) {
				s_unsupported = 1;
				fprintf(stderr, "gfx: could not decode vertex %d/%u of opcode 0x%02x; geometry disabled\n", i, count,
				        op);
				return;
			}
		}
		emit_primitive((u8)(op & 0xF8), (size_t)count);
	}
}

/* Accessors for the drawing stage. */
const float* gfxVertexData(size_t* count)
{
	*count = s_vert_count;
	return s_verts;
}

void gfxVertexReset(void) { s_vert_count = 0; }
