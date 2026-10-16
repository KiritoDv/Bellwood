#include "port/gba/GbaPort.h"
#include "port/gba/DesignFiles.h"
#include "port/gba/ECard.h"
#include "port/save/SaveGc.h"
#include "port/ui/cvar_prefixes.h"

#include <ship/Api.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#define this this_
extern "C" {
#include "GBA2/gba2.h"
#include "ac_port.h"
#include "m_needlework.h"
}
#undef this

API_EXPORT int32_t CVarGetInteger(const char* name, int32_t defaultValue);

namespace fs = std::filesystem;

namespace {

#define CVAR_GBA_ISLAND CVAR_ENHANCEMENT("Gba.IslandWithoutGba")
#define CVAR_GBA_ECARDS CVAR_ENHANCEMENT("Gba.EReaderFiles")
#define CVAR_GBA_DESIGNS CVAR_ENHANCEMENT("Gba.DesignFiles")

constexpr int kScanTimeoutFrames = 30;

enum class Scan { Idle, Waiting, Ready, Cancelled };

Scan sScan = Scan::Idle;
int sScanType = 0;
size_t sScanSize = 0;
std::vector<uint8_t> sPayload;
uint32_t sFrame = 0;
uint32_t sLastPoll = 0;

bool Enabled(const char* cvar) {
    return CVarGetInteger(cvar, 1) != 0;
}

} // namespace

namespace GbaPort {

bool ScanWaiting() {
    return sScan == Scan::Waiting;
}

std::string ScanDescription() {
    switch (sScanType) {
        case GBA2_EAPPLI_TYPE_PTERMINAL:
            return "a letter card";
        case GBA2_EAPPLI_TYPE_NEEDLEWORK:
            return "a design card";
        case GBA2_EAPPLI_TYPE_MUSICSCORE:
            return "a town tune card";
        default:
            return "an e-Reader card";
    }
}

std::string ECardDirectory() {
    return Ship::Context::GetPathRelativeToAppDirectory("ecards");
}

bool SubmitCard(const std::string& path, std::string* error) {
    if (sScan != Scan::Waiting) {
        *error = "Nothing is waiting for a card.";
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *error = "Cannot open " + path + ".";
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::vector<uint8_t>> payloads;
    if (!ECard::ReadPayloads(file, payloads, error)) {
        return false;
    }
    for (const auto& p : payloads) {
        if (p.size() != sScanSize) {
            continue;
        }
        sPayload = p;
        if (sScanType == GBA2_EAPPLI_TYPE_NEEDLEWORK) {
            mNW_original_design_c* design = reinterpret_cast<mNW_original_design_c*>(sPayload.data());
            SaveGcRead(p.data() + offsetof(mNW_original_design_c, design), design->design);
        }
        sScan = Scan::Ready;
        SPDLOG_INFO("e-Reader: read {} from {}", ScanDescription(), path);
        return true;
    }
    *error = "This is not " + ScanDescription() + ".";
    return false;
}

void CancelScan() {
    if (sScan == Scan::Waiting) {
        sScan = Scan::Cancelled;
    }
}

void Tick() {
    sFrame++;
    if (sScan != Scan::Idle && sFrame - sLastPoll > kScanTimeoutFrames) {
        sScan = Scan::Idle;
        sPayload.clear();
    }
}

} // namespace GbaPort

extern "C" {

int AC_GbaIslandWithoutGba(void) {
    return Enabled(CVAR_GBA_ISLAND);
}

int AC_GbaLinkAvailable(void) {
    return FALSE;
}

int AC_EReaderConnect(void) {
    return Enabled(CVAR_GBA_ECARDS) ? GBA2_GBA_STATE_SUCCESS : mGcgba_ConnectEnabled();
}

int AC_EReaderSendApp(u8* program, size_t size) {
    return Enabled(CVAR_GBA_ECARDS) ? GBA2_EAPPLI_SUCCESS : mGcgba_send_eAppri(program, size);
}

int AC_EReaderRead(u8* buf, size_t size, u8 type) {
    if (!Enabled(CVAR_GBA_ECARDS)) {
        return mGcgba_Read(buf, size, type);
    }
    sLastPoll = sFrame;
    switch (sScan) {
        case Scan::Idle:
            sScan = Scan::Waiting;
            sScanType = type;
            sScanSize = size;
            sPayload.clear();
            return GBA2_GBA_STATE_TRANSMITTING;
        case Scan::Waiting:
            return GBA2_GBA_STATE_TRANSMITTING;
        case Scan::Ready:
            memcpy(buf, sPayload.data(), size);
            sScan = Scan::Idle;
            sPayload.clear();
            return GBA2_GBA_STATE_SUCCESS;
        default:
            sScan = Scan::Idle;
            return GBA2_GBA_STATE_ERROR;
    }
}

int AC_DesignFilesEnabled(void) {
    return Enabled(CVAR_GBA_DESIGNS);
}

int AC_DesignExport(const u8* designs, int count) {
    std::string error;
    if (!DesignFiles::Export(designs, count, &error)) {
        SPDLOG_ERROR("designs: {}", error);
        return FALSE;
    }
    return TRUE;
}

int AC_DesignImportReady(void) {
    return DesignFiles::HasImport() ? TRUE : FALSE;
}

int AC_DesignImport(u8* designs, int count) {
    std::string error;
    if (!DesignFiles::Import(designs, count, &error)) {
        SPDLOG_WARN("designs: {}", error);
        return FALSE;
    }
    return TRUE;
}

} // extern "C"
