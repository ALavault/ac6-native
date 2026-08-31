#include "ac6/native_xex.h"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace ac6::native {
namespace {

constexpr std::uint32_t kXex2 = 0x58455832u;
constexpr std::uint32_t kImageBase = 0x82000000u;
constexpr std::uint32_t kHeaderFileFormatInfo = 0x000003ffu;
constexpr std::uint32_t kHeaderEntryPoint = 0x00010100u;
constexpr std::uint32_t kHeaderImageBase = 0x00010201u;
constexpr std::uint32_t kHeaderTlsInfo = 0x00020104u;
constexpr std::uint32_t kHeaderDefaultStackSize = 0x00020200u;
constexpr std::uint32_t kHeaderSystemFlags = 0x00030000u;
constexpr std::uint16_t kNormalEncryption = 1u;
constexpr std::uint16_t kNoEncryption = 0u;
constexpr std::uint16_t kBasicCompression = 1u;
constexpr std::uint16_t kNormalCompression = 2u;
constexpr std::size_t kMinimumHeader = 0x18u;
constexpr std::size_t kSecurityInfoSize = 0x160u;
constexpr std::size_t kMaximumFileSize = 256u * 1024u * 1024u;
constexpr std::array<std::uint8_t, 16> kXex2RetailKey = {
    0x20u, 0xB1u, 0x85u, 0xA5u, 0x9Du, 0x28u, 0xFDu, 0xC3u,
    0x40u, 0x58u, 0x3Fu, 0xBBu, 0x08u, 0x96u, 0xBFu, 0x91u};
constexpr std::array<std::uint8_t, 16> kAesZeroIv{};

void fail(std::string* error, const char* message) {
  if (error != nullptr) *error = message;
}

bool range(std::size_t offset, std::size_t length, std::size_t size) {
  return offset <= size && length <= size - offset;
}

bool read_bytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes,
                std::string* error) {
  std::error_code ec;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size == 0u || file_size > kMaximumFileSize ||
      file_size > std::numeric_limits<std::size_t>::max()) {
    fail(error, "XEX file size is invalid");
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    fail(error, "unable to open XEX");
    return false;
  }
  bytes.resize(static_cast<std::size_t>(file_size));
  stream.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  if (!stream) {
    fail(error, "unable to read XEX");
    return false;
  }
  return true;
}

std::uint16_t be16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) << 8u |
                                    bytes[offset + 1u]);
}

std::uint32_t be32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24u) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 8u) |
         static_cast<std::uint32_t>(bytes[offset + 3u]);
}

std::uint16_t le16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(bytes[offset + 1u] << 8u);
}

std::uint32_t le32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

bool optional_value(std::span<const std::uint8_t> bytes, std::uint32_t key,
                    std::uint32_t& value) {
  if (!range(0x14u, 4u, bytes.size())) return false;
  const std::uint32_t count = be32(bytes, 0x14u);
  if (count > (bytes.size() - kMinimumHeader) / 8u) return false;
  const std::size_t table = kMinimumHeader;
  const std::size_t table_size = static_cast<std::size_t>(count) * 8u;
  if (!range(table, table_size, bytes.size())) return false;
  for (std::size_t offset = table; offset < table + table_size; offset += 8u) {
    if (be32(bytes, offset) == key) {
      value = be32(bytes, offset + 4u);
      return true;
    }
  }
  return false;
}

bool image_range(std::uint32_t address, std::uint32_t length,
                 std::uint32_t base, std::uint32_t size) {
  const std::uint64_t begin = address;
  const std::uint64_t end = begin + length;
  const std::uint64_t image_begin = base;
  const std::uint64_t image_end = image_begin + size;
  return begin >= image_begin && end <= image_end && end >= begin;
}

bool validate_pe_image(std::span<const std::uint8_t> image,
                       std::string* error) {
  if (image.size() < 0x40u || image[0] != 'M' || image[1] != 'Z') {
    fail(error, "decoded XEX image has no DOS header");
    return false;
  }
  const std::uint32_t pe_offset = le32(image, 0x3cu);
  if (!range(pe_offset, 24u, image.size()) || image[pe_offset] != 'P' ||
      image[pe_offset + 1u] != 'E' || image[pe_offset + 2u] != 0u ||
      image[pe_offset + 3u] != 0u || le16(image, pe_offset + 4u) != 0x01f2u) {
    fail(error, "decoded XEX image has an invalid Xenon PE header");
    return false;
  }
  return true;
}

}  // namespace

