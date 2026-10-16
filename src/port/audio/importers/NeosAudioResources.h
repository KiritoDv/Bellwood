#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <ship/resource/Resource.h>
#include <ship/resource/ResourceFactoryBinary.h>

extern "C" {
#include "jaudio_NES/audiostruct.h"
}

namespace NeosAudio {

enum class ResourceType : uint32_t {
    AudioTable = 0x4154424C, // ATBL
    Sequence = 0x53455143,   // SEQC
    SoundFont = 0x53464E54,  // SFNT
    Instrument = 0x494E5354, // INST
    Drum = 0x4452554D,       // DRUM
    Sample = 0x41554643,     // AIFC
    AdpcmLoop = 0x4150434C,  // APCL
    AdpcmBook = 0x41504342,  // APCB
    Envelope = 0x45564C50,   // EVLP
};

struct AudioTableEntry {
    uint64_t crc;
    uint32_t size;
    int8_t medium;
    int8_t cachePolicy;
    int16_t shortData1;
    int16_t shortData2;
    int16_t shortData3;
};

class AudioTable : public Ship::Resource<AudioTableEntry> {
  public:
    using Resource::Resource;
    AudioTableEntry* GetPointer() override;
    size_t GetPointerSize() override;

    int16_t mMedium = 0;
    uint32_t mAddr = 0;
    std::vector<AudioTableEntry> mEntries;
};

class Sequence : public Ship::Resource<uint8_t> {
  public:
    using Resource::Resource;
    uint8_t* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<uint8_t> mData;
};

class SoundFont : public Ship::Resource<voiceinfo> {
  public:
    using Resource::Resource;
    voiceinfo* GetPointer() override;
    size_t GetPointerSize() override;

    voiceinfo mInfo = {};
    std::vector<voicetable*> mInstruments;
    std::vector<perctable*> mDrums;
    std::vector<percvoicetable> mSfx;
};

class Instrument : public Ship::Resource<voicetable> {
  public:
    using Resource::Resource;
    voicetable* GetPointer() override;
    size_t GetPointerSize() override;

    voicetable mInstrument = {};
};

class Drum : public Ship::Resource<perctable> {
  public:
    using Resource::Resource;
    perctable* GetPointer() override;
    size_t GetPointerSize() override;

    perctable mDrum = {};
};

class Sample : public Ship::Resource<smzwavetable> {
  public:
    using Resource::Resource;
    smzwavetable* GetPointer() override;
    size_t GetPointerSize() override;

    smzwavetable mSample = {};
    std::vector<uint8_t> mStorage;
};

class AdpcmLoop : public Ship::Resource<adpcmloop> {
  public:
    using Resource::Resource;
    adpcmloop* GetPointer() override;
    size_t GetPointerSize() override;

    adpcmloop mLoop = {};
};

class AdpcmBook : public Ship::Resource<adpcmbook> {
  public:
    using Resource::Resource;
    adpcmbook* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<uint8_t> mStorage;
};

class Envelope : public Ship::Resource<envdat> {
  public:
    using Resource::Resource;
    envdat* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<envdat> mPoints;
};

class AudioTableFactoryV0 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class SequenceFactoryV2 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class SoundFontFactoryV2 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class InstrumentFactoryV0 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class DrumFactoryV0 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class SampleFactoryV1 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class AdpcmLoopFactoryV1 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class AdpcmBookFactoryV0 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

class EnvelopeFactoryV0 : public Ship::ResourceFactoryBinary {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

void RegisterImporters();

} // namespace NeosAudio
