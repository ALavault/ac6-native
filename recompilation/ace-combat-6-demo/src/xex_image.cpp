#include "ac6demo/xex_image.hpp"

#include "ac6demo/runtime_error.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <utility>

#ifdef AC6_DEMO_HAVE_LZX
#include <mspack.h>
#endif

namespace ac6demo {

namespace {

constexpr std::uint32_t kXexMagic = 0x58455832U;  // XEX2
constexpr std::uint32_t kXexHeaderFileFormatInfo = 0x000003FFU;
constexpr std::uint32_t kXexHeaderEntryPoint = 0x00010100U;
constexpr std::uint32_t kXexHeaderImageBase = 0x00010201U;
constexpr std::uint32_t kXexHeaderTlsInfo = 0x00020104U;
constexpr std::uint32_t kXexHeaderDefaultStackSize = 0x00020200U;
// Key type 0x00 means the header's value word IS the datum, so this reads the
// flag bitmask directly rather than an offset into the file.
constexpr std::uint32_t kXexHeaderSystemFlags = 0x00030000U;
constexpr std::uint32_t kNormalCompression = 2U;
constexpr std::uint16_t kNormalEncryption = 1U;
constexpr std::uint32_t kImageBase = 0x82000000U;

// XenonRecomp's normal-XEX path uses this public LZX ABI. The implementation
// is supplied by the pinned libmspack source; the concrete lzxd_stream remains
// opaque here.
#ifdef AC6_DEMO_HAVE_LZX
extern "C" {
struct lzxd_stream;
lzxd_stream* lzxd_init(mspack_system*, mspack_file*, mspack_file*, int, int, int,
                       off_t, char);
int lzxd_decompress(lzxd_stream*, off_t);
void lzxd_free(lzxd_stream*);
}
#endif

[[nodiscard]] std::uint16_t be16(const std::vector<std::byte>& bytes,
                                 std::size_t offset) {
  if (offset + 2U > bytes.size()) {
    throw RuntimeTrap("truncated XEX 16-bit field");
  }
  const auto high = static_cast<std::uint32_t>(
      std::to_integer<unsigned char>(bytes[offset]));
  const auto low = static_cast<std::uint32_t>(
      std::to_integer<unsigned char>(bytes[offset + 1U]));
  return static_cast<std::uint16_t>((high << 8U) | low);
}

[[nodiscard]] std::uint32_t be32(const std::vector<std::byte>& bytes,
                                 std::size_t offset) {
  if (offset + 4U > bytes.size()) {
    throw RuntimeTrap("truncated XEX 32-bit field");
  }
  return (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) << 24U) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1U])) << 16U) |
         (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2U])) << 8U) |
         static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3U]));
}

[[nodiscard]] std::size_t optional_header_offset(const std::vector<std::byte>& bytes,
                                                 std::uint32_t key) {
  const auto header_count = be32(bytes, 0x14U);
  const std::size_t headers = 0x18U;
  const std::size_t end = headers + static_cast<std::size_t>(header_count) * 8U;
  if (end > bytes.size()) {
    throw RuntimeTrap("XEX optional-header table exceeds file");
  }
  for (std::size_t offset = headers; offset < end; offset += 8U) {
    if (be32(bytes, offset) == key) {
      return offset + 4U;
    }
  }
  return 0U;
}

[[nodiscard]] std::uint32_t optional_header_value(const std::vector<std::byte>& bytes,
                                                  std::uint32_t key) {
  const auto value_offset = optional_header_offset(bytes, key);
  if (value_offset == 0U) {
    return 0U;
  }
  return be32(bytes, value_offset);
}

[[nodiscard]] std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw RuntimeTrap("unable to open qualified XEX: " + path.string());
  }
  input.seekg(0, std::ios::end);
  const auto end = input.tellg();
  if (end <= 0) {
    throw RuntimeTrap("qualified XEX is empty");
  }
  input.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(end));
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!input) {
    throw RuntimeTrap("unable to read qualified XEX: " + path.string());
  }
  return bytes;
}

