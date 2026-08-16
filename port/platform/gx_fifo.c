/*
 * The write-gather FIFO.
 *
 * On hardware GXWGFifo is not memory: it is a register at 0xCC008000 wired
 * straight to the graphics processor's command queue. Writing a float to it
 * pushes that float into the GP's input stream. The declaration in GXFifo.h
 * pins it there with AT_ADDRESS, which expands to nothing off MetroWerks.
 *
 * This matters more than it looks. Vertex submission does not go through the
 * GX* functions at all -- GXPosition3f32, GXTexCoord2f32, GXColor1u32 and the
 * rest are static inlines in the header that write directly into this union.
 * So no amount of stubbing or intercepting GX entry points sees a single
 * vertex. Geometry only becomes visible to a port by reading what lands here.
 *
 * A real renderer therefore has two halves: the GX* calls, which set pipeline
 * state and can be intercepted normally, and this FIFO, which carries the
 * vertex stream and has to be parsed against whatever vertex format the most
 * recent GXSetVtxDesc/GXSetVtxAttrFmt calls described.
 *
 * For now it is ordinary memory. Writes land here and are discarded, which is
 * what lets the game run without a graphics backend.
 */

#include "Dolphin/GX/GXFifo.h"

volatile PPCWGPipe GXWGFifo;
