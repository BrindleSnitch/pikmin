/*
 * Generated stubs for GX* -- DO NOT EDIT BY HAND.
 *
 * Produced by tools/port/gen_stubs.py from the declarations in include/.
 * Every one of these does nothing and returns zero. They exist so the program
 * can link and run, which surfaces real problems in the code that is already
 * written; they are not an implementation and nothing here is correct.
 *
 * Replace them a subsystem at a time by deleting the entries from this file as
 * real versions appear.
 */

#include <stddef.h>

#include "Dolphin/gx.h"
#include "types.h"
#include "trace.h"

/* Call counting. Which of these the game actually uses, and how often, decides
 * what a renderer has to implement first -- measured rather than assumed. Set
 * PIKMIN_TRACE to have the totals printed at exit. */
#define STUB_COUNT 67
static unsigned long g_stub_hits[STUB_COUNT];
static const char* const g_stub_names[STUB_COUNT] = {
    "GXBegin",
    "GXBeginDisplayList",
    "GXCopyTex",
    "GXDrawDone",
    "GXEndDisplayList",
    "GXFlush",
    "GXInitLightAttn",
    "GXInitLightColor",
    "GXInitLightDir",
    "GXInitLightPos",
    "GXInitSpecularDir",
    "GXInitTexObj",
    "GXInitTexObjLOD",
    "GXInvalidateTexAll",
    "GXInvalidateVtxCache",
    "GXLoadLightObjImm",
    "GXLoadNrmMtxImm",
    "GXLoadPosMtxImm",
    "GXLoadTexMtxImm",
    "GXLoadTexObj",
    "GXPixModeSync",
    "GXSetAlphaCompare",
    "GXSetAlphaUpdate",
    "GXSetBlendMode",
    "GXSetChanAmbColor",
    "GXSetChanCtrl",
    "GXSetChanMatColor",
    "GXSetCoPlanar",
    "GXSetColorUpdate",
    "GXSetCopyFilter",
    "GXSetCullMode",
    "GXSetCurrentGXThread",
    "GXSetCurrentMtx",
    "GXSetDispCopyDst",
    "GXSetDispCopySrc",
    "GXSetDispCopyYScale",
    "GXSetDither",
    "GXSetFog",
    "GXSetFogRangeAdj",
    "GXSetLineWidth",
    "GXSetNumChans",
    "GXSetNumTevStages",
    "GXSetNumTexGens",
    "GXSetPixelFmt",
    "GXSetPointSize",
    "GXSetProjection",
    "GXSetScissor",
    "GXSetScissorBoxOffset",
    "GXSetTevAlphaIn",
    "GXSetTevAlphaOp",
    "GXSetTevColor",
    "GXSetTevColorIn",
    "GXSetTevColorOp",
    "GXSetTevColorS10",
    "GXSetTevKAlphaSel",
    "GXSetTevKColor",
    "GXSetTevKColorSel",
    "GXSetTevOp",
    "GXSetTevOrder",
    "GXSetTevSwapMode",
    "GXSetTevSwapModeTable",
    "GXSetTexCoordGen2",
    "GXSetTexCopyDst",
    "GXSetTexCopySrc",
    "GXSetVtxAttrFmtv",
    "GXSetZCompLoc",
    "GXSetZMode",
};
#define STUB_HIT(i) (g_stub_hits[(i)]++)

__attribute__((constructor)) static void stub_register(void)
{
    traceRegisterTable("GX", g_stub_hits, g_stub_names, STUB_COUNT);
}

void GXBegin(GXPrimitive type, GXVtxFmt format, u16 numVertices)
{
	STUB_HIT(0);
}

void GXBeginDisplayList(void* list, u32 size)
{
	STUB_HIT(1);
}

void GXCopyTex(void* dest, GXBool doClear)
{
	STUB_HIT(2);
}

void GXDrawDone(void)
{
	STUB_HIT(3);
}

u32 GXEndDisplayList(void)
{
	STUB_HIT(4);
	return 0;
}

void GXFlush(void)
{
	STUB_HIT(5);
}

void GXInitLightAttn(GXLightObj* obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2)
{
	STUB_HIT(6);
}

void GXInitLightColor(GXLightObj* obj, GXColor color)
{
	STUB_HIT(7);
}

void GXInitLightDir(GXLightObj* obj, f32 nX, f32 nY, f32 nZ)
{
	STUB_HIT(8);
}

void GXInitLightPos(GXLightObj* obj, f32 x, f32 y, f32 z)
{
	STUB_HIT(9);
}

void GXInitSpecularDir(GXLightObj* obj, f32 nX, f32 nY, f32 nZ)
{
	STUB_HIT(10);
}

void GXInitTexObj(GXTexObj* obj, void* imagePtr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode sWrap, GXTexWrapMode tWrap, GXBool useMIPmap)
{
	STUB_HIT(11);
}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter minFilter, GXTexFilter maxFilter, f32 minLOD, f32 maxLOD, f32 lodBias, GXBool doBiasClamp, GXBool doEdgeLOD, GXAnisotropy maxAniso)
{
	STUB_HIT(12);
}

void GXInvalidateTexAll(void)
{
	STUB_HIT(13);
}

void GXInvalidateVtxCache(void)
{
	STUB_HIT(14);
}

