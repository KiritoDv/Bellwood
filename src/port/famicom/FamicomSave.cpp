#include <libultraship.h>
#include <ship/Context.h>

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

fs::path SavePath() {
    return fs::path(Ship::Context::GetPathRelativeToAppDirectory("save")) / "famicom.bin";
}

} // namespace

extern "C" int FamicomPort_ReadSave(void* data, size_t size) {
    std::ifstream in(SavePath(), std::ios::binary);
    if (!in) {
        return 0;
    }
    in.read(static_cast<char*>(data), (std::streamsize)size);
    return (int)in.gcount();
}

extern "C" int FamicomPort_WriteSave(const void* data, size_t size) {
    std::error_code ec;
    fs::path path = SavePath();
    fs::create_directories(path.parent_path(), ec);

    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return 0;
        }
        out.write(static_cast<const char*>(data), (std::streamsize)size);
        if (!out) {
            return 0;
        }
    }

    fs::rename(tmp, path, ec);
    if (ec) {
        SPDLOG_ERROR("Famicom: could not write {}: {}", path.string(), ec.message());
        fs::remove(tmp, ec);
        return 0;
    }
    return 1;
}