[[nodiscard]] std::vector<std::byte> aes_cbc_decrypt(std::span<const std::byte> input,
                                                     std::span<const std::byte, 16> key) {
  if (input.empty() || (input.size() % 16U) != 0U) {
    throw RuntimeTrap("XEX AES payload is not block aligned");
  }
  EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
  if (context == nullptr) {
    throw RuntimeTrap("unable to allocate XEX AES context");
  }
  std::array<unsigned char, 16> iv{};
  std::vector<std::byte> output(input.size());
  int first = 0;
  int second = 0;
  const auto* key_bytes = reinterpret_cast<const unsigned char*>(key.data());
  const auto* input_bytes = reinterpret_cast<const unsigned char*>(input.data());
  auto* output_bytes = reinterpret_cast<unsigned char*>(output.data());
  const bool initialized = EVP_DecryptInit_ex(context, EVP_aes_128_cbc(), nullptr,
                                               key_bytes, iv.data()) == 1;
  const bool no_padding = initialized && EVP_CIPHER_CTX_set_padding(context, 0) == 1;
  const bool updated = no_padding &&
                       EVP_DecryptUpdate(context, output_bytes, &first, input_bytes,
                                         static_cast<int>(input.size())) == 1;
  const bool finalized = updated &&
                         EVP_DecryptFinal_ex(context, output_bytes + first, &second) == 1;
  EVP_CIPHER_CTX_free(context);
  if (!finalized || first + second != static_cast<int>(output.size())) {
    throw RuntimeTrap("XEX AES-CBC decryption failed");
  }
  return output;
}

[[nodiscard]] std::array<std::byte, 20> sha1(std::span<const std::byte> input) {
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  if (context == nullptr) {
    throw RuntimeTrap("unable to allocate XEX SHA-1 context");
  }
  std::array<std::byte, 20> digest{};
  unsigned int length = 0;
  const bool initialized = EVP_DigestInit_ex(context, EVP_sha1(), nullptr) == 1;
  const bool updated = initialized && EVP_DigestUpdate(
      context, input.data(), input.size()) == 1;
  const bool finalized = updated && EVP_DigestFinal_ex(
      context, reinterpret_cast<unsigned char*>(digest.data()), &length) == 1;
  EVP_MD_CTX_free(context);
  if (!finalized || length != digest.size()) {
    throw RuntimeTrap("XEX SHA-1 verification failed");
  }
  return digest;
}

#ifdef AC6_DEMO_HAVE_LZX
struct MemoryFile final {
  mspack_file file{};
  const std::byte* input{};
  std::size_t input_size{};
  std::size_t input_position{};
  std::byte* output{};
  std::size_t output_size{};
  std::size_t output_position{};
};

[[nodiscard]] MemoryFile& memory_file(mspack_file* file) {
  return *reinterpret_cast<MemoryFile*>(file);
}

mspack_file* memory_open(mspack_system*, const char*, int) { return nullptr; }
void memory_close(mspack_file*) {}

int memory_read(mspack_file* file, void* destination, int bytes) {
  auto& source = memory_file(file);
  const auto count = std::min<std::size_t>(static_cast<std::size_t>(bytes),
                                           source.input_size - source.input_position);
  std::memcpy(destination, source.input + source.input_position, count);
  source.input_position += count;
  return static_cast<int>(count);
}

int memory_write(mspack_file* file, void* source_bytes, int bytes) {
  auto& destination = memory_file(file);
  const auto count = std::min<std::size_t>(static_cast<std::size_t>(bytes),
                                           destination.output_size - destination.output_position);
  std::memcpy(destination.output + destination.output_position, source_bytes, count);
  destination.output_position += count;
  return static_cast<int>(count);
}

int memory_seek(mspack_file* file, off_t offset, int mode) {
  auto& source = memory_file(file);
  const auto origin = mode == MSPACK_SYS_SEEK_START
                          ? 0LL
                          : mode == MSPACK_SYS_SEEK_CUR
                                ? static_cast<long long>(source.input_position)
                                : static_cast<long long>(source.input_size);
  const auto position = origin + static_cast<long long>(offset);
  if (position < 0 || static_cast<unsigned long long>(position) > source.input_size) {
    return 1;
  }
  source.input_position = static_cast<std::size_t>(position);
  return 0;
}