void GXLoadLightObjImm(GXLightObj* obj, GXLightID light)
{
	STUB_HIT(15);
}

void GXLoadNrmMtxImm(const Mtx mtx, u32 id)
{
	STUB_HIT(16);
}

void GXLoadPosMtxImm(const Mtx mtx, u32 id)
{
	STUB_HIT(17);
}

void GXLoadTexMtxImm(const Mtx mtx, u32 id, GXTexMtxType type)
{
	STUB_HIT(18);
}

void GXLoadTexObj(GXTexObj* obj, GXTexMapID map)
{
	STUB_HIT(19);
}

void GXPixModeSync(void)
{
	STUB_HIT(20);
}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1)
{
	STUB_HIT(21);
}

void GXSetAlphaUpdate(GXBool enableUpdate)
{
	STUB_HIT(22);
}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor srcFactor, GXBlendFactor destFactor, GXLogicOp op)
{
	STUB_HIT(23);
}

void GXSetChanAmbColor(GXChannelID channel, GXColor color)
{
	STUB_HIT(24);
}

void GXSetChanCtrl(GXChannelID channel, GXBool doEnable, GXColorSrc ambSrc, GXColorSrc matSrc, u32 mask, GXDiffuseFn diffFunc, GXAttnFn attnFunc)
{
	STUB_HIT(25);
}

void GXSetChanMatColor(GXChannelID channel, GXColor color)
{
	STUB_HIT(26);
}

void GXSetCoPlanar(GXBool doEnable)
{
	STUB_HIT(27);
}

void GXSetColorUpdate(GXBool enableUpdate)
{
	STUB_HIT(28);
}

void GXSetCopyFilter(GXBool useAA, const u8 samplePattern[12][2], GXBool doVertFilt, const u8 vFilt[7])
{
	STUB_HIT(29);
}

void GXSetCullMode(GXCullMode mode)
{
	STUB_HIT(30);
}

struct OSThread* GXSetCurrentGXThread(void)
{
	STUB_HIT(31);
	return NULL;
}

void GXSetCurrentMtx(u32 id)
{
	STUB_HIT(32);
}

void GXSetDispCopyDst(u16 width, u16 height)
{
	STUB_HIT(33);
}

void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height)
{
	STUB_HIT(34);
}

u32 GXSetDispCopyYScale(f32 vertScale)
{
	STUB_HIT(35);
	return 0;
}

void GXSetDither(GXBool doDither)
{
	STUB_HIT(36);
}

void GXSetFog(GXFogType type, f32 startZ, f32 endZ, f32 nearZ, f32 farZ, GXColor color)
{
	STUB_HIT(37);
}

void GXSetFogRangeAdj(GXBool doEnable, u16 center, GXFogAdjTable* table)
{
	STUB_HIT(38);
}

void GXSetLineWidth(u8 width, GXTexOffset offset)
{
	STUB_HIT(39);
}

void GXSetNumChans(u8 count)
{
	STUB_HIT(40);
}

void GXSetNumTevStages(u8 count)
{
	STUB_HIT(41);
}

void GXSetNumTexGens(u8 nTexGens)
{
	STUB_HIT(42);
}

void GXSetPixelFmt(GXPixelFmt pixelFormat, GXZFmt16 zFormat)
{
	STUB_HIT(43);
}

void GXSetPointSize(u8 pointSize, GXTexOffset offset)
{
	STUB_HIT(44);
}

void GXSetProjection(const Mtx44 mtx, GXProjectionType type)
{
	STUB_HIT(45);
}

void GXSetScissor(u32 left, u32 top, u32 width, u32 height)
{
	STUB_HIT(46);
}

void GXSetScissorBoxOffset(s32 x, s32 y)
{
	STUB_HIT(47);
}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d)
{
	STUB_HIT(48);
}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool doClamp, GXTevRegID outReg)
{
	STUB_HIT(49);
}

void GXSetTevColor(GXTevRegID reg, GXColor color)
{
	STUB_HIT(50);
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d)
{
	STUB_HIT(51);
}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool doClamp, GXTevRegID outReg)
{
	STUB_HIT(52);
}

void GXSetTevColorS10(GXTevRegID reg, GXColorS10 color)
{
	STUB_HIT(53);
}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel)
{
	STUB_HIT(54);
}

void GXSetTevKColor(GXTevKColorID id, GXColor color)
{
	STUB_HIT(55);
}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel)
{
	STUB_HIT(56);
}

void GXSetTevOp(GXTevStageID stage, GXTevMode mode)
{
	STUB_HIT(57);
}

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color)
{
	STUB_HIT(58);
}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel rasSel, GXTevSwapSel texSel)
{
	STUB_HIT(59);
}

void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha)
{
	STUB_HIT(60);
}

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize, u32 pt_texmtx)
{
	STUB_HIT(61);
}

void GXSetTexCopyDst(u16 width, u16 height, GXTexFmt format, GXBool useMIPmap)
{
	STUB_HIT(62);
}

void GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height)
{
	STUB_HIT(63);
}

void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, GXVtxAttrFmtList* list)
{
	STUB_HIT(64);
}

void GXSetZCompLoc(GXBool isBeforeTex)
{
	STUB_HIT(65);
}

void GXSetZMode(GXBool enableCompare, GXCompare func, GXBool enableUpdate)
{
	STUB_HIT(66);
}
