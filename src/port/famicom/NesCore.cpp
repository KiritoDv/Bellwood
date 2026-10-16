#include "NesCore.h"

#include <algorithm>
#include <cstring>
#include <vector>

extern "C" {
void Sound_Write(uint16_t event, uint8_t value, uint16_t cycles);
uint8_t Sound_Read(uint16_t reg);
void Sound_SetC000(uint8_t* rom);
void Sound_SetE000(uint8_t* rom);
}

namespace {

enum Mirroring { kMirrorHorizontal, kMirrorVertical, kMirrorSingle0, kMirrorSingle1, kMirrorFour };

enum : uint8_t {
    kFlagC = 0x01,
    kFlagZ = 0x02,
    kFlagI = 0x04,
    kFlagD = 0x08,
    kFlagB = 0x10,
    kFlagU = 0x20,
    kFlagV = 0x40,
    kFlagN = 0x80,
};

enum : uint8_t {
    kIrqMapper = 0x01,
    kIrqFdsTimer = 0x02,
    kIrqFdsDisk = 0x04,
};

enum Mode : uint8_t { IMP, ACC, IMM, ZP0, ZPX, ZPY, ABS, ABX, ABY, IZX, IZY, REL, IND };

enum Op : uint8_t {
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY,
    EOR, INC, INX, INY, JMP, JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC, SED,
    SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA, KIL, SLO, RLA, SRE, RRA, SAX, LAX, DCP, ISB, ANC, ALR, ARR, XAA,
    AXS, AHX, SHY, SHX, TAS, LAS,
};

enum Kind : uint8_t { kRead, kWrite, kRmw, kOther };

// clang-format off
const Op kOps[256] = {
    BRK, ORA, KIL, SLO, NOP, ORA, ASL, SLO, PHP, ORA, ASL, ANC, NOP, ORA, ASL, SLO,
    BPL, ORA, KIL, SLO, NOP, ORA, ASL, SLO, CLC, ORA, NOP, SLO, NOP, ORA, ASL, SLO,
    JSR, AND, KIL, RLA, BIT, AND, ROL, RLA, PLP, AND, ROL, ANC, BIT, AND, ROL, RLA,
    BMI, AND, KIL, RLA, NOP, AND, ROL, RLA, SEC, AND, NOP, RLA, NOP, AND, ROL, RLA,
    RTI, EOR, KIL, SRE, NOP, EOR, LSR, SRE, PHA, EOR, LSR, ALR, JMP, EOR, LSR, SRE,
    BVC, EOR, KIL, SRE, NOP, EOR, LSR, SRE, CLI, EOR, NOP, SRE, NOP, EOR, LSR, SRE,
    RTS, ADC, KIL, RRA, NOP, ADC, ROR, RRA, PLA, ADC, ROR, ARR, JMP, ADC, ROR, RRA,
    BVS, ADC, KIL, RRA, NOP, ADC, ROR, RRA, SEI, ADC, NOP, RRA, NOP, ADC, ROR, RRA,
    NOP, STA, NOP, SAX, STY, STA, STX, SAX, DEY, NOP, TXA, XAA, STY, STA, STX, SAX,
    BCC, STA, KIL, AHX, STY, STA, STX, SAX, TYA, STA, TXS, TAS, SHY, STA, SHX, AHX,
    LDY, LDA, LDX, LAX, LDY, LDA, LDX, LAX, TAY, LDA, TAX, LAX, LDY, LDA, LDX, LAX,
    BCS, LDA, KIL, LAX, LDY, LDA, LDX, LAX, CLV, LDA, TSX, LAS, LDY, LDA, LDX, LAX,
    CPY, CMP, NOP, DCP, CPY, CMP, DEC, DCP, INY, CMP, DEX, AXS, CPY, CMP, DEC, DCP,
    BNE, CMP, KIL, DCP, NOP, CMP, DEC, DCP, CLD, CMP, NOP, DCP, NOP, CMP, DEC, DCP,
    CPX, SBC, NOP, ISB, CPX, SBC, INC, ISB, INX, SBC, NOP, SBC, CPX, SBC, INC, ISB,
    BEQ, SBC, KIL, ISB, NOP, SBC, INC, ISB, SED, SBC, NOP, ISB, NOP, SBC, INC, ISB,
};

const Mode kModes[256] = {
    IMP, IZX, IMP, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    ABS, IZX, IMP, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    IMP, IZX, IMP, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    IMP, IZX, IMP, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, ACC, IMM, IND, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    IMM, IZX, IMM, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABY, ABY,
    IMM, IZX, IMM, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABY, ABY,
    IMM, IZX, IMM, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    IMM, IZX, IMM, IZX, ZP0, ZP0, ZP0, ZP0, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    REL, IZY, IMP, IZY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
};
// clang-format on

Kind KindOf(Op op) {
    switch (op) {
        case ADC: case AND: case BIT: case CMP: case CPX: case CPY: case EOR: case LDA: case LDX: case LDY: case NOP:
        case ORA: case SBC: case LAX: case ANC: case ALR: case ARR: case XAA: case AXS: case LAS:
            return kRead;
        case STA: case STX: case STY: case SAX: case AHX: case SHY: case SHX: case TAS:
            return kWrite;
        case ASL: case DEC: case INC: case LSR: case ROL: case ROR: case SLO: case RLA: case SRE: case RRA: case DCP:
        case ISB:
            return kRmw;
        default:
            return kOther;
    }
}

constexpr int kDotsPerLine = 341;
constexpr int kPreRenderLine = 261;
constexpr int kVblankLine = 241;
constexpr uint64_t kFrameCycleLimit = 60000;

constexpr size_t kFdsLeadGap = 28300 / 8;
constexpr size_t kFdsBlockGap = 976 / 8;
constexpr int kFdsHeadDelay = 50000;
constexpr int kFdsByteDelay = 150;

uint8_t ReverseBits(uint8_t v) {
    v = (uint8_t)((v & 0xF0) >> 4 | (v & 0x0F) << 4);
    v = (uint8_t)((v & 0xCC) >> 2 | (v & 0x33) << 2);
    v = (uint8_t)((v & 0xAA) >> 1 | (v & 0x55) << 1);
    return v;
}

void FdsCrcUpdate(uint16_t& crc, uint8_t value) {
    for (int n = 0x01; n <= 0x80; n <<= 1) {
        uint16_t carry = crc & 1;
        crc >>= 1;
        if (carry) {
            crc ^= 0x8408;
        }
        if (value & n) {
            crc ^= 0x8000;
        }
    }
}

uint16_t FdsBlockCrc(const uint8_t* data, size_t size) {
    uint16_t crc = 0;
    FdsCrcUpdate(crc, 0x80);
    for (size_t i = 0; i < size; i++) {
        FdsCrcUpdate(crc, data[i]);
    }
    FdsCrcUpdate(crc, 0x00);
    FdsCrcUpdate(crc, 0x00);
    return crc;
}

struct BiosTrap {
    uint16_t addr;
    uint8_t operand;
    uint8_t opcode;
};

const BiosTrap kBiosTraps[] = {
    { 0xE239, 0x0E, 0x85 },
    { 0xE406, 0x01, 0x85 },
    { 0xE7A4, 0xA4, 0x4C },
    { 0xEEF4, 0x90, 0xA5 },
};

void RestoreBiosTraps(uint8_t* bios) {
    for (const BiosTrap& trap : kBiosTraps) {
        uint8_t* op = &bios[trap.addr - 0xE000];
        if (op[0] == 0x42 && op[1] == trap.operand) {
            op[0] = trap.opcode;
        }
    }
}

size_t FdsBlockSize(uint8_t type, uint32_t file_size) {
    switch (type) {
        case 1:
            return 56;
        case 2:
            return 2;
        case 3:
            return 16;
        case 4:
            return 1 + file_size;
        default:
            return 0;
    }
}

} // namespace

struct NesCore {
    uint8_t* image = nullptr;
    size_t imageSize = 0;
    uint8_t* prg = nullptr;
    size_t prgSize = 0;
    uint8_t* chr = nullptr;
    size_t chrSize = 0;
    bool chrWritable = false;
    uint8_t* sram = nullptr;
    uint8_t bios[NES_FDS_BIOS_SIZE] = {};
    int mapper = -1;
    bool fds = false;
    Mirroring hardMirroring = kMirrorHorizontal;

    uint8_t* prgWindow[5] = {};
    bool prgWritable[5] = {};
    uint8_t* chrWindow[8] = {};
    uint8_t* ntWindow[4] = {};

