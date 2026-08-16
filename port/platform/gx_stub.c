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

void GXBegin(GXPrimitive type, GXVtxFmt format, u16 numVertices)
{

}

void GXBeginDisplayList(void* list, u32 size)
{

}

void GXCallDisplayList(void* list, u32 numBytes)
{

}

void GXClearVtxDesc(void)
{

}

void GXCopyDisp(void* dest, GXBool doClear)
{

}

void GXCopyTex(void* dest, GXBool doClear)
{

}

void GXDrawDone(void)
{

}

u32 GXEndDisplayList(void)
{
	return 0;
}

void GXFlush(void)
{

}

GXFifoObj* GXInit(void* base, u32 size)
{
	return NULL;
}

void GXInitLightAttn(GXLightObj* obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2)
{

}

void GXInitLightColor(GXLightObj* obj, GXColor color)
{

}

void GXInitLightDir(GXLightObj* obj, f32 nX, f32 nY, f32 nZ)
{

}

void GXInitLightPos(GXLightObj* obj, f32 x, f32 y, f32 z)
{

}

void GXInitSpecularDir(GXLightObj* obj, f32 nX, f32 nY, f32 nZ)
{

}

void GXInitTexObj(GXTexObj* obj, void* imagePtr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode sWrap, GXTexWrapMode tWrap, GXBool useMIPmap)
{

}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter minFilter, GXTexFilter maxFilter, f32 minLOD, f32 maxLOD, f32 lodBias, GXBool doBiasClamp, GXBool doEdgeLOD, GXAnisotropy maxAniso)
{

}

void GXInvalidateTexAll(void)
{

}

void GXInvalidateVtxCache(void)
{

}

void GXLoadLightObjImm(GXLightObj* obj, GXLightID light)
{

}

void GXLoadNrmMtxImm(const Mtx mtx, u32 id)
{

}

void GXLoadPosMtxImm(const Mtx mtx, u32 id)
{

}

void GXLoadTexMtxImm(const Mtx mtx, u32 id, GXTexMtxType type)
{

}

void GXLoadTexObj(GXTexObj* obj, GXTexMapID map)
{

}

void GXPixModeSync(void)
{

}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1)
{

}

void GXSetAlphaUpdate(GXBool enableUpdate)
{

}

void GXSetArray(GXAttr attr, void* base_ptr, u8 stride)
{

}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor srcFactor, GXBlendFactor destFactor, GXLogicOp op)
{

}

void GXSetChanAmbColor(GXChannelID channel, GXColor color)
{

}

void GXSetChanCtrl(GXChannelID channel, GXBool doEnable, GXColorSrc ambSrc, GXColorSrc matSrc, u32 mask, GXDiffuseFn diffFunc, GXAttnFn attnFunc)
{

}

void GXSetChanMatColor(GXChannelID channel, GXColor color)
{

}

void GXSetCoPlanar(GXBool doEnable)
{

}

void GXSetColorUpdate(GXBool enableUpdate)
{

}

void GXSetCopyClear(GXColor clearColor, u32 clearZ)
{

}

void GXSetCopyFilter(GXBool useAA, const u8 samplePattern[12][2], GXBool doVertFilt, const u8 vFilt[7])
{

}

void GXSetCullMode(GXCullMode mode)
{

}

struct OSThread* GXSetCurrentGXThread(void)
{
	return NULL;
}

void GXSetCurrentMtx(u32 id)
{

}

void GXSetDispCopyDst(u16 width, u16 height)
{

}

void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height)
{

}

u32 GXSetDispCopyYScale(f32 vertScale)
{
	return 0;
}

void GXSetDither(GXBool doDither)
{

}

void GXSetFog(GXFogType type, f32 startZ, f32 endZ, f32 nearZ, f32 farZ, GXColor color)
{

}

void GXSetFogRangeAdj(GXBool doEnable, u16 center, GXFogAdjTable* table)
{

}

void GXSetLineWidth(u8 width, GXTexOffset offset)
{

}

void GXSetNumChans(u8 count)
{

}

void GXSetNumTevStages(u8 count)
{

}

void GXSetNumTexGens(u8 nTexGens)
{

}

void GXSetPixelFmt(GXPixelFmt pixelFormat, GXZFmt16 zFormat)
{

}

void GXSetPointSize(u8 pointSize, GXTexOffset offset)
{

}

void GXSetProjection(const Mtx44 mtx, GXProjectionType type)
{

}

void GXSetScissor(u32 left, u32 top, u32 width, u32 height)
{

}

void GXSetScissorBoxOffset(s32 x, s32 y)
{

}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d)
{

}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool doClamp, GXTevRegID outReg)
{

}

void GXSetTevColor(GXTevRegID reg, GXColor color)
{

}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d)
{

}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool doClamp, GXTevRegID outReg)
{

}

void GXSetTevColorS10(GXTevRegID reg, GXColorS10 color)
{

}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel)
{

}

void GXSetTevKColor(GXTevKColorID id, GXColor color)
{

}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel)
{

}

void GXSetTevOp(GXTevStageID stage, GXTevMode mode)
{

}

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color)
{

}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel rasSel, GXTevSwapSel texSel)
{

}

void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha)
{

}

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize, u32 pt_texmtx)
{

}

void GXSetTexCopyDst(u16 width, u16 height, GXTexFmt format, GXBool useMIPmap)
{

}

void GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height)
{

}

void GXSetViewport(f32 left, f32 top, f32 width, f32 height, f32 nearZ, f32 farZ)
{

}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac)
{

}

void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, GXVtxAttrFmtList* list)
{

}

void GXSetVtxDesc(GXAttr attr, GXAttrType type)
{

}

void GXSetVtxDescv(GXVtxDescList* attrPtr)
{

}

void GXSetZCompLoc(GXBool isBeforeTex)
{

}

void GXSetZMode(GXBool enableCompare, GXCompare func, GXBool enableUpdate)
{

}
