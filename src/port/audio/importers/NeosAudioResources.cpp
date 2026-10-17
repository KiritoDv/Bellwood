#include "NeosAudioResources.h"

#include <bridge/resourcebridge.h>
#include <ship/resource/ResourceLoader.h>
#include <ship/resource/ResourceManager.h>

#include <cstring>

namespace NeosAudio {

namespace {

std::shared_ptr<Ship::BinaryReader> GetReader(const std::shared_ptr<Ship::File>& file) {
    return std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
}

template <typename T> T* LoadChild(uint64_t crc) {
    if (crc == 0) {
        return nullptr;
    }

    const char* path = ResourceGetNameByCrc(crc);
    if (path == nullptr) {
        return nullptr;
    }

    auto resource = ResourceGetResourceManager()->LoadResourceProcess(path);
    return resource != nullptr ? static_cast<T*>(resource->GetRawPointer()) : nullptr;
}

adpcmloop sNoLoop = {};

constexpr size_t kSampleGuard = 64;

} // namespace

AudioTableEntry* AudioTable::GetPointer() {
    return mEntries.data();
}

size_t AudioTable::GetPointerSize() {
    return mEntries.size() * sizeof(AudioTableEntry);
}

uint8_t* Sequence::GetPointer() {
    return mData.data();
}

size_t Sequence::GetPointerSize() {
    return mData.size();
}

voiceinfo* SoundFont::GetPointer() {
    return &mInfo;
}

size_t SoundFont::GetPointerSize() {
    return sizeof(mInfo);
}

voicetable* Instrument::GetPointer() {
    return &mInstrument;
}

size_t Instrument::GetPointerSize() {
    return sizeof(mInstrument);
}

perctable* Drum::GetPointer() {
    return &mDrum;
}

size_t Drum::GetPointerSize() {
    return sizeof(mDrum);
}

smzwavetable* Sample::GetPointer() {
    return &mSample;
}

size_t Sample::GetPointerSize() {
    return sizeof(mSample);
}

adpcmloop* AdpcmLoop::GetPointer() {
    return &mLoop;
}

size_t AdpcmLoop::GetPointerSize() {
    return sizeof(mLoop);
}

adpcmbook* AdpcmBook::GetPointer() {
    return reinterpret_cast<adpcmbook*>(mStorage.data());
}

size_t AdpcmBook::GetPointerSize() {
    return mStorage.size();
}

envdat* Envelope::GetPointer() {
    return mPoints.data();
}

size_t Envelope::GetPointerSize() {
    return mPoints.size() * sizeof(envdat);
}

std::shared_ptr<Ship::IResource> AudioTableFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                                   std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto table = std::make_shared<AudioTable>(initData);
    auto reader = GetReader(file);

    table->mMedium = reader->ReadInt16();
    table->mAddr = reader->ReadUInt32();
    uint32_t count = reader->ReadUInt32();

    table->mEntries.resize(count);
    for (auto& entry : table->mEntries) {
        entry.crc = reader->ReadUInt64();
        entry.size = reader->ReadUInt32();
        entry.medium = reader->ReadInt8();
        entry.cachePolicy = reader->ReadInt8();
        entry.shortData1 = reader->ReadInt16();
        entry.shortData2 = reader->ReadInt16();
        entry.shortData3 = reader->ReadInt16();
    }

    return table;
}

std::shared_ptr<Ship::IResource> SequenceFactoryV2::ReadResource(std::shared_ptr<Ship::File> file,
                                                                 std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto sequence = std::make_shared<Sequence>(initData);
    auto reader = GetReader(file);

    uint32_t size = reader->ReadUInt32();
    sequence->mData.resize(size);
    reader->Read(reinterpret_cast<char*>(sequence->mData.data()), (int32_t)size);

    return sequence;
}

