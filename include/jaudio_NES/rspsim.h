#ifndef RSPSIM_H
#define RSPSIM_H

#include "types.h"
#include "jaudio_NES/neosabi.h"

#define RSPSIM_MODE_INIT 0
#define RSPSIM_MODE_MOVE 1

extern s32 RspStart(NaAcmd* pTaskCmds, s32 allTasks);
extern void RspStart2(NaAcmd* pTaskCmds, s32 allTasks, s32 mode);

#endif
