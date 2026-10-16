#include "port/gba/ECard.h"

#include <cstring>
#include <memory>

namespace ECard {
namespace {

constexpr size_t kBlockSize = 104;
constexpr size_t kBlockHeaderBytes = 2;
constexpr size_t kLongStripBlocks = 28;
constexpr size_t kShortStripBlocks = 18;
constexpr size_t kFragmentData = 48;
constexpr size_t kFragmentSize = 64;
constexpr size_t kHeaderBlocks = 12;
constexpr size_t kMaxOutput = 1 << 20;

class BitReader {
  public:
    BitReader(const uint8_t* data, size_t size) : mData(data), mSize(size) {
    }

    bool Read(int count, uint32_t* value) {
        uint32_t v = 0;
        for (int i = 0; i < count; i++) {
            if (mBits == 0) {
                if (mPos >= mSize) {
                    return false;
                }
                mWord = 0;
                for (int b = 0; b < 4; b++) {
                    mWord = (mWord << 8) | (mPos + b < mSize ? mData[mPos + b] : 0);
                }
                mPos += 4;
                mBits = 32;
            }
            mBits--;
            v = (v << 1) | ((mWord >> mBits) & 1);
        }
        *value = v;
        return true;
    }

  private:
    const uint8_t* mData;
    size_t mSize;
    size_t mPos = 0;
    uint32_t mWord = 0;
    int mBits = 0;
};

struct Node {
    int left;
    int right;
    uint32_t value;
};

bool LoadTree(BitReader& bits, std::vector<Node>& nodes, int* root) {
    std::vector<int> stack;
    for (;;) {
        uint32_t bit;
        if (!bits.Read(1, &bit)) {
            return false;
        }
        if (bit != 0) {
            if (stack.size() < 2) {
                break;
            }
            int right = stack.back();
            stack.pop_back();
            int left = stack.back();
            stack.pop_back();
            nodes.push_back({ left, right, 0 });
            stack.push_back((int)nodes.size() - 1);
        } else {
            uint32_t width;
            if (!bits.Read(8, &width)) {
                return false;
            }
            nodes.push_back({ -1, -1, width });
            stack.push_back((int)nodes.size() - 1);
        }
        if (nodes.size() > 4096) {
            return false;
        }
    }
    *root = stack.empty() ? -1 : stack.back();
    return true;
}

bool ReadTree(BitReader& bits, const std::vector<Node>& nodes, int root, uint32_t* value) {
    int i = root;
    while (nodes[i].right != -1) {
        uint32_t bit;
        if (!bits.Read(1, &bit)) {
            return false;
        }
        i = bit != 0 ? nodes[i].right : nodes[i].left;
    }
    if (nodes[i].value > 32) {
        return false;
    }
    return bits.Read((int)nodes[i].value, value);
}

bool LooksLikeRaw(const std::vector<uint8_t>& raw, size_t blocks) {
    size_t strip = blocks * kBlockSize;
    if (raw.empty() || raw.size() % strip != 0) {
        return false;
    }
    uint8_t type = blocks == kLongStripBlocks ? 0x03 : 0x02;
    for (size_t s = 0; s < raw.size(); s += strip) {
        if (raw[s + 1] != type) {
            return false;
        }
    }
    return true;
}

} // namespace

bool RawToBin(const std::vector<uint8_t>& raw, std::vector<uint8_t>& bin) {
    size_t blocks;
    if (LooksLikeRaw(raw, kLongStripBlocks)) {
        blocks = kLongStripBlocks;
    } else if (LooksLikeRaw(raw, kShortStripBlocks)) {
        blocks = kShortStripBlocks;
    } else {
        return false;
    }
    size_t strip = blocks * kBlockSize;
    bin.clear();
    for (size_t s = 0; s < raw.size(); s += strip) {
        const uint8_t* p = raw.data() + s;
        uint8_t header[kHeaderBlocks * kBlockHeaderBytes];
        for (size_t b = 0; b < kHeaderBlocks; b++) {
            header[b * 2] = p[b * kBlockSize];
            header[b * 2 + 1] = p[b * kBlockSize + 1];
        }
        size_t interleave = header[7];
        std::vector<uint8_t> stream;
        for (size_t b = 0; b < blocks; b++) {
            stream.insert(stream.end(), p + b * kBlockSize + kBlockHeaderBytes, p + (b + 1) * kBlockSize);
        }
        if (interleave == 0 || interleave * kFragmentSize > stream.size()) {
            return false;
        }
        for (size_t f = 0; f < interleave; f++) {
            for (size_t j = 0; j < kFragmentData; j++) {
                bin.push_back(stream[j * interleave + f]);
            }
        }
    }
    return true;
}

bool Vpk0Decompress(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    BitReader bits(data, size);
    uint32_t magic;
    uint32_t outSize;
    uint32_t method;
    if (!bits.Read(32, &magic) || magic != 0x76706B30 || !bits.Read(32, &outSize) || !bits.Read(8, &method) ||
        method > 1 || outSize == 0 || outSize > kMaxOutput) {
        return false;
    }
    std::vector<Node> nodes;
    int dispRoot;
    int lenRoot;
    if (!LoadTree(bits, nodes, &dispRoot) || !LoadTree(bits, nodes, &lenRoot)) {
        return false;
    }
    out.clear();
    out.reserve(outSize);
    while (out.size() < outSize) {
        uint32_t flag;
        if (!bits.Read(1, &flag)) {
            return false;
        }
        if (flag == 0) {
            uint32_t byte;
            if (!bits.Read(8, &byte)) {
                return false;
            }
            out.push_back((uint8_t)byte);
            continue;
        }
        if (dispRoot < 0 || lenRoot < 0) {
            return false;
        }
        uint32_t d;
        if (!ReadTree(bits, nodes, dispRoot, &d)) {
            return false;
        }
        int64_t disp = d;
        if (method == 1) {
            if (disp > 2) {
                disp = disp * 4 - 8;
            } else {
                uint32_t extra;
                if (!ReadTree(bits, nodes, dispRoot, &extra)) {
                    return false;
                }
                disp = disp + 4 * (int64_t)extra - 7;
            }
        }
        uint32_t len;
        if (!ReadTree(bits, nodes, lenRoot, &len)) {
            return false;
        }
        if (len == 0 || disp <= 0 || (size_t)disp > out.size() || out.size() + len > outSize) {
            return false;
        }
        for (uint32_t i = 0; i < len; i++) {
            out.push_back(out[out.size() - (size_t)disp]);
        }
    }
    return true;
}

bool ReadPayloads(const std::vector<uint8_t>& file, std::vector<std::vector<uint8_t>>& payloads, std::string* error) {
    payloads.clear();
    if (file.empty()) {
        *error = "The file is empty.";
        return false;
    }
    std::vector<uint8_t> bin;
    const std::vector<uint8_t>* data = &file;
    if (RawToBin(file, bin)) {
        data = &bin;
    }
    const uint8_t* p = data->data();
    for (size_t i = 0; i + 4 <= data->size(); i++) {
        if (memcmp(p + i, "vpk0", 4) != 0) {
            continue;
        }
        std::vector<uint8_t> out;
        if (Vpk0Decompress(p + i, data->size() - i, out)) {
            payloads.push_back(std::move(out));
        }
    }
    if (payloads.empty()) {
        payloads.push_back(file);
    }
    return true;
}

} // namespace ECard
