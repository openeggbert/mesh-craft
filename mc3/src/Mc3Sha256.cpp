#include <MeshCraft/Mc3/Mc3Sha256.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>

namespace MeshCraft::Mc3 {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

class Sha256 {
public:
    Sha256() {
        state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    }

    void update(const unsigned char* data, std::size_t len) {
        total_len_ += len;
        while (len > 0) {
            const std::size_t take = std::min(len, std::size_t{64} - buffer_len_);
            std::memcpy(buffer_.data() + buffer_len_, data, take);
            buffer_len_ += take;
            data += take;
            len -= take;
            if (buffer_len_ == 64) {
                process(buffer_.data());
                buffer_len_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bit_len = total_len_ * 8;
        unsigned char pad = 0x80;
        update(&pad, 1);
        pad = 0x00;
        while (buffer_len_ != 56) update(&pad, 1);
        std::array<unsigned char, 8> len_be{};
        for (int i = 0; i < 8; ++i) len_be[7 - i] = static_cast<unsigned char>(bit_len >> (8 * i));
        update(len_be.data(), 8);

        std::array<std::uint8_t, 32> out{};
        for (int i = 0; i < 8; ++i) {
            out[static_cast<std::size_t>(i) * 4 + 0] = static_cast<std::uint8_t>(state_[static_cast<std::size_t>(i)] >> 24);
            out[static_cast<std::size_t>(i) * 4 + 1] = static_cast<std::uint8_t>(state_[static_cast<std::size_t>(i)] >> 16);
            out[static_cast<std::size_t>(i) * 4 + 2] = static_cast<std::uint8_t>(state_[static_cast<std::size_t>(i)] >> 8);
            out[static_cast<std::size_t>(i) * 4 + 3] = static_cast<std::uint8_t>(state_[static_cast<std::size_t>(i)]);
        }
        return out;
    }

private:
    void process(const unsigned char* block) {
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[static_cast<std::size_t>(i)] =
                (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24) |
                (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[static_cast<std::size_t>(i) - 15], 7) ^
                                      rotr(w[static_cast<std::size_t>(i) - 15], 18) ^
                                      (w[static_cast<std::size_t>(i) - 15] >> 3);
            const std::uint32_t s1 = rotr(w[static_cast<std::size_t>(i) - 2], 17) ^
                                      rotr(w[static_cast<std::size_t>(i) - 2], 19) ^
                                      (w[static_cast<std::size_t>(i) - 2] >> 10);
            w[static_cast<std::size_t>(i)] = w[static_cast<std::size_t>(i) - 16] + s0 +
                                              w[static_cast<std::size_t>(i) - 7] + s1;
        }

        auto [a, b, c, d, e, f, g, h] = std::tuple{state_[0], state_[1], state_[2], state_[3],
                                                    state_[4], state_[5], state_[6], state_[7]};
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + s1 + ch + kRoundConstants[static_cast<std::size_t>(i)] +
                                         w[static_cast<std::size_t>(i)];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    std::uint64_t total_len_ = 0;
};

} // namespace

std::string sha256Hex(const std::string& data) {
    Sha256 hasher;
    hasher.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    const auto digest = hasher.finish();

    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.resize(64);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        hex[i * 2]     = kHexDigits[digest[i] >> 4];
        hex[i * 2 + 1] = kHexDigits[digest[i] & 0x0F];
    }
    return hex;
}

} // namespace MeshCraft::Mc3
