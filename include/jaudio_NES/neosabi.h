#ifndef NEOSABI_H
#define NEOSABI_H

#include "types.h"
#include "PR/abi.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SHIFTL
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#endif

#define A_CMD_SPNOOP 0
#define A_CMD_ADPCM 1
#define A_CMD_CLEARBUFF 2
#define A_CMD_UNK3 3
#define A_CMD_ADDMIXER 4
#define A_CMD_RESAMPLE 5
#define A_CMD_RESAMPLE_ZOH 6
#define A_CMD_FIRFILTER 7
#define A_CMD_SETBUFF 8
#define A_CMD_DUPLICATE 9
#define A_CMD_DMEMMOVE 10
#define A_CMD_LOADADPCM 11
#define A_CMD_MIXER 12
#define A_CMD_INTERLEAVE 13
#define A_CMD_DISTFILTER 14
#define A_CMD_SETLOOP 15
#define A_CMD_UNK16 16
#define A_CMD_HALFCUT 17
#define A_CMD_SETENVPARAM 18
#define A_CMD_ENVMIXER 19
#define A_CMD_LOADBUFFER2 20
#define A_CMD_SAVEBUFFER2 21
#define A_CMD_SETENVPARAM2 22
#define A_CMD_PCM8DEC 23
#define A_CMD_LOADCACHE 24
#define A_CMD_EXIT 25

typedef struct {
    u32 w0;
    uintptr_t w1;
} NaAwords;

typedef union {
    NaAwords words;
    long long int force_union_align;
} NaAcmd;

#define ACMD_PTR(p) ((uintptr_t)(p))

#undef aADPCMdec
#define aADPCMdec(pkt, f, s)                                                    \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_ADPCM, 24, 8) | _SHIFTL(f, 16, 8);         \
        _a->words.w1 = ACMD_PTR(s);                                             \
    }

#undef aClearBuffer
#define aClearBuffer(pkt, d, c)                                                 \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_CLEARBUFF, 24, 8) | _SHIFTL(d, 0, 24);     \
        _a->words.w1 = (u32)(c);                                                \
    }

#undef aMix
#define aMix(pkt, f, g, i, o)                                                                       \
    {                                                                                               \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                      \
        _a->words.w0 = (_SHIFTL(A_CMD_MIXER, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(g, 0, 16));       \
        _a->words.w1 = _SHIFTL(i, 16, 16) | _SHIFTL(o, 0, 16);                                      \
    }

#undef aResample
#define aResample(pkt, f, p, s)                                                                     \
    {                                                                                               \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                      \
        _a->words.w0 = (_SHIFTL(A_CMD_RESAMPLE, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(p, 0, 16));    \
        _a->words.w1 = ACMD_PTR(s);                                                                 \
    }

#undef aSetBuffer
#define aSetBuffer(pkt, f, i, o, c)                                                                 \
    {                                                                                               \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                      \
        _a->words.w0 = (_SHIFTL(A_CMD_SETBUFF, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(i, 0, 16));     \
        _a->words.w1 = _SHIFTL(o, 16, 16) | _SHIFTL(c, 0, 16);                                      \
    }

#undef aSetLoop
#define aSetLoop(pkt, a)                                                        \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_SETLOOP, 24, 8);                           \
        _a->words.w1 = ACMD_PTR(a);                                             \
    }

#undef aDMEMMove
#define aDMEMMove(pkt, i, o, c)                                                 \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_DMEMMOVE, 24, 8) | _SHIFTL(i, 0, 24);      \
        _a->words.w1 = _SHIFTL(o, 16, 16) | _SHIFTL(c, 0, 16);                  \
    }

#undef aLoadADPCM
#define aLoadADPCM(pkt, c, d)                                                   \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_LOADADPCM, 24, 8) | _SHIFTL(c, 0, 24);     \
        _a->words.w1 = ACMD_PTR(d);                                             \
    }

#undef aUnkCmd3
#define aUnkCmd3(pkt, a1, a2, a3)                                               \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_UNK3, 24, 8) | _SHIFTL(a3, 0, 16);         \
        _a->words.w1 = _SHIFTL(a1, 16, 16) | _SHIFTL(a2, 0, 16);                \
    }

#undef aHalfCut
#define aHalfCut(pkt, src, dst, len)                                            \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_HALFCUT, 24, 8) | _SHIFTL(len, 0, 16);     \
        _a->words.w1 = _SHIFTL(src, 16, 16) | _SHIFTL(dst, 0, 16);              \
    }

#undef aSetEnvParam
#define aSetEnvParam(pkt, revVol, rampRev, rampL, rampR)                                                       \
    {                                                                                                          \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                                 \
        _a->words.w0 = _SHIFTL(A_CMD_SETENVPARAM, 24, 8) | _SHIFTL(revVol, 16, 8) | _SHIFTL(rampRev, 0, 16);   \
        _a->words.w1 = _SHIFTL(rampL, 16, 16) | _SHIFTL(rampR, 0, 16);                                         \
    }