bool inspect_xex_metadata_bytes(std::span<const std::uint8_t> bytes,
                                XexMetadata& metadata, std::string* error) {
  metadata = {};
  if (!range(0u, kMinimumHeader, bytes.size()) || be32(bytes, 0u) != kXex2) {
    fail(error, "input is not XEX2");
    return false;
  }

  const std::uint32_t header_size = be32(bytes, 8u);
  const std::uint32_t security_offset = be32(bytes, 0x10u);
  if (header_size < kMinimumHeader || header_size > bytes.size() ||
      !range(security_offset, kSecurityInfoSize, bytes.size())) {
    fail(error, "XEX header bounds are invalid");
    return false;
  }
  const std::uint32_t image_size = be32(bytes, security_offset + 4u);
  const std::uint32_t load_address = be32(bytes, security_offset + 0x110u);
  if (image_size == 0u || load_address != kImageBase ||
      static_cast<std::uint64_t>(load_address) + image_size > 0x100000000ull) {
    fail(error, "XEX image identity is outside NTSC-U/J Xenon contract");
    return false;
  }

  std::uint32_t format_info = 0u;
  std::uint32_t entry_point = 0u;
  std::uint32_t image_base = 0u;
  std::uint32_t tls_info = 0u;
  std::uint32_t stack_size = 0u;
  std::uint32_t system_flags = 0u;
  if (!optional_value(bytes, kHeaderFileFormatInfo, format_info) ||
      !optional_value(bytes, kHeaderEntryPoint, entry_point) ||
      !optional_value(bytes, kHeaderImageBase, image_base) ||
      !optional_value(bytes, kHeaderTlsInfo, tls_info) ||
      !optional_value(bytes, kHeaderDefaultStackSize, stack_size) ||
      !optional_value(bytes, kHeaderSystemFlags, system_flags) ||
      !range(format_info, 8u, bytes.size()) || !range(tls_info, 16u, bytes.size()) ||
      image_base != kImageBase || !image_range(entry_point, 4u, load_address, image_size) ||
      stack_size == 0u) {
    fail(error, "XEX qualified metadata is missing or outside bounds");
    return false;
  }

  const std::uint32_t tls_info_size = be32(bytes, tls_info);
  const std::uint32_t tls_address = be32(bytes, tls_info + 4u);
  const std::uint32_t tls_data_size = be32(bytes, tls_info + 8u);
  const std::uint32_t tls_raw_size = be32(bytes, tls_info + 12u);
  const std::uint16_t encryption = be16(bytes, format_info + 4u);
  const std::uint16_t compression = be16(bytes, format_info + 6u);
  if (tls_info_size < 16u || !range(tls_info, tls_info_size, bytes.size()) ||
      tls_raw_size < tls_data_size || tls_data_size == 0u ||
      !image_range(tls_address, tls_raw_size, load_address, image_size) ||
      (encryption != kNoEncryption && encryption != kNormalEncryption) ||
      (compression != kBasicCompression && compression != kNormalCompression)) {
    fail(error, "XEX encryption, compression, or TLS is unsupported");
    return false;
  }

  metadata = XexMetadata{header_size, security_offset, image_size, load_address,
                         entry_point, image_base, tls_address, tls_data_size,
                         tls_raw_size, stack_size, system_flags, encryption,
                         compression};
  return true;
}

bool aes_cbc_decrypt(std::span<const std::uint8_t> input,
                     std::span<const std::uint8_t> key,
                     std::vector<std::uint8_t>& output, std::string* error) {
  if (key.size() != 16u || input.size() % 16u != 0u) {
    fail(error, "XEX AES payload is not block aligned");
    return false;
  }
  output.assign(input.size(), 0u);
  EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
  if (context == nullptr ||
      EVP_DecryptInit_ex(context, EVP_aes_128_cbc(), nullptr, key.data(),
                         kAesZeroIv.data()) != 1 ||
      EVP_CIPHER_CTX_set_padding(context, 0) != 1) {
    if (context != nullptr) EVP_CIPHER_CTX_free(context);
    fail(error, "unable to initialize XEX AES");
    return false;
  }
  int written = 0;
  int final_written = 0;
  const bool update_ok = EVP_DecryptUpdate(
                             context, output.data(), &written, input.data(),
                             static_cast<int>(input.size())) == 1;
  const bool final_ok = update_ok &&
                        EVP_DecryptFinal_ex(context, output.data() + written,
                                            &final_written) == 1;
  EVP_CIPHER_CTX_free(context);
  if (!final_ok || static_cast<std::size_t>(written + final_written) != output.size()) {
    output.clear();
    fail(error, "XEX AES decryption failed");
    return false;
  }
  return true;
}