    uint8_t a = 0, x = 0, y = 0, s = 0xFD, p = kFlagI | kFlagU;
    uint16_t pc = 0;
    uint64_t cycles = 0;
    bool nmiLine = false;
    bool nmiPending = false;
    bool irqAllowed = false;
    uint8_t irqSources = 0;
    uint8_t openBus = 0;
    bool jammed = false;

    uint8_t ram[0x800] = {};
    uint8_t vram[0x1000] = {};
    uint8_t palette[32] = {};
    uint8_t oam[256] = {};
    uint8_t chrRam[0x8000] = {};

    uint8_t ctrl = 0, mask = 0, status = 0, oamAddr = 0;
    uint16_t v = 0, t = 0;
    uint8_t fineX = 0;
    bool w = false;
    uint8_t readBuffer = 0;
    uint8_t ppuBus = 0;
    int scanline = 0;
    int dot = 0;
    bool oddFrame = false;
    bool frameDone = false;
    uint64_t ppuClock = 0;

    uint8_t ntByte = 0, atLatch = 0, ptLo = 0, ptHi = 0;
    uint16_t bgLo = 0, bgHi = 0, atLo = 0, atHi = 0;

    int spriteCount = 0;
    uint8_t spriteIndex[8] = {};
    uint8_t spriteY[8] = {};
    uint8_t spriteTile[8] = {};
    uint8_t spriteAttr[8] = {};
    uint8_t spriteX[8] = {};
    uint8_t spriteLo[8] = {};
    uint8_t spriteHi[8] = {};
    int lineSpriteCount = 0;
    bool lineSpriteZero[8] = {};
    uint8_t lineSpriteAttr[8] = {};
    uint8_t lineSpriteX[8] = {};
    uint8_t lineSpriteLo[8] = {};
    uint8_t lineSpriteHi[8] = {};

    uint16_t frame[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT] = {};

    uint8_t buttons[2] = {};
    uint8_t shift[2] = {};
    bool strobe = false;
    bool audio = true;

    bool ppuA12 = false;
    uint64_t a12LowSince = 0;

    uint8_t mmc1Shift = 0x10;
    uint8_t mmc1Control = 0x0C;
    uint8_t mmc1Chr0 = 0, mmc1Chr1 = 0, mmc1Prg = 0;
    uint64_t mmc1LastWrite = 0;

    uint8_t mmc3Select = 0;
    uint8_t mmc3Regs[8] = {};
    uint8_t mmc3Latch = 0;
    uint8_t mmc3Counter = 0;
    bool mmc3Reload = false;
    bool mmc3IrqEnabled = false;

    uint8_t mmc2Chr[4] = {};
    uint8_t mmc2Latch[2] = { 0xFE, 0xFE };

    std::vector<uint8_t> disk;
    size_t diskSideOffset = 0;
    size_t diskSideSize = 0;
    bool diskDirty = false;
    size_t diskPos = 0;
    int diskDelay = 0;
    bool fdsMotorOn = false;
    bool fdsResetTransfer = false;
    bool fdsReadMode = false;
    bool fdsCrcControl = false;
    bool fdsPrevCrcControl = false;
    bool fdsDiskReady = false;
    bool fdsDiskIrqEnabled = false;
    bool fdsTransferComplete = false;
    bool fdsEndOfHead = false;
    bool fdsScanning = false;
    bool fdsGapEnded = false;
    uint16_t fdsCrc = 0;
    uint8_t fdsReadData = 0;
    uint8_t fdsWriteData = 0;
    bool fdsDiskRegEnabled = false;
    bool fdsSoundRegEnabled = false;
    uint16_t fdsIrqReload = 0;
    uint16_t fdsIrqCounter = 0;
    bool fdsIrqEnabled = false;
    bool fdsIrqRepeat = false;
    bool fdsGameStarted = false;

    size_t PrgBanks8() const {
        return std::max<size_t>(1, prgSize / 0x2000);
    }

    size_t ChrBanks1() const {
        return std::max<size_t>(1, chrSize / 0x400);
    }

    void SetPrg8(int slot, int bank) {
        size_t banks = PrgBanks8();
        size_t b = (size_t)(((bank % (int)banks) + (int)banks) % (int)banks);
        prgWindow[slot + 1] = prg + b * 0x2000;
        prgWritable[slot + 1] = false;
    }

    void SetPrg16(int slot, int bank) {
        SetPrg8(slot * 2, bank * 2);
        SetPrg8(slot * 2 + 1, bank * 2 + 1);
    }

    void SetPrg32(int bank) {
        SetPrg16(0, bank * 2);
        SetPrg16(1, bank * 2 + 1);
    }

    void SetChr1(int slot, int bank) {
        size_t banks = ChrBanks1();
        size_t b = (size_t)(((bank % (int)banks) + (int)banks) % (int)banks);
        chrWindow[slot] = chr + b * 0x400;
    }

    void SetChr4(int slot, int bank) {
        for (int i = 0; i < 4; i++) {
            SetChr1(slot * 4 + i, bank * 4 + i);
        }
    }

    void SetChr8(int bank) {
        SetChr4(0, bank * 2);
        SetChr4(1, bank * 2 + 1);
    }

    void SetMirroring(Mirroring m) {
        static const uint8_t kPages[5][4] = {
            { 0, 0, 1, 1 }, { 0, 1, 0, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 0, 1, 2, 3 },
        };
        for (int i = 0; i < 4; i++) {
            ntWindow[i] = vram + kPages[m][i] * 0x400;
        }
    }

    void UpdateSoundRom() {
        Sound_SetC000(prgWindow[3]);
        Sound_SetE000(prgWindow[4]);
    }

    void MapperReset() {
        if (fds) {
            for (int i = 0; i < 4; i++) {
                prgWindow[i] = sram + i * 0x2000;
                prgWritable[i] = true;
            }
            prgWindow[4] = bios;
            prgWritable[4] = false;
            SetChr8(0);
            SetMirroring(kMirrorHorizontal);
            UpdateSoundRom();
            return;
        }

        prgWindow[0] = sram;
        prgWritable[0] = true;
        SetMirroring(hardMirroring);
        SetChr8(0);

        switch (mapper) {
            case 1:
                mmc1Shift = 0x10;
                mmc1Control = 0x0C;
                mmc1Chr0 = mmc1Chr1 = mmc1Prg = 0;
                Mmc1Update();
                break;
            case 2:
                SetPrg16(0, 0);
                SetPrg16(1, -1);
                break;
            case 4:
                mmc3Select = 0;
                memset(mmc3Regs, 0, sizeof(mmc3Regs));
                mmc3Regs[6] = 0;
                mmc3Regs[7] = 1;
                mmc3Latch = mmc3Counter = 0;
                mmc3Reload = false;
                mmc3IrqEnabled = false;
                Mmc3Update();
                break;
            case 9:
                SetPrg8(0, 0);
                SetPrg8(1, -3);
                SetPrg8(2, -2);
                SetPrg8(3, -1);
                memset(mmc2Chr, 0, sizeof(mmc2Chr));
                mmc2Latch[0] = mmc2Latch[1] = 0xFE;
                Mmc2UpdateChr();
                break;
            case 10:
                SetPrg16(0, 0);
                SetPrg16(1, -1);
                memset(mmc2Chr, 0, sizeof(mmc2Chr));
                mmc2Latch[0] = mmc2Latch[1] = 0xFE;
                Mmc2UpdateChr();
                break;
            default:
                SetPrg32(0);
                if (prgSize <= 0x4000) {
                    SetPrg16(1, 0);
                }
                break;
        }
        UpdateSoundRom();
    }

    void Mmc1Update() {
        static const Mirroring kMirror[4] = { kMirrorSingle0, kMirrorSingle1, kMirrorVertical, kMirrorHorizontal };
        SetMirroring(kMirror[mmc1Control & 3]);

        int outer = prgSize > 0x40000 ? (mmc1Chr0 & 0x10) : 0;
        int bank = mmc1Prg & 0x0F;
        switch ((mmc1Control >> 2) & 3) {
            case 0:
            case 1:
                SetPrg32((outer | bank) >> 1);
                break;
            case 2:
                SetPrg16(0, outer);
                SetPrg16(1, outer | bank);
                break;
            case 3:
                SetPrg16(0, outer | bank);
                SetPrg16(1, outer | 0x0F);
                break;
        }

        if (mmc1Control & 0x10) {
            SetChr4(0, mmc1Chr0);
            SetChr4(1, mmc1Chr1);
        } else {
            SetChr8(mmc1Chr0 >> 1);
        }
        UpdateSoundRom();
    }