std::shared_ptr<Ship::IResource> SoundFontFactoryV2::ReadResource(std::shared_ptr<Ship::File> file,
                                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto font = std::make_shared<SoundFont>(initData);
    auto reader = GetReader(file);

    uint8_t numInstruments = reader->ReadUByte();
    uint8_t numDrums = reader->ReadUByte();
    uint8_t bank1 = reader->ReadUByte();
    uint8_t bank2 = reader->ReadUByte();
    uint16_t numSfx = reader->ReadUInt16();

    font->mInstruments.resize(numInstruments);
    for (auto& instrument : font->mInstruments) {
        instrument = LoadChild<voicetable>(reader->ReadUInt64());
    }

    font->mDrums.resize(numDrums);
    for (auto& drum : font->mDrums) {
        drum = LoadChild<perctable>(reader->ReadUInt64());
    }

    font->mSfx.resize(numSfx);
    for (auto& sfx : font->mSfx) {
        sfx.tuned_sample.wavetable = LoadChild<smzwavetable>(reader->ReadUInt64());
        sfx.tuned_sample.tuning = reader->ReadFloat();
    }

    font->mInfo.num_instruments = numInstruments;
    font->mInfo.num_drums = numDrums;
    font->mInfo.wave_bank_id0 = bank1;
    font->mInfo.wave_bank_id1 = bank2;
    font->mInfo.num_sfx = numSfx;
    font->mInfo.instruments = font->mInstruments.empty() ? nullptr : font->mInstruments.data();
    font->mInfo.percussion = font->mDrums.empty() ? nullptr : font->mDrums.data();
    font->mInfo.effects = font->mSfx.empty() ? nullptr : font->mSfx.data();

    return font;
}

std::shared_ptr<Ship::IResource> InstrumentFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                                   std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto instrument = std::make_shared<Instrument>(initData);
    auto reader = GetReader(file);
    voicetable& vt = instrument->mInstrument;

    reader->ReadUByte();
    vt.normal_range_low = reader->ReadUByte();
    vt.normal_range_high = reader->ReadUByte();
    vt.adsr_decay_idx = reader->ReadUByte();
    vt.envelope = LoadChild<envdat>(reader->ReadUInt64());

    wtstr* samples[] = { &vt.low_pitch_tuned_sample, &vt.normal_pitch_tuned_sample, &vt.high_pitch_tuned_sample };
    for (wtstr* sample : samples) {
        sample->wavetable = LoadChild<smzwavetable>(reader->ReadUInt64());
        sample->tuning = reader->ReadFloat();
    }

    vt.is_relocated = TRUE;
    return instrument;
}

std::shared_ptr<Ship::IResource> DrumFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                             std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto drum = std::make_shared<Drum>(initData);
    auto reader = GetReader(file);
    perctable& pt = drum->mDrum;

    pt.adsr_decay_idx = reader->ReadUByte();
    pt.pan = reader->ReadUByte();
    reader->ReadUByte();
    pt.tuned_sample.wavetable = LoadChild<smzwavetable>(reader->ReadUInt64());
    pt.tuned_sample.tuning = reader->ReadFloat();
    pt.envelope = LoadChild<envdat>(reader->ReadUInt64());

    pt.is_relocated = TRUE;
    return drum;
}

std::shared_ptr<Ship::IResource> SampleFactoryV1::ReadResource(std::shared_ptr<Ship::File> file,
                                                               std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto sample = std::make_shared<Sample>(initData);
    auto reader = GetReader(file);
    smzwavetable& wave = sample->mSample;

    uint8_t codec = reader->ReadUByte();
    reader->ReadUByte();
    uint8_t bit26 = reader->ReadUByte();
    uint32_t size = reader->ReadUInt32();
    adpcmloop* loop = LoadChild<adpcmloop>(reader->ReadUInt64());
    adpcmbook* book = LoadChild<adpcmbook>(reader->ReadUInt64());

    sample->mStorage.assign(size + 2 * kSampleGuard, 0);
    uint8_t* data = sample->mStorage.data() + kSampleGuard;
    reader->Read(reinterpret_cast<char*>(data), (int32_t)size);

    wave.bit31 = 0;
    wave.codec = codec;
    wave.medium = MEDIUM_RAM;
    wave.bit26 = bit26;
    wave.is_relocated = TRUE;
    wave.size = size;
    wave.sample = data;
    wave.loop = loop != nullptr ? loop : &sNoLoop;
    wave.book = book;

    return sample;
}