#undef aLoadCache
#define aLoadCache(pkt, dst, src, len)                                                                         \
    {                                                                                                          \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                                 \
        _a->words.w0 = _SHIFTL(A_CMD_LOADCACHE, 24, 8) | _SHIFTL((len) >> 4, 16, 8) | _SHIFTL(src, 0, 16);     \
        _a->words.w1 = ACMD_PTR(dst);                                                                          \
    }

#undef aLoadBuffer2
#define aLoadBuffer2(pkt, dst, src, len)                                                                       \
    {                                                                                                          \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                                 \
        _a->words.w0 = _SHIFTL(A_CMD_LOADBUFFER2, 24, 8) | _SHIFTL((len) >> 4, 16, 8) | _SHIFTL(src, 0, 16);   \
        _a->words.w1 = ACMD_PTR(dst);                                                                          \
    }

#undef aSaveBuffer2
#define aSaveBuffer2(pkt, dst, src, len)                                                                       \
    {                                                                                                          \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                                 \
        _a->words.w0 = _SHIFTL(A_CMD_SAVEBUFFER2, 24, 8) | _SHIFTL((len) >> 4, 16, 8) | _SHIFTL(src, 0, 16);   \
        _a->words.w1 = ACMD_PTR(dst);                                                                          \
    }

#undef aInterleave2
#define aInterleave2(pkt, o, l, r, c)                                                                          \
    {                                                                                                          \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                                 \
        _a->words.w0 = (_SHIFTL(A_CMD_INTERLEAVE, 24, 8) | _SHIFTL(c >> 4, 16, 8) | _SHIFTL(o, 0, 16));        \
        _a->words.w1 = _SHIFTL(l, 16, 16) | _SHIFTL(r, 0, 16);                                                 \
    }

#undef aSetEnvParam2
#define aSetEnvParam2(pkt, volL, volR)                                          \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_SETENVPARAM2, 24, 8);                      \
        _a->words.w1 = _SHIFTL(volL, 16, 16) | _SHIFTL(volR, 0, 16);            \
    }

#undef aPCM8dec
#define aPCM8dec(pkt, flags, state)                                             \
    {                                                                           \
        NaAcmd* _a = (NaAcmd*)pkt;                                                  \
        _a->words.w0 = _SHIFTL(A_CMD_PCM8DEC, 24, 8) | _SHIFTL(flags, 16, 8);   \
        _a->words.w1 = ACMD_PTR(state);                                         \
    }

#undef aDistFilter
#define aDistFilter(pkt, gain, dmem_in, dmem_out, len)                                                     \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = _SHIFTL(A_CMD_DISTFILTER, 24, 8) | _SHIFTL(gain, 16, 8) | _SHIFTL(len, 0, 16);      \
        _a->words.w1 = _SHIFTL(dmem_in, 16, 16) | _SHIFTL(dmem_out, 0, 16);                                \
    }

#undef aNoiseFilter
#define aNoiseFilter(pkt, buf, f, addr, len)                                                               \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = _SHIFTL(A_CMD_SPNOOP, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(addr, 0, 16);            \
        _a->words.w1 = _SHIFTL(len, 16, 16) | _SHIFTL(buf, 0, 16);                                         \
    }

#undef aFirFilter
#define aFirFilter(pkt, f, bufSize, addr)                                                                  \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = _SHIFTL(A_CMD_FIRFILTER, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(bufSize, 0, 16);      \
        _a->words.w1 = ACMD_PTR(addr);                                                                     \
    }

#undef aFirLoadTable
#define aFirLoadTable(pkt, size, addr) aFirFilter(pkt, 2, size, addr)

#undef aEnvMixer2
#define aEnvMixer2(pkt, dmemi, count, swapLR, x0, x1, x2, x3, m, bits)                                     \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = (bits | _SHIFTL(dmemi >> 4, 16, 8) | _SHIFTL(count, 8, 8) | _SHIFTL(swapLR, 4, 1) | \
                        _SHIFTL(x0, 3, 1) | _SHIFTL(x1, 2, 1) | _SHIFTL(x2, 1, 1) | _SHIFTL(x3, 0, 1));    \
        _a->words.w1 = (u32)(m);                                                                           \
    }

#undef aAddMixer
#define aAddMixer(pkt, count, dmemi, dmemo, a4)                                                            \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = (_SHIFTL(A_CMD_ADDMIXER, 24, 8) | _SHIFTL(count >> 4, 16, 8) | _SHIFTL(a4, 0, 16)); \
        _a->words.w1 = _SHIFTL(dmemi, 16, 16) | _SHIFTL(dmemo, 0, 16);                                     \
    }

#undef aResampleZoh
#define aResampleZoh(pkt, pitch, pitchAccu)                                                                \
    {                                                                                                      \
        NaAcmd* _a = (NaAcmd*)pkt;                                                                             \
        _a->words.w0 = (_SHIFTL(A_CMD_RESAMPLE_ZOH, 24, 8) | _SHIFTL(pitch, 0, 16));                       \
        _a->words.w1 = _SHIFTL(pitchAccu, 0, 16);                                                          \
    }

#ifdef __cplusplus
}
#endif

#endif