    void Mmc3Update() {
        int r6 = mmc3Regs[6] & 0x3F;
        int r7 = mmc3Regs[7] & 0x3F;
        if (mmc3Select & 0x40) {
            SetPrg8(0, -2);
            SetPrg8(1, r7);
            SetPrg8(2, r6);
        } else {
            SetPrg8(0, r6);
            SetPrg8(1, r7);
            SetPrg8(2, -2);
        }
        SetPrg8(3, -1);

        int inv = (mmc3Select & 0x80) ? 4 : 0;
        SetChr1(0 ^ inv, mmc3Regs[0] & 0xFE);
        SetChr1(1 ^ inv, mmc3Regs[0] | 1);
        SetChr1(2 ^ inv, mmc3Regs[1] & 0xFE);
        SetChr1(3 ^ inv, mmc3Regs[1] | 1);
        SetChr1(4 ^ inv, mmc3Regs[2]);
        SetChr1(5 ^ inv, mmc3Regs[3]);
        SetChr1(6 ^ inv, mmc3Regs[4]);
        SetChr1(7 ^ inv, mmc3Regs[5]);
        UpdateSoundRom();
    }

    void Mmc3ClockIrq() {
        if (mmc3Counter == 0 || mmc3Reload) {
            mmc3Counter = mmc3Latch;
            mmc3Reload = false;
        } else {
            mmc3Counter--;
        }
        if (mmc3Counter == 0 && mmc3IrqEnabled) {
            irqSources |= kIrqMapper;
        }
    }

    void Mmc2UpdateChr() {
        SetChr4(0, mmc2Latch[0] == 0xFD ? mmc2Chr[0] : mmc2Chr[1]);
        SetChr4(1, mmc2Latch[1] == 0xFD ? mmc2Chr[2] : mmc2Chr[3]);
    }

    void MapperWrite(uint16_t addr, uint8_t value) {
        switch (mapper) {
            case 1: {
                if (value & 0x80) {
                    mmc1Shift = 0x10;
                    mmc1Control |= 0x0C;
                    Mmc1Update();
                    mmc1LastWrite = cycles;
                    return;
                }
                if (cycles == mmc1LastWrite + 1) {
                    mmc1LastWrite = cycles;
                    return;
                }
                mmc1LastWrite = cycles;
                bool done = mmc1Shift & 1;
                mmc1Shift = (uint8_t)((mmc1Shift >> 1) | ((value & 1) << 4));
                if (done) {
                    switch ((addr >> 13) & 3) {
                        case 0:
                            mmc1Control = mmc1Shift;
                            break;
                        case 1:
                            mmc1Chr0 = mmc1Shift;
                            break;
                        case 2:
                            mmc1Chr1 = mmc1Shift;
                            break;
                        case 3:
                            mmc1Prg = mmc1Shift;
                            break;
                    }
                    mmc1Shift = 0x10;
                    Mmc1Update();
                }
                break;
            }
            case 2:
                SetPrg16(0, value);
                UpdateSoundRom();
                break;
            case 3:
                SetChr8(value);
                break;
            case 4:
                switch (addr & 0xE001) {
                    case 0x8000:
                        mmc3Select = value;
                        Mmc3Update();
                        break;
                    case 0x8001:
                        mmc3Regs[mmc3Select & 7] = value;
                        Mmc3Update();
                        break;
                    case 0xA000:
                        if (hardMirroring != kMirrorFour) {
                            SetMirroring((value & 1) ? kMirrorHorizontal : kMirrorVertical);
                        }
                        break;
                    case 0xC000:
                        mmc3Latch = value;
                        break;
                    case 0xC001:
                        mmc3Counter = 0;
                        mmc3Reload = true;
                        break;
                    case 0xE000:
                        mmc3IrqEnabled = false;
                        irqSources &= ~kIrqMapper;
                        break;
                    case 0xE001:
                        mmc3IrqEnabled = true;
                        break;
                }
                break;
            case 7:
                SetPrg32(value & 0x0F);
                SetMirroring((value & 0x10) ? kMirrorSingle1 : kMirrorSingle0);
                UpdateSoundRom();
                break;
            case 9:
            case 10:
                switch (addr & 0xF000) {
                    case 0xA000:
                        if (mapper == 9) {
                            SetPrg8(0, value & 0x0F);
                        } else {
                            SetPrg16(0, value & 0x0F);
                        }
                        UpdateSoundRom();
                        break;
                    case 0xB000:
                    case 0xC000:
                    case 0xD000:
                    case 0xE000:
                        mmc2Chr[((addr >> 12) & 0xF) - 0xB] = value & 0x1F;
                        Mmc2UpdateChr();
                        break;
                    case 0xF000:
                        SetMirroring((value & 1) ? kMirrorHorizontal : kMirrorVertical);
                        break;
                }
                break;
            case 11:
                SetPrg32(value & 0x03);
                SetChr8(value >> 4);
                UpdateSoundRom();
                break;
            case 66:
                SetPrg32((value >> 4) & 0x03);
                SetChr8(value & 0x03);
                UpdateSoundRom();
                break;
            default:
                break;
        }
    }

    void PpuBusAddress(uint16_t addr) {
        if (mapper != 4) {
            return;
        }
        bool a12 = (addr & 0x1000) != 0;
        if (a12 && !ppuA12 && ppuClock - a12LowSince >= 9) {
            Mmc3ClockIrq();
        }
        if (!a12 && ppuA12) {
            a12LowSince = ppuClock;
        }
        ppuA12 = a12;
    }

    uint8_t ChrFetch(uint16_t addr) {
        PpuBusAddress(addr);
        uint8_t value = chrWindow[(addr >> 10) & 7][addr & 0x3FF];
        if (mapper == 9 || mapper == 10) {
            uint16_t lo = addr & 0x1FF8;
            bool left = addr < 0x1000;
            if (left && (mapper == 9 ? addr == 0x0FD8 : lo == 0x0FD8)) {
                mmc2Latch[0] = 0xFD;
                Mmc2UpdateChr();
            } else if (left && (mapper == 9 ? addr == 0x0FE8 : lo == 0x0FE8)) {
                mmc2Latch[0] = 0xFE;
                Mmc2UpdateChr();
            } else if (lo == 0x1FD8) {
                mmc2Latch[1] = 0xFD;
                Mmc2UpdateChr();
            } else if (lo == 0x1FE8) {
                mmc2Latch[1] = 0xFE;
                Mmc2UpdateChr();
            }
        }
        return value;
    }

    uint8_t PaletteIndex(uint16_t addr) const {
        uint8_t i = addr & 0x1F;
        if ((i & 0x13) == 0x10) {
            i &= ~0x10;
        }
        return i;
    }

    uint8_t VramRead(uint16_t addr) {
        addr &= 0x3FFF;
        if (addr < 0x2000) {
            return ChrFetch(addr);
        }
        if (addr < 0x3F00) {
            return ntWindow[(addr >> 10) & 3][addr & 0x3FF];
        }
        return palette[PaletteIndex(addr)];
    }

    void VramWrite(uint16_t addr, uint8_t value) {
        addr &= 0x3FFF;
        if (addr < 0x2000) {
            PpuBusAddress(addr);
            if (chrWritable) {
                chrWindow[(addr >> 10) & 7][addr & 0x3FF] = value;
            }
        } else if (addr < 0x3F00) {
            ntWindow[(addr >> 10) & 3][addr & 0x3FF] = value;
        } else {
            palette[PaletteIndex(addr)] = value & 0x3F;
        }
    }

    bool Rendering() const {
        return (mask & 0x18) != 0;
    }

    void UpdateNmi() {
        bool line = (status & 0x80) && (ctrl & 0x80);
        if (line && !nmiLine) {
            nmiPending = true;
        }
        nmiLine = line;
    }

    void IncrementX() {
        if ((v & 0x001F) == 31) {
            v &= ~0x001F;
            v ^= 0x0400;
        } else {
            v++;
        }
    }

    void IncrementY() {
        if ((v & 0x7000) != 0x7000) {
            v += 0x1000;
            return;
        }
        v &= ~0x7000;
        int row = (v & 0x03E0) >> 5;
        if (row == 29) {
            row = 0;
            v ^= 0x0800;
        } else if (row == 31) {
            row = 0;
        } else {
            row++;
        }
        v = (uint16_t)((v & ~0x03E0) | (row << 5));
    }

