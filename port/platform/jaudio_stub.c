/*
 * Generated stubs for Jac_* -- DO NOT EDIT BY HAND.
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

#include "types.h"

void Jac_AddDVDBuffer(u8* buf, u32 bufferSize)
{

}

void Jac_BackDVDBuffer(void)
{

}

int Jac_CheckFreeEvents(void)
{
	return 0;
}

int Jac_CreateEvent(u32 eventType, struct SVector_* eventPos)
{
	return 0;
}

BOOL Jac_DemoFrame(int)
{
	return 0;
}

BOOL Jac_DestroyEvent(s32 idx)
{
	return 0;
}

void Jac_EnterBossMode(void)
{

}

void Jac_ExitBossMode(void)
{

}

void Jac_FinishDemo(void)
{

}

void Jac_FinishPartsFindDemo(void)
{

}

void Jac_FinishTextDemo(void)
{

}

void Jac_Freeze(void)
{

}

void Jac_Freeze_Precall(void)
{

}

int Jac_GetActiveEvents(u32* outCount)
{
	return 0;
}

void Jac_Gsync(void)
{

}

void Jac_InitAllEvent(void)
{

}

void Jac_Orima_Formation(s32, s32)
{

}

void Jac_Orima_Walk(s32 groundSoundID, u32 p2)
{

}

void Jac_OutputMode(int mode)
{

}

void Jac_Piki_Number(u32)
{

}

BOOL Jac_PlayEventAction(int eventIdx, int actionId)
{
	return 0;
}

void Jac_PlayOrimaSe(u32 orimaSoundID)
{

}

void Jac_PlaySystemSe(s32)
{

}

void Jac_SceneExit(u32 sceneID, u32 stageID)
{

}

void Jac_SceneSetup(u32 sceneID, u32 stageID)
{

}

void Jac_SetBGMVolume(u8 vol)
{

}

void Jac_SetDemoOnyons(int)
{

}

void Jac_SetDemoPartsCount(int)
{

}

void Jac_SetDemoPartsID(int)
{

}

void Jac_SetSEVolume(u8 vol)
{

}

void Jac_Start(void* heap, u32 heapSize, u32 aramSize, immut char* rootPath)
{

}

void Jac_StartDemo(u32)
{

}

void Jac_StartPartsFindDemo(u32 jingleType, BOOL hasAudio)
{

}

void Jac_StartTextDemo(int)
{

}

BOOL Jac_StopEventAction(int eventIdx, int actionId)
{
	return 0;
}

void Jac_StopOrimaSe(s32 orimaSoundID)
{

}

void Jac_StopSe(s32)
{

}

void Jac_StopSystemSe(s32)
{

}

int Jac_StreamMovieGetPicture(void* pictureBuffer, int* widthOut, int* heightOut)
{
	return 0;
}

void Jac_StreamMovieInit(immut char* filepath, u8* movieWorkBuffer, int movieWorkSize)
{

}

void Jac_StreamMovieStop(void)
{

}

void Jac_StreamMovieUpdate(void)
{

}

void Jac_UpdateCamera(struct SVector_* listenerPos, struct SVector_* listenerDir)
{

}

BOOL Jac_UpdateEventPosition(int idx, struct SVector_* eventPos)
{
	return 0;
}
