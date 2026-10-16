#ifndef BUILD_H
#define BUILD_H

#include <libultraship/libultra.h>

#ifdef __cplusplus
#define extern_s extern "C"
#else
#define extern_s extern
#endif

// Port version information
extern_s const char gBuildVersion[];
extern_s const u16 gBuildVersionMajor;
extern_s const u16 gBuildVersionMinor;
extern_s const u16 gBuildVersionPatch;

extern_s const char gGitBranch[];
extern_s const char gGitCommitHash[];
extern_s const char gGitCommitTag[];
extern_s const char gBuildTeam[];
extern_s const char gBuildDate[];
extern_s const char gBuildMakeOption[];

#undef extern_s

#endif