    void IncrementAddress() {
        if (Rendering() && (scanline < 240 || scanline == kPreRenderLine)) {
            IncrementX();
            IncrementY();
        } else {
            v = (uint16_t)((v + ((ctrl & 0x04) ? 32 : 1)) & 0x7FFF);
        }
        PpuBusAddress(v & 0x3FFF);
    }

    uint8_t PpuRegRead(int reg) {
        switch (reg) {
            case 2: {
                uint8_t value = (uint8_t)((status & 0xE0) | (ppuBus & 0x1F));
                if (scanline == kVblankLine && dot <= 3) {
                    nmiPending = false;
                }
                status &= ~0x80;
                w = false;
                UpdateNmi();
                ppuBus = value;
                return value;
            }
            case 4: {
                uint8_t value = oam[oamAddr];
                if ((oamAddr & 3) == 2) {
                    value &= 0xE3;
                }
                ppuBus = value;
                return value;
            }
            case 7: {
                uint16_t addr = v & 0x3FFF;
                uint8_t value;
                if (addr >= 0x3F00) {
                    value = (uint8_t)((palette[PaletteIndex(addr)] & ((mask & 1) ? 0x30 : 0x3F)) | (ppuBus & 0xC0));
                    readBuffer = ntWindow[((addr - 0x1000) >> 10) & 3][addr & 0x3FF];
                } else {
                    value = readBuffer;
                    readBuffer = VramRead(addr);
                }
                IncrementAddress();
                ppuBus = value;
                return value;
            }
            default:
                return ppuBus;
        }
    }

    void PpuRegWrite(int reg, uint8_t value) {
        ppuBus = value;
        switch (reg) {
            case 0:
                ctrl = value;
                t = (uint16_t)((t & 0xF3FF) | ((value & 0x03) << 10));
                UpdateNmi();
                break;
            case 1:
                mask = value;
                break;
            case 3:
                oamAddr = value;
                break;
            case 4:
                oam[oamAddr++] = value;
                break;
            case 5:
                if (!w) {
                    t = (uint16_t)((t & 0xFFE0) | (value >> 3));
                    fineX = value & 7;
                } else {
                    t = (uint16_t)((t & 0x8C1F) | ((value & 0x07) << 12) | ((value & 0xF8) << 2));
                }
                w = !w;
                break;
            case 6:
                if (!w) {
                    t = (uint16_t)((t & 0x00FF) | ((value & 0x3F) << 8));
                } else {
                    t = (uint16_t)((t & 0xFF00) | value);
                    v = t;
                    PpuBusAddress(v & 0x3FFF);
                }
                w = !w;
                break;
            case 7:
                VramWrite(v, value);
                IncrementAddress();
                break;
        }
    }

    void ShiftBackground() {
        bgLo <<= 1;
        bgHi <<= 1;
        atLo <<= 1;
        atHi <<= 1;
    }

    void ReloadBackground() {
        bgLo = (uint16_t)((bgLo & 0xFF00) | ptLo);
        bgHi = (uint16_t)((bgHi & 0xFF00) | ptHi);
        atLo = (uint16_t)((atLo & 0xFF00) | ((atLatch & 1) ? 0xFF : 0x00));
        atHi = (uint16_t)((atHi & 0xFF00) | ((atLatch & 2) ? 0xFF : 0x00));
    }

    uint16_t BackgroundPatternAddress() const {
        return (uint16_t)(((ctrl & 0x10) << 8) + ntByte * 16 + ((v >> 12) & 7));
    }