off_t memory_tell(mspack_file* file) {
  return static_cast<off_t>(memory_file(file).input_position);
}

void memory_message(mspack_file*, const char*, ...) {}

void* memory_alloc(mspack_system*, std::size_t bytes) { return std::calloc(1U, bytes); }
void memory_free(void* pointer) { std::free(pointer); }
void memory_copy(void* source, void* destination, std::size_t bytes) {
  std::memcpy(destination, source, bytes);
}
#endif

[[nodiscard]] std::vector<std::byte> decompress_normal_payload(
    const std::vector<std::byte>& payload, std::size_t format_info,
    const std::vector<std::byte>& xex, std::uint32_t image_size) {
  auto block_size = be32(xex, format_info + 12U);
  const auto window_size = be32(xex, format_info + 8U);
  if (window_size == 0U || (window_size & (window_size - 1U)) != 0U ||
      window_size > (1U << 25U) || block_size < 24U) {
    throw RuntimeTrap("invalid XEX LZX parameters");
  }
  std::size_t stream_offset = 0U;
  std::vector<std::byte> compressed;
  std::array<std::byte, 20> expected_hash{};
  std::copy_n(xex.begin() + static_cast<std::ptrdiff_t>(format_info + 16U),
              expected_hash.size(), expected_hash.begin());
  std::size_t block_count = 0U;
  while (block_size != 0U) {
    if (block_size < 24U || stream_offset + block_size > payload.size()) {
      throw RuntimeTrap("XEX LZX block exceeds decrypted payload");
    }
    const auto stream = std::span<const std::byte>(payload.data() + stream_offset, block_size);
    if (sha1(stream) != expected_hash) {
      throw RuntimeTrap("XEX LZX block SHA-1 mismatch");
    }
    const auto next_block_size = be32(payload, stream_offset);
    std::size_t cursor = stream_offset + 24U;
    const std::size_t end = stream_offset + block_size;
    while (cursor < end) {
      if (cursor + 2U > end) {
        throw RuntimeTrap("truncated XEX LZX chunk length");
      }
      const auto chunk_size = static_cast<std::size_t>(be16(payload, cursor));
      cursor += 2U;
      if (chunk_size == 0U) {
        break;
      }
      if (cursor + chunk_size > end) {
        throw RuntimeTrap("XEX LZX chunk exceeds block");
      }
      compressed.insert(compressed.end(), payload.begin() + static_cast<std::ptrdiff_t>(cursor),
                        payload.begin() + static_cast<std::ptrdiff_t>(cursor + chunk_size));
      cursor += chunk_size;
    }
    std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(stream_offset + 4U),
                expected_hash.size(), expected_hash.begin());
    stream_offset += block_size;
    block_size = next_block_size;
    if (++block_count > 0x10000U) {
      throw RuntimeTrap("XEX LZX block chain is too long");
    }
  }

  #ifndef AC6_DEMO_HAVE_LZX
  (void)compressed;
  (void)image_size;
  throw RuntimeTrap("XEX LZX decoder is not linked in this build");
  #else
  std::vector<std::byte> image(image_size);
  MemoryFile input{};
  input.input = compressed.data();
  input.input_size = compressed.size();
  MemoryFile output{};
  output.output = image.data();
  output.output_size = image.size();
  mspack_system system{};
  system.open = memory_open;
  system.close = memory_close;
  system.read = memory_read;
  system.write = memory_write;
  system.seek = memory_seek;
  system.tell = memory_tell;
  system.message = memory_message;
  system.alloc = memory_alloc;
  system.free = memory_free;
  system.copy = memory_copy;

  const auto window_bits = static_cast<int>(std::countr_zero(window_size));
  auto* stream = lzxd_init(&system, &input.file, &output.file, window_bits, 0, 0x8000,
                           static_cast<off_t>(image.size()), 0);
  if (stream == nullptr) {
    throw RuntimeTrap("unable to initialize XEX LZX decoder");
  }
  const auto result = lzxd_decompress(stream, static_cast<off_t>(image.size()));
  lzxd_free(stream);
  if (result != MSPACK_ERR_OK || output.output_position != image.size()) {
    throw RuntimeTrap("XEX LZX decompression failed");
  }
  return image;
  #endif
}

}  // namespace

