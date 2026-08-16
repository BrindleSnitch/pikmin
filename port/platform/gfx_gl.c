/*
 * The beginnings of a real graphics backend: an offscreen GLES3 target that
 * frames can actually be written out of.
 *
 * There is no display server on this device -- no X socket, no DISPLAY -- so
 * rendering goes to a surfaceless EGL context on Mesa's software rasteriser and
 * frames are written out as PNGs. That avoids needing anything installed to see
 * output, and the same context works unchanged against a real surface later.
 *
 * What is implemented here is the frame scaffolding only: context creation, the
 * clear colour the game asks for, the viewport, and the copy-to-display step
 * that marks a frame boundary. Geometry is not drawn yet. The measured GX
 * profile says that work is dominated by GXCallDisplayList (438744 calls
 * against 3147 GXBegin over 363 frames), so the next stage is parsing those
 * display list command streams rather than chasing the write-gather FIFO.
 *
 *   PIKMIN_FRAMES       directory for PNG output (default /root/pikmin-frames)
 *   PIKMIN_FRAME_EVERY  write one frame in N (default 60; 0 disables)
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "Dolphin/gx.h"
#include "types.h"

/* The GameCube's usual NTSC framebuffer. */
#define FB_WIDTH  640
#define FB_HEIGHT 480

static EGLDisplay s_dpy = EGL_NO_DISPLAY;
static EGLContext s_ctx = EGL_NO_CONTEXT;
static EGLSurface s_surf = EGL_NO_SURFACE;
static int s_ready;

static float s_clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static unsigned long s_frame;
static char s_outdir[512];
static long s_every = 60;

/* ------------------------------------------------------------------ PNG --
 * libpng is not installed, but a PNG is only zlib-compressed scanlines wrapped
 * in four chunks, and zlib is here. Each row is prefixed with a zero filter
 * byte; rows arrive bottom-up from glReadPixels and are emitted in reverse.
 */
static void put_be32(unsigned char* p, unsigned long v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static void png_chunk(FILE* f, const char* tag, const unsigned char* data, unsigned long len)
{
	unsigned char hdr[8];
	unsigned char crcbuf[4];
	unsigned long crc;

	put_be32(hdr, len);
	memcpy(hdr + 4, tag, 4);
	fwrite(hdr, 1, 8, f);
	if (len) {
		fwrite(data, 1, len, f);
	}

	crc = crc32(0, (const Bytef*)tag, 4);
	if (len) {
		crc = crc32(crc, (const Bytef*)data, len);
	}
	put_be32(crcbuf, crc);
	fwrite(crcbuf, 1, 4, f);
}

static int png_write(const char* path, const unsigned char* rgba, int w, int h)
{
	FILE* f;
	unsigned char ihdr[13];
	unsigned char* raw;
	unsigned char* comp;
	uLongf complen;
	long rawlen;
	int y;

	rawlen = (long)h * (1 + (long)w * 4);
	raw    = (unsigned char*)malloc(rawlen);
	if (!raw) {
		return 0;
	}
	for (y = 0; y < h; y++) {
		unsigned char* dst = raw + (long)y * (1 + (long)w * 4);
		/* glReadPixels returns the bottom row first */
		const unsigned char* src = rgba + (long)(h - 1 - y) * (long)w * 4;
		int x;
		*dst++ = 0;
		memcpy(dst, src, (size_t)w * 4);
		/* Force opaque. The framebuffer's alpha carries GX blending state, not
		 * coverage, so honouring it would render every frame invisible in an
		 * image viewer. */
		for (x = 0; x < w; x++) {
			dst[x * 4 + 3] = 0xFF;
		}
	}

	complen = compressBound((uLong)rawlen);
	comp    = (unsigned char*)malloc(complen);
	if (!comp || compress2(comp, &complen, raw, (uLong)rawlen, 6) != Z_OK) {
		free(raw);
		free(comp);
		return 0;
	}
	free(raw);

	f = fopen(path, "wb");
	if (!f) {
		free(comp);
		return 0;
	}
	fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);

	put_be32(ihdr, (unsigned long)w);
	put_be32(ihdr + 4, (unsigned long)h);
	ihdr[8]  = 8; /* bit depth */
	ihdr[9]  = 6; /* RGBA */
	ihdr[10] = 0;
	ihdr[11] = 0;
	ihdr[12] = 0;
	png_chunk(f, "IHDR", ihdr, 13);
	png_chunk(f, "IDAT", comp, complen);
	png_chunk(f, "IEND", NULL, 0);

	fclose(f);
	free(comp);
	return 1;
}

