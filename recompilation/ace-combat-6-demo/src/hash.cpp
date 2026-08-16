#include "ac6demo/hash.hpp"

#include "ac6demo/endian.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace ac6demo {

namespace {

constexpr std::array<std::uint32_t, 8> kInitial = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

}  // namespace

void Sha256::reset() noexcept {
  state_ = kInitial;
  buffer_.fill(std::byte{0});
  bit_count_ = 0;
  buffered_ = 0;
}

std::uint32_t Sha256::rotr(std::uint32_t value, unsigned amount) noexcept {
  return (value >> amount) | (value << (32U - amount));
}

void Sha256::transform(const std::byte* block) noexcept {
  std::array<std::uint32_t, 64> words{};
  for (std::size_t index = 0; index < 16U; ++index) {
    const std::span<const std::byte> input(block, 64U);
    words[index] = read_be32(input, index * 4U);
  }
  for (std::size_t index = 16U; index < words.size(); ++index) {
    const std::uint32_t s0 = rotr(words[index - 15U], 7U) ^
                             rotr(words[index - 15U], 18U) ^
                             (words[index - 15U] >> 3U);
    const std::uint32_t s1 = rotr(words[index - 2U], 17U) ^
                             rotr(words[index - 2U], 19U) ^
                             (words[index - 2U] >> 10U);
    words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
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
    const std::uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
    const std::uint32_t choose = (e & f) ^ ((~e) & g);
    const std::uint32_t temp1 = h + s1 + choose + k[index] + words[index];
    const std::uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
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
  bit_count_ += static_cast<std::uint64_t>(bytes.size()) * 8U;
  while (!bytes.empty()) {
    const std::size_t copy_size = std::min(buffer_.size() - buffered_, bytes.size());
    std::memcpy(buffer_.data() + buffered_, bytes.data(), copy_size);
    buffered_ += copy_size;
    bytes = bytes.subspan(copy_size);
    if (buffered_ == buffer_.size()) {
      transform(buffer_.data());
      buffered_ = 0;
    }
  }
}

std::array<std::byte, 32> Sha256::finish() noexcept {
  const std::uint64_t original_bits = bit_count_;
  const std::byte one = std::byte{0x80};
  update(std::span<const std::byte>(&one, 1U));
  const std::array<std::byte, 64> zeros{};
  while (buffered_ != 56U) {
    const std::size_t amount = buffered_ < 56U ? 56U - buffered_ : 64U - buffered_;
    update(std::span<const std::byte>(zeros.data(), amount));
  }
  std::array<std::byte, 8> length{};
  for (std::size_t index = 0; index < length.size(); ++index) {
    length[7U - index] = static_cast<std::byte>(original_bits >> (index * 8U));
  }
  update(length);

  std::array<std::byte, 32> result{};
  for (std::size_t index = 0; index < state_.size(); ++index) {
    result[index * 4U] = static_cast<std::byte>(state_[index] >> 24U);
    result[index * 4U + 1U] = static_cast<std::byte>(state_[index] >> 16U);
    result[index * 4U + 2U] = static_cast<std::byte>(state_[index] >> 8U);
    result[index * 4U + 3U] = static_cast<std::byte>(state_[index]);
  }
  return result;
}

std::string Sha256::bytes(std::span<const std::byte> payload) {
  Sha256 digest;
  digest.update(payload);
  const auto result = digest.finish();
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const std::byte value : result) {
    stream << std::setw(2) << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  return stream.str();
}

std::string Sha256::file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("unable to read for SHA-256: " + path.string());
  }
  Sha256 digest;
  std::array<std::byte, 1024U * 1024U> buffer{};
  while (input) {
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) {
      digest.update(std::span<const std::byte>(buffer.data(), static_cast<std::size_t>(count)));
    }
  }
  if (!input.eof()) {
    throw std::runtime_error("error while hashing: " + path.string());
  }
  const auto result = digest.finish();
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const std::byte value : result) {
    stream << std::setw(2) << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  return stream.str();
}

}  // namespace ac6demo
