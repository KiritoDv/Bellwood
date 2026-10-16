#include "EvwAnimeFactory.h"
#include "types/EvwAnime.h"

#include <ship/utils/binarytools/BinaryReader.h>
#include "spdlog/spdlog.h"

namespace AC {

namespace {

void ReadKeyFrames(Ship::BinaryReader& reader, uint32_t count, EvwAnimeStorage& s) {
    s.keyframes.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        s.keyframes[i] = reader.ReadUInt16();
    }
}

} // namespace

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryEvwAnimeV0::ReadResource(std::shared_ptr<Ship::File> file,
                                              std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto evw = std::make_shared<EvwAnime>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);

    const uint32_t count = reader->ReadUInt32();
    evw->mEntries.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        auto storage = std::make_unique<EvwAnimeStorage>();
        auto& s = *storage;
        auto& e = evw->mEntries[i];
        e.segment = reader->ReadInt8();
        e.type = reader->ReadInt16();
        e.data_p = nullptr;

        switch (e.type) {
            case EVW_ANIME_SCROLL1:
            case EVW_ANIME_SCROLL2: {
                const int rows = e.type == EVW_ANIME_SCROLL1 ? 1 : 2;
                for (int r = 0; r < rows; r++) {
                    s.scroll[r].x = reader->ReadInt8();
                    s.scroll[r].y = reader->ReadInt8();
                    s.scroll[r].width = reader->ReadUByte();
                    s.scroll[r].height = reader->ReadUByte();
                }
                e.data_p = s.scroll;
                break;
            }
            case EVW_ANIME_COLREG_MANUAL:
            case EVW_ANIME_COLREG_LINEAR:
            case EVW_ANIME_COLREG_NONLINEAR: {
                s.colreg.frame_count = reader->ReadUInt16();
                s.colreg.key_count = reader->ReadUInt16();
                ReadKeyFrames(*reader, reader->ReadUInt16(), s);
                s.prim.resize(reader->ReadUInt16());
                for (auto& p : s.prim) {
                    p.r = reader->ReadUByte();
                    p.g = reader->ReadUByte();
                    p.b = reader->ReadUByte();
                    p.a = reader->ReadUByte();
                    p.l = reader->ReadUByte();
                }
                s.env.resize(reader->ReadUInt16());
                for (auto& c : s.env) {
                    c.r = reader->ReadUByte();
                    c.g = reader->ReadUByte();
                    c.b = reader->ReadUByte();
                    c.a = reader->ReadUByte();
                }
                s.colreg.prim_colors = s.prim.empty() ? nullptr : s.prim.data();
                s.colreg.env_colors = s.env.empty() ? nullptr : s.env.data();
                s.colreg.keyframes = s.keyframes.empty() ? nullptr : s.keyframes.data();
                e.data_p = &s.colreg;
                break;
            }
            case EVW_ANIME_TEXANIME: {
                s.texanime.frame_count = reader->ReadUInt16();
                s.texanime.key_count = reader->ReadUInt16();
                const uint32_t textures = reader->ReadUInt32();
                s.textures.reserve(textures);
                for (uint32_t t = 0; t < textures; t++) {
                    s.textures.push_back("__OTR__" + reader->ReadString());
                }
                for (auto& t : s.textures) {
                    s.textureTable.push_back((void*)t.c_str());
                }
                s.pattern.resize(reader->ReadUInt32());
                for (auto& p : s.pattern) {
                    p = reader->ReadUByte();
                }
                ReadKeyFrames(*reader, reader->ReadUInt32(), s);
                s.texanime.texture_tbl = s.textureTable.empty() ? nullptr : s.textureTable.data();
                s.texanime.animation_pattern = s.pattern.empty() ? nullptr : s.pattern.data();
                s.texanime.keyframes = s.keyframes.empty() ? nullptr : s.keyframes.data();
                e.data_p = &s.texanime;
                break;
            }
            default:
                SPDLOG_ERROR("EvwAnime {}: entry {} has unknown type {}", initData->Path, i, e.type);
                return nullptr;
        }
        evw->mStorage.push_back(std::move(storage));
    }

    return evw;
}

} // namespace AC