GuestImage load_xex_image(const std::filesystem::path& path) {
  const auto xex = read_file(path);
  if (be32(xex, 0U) != kXexMagic) {
    throw RuntimeTrap("qualified input is not XEX2");
  }
  const auto header_size = be32(xex, 8U);
  const auto security_offset = be32(xex, 0x10U);
  if (header_size < 0x18U || header_size > xex.size() ||
      security_offset + 0x160U > xex.size()) {
    throw RuntimeTrap("invalid XEX header bounds");
  }
  const auto image_size = be32(xex, security_offset + 4U);
  const auto load_address = be32(xex, security_offset + 0x110U);
  if (image_size == 0U || load_address != kImageBase ||
      static_cast<std::uint64_t>(load_address) + image_size > 0x1'0000'0000ULL) {
    throw RuntimeTrap("XEX image identity or bounds mismatch");
  }

  const auto format_info = optional_header_value(xex, kXexHeaderFileFormatInfo);
  const auto entry_point = optional_header_value(xex, kXexHeaderEntryPoint);
  const auto image_base = optional_header_value(xex, kXexHeaderImageBase);
  const auto tls_info_offset = optional_header_value(xex, kXexHeaderTlsInfo);
  const auto stack_size = optional_header_value(xex, kXexHeaderDefaultStackSize);
  const auto system_flags = optional_header_value(xex, kXexHeaderSystemFlags);
  if (format_info == 0U || format_info + 12U > xex.size() || entry_point == 0U ||
      image_base != kImageBase || tls_info_offset == 0U ||
      tls_info_offset + 16U > xex.size() || stack_size == 0U) {
    throw RuntimeTrap("XEX lacks qualified image metadata");
  }
  const auto tls_info_size = be32(xex, tls_info_offset + 0U);
  const auto tls_address = be32(xex, tls_info_offset + 4U);
  const auto tls_data_size = be32(xex, tls_info_offset + 8U);
  const auto tls_raw_size = be32(xex, tls_info_offset + 12U);
  if (tls_info_size < 16U || tls_info_offset + tls_info_size > xex.size() ||
      tls_address < load_address ||
      static_cast<std::uint64_t>(tls_address) + tls_raw_size >
          static_cast<std::uint64_t>(load_address) + image_size ||
      tls_raw_size < tls_data_size || tls_data_size == 0U) {
    throw RuntimeTrap("XEX TLS metadata is outside the qualified image");
  }
  const auto encryption = be16(xex, format_info + 4U);
  const auto compression = be16(xex, format_info + 6U);
  if (encryption != kNormalEncryption || compression != kNormalCompression) {
    throw RuntimeTrap("XEX encryption/compression mode is outside the qualified demo");
  }

  constexpr std::array<std::byte, 16> retail_key{
      std::byte{0x20}, std::byte{0xB1}, std::byte{0x85}, std::byte{0xA5},
      std::byte{0x9D}, std::byte{0x28}, std::byte{0xFD}, std::byte{0xC3},
      std::byte{0x40}, std::byte{0x58}, std::byte{0x3F}, std::byte{0xBB},
      std::byte{0x08}, std::byte{0x96}, std::byte{0xBF}, std::byte{0x91}};
  const auto decrypted_key = aes_cbc_decrypt(
      std::span<const std::byte>(xex.data() + security_offset + 0x150U, 16U), retail_key);
  if (decrypted_key.size() != 16U) {
    throw RuntimeTrap("XEX AES key has an invalid size");
  }
  std::array<std::byte, 16> decrypted_key_array{};
  std::copy(decrypted_key.begin(), decrypted_key.end(), decrypted_key_array.begin());
  const auto decrypted_payload = aes_cbc_decrypt(
      std::span<const std::byte>(xex.data() + header_size, xex.size() - header_size),
      decrypted_key_array);
  return GuestImage{load_address, entry_point, tls_address, tls_data_size, tls_raw_size,
                    stack_size, system_flags,
                    decompress_normal_payload(decrypted_payload, format_info, xex, image_size)};
}

}  // namespace ac6demo
