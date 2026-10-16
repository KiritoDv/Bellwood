#pragma once

#include "port/importer/types/KeyFrame.h"

#include <ship/resource/Resource.h>

#include <cstdint>
#include <vector>

namespace AC {

typedef struct {
    int32_t num_check_frames;
    int32_t* check_frame_tbl;
} NpcSeData;

typedef struct {
    NpcSeData se_data;
    uint16_t se_no;
} NpcOtherSeData;

typedef struct {
    NpcSeData* lfoot_data;
    NpcSeData* rfoot_data;
    int32_t other_se_type;
    NpcOtherSeData* other_se_data;
} NpcSeDataTable;

typedef struct {
    int16_t feel_type;
    uint8_t max;
    uint8_t set_num;
    uint32_t* set_p;
} NpcFeelEffect;

typedef struct {
    KeyFrameAnimationData anim;
    float start_frame;
    float end_frame;
    int32_t mode;
    float morph_counter;
    uint8_t* eye_seq_p;
    int16_t eye_seq_type;
    int16_t eye_seq_stop_frame;
    uint8_t* mouth_seq_p;
    int16_t mouth_seq_type;
    int16_t mouth_seq_stop_frame;
    int16_t feel_effect_set_frame;
    int16_t feel_effect_type;
    NpcFeelEffect* feel_effect;
    NpcSeDataTable* se_data_table;
} NpcAnimationData;

class NpcAnimation : public Ship::Resource<NpcAnimationData> {
  public:
    using Resource::Resource;

    NpcAnimation() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    NpcAnimationData* GetPointer() override;
    size_t GetPointerSize() override;

    NpcAnimationData mData{};

    std::vector<uint8_t> mFlags;
    std::vector<KeyFrame> mKeyFrames;
    std::vector<s16> mKeyCounts;
    std::vector<s16> mFixedValues;
    std::vector<uint8_t> mEyeSeq;
    std::vector<uint8_t> mMouthSeq;
    NpcFeelEffect mFeel{};
    std::vector<uint32_t> mFeelSet;
    NpcSeDataTable mSeTable{};
    NpcSeData mLfoot{};
    NpcSeData mRfoot{};
    std::vector<int32_t> mLfootFrames;
    std::vector<int32_t> mRfootFrames;
    std::vector<NpcOtherSeData> mOtherSe;
    std::vector<std::vector<int32_t>> mOtherSeFrames;
};

} // namespace AC