    void BackgroundFetch() {
        switch (dot & 7) {
            case 1:
                if (dot >= 9) {
                    ReloadBackground();
                }
                ntByte = VramRead(0x2000 | (v & 0x0FFF));
                break;
            case 3: {
                uint8_t at = VramRead((uint16_t)(0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07)));
                int sh = ((v >> 4) & 4) | (v & 2);
                atLatch = (at >> sh) & 3;
                break;
            }
            case 5:
                ptLo = ChrFetch(BackgroundPatternAddress());
                break;
            case 7:
                ptHi = ChrFetch((uint16_t)(BackgroundPatternAddress() + 8));
                break;
            case 0:
                IncrementX();
                break;
        }
    }

    void EvaluateSprites() {
        int height = (ctrl & 0x20) ? 16 : 8;
        spriteCount = 0;
        for (int i = 0; i < 64; i++) {
            int row = scanline - oam[i * 4];
            if (row < 0 || row >= height) {
                continue;
            }
            if (spriteCount == 8) {
                status |= 0x20;
                break;
            }
            spriteIndex[spriteCount] = (uint8_t)i;
            spriteY[spriteCount] = oam[i * 4];
            spriteTile[spriteCount] = oam[i * 4 + 1];
            spriteAttr[spriteCount] = oam[i * 4 + 2];
            spriteX[spriteCount] = oam[i * 4 + 3];
            spriteCount++;
        }
    }

    uint16_t SpritePatternAddress(int slot) const {
        int height = (ctrl & 0x20) ? 16 : 8;
        uint8_t tile = 0xFF;
        int row = 0;
        uint8_t attr = 0;
        if (slot < spriteCount) {
            tile = spriteTile[slot];
            attr = spriteAttr[slot];
            row = scanline - spriteY[slot];
            if (attr & 0x80) {
                row = height - 1 - row;
            }
        }
        if (height == 16) {
            uint16_t base = (tile & 1) ? 0x1000 : 0x0000;
            tile &= 0xFE;
            if (row >= 8) {
                tile++;
                row -= 8;
            }
            return (uint16_t)(base + tile * 16 + row);
        }
        return (uint16_t)(((ctrl & 0x08) ? 0x1000 : 0x0000) + tile * 16 + row);
    }

    void SpriteFetch() {
        int slot = (dot - 257) / 8;
        switch ((dot - 257) & 7) {
            case 0:
                VramRead(0x2000 | (v & 0x0FFF));
                break;
            case 2:
                VramRead(0x2000 | (v & 0x0FFF));
                break;
            case 4: {
                uint8_t lo = ChrFetch(SpritePatternAddress(slot));
                spriteLo[slot] = (slot < spriteCount && (spriteAttr[slot] & 0x40)) ? ReverseBits(lo) : lo;
                break;
            }
            case 6: {
                uint8_t hi = ChrFetch((uint16_t)(SpritePatternAddress(slot) + 8));
                spriteHi[slot] = (slot < spriteCount && (spriteAttr[slot] & 0x40)) ? ReverseBits(hi) : hi;
                break;
            }
        }
    }

    void LatchLineSprites() {
        lineSpriteCount = spriteCount;
        for (int i = 0; i < spriteCount; i++) {
            lineSpriteZero[i] = spriteIndex[i] == 0;
            lineSpriteAttr[i] = spriteAttr[i];
            lineSpriteX[i] = spriteX[i];
            lineSpriteLo[i] = spriteLo[i];
            lineSpriteHi[i] = spriteHi[i];
        }
    }

    void RenderPixel(int px) {
        uint8_t color;

        if (!Rendering()) {
            color = ((v & 0x3F00) == 0x3F00) ? palette[PaletteIndex(v)] : palette[0];
        } else {
            uint8_t bgPixel = 0;
            uint8_t bgPalette = 0;
            if ((mask & 0x08) && (px >= 8 || (mask & 0x02))) {
                int bit = 15 - fineX;
                bgPixel = (uint8_t)(((bgLo >> bit) & 1) | (((bgHi >> bit) & 1) << 1));
                bgPalette = (uint8_t)(((atLo >> bit) & 1) | (((atHi >> bit) & 1) << 1));
            }

            uint8_t spPixel = 0;
            uint8_t spPalette = 0;
            bool spBehind = false;
            bool spZero = false;
            if ((mask & 0x10) && (px >= 8 || (mask & 0x04))) {
                for (int i = 0; i < lineSpriteCount; i++) {
                    int off = px - lineSpriteX[i];
                    if (off < 0 || off > 7) {
                        continue;
                    }
                    uint8_t pix = (uint8_t)(((lineSpriteLo[i] >> (7 - off)) & 1) |
                                            (((lineSpriteHi[i] >> (7 - off)) & 1) << 1));
                    if (pix == 0) {
                        continue;
                    }
                    spPixel = pix;
                    spPalette = (uint8_t)((lineSpriteAttr[i] & 3) + 4);
                    spBehind = (lineSpriteAttr[i] & 0x20) != 0;
                    spZero = lineSpriteZero[i];
                    break;
                }
            }

            if (spZero && bgPixel != 0 && px != 255) {
                status |= 0x40;
            }

            uint8_t index;
            if (bgPixel == 0 && spPixel == 0) {
                index = 0;
            } else if (bgPixel == 0) {
                index = (uint8_t)(spPalette * 4 + spPixel);
            } else if (spPixel == 0 || spBehind) {
                index = (uint8_t)(bgPalette * 4 + bgPixel);
            } else {
                index = (uint8_t)(spPalette * 4 + spPixel);
            }
            color = palette[PaletteIndex(index)];
        }

        if (mask & 0x01) {
            color &= 0x30;
        }
        frame[scanline * NES_SCREEN_WIDTH + px] = (uint16_t)(color | ((mask & 0xE0) << 1));
    }

    void EndScanline() {
        if (audio) {
            Sound_Write(0, 0, (uint16_t)(scanline * 114));
        }
    }

    void PpuStep() {
        ppuClock++;
        const bool visible = scanline < 240;
        const bool pre = scanline == kPreRenderLine;

        if (visible || pre) {
            const bool rendering = Rendering();

            if (pre && dot == 1) {
                status &= ~0xE0;
                UpdateNmi();
            }

            if (rendering && ((dot >= 2 && dot <= 257) || (dot >= 322 && dot <= 337))) {
                ShiftBackground();
            }

            if (visible && dot >= 1 && dot <= 256) {
                if (dot == 1) {
                    LatchLineSprites();
                }
                RenderPixel(dot - 1);
            }

            if (rendering) {
                if ((dot >= 1 && dot <= 256) || (dot >= 321 && dot <= 336)) {
                    BackgroundFetch();
                }
                if (dot == 256) {
                    IncrementY();
                } else if (dot == 257) {
                    ReloadBackground();
                    v = (uint16_t)((v & ~0x041F) | (t & 0x041F));
                    if (visible) {
                        EvaluateSprites();
                    } else {
                        spriteCount = 0;
                    }
                } else if (dot == 337 || dot == 339) {
                    if (dot == 337) {
                        ReloadBackground();
                    }
                    ntByte = VramRead(0x2000 | (v & 0x0FFF));
                }
                if (dot >= 257 && dot <= 320) {
                    oamAddr = 0;
                    SpriteFetch();
                }
                if (pre && dot >= 280 && dot <= 304) {
                    v = (uint16_t)((v & ~0x7BE0) | (t & 0x7BE0));
                }
            } else if (dot == 257) {
                spriteCount = 0;
            }

            if (pre && dot == 339 && oddFrame && rendering) {
                dot = 340;
            }
        } else if (scanline == kVblankLine && dot == 1) {
            status |= 0x80;
            UpdateNmi();
            frameDone = true;
        }

        if (++dot >= kDotsPerLine) {
            dot = 0;
            EndScanline();
            if (++scanline > kPreRenderLine) {
                scanline = 0;
                oddFrame = !oddFrame;
            }
        }
    }

    void BuildDiskStream(size_t side) {
        const uint8_t* qd = image + side * NES_QD_SIDE_SIZE;
        size_t size = std::min<size_t>(NES_QD_SIDE_SIZE, imageSize - side * NES_QD_SIDE_SIZE);

        disk.assign(kFdsLeadGap, 0);
        size_t pos = 0;
        uint32_t fileSize = 0;
        while (pos < size) {
            size_t len = FdsBlockSize(qd[pos], fileSize);
            if (len == 0 || pos + len + 2 > size) {
                break;
            }
            if (qd[pos] == 3) {
                fileSize = (uint32_t)(qd[pos + 13] | (qd[pos + 14] << 8));
            }
            disk.push_back(0x80);
            disk.insert(disk.end(), qd + pos, qd + pos + len + 2);
            disk.insert(disk.end(), kFdsBlockGap, 0);
            pos += len + 2;
        }
        disk.resize(disk.size() + (size - pos), 0);
        diskSideOffset = side * NES_QD_SIDE_SIZE;
        diskSideSize = size;
        diskDirty = false;
    }

    void SyncDisk() {
        if (!fds || !diskDirty) {
            return;
        }
        std::vector<uint8_t> out(diskSideSize, 0);
        size_t pos = 0;
        size_t o = 0;
        uint32_t fileSize = 0;
        int blocks = 0;
        while (pos < disk.size()) {
            while (pos < disk.size() && disk[pos] != 0x80) {
                pos++;
            }
            if (++pos >= disk.size()) {
                break;
            }
            size_t len = FdsBlockSize(disk[pos], fileSize);
            if (len == 0 || pos + len > disk.size() || o + len + 2 > out.size()) {
                break;
            }
            if (disk[pos] == 3) {
                fileSize = (uint32_t)(disk[pos + 13] | (disk[pos + 14] << 8));
            }
            memcpy(&out[o], &disk[pos], len);
            uint16_t crc = FdsBlockCrc(&out[o], len);
            out[o + len] = (uint8_t)crc;
            out[o + len + 1] = (uint8_t)(crc >> 8);
            o += len + 2;
            pos += len + 2;
            blocks++;
        }
        if (blocks >= 2 && out[0] == 1) {
            memcpy(image + diskSideOffset, out.data(), out.size());
        }
        diskDirty = false;
    }

    void FdsClock() {
        if (fdsIrqEnabled) {
            if (fdsIrqCounter == 0) {
                irqSources |= kIrqFdsTimer;
                fdsIrqCounter = fdsIrqReload;
                if (!fdsIrqRepeat) {
                    fdsIrqEnabled = false;
                }
            } else {
                fdsIrqCounter--;
            }
        }

        if (!fdsMotorOn) {
            fdsEndOfHead = true;
            fdsScanning = false;
            return;
        }
        if (fdsResetTransfer && !fdsScanning) {
            return;
        }
        if (fdsEndOfHead) {
            diskDelay = kFdsHeadDelay;
            fdsEndOfHead = false;
            diskPos = 0;
            fdsGapEnded = false;
            return;
        }
        if (diskDelay > 0) {
            diskDelay--;
            return;
        }

        fdsScanning = true;
        uint8_t data = 0;
        bool needIrq = fdsDiskIrqEnabled;

        if (fdsReadMode) {
            data = diskPos < disk.size() ? disk[diskPos] : 0;
            if (!fdsPrevCrcControl) {
                FdsCrcUpdate(fdsCrc, data);
            }
            if (!fdsDiskReady) {
                fdsGapEnded = false;
                fdsCrc = 0;
            } else if (data != 0 && !fdsGapEnded) {
                fdsGapEnded = true;
                needIrq = false;
            }
            if (fdsGapEnded) {
                fdsTransferComplete = true;
                fdsReadData = data;
                if (needIrq) {
                    irqSources |= kIrqFdsDisk;
                }
            }
        } else {
            if (!fdsCrcControl) {
                fdsTransferComplete = true;
                data = fdsWriteData;
                if (needIrq) {
                    irqSources |= kIrqFdsDisk;
                }
            }
            if (!fdsDiskReady) {
                data = 0;
            }
            if (!fdsCrcControl) {
                FdsCrcUpdate(fdsCrc, data);
            } else {
                if (!fdsPrevCrcControl) {
                    FdsCrcUpdate(fdsCrc, 0);
                    FdsCrcUpdate(fdsCrc, 0);
                }
                data = (uint8_t)fdsCrc;
                fdsCrc >>= 8;
            }
            if (diskPos >= 2 && diskPos - 2 < disk.size()) {
                disk[diskPos - 2] = data;
                diskDirty = true;
            }
            fdsGapEnded = false;
        }

        fdsPrevCrcControl = fdsCrcControl;
        if (++diskPos >= disk.size()) {
            fdsMotorOn = false;
        } else {
            diskDelay = kFdsByteDelay;
        }
    }

    uint8_t FdsRead(uint16_t addr) {
        if (addr >= 0x4040 && addr < 0x40A0) {
            if (!fdsSoundRegEnabled) {
                return openBus;
            }
            if (addr == 0x4090 || addr == 0x4092) {
                return (uint8_t)((Sound_Read(addr) & 0x3F) | (openBus & 0xC0));
            }
            return openBus;
        }
        if (!fdsDiskRegEnabled) {
            return openBus;
        }
        switch (addr) {
            case 0x4030: {
                uint8_t value = openBus & 0x2C;
                value |= (irqSources & kIrqFdsTimer) ? 0x01 : 0x00;
                value |= fdsTransferComplete ? 0x02 : 0x00;
                fdsTransferComplete = false;
                irqSources &= ~(kIrqFdsTimer | kIrqFdsDisk);
                return value;
            }
            case 0x4031:
                fdsTransferComplete = false;
                irqSources &= ~kIrqFdsDisk;
                return fdsReadData;
            case 0x4032: {
                uint8_t value = openBus & 0xF8;
                value |= fdsScanning ? 0x00 : 0x02;
                return value;
            }
            case 0x4033:
                return 0x80;
            default:
                return openBus;
        }
    }

    void FdsWrite(uint16_t addr, uint8_t value) {
        if (!fdsDiskRegEnabled && addr >= 0x4024 && addr <= 0x4026) {
            return;
        }
        switch (addr) {
            case 0x4020:
                fdsIrqReload = (uint16_t)((fdsIrqReload & 0xFF00) | value);
                irqSources &= ~kIrqFdsTimer;
                break;
            case 0x4021:
                fdsIrqReload = (uint16_t)((fdsIrqReload & 0x00FF) | (value << 8));
                irqSources &= ~kIrqFdsTimer;
                break;
            case 0x4022:
                fdsIrqRepeat = (value & 0x01) != 0;
                fdsIrqEnabled = (value & 0x02) != 0 && fdsDiskRegEnabled;
                if (fdsIrqEnabled) {
                    fdsIrqCounter = fdsIrqReload;
                } else {
                    irqSources &= ~kIrqFdsTimer;
                }
                break;
            case 0x4023:
                fdsDiskRegEnabled = (value & 0x01) != 0;
                fdsSoundRegEnabled = (value & 0x02) != 0;
                if (!fdsDiskRegEnabled) {
                    fdsIrqEnabled = false;
                    irqSources &= ~(kIrqFdsTimer | kIrqFdsDisk);
                }
                break;
            case 0x4024:
                fdsWriteData = value;
                fdsTransferComplete = false;
                irqSources &= ~kIrqFdsDisk;
                break;
            case 0x4025:
                fdsMotorOn = (value & 0x01) != 0;
                fdsResetTransfer = (value & 0x02) != 0;
                fdsReadMode = (value & 0x04) != 0;
                SetMirroring((value & 0x08) ? kMirrorHorizontal : kMirrorVertical);
                fdsCrcControl = (value & 0x10) != 0;
                fdsDiskReady = (value & 0x40) != 0;
                fdsDiskIrqEnabled = (value & 0x80) != 0;
                irqSources &= ~kIrqFdsDisk;
                break;
            case 0x4026:
                break;
            default:
                if (addr >= 0x4040 && addr < 0x40A0 && fdsSoundRegEnabled) {
                    Sound_Write(addr, value, 0);
                }
                break;
        }
    }

    void Tick() {
        cycles++;
        PpuStep();
        PpuStep();
        PpuStep();
        if (fds) {
            FdsClock();
        }
    }

    uint8_t InputRead(int port) {
        uint8_t bit;
        if (strobe) {
            bit = buttons[port] & 1;
        } else {
            bit = shift[port] & 1;
            shift[port] = (uint8_t)((shift[port] >> 1) | 0x80);
        }
        return (uint8_t)((openBus & 0xE0) | bit);
    }

    uint8_t Peek(uint16_t addr) {
        if (addr < 0x2000) {
            return ram[addr & 0x07FF];
        }
        if (addr < 0x4000) {
            return PpuRegRead(addr & 7);
        }
        if (addr < 0x4020) {
            switch (addr) {
                case 0x4015:
                    return (uint8_t)((Sound_Read(0x4015) & 0xDF) | (openBus & 0x20));
                case 0x4016:
                    return InputRead(0);
                case 0x4017:
                    return InputRead(1);
                default:
                    return openBus;
            }
        }
        if (addr < 0x6000) {
            return fds ? FdsRead(addr) : openBus;
        }
        uint8_t* window = prgWindow[(addr - 0x6000) >> 13];
        return window != nullptr ? window[addr & 0x1FFF] : openBus;
    }

    void Poke(uint16_t addr, uint8_t value) {
        if (addr < 0x2000) {
            ram[addr & 0x07FF] = value;
        } else if (addr < 0x4000) {
            PpuRegWrite(addr & 7, value);
        } else if (addr < 0x4020) {
            switch (addr) {
                case 0x4014:
                    OamDma(value);
                    break;
                case 0x4016: {
                    bool next = (value & 1) != 0;
                    if (next || strobe) {
                        shift[0] = buttons[0];
                        shift[1] = buttons[1];
                    }
                    strobe = next;
                    break;
                }
                default:
                    if (addr <= 0x4013 || addr == 0x4015 || addr == 0x4017) {
                        Sound_Write(addr, value, 0);
                    }
                    break;
            }
        } else if (addr < 0x6000) {
            if (fds) {
                FdsWrite(addr, value);
            }
        } else {
            int slot = (addr - 0x6000) >> 13;
            if (prgWritable[slot] && prgWindow[slot] != nullptr) {
                prgWindow[slot][addr & 0x1FFF] = value;
            }
            if (addr >= 0x8000 && !fds) {
                MapperWrite(addr, value);
            }
        }
    }

    uint8_t Read(uint16_t addr) {
        Tick();
        openBus = Peek(addr);
        return openBus;
    }

    void Write(uint16_t addr, uint8_t value) {
        Tick();
        openBus = value;
        Poke(addr, value);
    }

    void OamDma(uint8_t page) {
        Tick();
        if (cycles & 1) {
            Tick();
        }
        for (int i = 0; i < 256; i++) {
            uint8_t value = Read((uint16_t)((page << 8) | i));
            Tick();
            oam[oamAddr++] = value;
        }
    }

    void Push(uint8_t value) {
        Write((uint16_t)(0x0100 | s), value);
        s--;
    }

    uint8_t Pull() {
        s++;
        return Read((uint16_t)(0x0100 | s));
    }

    void SetZN(uint8_t value) {
        p = (uint8_t)((p & ~(kFlagZ | kFlagN)) | (value == 0 ? kFlagZ : 0) | (value & kFlagN));
    }

    void Adc(uint8_t value) {
        unsigned sum = (unsigned)a + value + (p & kFlagC);
        p &= ~(kFlagC | kFlagV);
        if (sum > 0xFF) {
            p |= kFlagC;
        }
        if (~(a ^ value) & (a ^ sum) & 0x80) {
            p |= kFlagV;
        }
        a = (uint8_t)sum;
        SetZN(a);
    }

    void Compare(uint8_t reg, uint8_t value) {
        p = (uint8_t)((p & ~kFlagC) | (reg >= value ? kFlagC : 0));
        SetZN((uint8_t)(reg - value));
    }

    uint8_t Asl(uint8_t value) {
        p = (uint8_t)((p & ~kFlagC) | (value >> 7));
        value <<= 1;
        SetZN(value);
        return value;
    }

    uint8_t Lsr(uint8_t value) {
        p = (uint8_t)((p & ~kFlagC) | (value & 1));
        value >>= 1;
        SetZN(value);
        return value;
    }

    uint8_t Rol(uint8_t value) {
        uint8_t carry = p & kFlagC;
        p = (uint8_t)((p & ~kFlagC) | (value >> 7));
        value = (uint8_t)((value << 1) | carry);
        SetZN(value);
        return value;
    }

    uint8_t Ror(uint8_t value) {
        uint8_t carry = p & kFlagC;
        p = (uint8_t)((p & ~kFlagC) | (value & 1));
        value = (uint8_t)((value >> 1) | (carry << 7));
        SetZN(value);
        return value;
    }

    uint16_t Indexed(uint8_t index, bool always_dummy) {
        uint16_t lo = Read(pc++);
        uint16_t hi = Read(pc++);
        uint16_t base = (uint16_t)(lo | (hi << 8));
        uint16_t addr = (uint16_t)(base + index);
        if (always_dummy || ((base ^ addr) & 0xFF00)) {
            Read((uint16_t)((base & 0xFF00) | (addr & 0x00FF)));
        }
        return addr;
    }

    uint16_t EffectiveAddress(Mode mode, bool always_dummy) {
        switch (mode) {
            case ZP0:
                return Read(pc++);
            case ZPX: {
                uint8_t base = Read(pc++);
                Read(base);
                return (uint8_t)(base + x);
            }
            case ZPY: {
                uint8_t base = Read(pc++);
                Read(base);
                return (uint8_t)(base + y);
            }
            case ABS: {
                uint16_t lo = Read(pc++);
                uint16_t hi = Read(pc++);
                return (uint16_t)(lo | (hi << 8));
            }
            case ABX:
                return Indexed(x, always_dummy);
            case ABY:
                return Indexed(y, always_dummy);
            case IZX: {
                uint8_t ptr = Read(pc++);
                Read(ptr);
                ptr = (uint8_t)(ptr + x);
                uint16_t lo = Read(ptr);
                uint16_t hi = Read((uint8_t)(ptr + 1));
                return (uint16_t)(lo | (hi << 8));
            }
            case IZY: {
                uint8_t ptr = Read(pc++);
                uint16_t lo = Read(ptr);
                uint16_t hi = Read((uint8_t)(ptr + 1));
                uint16_t base = (uint16_t)(lo | (hi << 8));
                uint16_t addr = (uint16_t)(base + y);
                if (always_dummy || ((base ^ addr) & 0xFF00)) {
                    Read((uint16_t)((base & 0xFF00) | (addr & 0x00FF)));
                }
                return addr;
            }
            default:
                return pc;
        }
    }

    void ExecuteRead(Op op, uint8_t value) {
        switch (op) {
            case ADC:
                Adc(value);
                break;
            case SBC:
                Adc((uint8_t)~value);
                break;
            case AND:
                a &= value;
                SetZN(a);
                break;
            case ORA:
                a |= value;
                SetZN(a);
                break;
            case EOR:
                a ^= value;
                SetZN(a);
                break;
            case CMP:
                Compare(a, value);
                break;
            case CPX:
                Compare(x, value);
                break;
            case CPY:
                Compare(y, value);
                break;
            case BIT:
                p = (uint8_t)((p & ~(kFlagZ | kFlagV | kFlagN)) | (value & (kFlagV | kFlagN)) |
                              ((a & value) ? 0 : kFlagZ));
                break;
            case LDA:
                a = value;
                SetZN(a);
                break;
            case LDX:
                x = value;
                SetZN(x);
                break;
            case LDY:
                y = value;
                SetZN(y);
                break;
            case LAX:
                a = x = value;
                SetZN(a);
                break;
            case ANC:
                a &= value;
                SetZN(a);
                p = (uint8_t)((p & ~kFlagC) | ((a >> 7) & 1));
                break;
            case ALR:
                a &= value;
                a = Lsr(a);
                break;
            case ARR: {
                a &= value;
                a = (uint8_t)((a >> 1) | ((p & kFlagC) << 7));
                SetZN(a);
                uint8_t bit6 = (a >> 6) & 1;
                uint8_t bit5 = (a >> 5) & 1;
                p = (uint8_t)((p & ~(kFlagC | kFlagV)) | bit6 | ((bit6 ^ bit5) << 6));
                break;
            }
            case XAA:
                a = (uint8_t)((a | 0xEE) & x & value);
                SetZN(a);
                break;
            case AXS: {
                uint8_t ax = a & x;
                p = (uint8_t)((p & ~kFlagC) | (ax >= value ? kFlagC : 0));
                x = (uint8_t)(ax - value);
                SetZN(x);
                break;
            }
            case LAS:
                a = x = s = value & s;
                SetZN(a);
                break;
            default:
                break;
        }
    }

    uint8_t ExecuteRmw(Op op, uint8_t value) {
        switch (op) {
            case ASL:
                return Asl(value);
            case LSR:
                return Lsr(value);
            case ROL:
                return Rol(value);
            case ROR:
                return Ror(value);
            case INC:
                value++;
                SetZN(value);
                return value;
            case DEC:
                value--;
                SetZN(value);
                return value;
            case SLO:
                value = Asl(value);
                a |= value;
                SetZN(a);
                return value;
            case RLA:
                value = Rol(value);
                a &= value;
                SetZN(a);
                return value;
            case SRE:
                value = Lsr(value);
                a ^= value;
                SetZN(a);
                return value;
            case RRA:
                value = Ror(value);
                Adc(value);
                return value;
            case DCP:
                value--;
                Compare(a, value);
                return value;
            case ISB:
                value++;
                Adc((uint8_t)~value);
                return value;
            default:
                return value;
        }
    }

    uint8_t StoreValue(Op op, uint16_t addr) {
        uint8_t high = (uint8_t)((addr >> 8) + 1);
        switch (op) {
            case STA:
                return a;
            case STX:
                return x;
            case STY:
                return y;
            case SAX:
                return a & x;
            case SHY:
                return y & high;
            case SHX:
                return x & high;
            case AHX:
                return a & x & high;
            case TAS:
                s = a & x;
                return s & high;
            default:
                return 0;
        }
    }

    void Branch(bool taken) {
        int8_t offset = (int8_t)Read(pc++);
        if (!taken) {
            return;
        }
        Read(pc);
        uint16_t target = (uint16_t)(pc + offset);
        if ((target ^ pc) & 0xFF00) {
            Read((uint16_t)((pc & 0xFF00) | (target & 0x00FF)));
        }
        pc = target;
    }

    void Interrupt(uint16_t vector) {
        Read(pc);
        Read(pc);
        Push((uint8_t)(pc >> 8));
        Push((uint8_t)pc);
        Push((uint8_t)((p & ~kFlagB) | kFlagU));
        p |= kFlagI;
        uint16_t lo = Read(vector);
        uint16_t hi = Read((uint16_t)(vector + 1));
        pc = (uint16_t)(lo | (hi << 8));
    }

    void ExecuteOther(Op op, Mode mode) {
        switch (op) {
            case BRK: {
                Read(pc++);
                Push((uint8_t)(pc >> 8));
                Push((uint8_t)pc);
                uint16_t vector = 0xFFFE;
                if (nmiPending) {
                    nmiPending = false;
                    vector = 0xFFFA;
                }
                Push((uint8_t)(p | kFlagB | kFlagU));
                p |= kFlagI;
                uint16_t lo = Read(vector);
                uint16_t hi = Read((uint16_t)(vector + 1));
                pc = (uint16_t)(lo | (hi << 8));
                break;
            }
            case JSR: {
                uint16_t lo = Read(pc++);
                Read((uint16_t)(0x0100 | s));
                Push((uint8_t)(pc >> 8));
                Push((uint8_t)pc);
                uint16_t hi = Read(pc);
                pc = (uint16_t)(lo | (hi << 8));
                break;
            }
            case RTS: {
                Read(pc);
                Read((uint16_t)(0x0100 | s));
                uint16_t lo = Pull();
                uint16_t hi = Pull();
                pc = (uint16_t)(lo | (hi << 8));
                Read(pc);
                pc++;
                break;
            }
            case RTI: {
                Read(pc);
                Read((uint16_t)(0x0100 | s));
                p = (uint8_t)((Pull() & ~kFlagB) | kFlagU);
                uint16_t lo = Pull();
                uint16_t hi = Pull();
                pc = (uint16_t)(lo | (hi << 8));
                break;
            }
            case JMP:
                if (mode == ABS) {
                    pc = EffectiveAddress(ABS, false);
                } else {
                    uint16_t ptr = EffectiveAddress(ABS, false);
                    uint16_t lo = Read(ptr);
                    uint16_t hi = Read((uint16_t)((ptr & 0xFF00) | ((ptr + 1) & 0x00FF)));
                    pc = (uint16_t)(lo | (hi << 8));
                }
                break;
            case PHA:
                Read(pc);
                Push(a);
                break;
            case PHP:
                Read(pc);
                Push((uint8_t)(p | kFlagB | kFlagU));
                break;
            case PLA:
                Read(pc);
                Read((uint16_t)(0x0100 | s));
                a = Pull();
                SetZN(a);
                break;
            case PLP:
                Read(pc);
                Read((uint16_t)(0x0100 | s));
                p = (uint8_t)((Pull() & ~kFlagB) | kFlagU);
                break;
            case BPL:
                Branch(!(p & kFlagN));
                break;
            case BMI:
                Branch((p & kFlagN) != 0);
                break;
            case BVC:
                Branch(!(p & kFlagV));
                break;
            case BVS:
                Branch((p & kFlagV) != 0);
                break;
            case BCC:
                Branch(!(p & kFlagC));
                break;
            case BCS:
                Branch((p & kFlagC) != 0);
                break;
            case BNE:
                Branch(!(p & kFlagZ));
                break;
            case BEQ:
                Branch((p & kFlagZ) != 0);
                break;
            case KIL:
                jammed = true;
                break;
            default:
                Read(pc);
                switch (op) {
                    case CLC:
                        p &= ~kFlagC;
                        break;
                    case SEC:
                        p |= kFlagC;
                        break;
                    case CLI:
                        p &= ~kFlagI;
                        break;
                    case SEI:
                        p |= kFlagI;
                        break;
                    case CLV:
                        p &= ~kFlagV;
                        break;
                    case CLD:
                        p &= ~kFlagD;
                        break;
                    case SED:
                        p |= kFlagD;
                        break;
                    case TAX:
                        x = a;
                        SetZN(x);
                        break;
                    case TAY:
                        y = a;
                        SetZN(y);
                        break;
                    case TXA:
                        a = x;
                        SetZN(a);
                        break;
                    case TYA:
                        a = y;
                        SetZN(a);
                        break;
                    case TSX:
                        x = s;
                        SetZN(x);
                        break;
                    case TXS:
                        s = x;
                        break;
                    case INX:
                        x++;
                        SetZN(x);
                        break;
                    case INY:
                        y++;
                        SetZN(y);
                        break;
                    case DEX:
                        x--;
                        SetZN(x);
                        break;
                    case DEY:
                        y--;
                        SetZN(y);
                        break;
                    default:
                        break;
                }
                break;
        }
    }

    void Step() {
        if (jammed) {
            Tick();
            return;
        }
        if (nmiPending) {
            nmiPending = false;
            Interrupt(0xFFFA);
            irqAllowed = false;
            return;
        }
        if (irqSources != 0 && irqAllowed) {
            Interrupt(0xFFFE);
            irqAllowed = false;
            return;
        }

        if (pc < 0xE000) {
            fdsGameStarted = true;
        }

        uint8_t iBefore = p & kFlagI;
        uint8_t opcode = Read(pc++);
        Op op = kOps[opcode];
        Mode mode = kModes[opcode];

        switch (KindOf(op)) {
            case kRead:
                if (mode == IMP) {
                    Read(pc);
                } else if (mode == IMM) {
                    ExecuteRead(op, Read(pc++));
                } else {
                    ExecuteRead(op, Read(EffectiveAddress(mode, false)));
                }
                break;
            case kWrite: {
                uint16_t addr = EffectiveAddress(mode, true);
                Write(addr, StoreValue(op, addr));
                break;
            }
            case kRmw:
                if (mode == ACC) {
                    Read(pc);
                    a = ExecuteRmw(op, a);
                } else {
                    uint16_t addr = EffectiveAddress(mode, true);
                    uint8_t value = Read(addr);
                    Write(addr, value);
                    Write(addr, ExecuteRmw(op, value));
                }
                break;
            case kOther:
                ExecuteOther(op, mode);
                break;
        }

        uint8_t iPoll = (op == CLI || op == SEI || op == PLP) ? iBefore : (uint8_t)(p & kFlagI);
        irqAllowed = iPoll == 0;
    }

    void Reset(bool hard) {
        if (hard) {
            memset(ram, 0, sizeof(ram));
            memset(vram, 0, sizeof(vram));
            memset(palette, 0, sizeof(palette));
            memset(oam, 0, sizeof(oam));
            if (chrWritable) {
                memset(chrRam, 0, sizeof(chrRam));
            }
            a = x = y = 0;
            s = 0;
            p = kFlagI | kFlagU;
            MapperReset();
            if (fds) {
                memset(sram, 0, NES_SRAM_SIZE);
            }
            scanline = 0;
            dot = 0;
            oddFrame = false;
        }

        ctrl = 0;
        mask = 0;
        w = false;
        readBuffer = 0;
        status &= 0x80;
        nmiLine = false;
        nmiPending = false;
        irqSources = 0;
        irqAllowed = false;
        jammed = false;
        strobe = false;

        fdsMotorOn = false;
        fdsDiskRegEnabled = false;
        fdsSoundRegEnabled = false;
        fdsIrqEnabled = false;
        fdsTransferComplete = false;
        fdsScanning = false;
        fdsEndOfHead = true;
        fdsGameStarted = false;

        Sound_Write(0x4015, 0x00, 0);
        Sound_Write(0x4017, 0x00, 0);

        s = (uint8_t)(s - 3);
        p |= kFlagI;
        for (int i = 0; i < 5; i++) {
            Tick();
        }
        uint16_t lo = Read(0xFFFC);
        uint16_t hi = Read(0xFFFD);
        pc = (uint16_t)(lo | (hi << 8));
    }

    int Load(uint8_t* data, size_t size, uint8_t* ram_buffer, const uint8_t* bios_rom) {
        image = data;
        imageSize = size;
        sram = ram_buffer;

        if (size >= 16 && memcmp(data, "NES\x1A", 4) == 0) {
            const uint8_t* h = data;
            size_t prgLen = (size_t)h[4] * 0x4000;
            size_t chrLen = (size_t)h[5] * 0x2000;
            size_t offset = 16 + ((h[6] & 0x04) ? 512 : 0);
            int id = h[6] >> 4;
            bool archaic = (h[7] & 0x0C) == 0 && (h[12] | h[13] | h[14] | h[15]) != 0;
            if (!archaic) {
                id |= h[7] & 0xF0;
            }
            if (prgLen == 0 || offset + prgLen + chrLen > size) {
                return NES_LOAD_BAD_IMAGE;
            }
            switch (id) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 7:
                case 9:
                case 10:
                case 11:
                case 66:
                    break;
                default:
                    return NES_LOAD_UNSUPPORTED_MAPPER;
            }
            mapper = id;
            fds = false;
            prg = data + offset;
            prgSize = prgLen;
            if (chrLen != 0) {
                chr = data + offset + prgLen;
                chrSize = chrLen;
                chrWritable = false;
            } else {
                chr = chrRam;
                chrSize = 0x2000;
                chrWritable = true;
            }
            if (h[6] & 0x08) {
                hardMirroring = kMirrorFour;
            } else {
                hardMirroring = (h[6] & 0x01) ? kMirrorVertical : kMirrorHorizontal;
            }
        } else if (size >= NES_QD_SIDE_SIZE && data[0] == 0x01 && memcmp(data + 1, "*NINTENDO-HVC*", 14) == 0) {
            if (bios_rom == nullptr) {
                return NES_LOAD_NO_BIOS;
            }
            mapper = 20;
            fds = true;
            memcpy(bios, bios_rom, NES_FDS_BIOS_SIZE);
            RestoreBiosTraps(bios);
            prg = sram;
            prgSize = NES_SRAM_SIZE;
            chr = chrRam;
            chrSize = 0x2000;
            chrWritable = true;
            hardMirroring = kMirrorHorizontal;
            BuildDiskStream(0);
        } else {
            return NES_LOAD_BAD_IMAGE;
        }

        Reset(true);
        return NES_LOAD_OK;
    }

    void RunFrame(bool with_audio) {
        audio = with_audio;
        frameDone = false;
        uint64_t start = cycles;
        while (!frameDone && cycles - start < kFrameCycleLimit) {
            Step();
        }
    }
};