/* ------------------------------------------------------------- context --*/
static void gfx_init_geometry(void);

static void gfx_init(void)
{
	PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay;
	EGLint maj, min, n;
	EGLConfig cfg;
	const char* env;

	if (s_ready) {
		return;
	}
	s_ready = 1; /* set first: a failure here must not retry every frame */

	env = getenv("PIKMIN_FRAMES");
	snprintf(s_outdir, sizeof(s_outdir), "%s", env && *env ? env : "/root/pikmin-frames");
	env = getenv("PIKMIN_FRAME_EVERY");
	if (env && *env) {
		s_every = strtol(env, NULL, 10);
	}

	getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	if (getPlatformDisplay) {
		s_dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	}
	if (s_dpy == EGL_NO_DISPLAY) {
		s_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	}
	if (!eglInitialize(s_dpy, &maj, &min)) {
		fprintf(stderr, "gfx: eglInitialize failed (0x%x); frames will not be written\n", eglGetError());
		s_dpy = EGL_NO_DISPLAY;
		return;
	}
	eglBindAPI(EGL_OPENGL_ES_API);

	{
		EGLint attr[] = { EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
			              EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
			              EGL_RED_SIZE,        8,
			              EGL_GREEN_SIZE,      8,
			              EGL_BLUE_SIZE,       8,
			              EGL_ALPHA_SIZE,      8,
			              EGL_DEPTH_SIZE,      24,
			              EGL_NONE };
		if (!eglChooseConfig(s_dpy, attr, &cfg, 1, &n) || n < 1) {
			fprintf(stderr, "gfx: no suitable EGL config\n");
			s_dpy = EGL_NO_DISPLAY;
			return;
		}
	}
	{
		EGLint pb[] = { EGL_WIDTH, FB_WIDTH, EGL_HEIGHT, FB_HEIGHT, EGL_NONE };
		s_surf      = eglCreatePbufferSurface(s_dpy, cfg, pb);
	}
	{
		EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
		s_ctx       = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, ca);
	}
	if (s_ctx == EGL_NO_CONTEXT || !eglMakeCurrent(s_dpy, s_surf, s_surf, s_ctx)) {
		fprintf(stderr, "gfx: could not create/bind context (0x%x)\n", eglGetError());
		s_dpy = EGL_NO_DISPLAY;
		return;
	}

	fprintf(stderr, "gfx: %s / %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
	fprintf(stderr, "gfx: writing every %ld frame(s) to %s\n", s_every, s_outdir);
	glViewport(0, 0, FB_WIDTH, FB_HEIGHT);
	/* Depth testing stays off for now. GX clips z to [-w, 0] where GL expects
	 * [-w, w], so depth comparisons would be meaningful only after that range
	 * is remapped -- and getting geometry visible at all comes first. */
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	gfx_init_geometry();
}

/* ----------------------------------------------------------- geometry --
 * Vertices arrive from gfx_vtx.c already in clip space, so the vertex shader is
 * a passthrough and no uniforms are needed yet. Everything is drawn in one flat
 * colour: the point of this stage is to confirm the display list parser is
 * producing geometry in the right places, not to look right.
 */
extern const float* gfxVertexData(size_t* count);
extern void gfxVertexReset(void);

static GLuint s_prog, s_vbo, s_vao;

static GLuint compile(GLenum kind, const char* src)
{
	GLuint sh = glCreateShader(kind);
	GLint ok  = 0;
	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(sh, sizeof(log), NULL, log);
		fprintf(stderr, "gfx: shader compile failed: %s\n", log);
		glDeleteShader(sh);
		return 0;
	}
	return sh;
}