std::shared_ptr<Ship::IResource> AdpcmLoopFactoryV1::ReadResource(std::shared_ptr<Ship::File> file,
                                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto loop = std::make_shared<AdpcmLoop>(initData);
    auto reader = GetReader(file);
    adpcmloop& data = loop->mLoop;

    data.loop_start = reader->ReadUInt32();
    data.loop_end = reader->ReadUInt32();
    data.count = reader->ReadUInt32();
    data.sample_end = reader->ReadUInt32();
    if (data.count != 0) {
        for (s16& state : data.predictor_state) {
            state = reader->ReadInt16();
        }
    }

    return loop;
}

std::shared_ptr<Ship::IResource> AdpcmBookFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                                  std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto book = std::make_shared<AdpcmBook>(initData);
    auto reader = GetReader(file);

    int32_t order = reader->ReadInt32();
    int32_t numPredictors = reader->ReadInt32();
    uint32_t count = reader->ReadUInt32();

    book->mStorage.assign(sizeof(adpcmbook) + count * sizeof(s16), 0);
    adpcmbook* data = book->GetPointer();
    data->order = order;
    data->n_predictors = numPredictors;
    for (uint32_t i = 0; i < count; i++) {
        data->codebook[i] = reader->ReadInt16();
    }

    return book;
}

std::shared_ptr<Ship::IResource> EnvelopeFactoryV0::ReadResource(std::shared_ptr<Ship::File> file,
                                                                 std::shared_ptr<Ship::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto envelope = std::make_shared<Envelope>(initData);
    auto reader = GetReader(file);

    uint32_t count = reader->ReadUInt32();
    envelope->mPoints.resize(count);
    for (auto& point : envelope->mPoints) {
        point.delay = reader->ReadInt16();
        point.value = reader->ReadInt16();
    }

    return envelope;
}

void RegisterImporters() {
    auto loader = ResourceGetResourceManager()->GetResourceLoader();

    loader->RegisterResourceFactory(std::make_shared<AudioTableFactoryV0>(), RESOURCE_FORMAT_BINARY, "AudioTable",
                                    static_cast<uint32_t>(ResourceType::AudioTable), 0);
    loader->RegisterResourceFactory(std::make_shared<SequenceFactoryV2>(), RESOURCE_FORMAT_BINARY, "Sequence",
                                    static_cast<uint32_t>(ResourceType::Sequence), 2);
    loader->RegisterResourceFactory(std::make_shared<SoundFontFactoryV2>(), RESOURCE_FORMAT_BINARY, "SoundFont",
                                    static_cast<uint32_t>(ResourceType::SoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<InstrumentFactoryV0>(), RESOURCE_FORMAT_BINARY, "Instrument",
                                    static_cast<uint32_t>(ResourceType::Instrument), 0);
    loader->RegisterResourceFactory(std::make_shared<DrumFactoryV0>(), RESOURCE_FORMAT_BINARY, "Drum",
                                    static_cast<uint32_t>(ResourceType::Drum), 0);
    loader->RegisterResourceFactory(std::make_shared<SampleFactoryV1>(), RESOURCE_FORMAT_BINARY, "Sample",
                                    static_cast<uint32_t>(ResourceType::Sample), 1);
    loader->RegisterResourceFactory(std::make_shared<AdpcmLoopFactoryV1>(), RESOURCE_FORMAT_BINARY, "AdpcmLoop",
                                    static_cast<uint32_t>(ResourceType::AdpcmLoop), 1);
    loader->RegisterResourceFactory(std::make_shared<AdpcmBookFactoryV0>(), RESOURCE_FORMAT_BINARY, "AdpcmBook",
                                    static_cast<uint32_t>(ResourceType::AdpcmBook), 0);
    loader->RegisterResourceFactory(std::make_shared<EnvelopeFactoryV0>(), RESOURCE_FORMAT_BINARY, "Envelope",
                                    static_cast<uint32_t>(ResourceType::Envelope), 0);
}

} // namespace NeosAudio
