#include "ac6demo_native/sha256.hpp"

#include "posix_fd.hpp"

#include <array>
#include <iomanip>
#include <sstream>

namespace ac6demo_native {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::uint32_t rotate_right(std::uint32_t value,
                                     std::uint32_t amount) noexcept {
    return (value >> amount) | (value << (32U - amount));
}

constexpr std::uint32_t choose(std::uint32_t x, std::uint32_t y,
                               std::uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

constexpr std::uint32_t majority(std::uint32_t x, std::uint32_t y,
                                 std::uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr std::uint32_t big_sigma0(std::uint32_t x) noexcept {
    return rotate_right(x, 2U) ^ rotate_right(x, 13U) ^ rotate_right(x, 22U);
}

constexpr std::uint32_t big_sigma1(std::uint32_t x) noexcept {
    return rotate_right(x, 6U) ^ rotate_right(x, 11U) ^ rotate_right(x, 25U);
}

constexpr std::uint32_t small_sigma0(std::uint32_t x) noexcept {
    return rotate_right(x, 7U) ^ rotate_right(x, 18U) ^ (x >> 3U);
}

constexpr std::uint32_t small_sigma1(std::uint32_t x) noexcept {
    return rotate_right(x, 17U) ^ rotate_right(x, 19U) ^ (x >> 10U);
}

}  // namespace

Sha256::Sha256() noexcept
    : state_{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
             0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U},
      bit_count_(0),
      buffer_{},
      buffered_(0),
      finalized_(false) {}

void Sha256::transform(const std::uint8_t* block) noexcept {
    std::uint32_t schedule[64]{};
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t offset = i * 4U;
        schedule[i] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                      (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                      (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                      static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        schedule[i] = small_sigma1(schedule[i - 2U]) + schedule[i - 7U] +
                      small_sigma0(schedule[i - 15U]) + schedule[i - 16U];
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t t1 = h + big_sigma1(e) + choose(e, f, g) +
                                 kRoundConstants[i] + schedule[i];
        const std::uint32_t t2 = big_sigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(std::span<const std::byte> bytes) noexcept {
    update(bytes.data(), bytes.size());
}

void Sha256::update(const void* data, std::size_t size) noexcept {
    if (finalized_ || (size != 0U && data == nullptr)) {
        return;
    }
    const auto* input = static_cast<const std::uint8_t*>(data);
    bit_count_ += static_cast<std::uint64_t>(size) * 8U;
    while (size != 0U) {
        const std::size_t available = 64U - buffered_;
        const std::size_t take = size < available ? size : available;
        for (std::size_t i = 0; i < take; ++i) {
            buffer_[buffered_ + i] = input[i];
        }
        buffered_ += take;
        input += take;
        size -= take;
        if (buffered_ == 64U) {
            transform(buffer_);
            buffered_ = 0;
        }
    }
}

std::string Sha256::final_hex() {
    if (!finalized_) {
        const std::uint64_t original_bits = bit_count_;
        buffer_[buffered_++] = 0x80U;
        if (buffered_ > 56U) {
            while (buffered_ < 64U) {
                buffer_[buffered_++] = 0U;
            }
            transform(buffer_);
            buffered_ = 0;
        }
        while (buffered_ < 56U) {
            buffer_[buffered_++] = 0U;
        }
        for (std::size_t i = 0; i < 8U; ++i) {
            buffer_[63U - i] = static_cast<std::uint8_t>(original_bits >> (i * 8U));
        }
        transform(buffer_);
        buffered_ = 0;
        finalized_ = true;
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint32_t word : state_) {
        output << std::setw(8) << word;
    }
    return output.str();
}

std::string sha256_bytes(std::span<const std::byte> bytes) {
    Sha256 hasher;
    hasher.update(bytes);
    return hasher.final_hex();
}

std::string sha256_file(const std::filesystem::path& path, std::string* error) {
    detail::UniqueFd input(detail::open_regular_path(path, error));
    if (!input) {
        return {};
    }
    std::uint64_t size = 0;
    bool regular = false;
    if (!detail::stat_fd(input.get(), &size, &regular, error) || !regular) {
        return {};
    }
    return detail::sha256_fd(input.get(), size, error);
}

}  // namespace ac6demo_native
