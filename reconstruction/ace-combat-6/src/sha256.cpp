#include "ac6/sha256.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace ac6 {
namespace {

class Sha256 final {
 public:
  void update(const std::uint8_t* data, std::size_t size) noexcept {
    bit_length_ += static_cast<std::uint64_t>(size) * 8u;
    while (size != 0) {
      const std::size_t count = std::min(size, block_.size() - block_size_);
      std::memcpy(block_.data() + block_size_, data, count);
      block_size_ += count;
      data += count;
      size -= count;
      if (block_size_ == block_.size()) {
        transform(block_.data());
        block_size_ = 0;
      }
    }
  }

  Sha256Digest digest() const noexcept {
    Sha256 copy = *this;
    copy.finish();
    Sha256Digest result{};
    for (std::size_t index = 0; index < copy.state_.size(); ++index) {
      const std::uint32_t word = copy.state_[index];
      result[index * 4u] = static_cast<std::uint8_t>(word >> 24u);
      result[index * 4u + 1u] = static_cast<std::uint8_t>(word >> 16u);
      result[index * 4u + 2u] = static_cast<std::uint8_t>(word >> 8u);
      result[index * 4u + 3u] = static_cast<std::uint8_t>(word);
    }
    return result;
  }

 private:
  static constexpr std::array<std::uint32_t, 64> kRoundConstants{
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
      0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
      0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
      0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
      0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
      0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
      0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
      0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
      0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
      0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
      0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  static std::uint32_t rotate_right(std::uint32_t value, unsigned count) noexcept {
    return (value >> count) | (value << (32u - count));
  }

  void transform(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
      const std::size_t offset = index * 4u;
      words[index] = (static_cast<std::uint32_t>(block[offset]) << 24u) |
                     (static_cast<std::uint32_t>(block[offset + 1u]) << 16u) |
                     (static_cast<std::uint32_t>(block[offset + 2u]) << 8u) |
                     static_cast<std::uint32_t>(block[offset + 3u]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
      const std::uint32_t s0 = rotate_right(words[index - 15u], 7u) ^
                               rotate_right(words[index - 15u], 18u) ^
                               (words[index - 15u] >> 3u);
      const std::uint32_t s1 = rotate_right(words[index - 2u], 17u) ^
                               rotate_right(words[index - 2u], 19u) ^
                               (words[index - 2u] >> 10u);
      words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
      const std::uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^
                               rotate_right(e, 25u);
      const std::uint32_t choose = (e & f) ^ ((~e) & g);
      const std::uint32_t temporary1 =
          h + s1 + choose + kRoundConstants[index] + words[index];
      const std::uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^
                               rotate_right(a, 22u);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temporary2 = s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
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

  void finish() noexcept {
    block_[block_size_++] = 0x80u;
    if (block_size_ > 56u) {
      std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                block_.end(), 0u);
      transform(block_.data());
      block_size_ = 0;
    }
    std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
              block_.begin() + 56, 0u);
    for (std::size_t index = 0; index < 8; ++index) {
      block_[56u + index] =
          static_cast<std::uint8_t>(bit_length_ >> (56u - index * 8u));
    }
    transform(block_.data());
  }

  std::array<std::uint32_t, 8> state_{
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::array<std::uint8_t, 64> block_{};
  std::size_t block_size_{};
  std::uint64_t bit_length_{};
};

int hex_value(char value) noexcept {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

}  // namespace

Sha256Digest sha256_bytes(std::span<const std::uint8_t> bytes) noexcept {
  Sha256 sha;
  sha.update(bytes.data(), bytes.size());
  return sha.digest();
}

bool sha256_file(const std::filesystem::path& path, Sha256Digest& digest,
                 std::uint64_t maximum_size) noexcept {
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error || size > maximum_size) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  Sha256 sha;
  std::array<std::uint8_t, 1024 * 1024> bytes{};
  while (input) {
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) sha.update(bytes.data(), static_cast<std::size_t>(count));
  }
  if (!input.eof()) return false;
  digest = sha.digest();
  return true;
}

std::string sha256_hex(const Sha256Digest& digest) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const std::uint8_t byte : digest) {
    result.push_back(kHex[byte >> 4u]);
    result.push_back(kHex[byte & 0x0fu]);
  }
  return result;
}

bool parse_sha256(std::string_view text, Sha256Digest& digest) noexcept {
  if (text.size() != 64) return false;
  Sha256Digest parsed{};
  for (std::size_t index = 0; index < parsed.size(); ++index) {
    const int high = hex_value(text[index * 2u]);
    const int low = hex_value(text[index * 2u + 1u]);
    if (high < 0 || low < 0) return false;
    parsed[index] = static_cast<std::uint8_t>((high << 4) | low);
  }
  digest = parsed;
  return true;
}

bool equal_sha256(std::string_view text, const Sha256Digest& digest) noexcept {
  Sha256Digest parsed{};
  return parse_sha256(text, parsed) && parsed == digest;
}

}  // namespace ac6
