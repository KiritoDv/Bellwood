#include "NpcAnimationFactory.h"
#include "types/NpcAnimation.h"

#include <ship/utils/binarytools/BinaryReader.h>

namespace AC {

namespace {

template <typename T> T* DataOrNull(std::vector<T>& v) {
    return v.empty() ? nullptr : v.data();
}

void ReadBytes(Ship::BinaryReader& reader, std::vector<uint8_t>& out) {
    const uint32_t count = reader.ReadUInt32();
    out.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        out[i] = reader.ReadUByte();
    }
}

bool ReadSeData(Ship::BinaryReader& reader, NpcSeData& se, std::vector<int32_t>& frames) {
    if (reader.ReadUByte() == 0) {
        return false;
    }
    se.num_check_frames = reader.ReadInt32();
    const uint32_t count = reader.ReadUInt32();
    frames.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        frames[i] = reader.ReadInt32();
    }
    se.check_frame_tbl = DataOrNull(frames);
    return true;
}

} // namespace

std::shared_ptr<Ship::IResource>
ResourceFactoryBinaryNpcAnimationV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto npc = std::make_shared<NpcAnimation>(initData);
    auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
    auto& d = npc->mData;

    reader->ReadUByte();
    ReadBytes(*reader, npc->mFlags);

    const uint32_t keyFrames = reader->ReadUInt32();
    npc->mKeyFrames.resize(keyFrames);
    for (uint32_t i = 0; i < keyFrames; i++) {
        npc->mKeyFrames[i].frame = reader->ReadInt16();
        npc->mKeyFrames[i].value = reader->ReadInt16();
        npc->mKeyFrames[i].velocity = reader->ReadInt16();
    }
    const uint32_t keyCounts = reader->ReadUInt32();
    npc->mKeyCounts.resize(keyCounts);
    for (uint32_t i = 0; i < keyCounts; i++) {
        npc->mKeyCounts[i] = reader->ReadInt16();
    }
    const uint32_t fixed = reader->ReadUInt32();
    npc->mFixedValues.resize(fixed);
    for (uint32_t i = 0; i < fixed; i++) {
        npc->mFixedValues[i] = reader->ReadInt16();
    }
    d.anim.bitFlags = DataOrNull(npc->mFlags);
    d.anim.keyFrames = DataOrNull(npc->mKeyFrames);
    d.anim.kfNums = DataOrNull(npc->mKeyCounts);
    d.anim.presetValues = DataOrNull(npc->mFixedValues);
    d.anim.unk_10[0] = reader->ReadUByte();
    d.anim.unk_10[1] = reader->ReadUByte();
    d.anim.duration = reader->ReadInt16();

    d.start_frame = reader->ReadFloat();
    d.end_frame = reader->ReadFloat();
    d.mode = reader->ReadInt32();
    d.morph_counter = reader->ReadFloat();
    ReadBytes(*reader, npc->mEyeSeq);
    d.eye_seq_p = DataOrNull(npc->mEyeSeq);
    d.eye_seq_type = reader->ReadInt16();
    d.eye_seq_stop_frame = reader->ReadInt16();
    ReadBytes(*reader, npc->mMouthSeq);
    d.mouth_seq_p = DataOrNull(npc->mMouthSeq);
    d.mouth_seq_type = reader->ReadInt16();
    d.mouth_seq_stop_frame = reader->ReadInt16();
    d.feel_effect_set_frame = reader->ReadInt16();
    d.feel_effect_type = reader->ReadInt16();

    if (reader->ReadUByte() != 0) {
        npc->mFeel.feel_type = reader->ReadInt16();
        npc->mFeel.max = reader->ReadUByte();
        npc->mFeel.set_num = reader->ReadUByte();
        const uint32_t count = reader->ReadUInt32();
        npc->mFeelSet.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            npc->mFeelSet[i] = reader->ReadUInt32();
        }
        npc->mFeel.set_p = DataOrNull(npc->mFeelSet);
        d.feel_effect = &npc->mFeel;
    }

    if (reader->ReadUByte() != 0) {
        auto& table = npc->mSeTable;
        if (ReadSeData(*reader, npc->mLfoot, npc->mLfootFrames)) {
            table.lfoot_data = &npc->mLfoot;
        }
        if (ReadSeData(*reader, npc->mRfoot, npc->mRfootFrames)) {
            table.rfoot_data = &npc->mRfoot;
        }
        table.other_se_type = reader->ReadInt32();
        const uint32_t others = reader->ReadUInt32();
        npc->mOtherSe.resize(others);
        npc->mOtherSeFrames.resize(others);
        for (uint32_t i = 0; i < others; i++) {
            ReadSeData(*reader, npc->mOtherSe[i].se_data, npc->mOtherSeFrames[i]);
            npc->mOtherSe[i].se_no = reader->ReadUInt16();
        }
        table.other_se_data = DataOrNull(npc->mOtherSe);
        d.se_data_table = &table;
    }

    return npc;
}

} // namespace AC