extern "C" {

NesCore* NesCore_Create(void) {
    return new NesCore();
}

void NesCore_Destroy(NesCore* core) {
    delete core;
}

int NesCore_Load(NesCore* core, uint8_t* image, size_t size, uint8_t* sram, const uint8_t* bios) {
    return core->Load(image, size, sram, bios);
}

int NesCore_GetMapper(const NesCore* core) {
    return core->mapper;
}

int NesCore_IsDiskSystem(const NesCore* core) {
    return core->fds;
}

void NesCore_Reset(NesCore* core, int hard) {
    core->Reset(hard != 0);
}

void NesCore_SetButtons(NesCore* core, int port, uint8_t buttons) {
    if (port >= 0 && port < 2) {
        core->buttons[port] = buttons;
    }
}

void NesCore_RunFrame(NesCore* core, int audio) {
    core->RunFrame(audio != 0);
}

const uint16_t* NesCore_GetFrame(const NesCore* core) {
    return core->frame;
}

uint8_t* NesCore_GetRam(NesCore* core) {
    return core->ram;
}

int NesCore_DiskBusy(const NesCore* core) {
    return core->fds && (core->fdsMotorOn || !core->fdsGameStarted);
}

void NesCore_SyncDisk(NesCore* core) {
    core->SyncDisk();
}
}