bool load_xex_image_bytes(std::span<const std::uint8_t> bytes,
                          XexImage& image, std::string* error) {
  image = {};
  XexMetadata metadata;
  if (!inspect_xex_metadata_bytes(bytes, metadata, error)) return false;
  if (metadata.header_size > bytes.size()) {
    fail(error, "XEX payload starts beyond file");
    return false;
  }

  std::vector<std::uint8_t> source;
  const std::span<const std::uint8_t> payload = bytes.subspan(metadata.header_size);
  if (metadata.encryption == kNormalEncryption) {
    if (!range(static_cast<std::size_t>(metadata.security_offset) + 0x150u,
               16u, bytes.size())) {
      fail(error, "XEX AES key is outside security header");
      return false;
    }
    std::vector<std::uint8_t> session_key;
    if (!aes_cbc_decrypt(bytes.subspan(metadata.security_offset + 0x150u, 16u),
                         kXex2RetailKey, session_key, error) ||
        !aes_cbc_decrypt(payload, session_key, source, error)) {
      return false;
    }
  } else {
    source.assign(payload.begin(), payload.end());
  }

  std::uint32_t format_info = 0u;
  if (!optional_value(bytes, kHeaderFileFormatInfo, format_info) ||
      !range(format_info, 8u, bytes.size())) {
    fail(error, "XEX file-format information is missing");
    return false;
  }
  const std::uint32_t info_size = be32(bytes, format_info);
  const std::uint16_t compression = be16(bytes, format_info + 6u);
  if (compression == 0u) {
    if (source.size() < metadata.image_size) {
      fail(error, "uncompressed XEX image is truncated");
      return false;
    }
    image.image.assign(source.begin(), source.begin() + metadata.image_size);
  } else if (compression == kBasicCompression) {
    if (info_size < 16u || info_size % 8u != 0u ||
        !range(format_info, info_size, bytes.size())) {
      fail(error, "XEX basic-compression table is invalid");
      return false;
    }
    const std::size_t block_count = static_cast<std::size_t>(info_size / 8u - 1u);
    std::uint64_t expanded_size = 0u;
    for (std::size_t block = 0u; block != block_count; ++block) {
      const std::size_t record = static_cast<std::size_t>(format_info) + 8u + block * 8u;
      const std::uint64_t data_size = be32(bytes, record);
      const std::uint64_t zero_size = be32(bytes, record + 4u);
      expanded_size += data_size + zero_size;
      if (expanded_size > metadata.image_size) {
        fail(error, "XEX basic-compression image exceeds security size");
        return false;
      }
    }
    if (expanded_size == 0u || expanded_size > metadata.image_size) {
      fail(error, "XEX basic-compression image size is outside security size");
      return false;
    }
    image.image.assign(static_cast<std::size_t>(expanded_size), 0u);
    std::size_t source_offset = 0u;
    std::size_t destination_offset = 0u;
    for (std::size_t block = 0u; block != block_count; ++block) {
      const std::size_t record = static_cast<std::size_t>(format_info) + 8u + block * 8u;
      const std::size_t data_size = be32(bytes, record);
      const std::size_t zero_size = be32(bytes, record + 4u);
      if (data_size > source.size() - std::min(source_offset, source.size()) ||
          destination_offset > image.image.size() ||
          data_size + zero_size > image.image.size() - destination_offset) {
        fail(error, "XEX basic-compression block is outside payload");
        return false;
      }
      std::copy_n(source.data() + source_offset, data_size,
                  image.image.data() + destination_offset);
      source_offset += data_size;
      destination_offset += data_size + zero_size;
    }
    if (source_offset > source.size()) {
      fail(error, "XEX basic-compression payload is truncated");
      return false;
    }
  } else {
    fail(error, "XEX normal/LZX compression is not implemented");
    return false;
  }
  if (!validate_pe_image(image.image, error)) return false;
  image.metadata = metadata;
  return true;
}

bool load_xex_image_file(const std::filesystem::path& path, XexImage& image,
                         std::string* error) {
  std::vector<std::uint8_t> bytes;
  if (!read_bytes(path, bytes, error)) return false;
  return load_xex_image_bytes(bytes, image, error);
}

bool inspect_xex_metadata(const std::filesystem::path& path,
                          XexMetadata& metadata, std::string* error) {
  std::vector<std::uint8_t> bytes;
  if (!read_bytes(path, bytes, error)) return false;
  return inspect_xex_metadata_bytes(bytes, metadata, error);
}

}  // namespace ac6::native