static void gfx_init_geometry(void)
{
	static const char* vs = "#version 300 es\n"
	                        "layout(location=0) in vec4 aPos;\n"
	                        "void main(){ gl_Position = aPos; }\n";
	static const char* fs = "#version 300 es\n"
	                        "precision mediump float;\n"
	                        "out vec4 o;\n"
	                        "void main(){ o = vec4(0.85,0.86,0.90,1.0); }\n";
	GLuint v, f;
	GLint ok = 0;

	v = compile(GL_VERTEX_SHADER, vs);
	f = compile(GL_FRAGMENT_SHADER, fs);
	if (!v || !f) {
		return;
	}
	s_prog = glCreateProgram();
	glAttachShader(s_prog, v);
	glAttachShader(s_prog, f);
	glLinkProgram(s_prog);
	glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(s_prog, sizeof(log), NULL, log);
		fprintf(stderr, "gfx: program link failed: %s\n", log);
		s_prog = 0;
		return;
	}
	glDeleteShader(v);
	glDeleteShader(f);

	glGenVertexArrays(1, &s_vao);
	glGenBuffers(1, &s_vbo);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	fprintf(stderr, "gfx: geometry pipeline ready\n");
}

static void gfx_draw_frame(void)
{
	size_t floats;
	const float* data = gfxVertexData(&floats);
	static unsigned long announced;

	if (!s_prog || !data || floats < 12) {
		return;
	}
	if (!announced) {
		announced = 1;
		fprintf(stderr, "gfx: first drawn frame has %lu triangles\n", (unsigned long)(floats / 4 / 3));
	}

	glUseProgram(s_prog);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(floats * sizeof(float)), data, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(floats / 4));
}

/* ------------------------------------------------------------------ GX --*/
GXFifoObj* GXInit(void* base, u32 size)
{
	(void)base, (void)size;
	gfx_init();
	return NULL;
}

void GXSetCopyClear(GXColor clearColor, u32 clearZ)
{
	static int announced;
	(void)clearZ;
	s_clear[0] = clearColor.r / 255.0f;
	s_clear[1] = clearColor.g / 255.0f;
	s_clear[2] = clearColor.b / 255.0f;
	s_clear[3] = clearColor.a / 255.0f;
	if (!announced) {
		announced = 1;
		fprintf(stderr, "gfx: first clear colour rgba(%u,%u,%u,%u)\n", clearColor.r, clearColor.g, clearColor.b,
		        clearColor.a);
	}
}

void GXSetViewport(f32 left, f32 top, f32 width, f32 height, f32 nearZ, f32 farZ)
{
	(void)nearZ, (void)farZ;
	if (s_dpy == EGL_NO_DISPLAY) {
		return;
	}
	/* GX measures top-down from the top-left; GL measures bottom-up. */
	glViewport((GLint)left, (GLint)(FB_HEIGHT - (top + height)), (GLsizei)width, (GLsizei)height);
}

void GXCopyDisp(void* dest, GXBool doClear)
{
	static unsigned char* pixels;
	char path[640];

	(void)dest, (void)doClear;
	if (s_dpy == EGL_NO_DISPLAY) {
		return;
	}

	s_frame++;
	gfx_draw_frame();

	if (s_every > 0 && (s_frame % (unsigned long)s_every) == 0) {
		if (!pixels) {
			pixels = (unsigned char*)malloc((size_t)FB_WIDTH * FB_HEIGHT * 4);
		}
		if (pixels) {
			glReadPixels(0, 0, FB_WIDTH, FB_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			snprintf(path, sizeof(path), "%s/frame_%06lu.png", s_outdir, s_frame);
			if (png_write(path, pixels, FB_WIDTH, FB_HEIGHT)) {
				fprintf(stderr, "gfx: wrote %s\n", path);
			} else {
				fprintf(stderr, "gfx: failed writing %s\n", path);
			}
		}
	}

	/* GXCopyDisp ends a frame; the next one starts from the clear colour with
	 * an empty geometry buffer. */
	glClearColor(s_clear[0], s_clear[1], s_clear[2], s_clear[3]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gfxVertexReset();
}
