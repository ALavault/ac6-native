#include "ac6/retail_content.h"

#include <zlib.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

void require(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", line, expression);
    std::abort();
  }
}
#define REQUIRE(value) require((value), #value, __LINE__)

class TempRoot final {
 public:
  TempRoot() {
    static std::atomic<unsigned> next{};
    path_ = std::filesystem::temp_directory_path() /
            ("ac6-retail-content-" + std::to_string(::getpid()) + "-" +
             std::to_string(next++));
    REQUIRE(std::filesystem::create_directories(path_));
  }
  ~TempRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(static_cast<bool>(output));
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(static_cast<bool>(output));
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(static_cast<bool>(input));
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void put_be32(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> deflate_raw(std::span<const std::uint8_t> payload) {
  z_stream stream{};
  REQUIRE(deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                       8, Z_DEFAULT_STRATEGY) == Z_OK);
  std::vector<std::uint8_t> compressed(compressBound(payload.size()));
  stream.next_in = const_cast<Bytef*>(payload.data());
  stream.avail_in = static_cast<uInt>(payload.size());
  stream.next_out = compressed.data();
  stream.avail_out = static_cast<uInt>(compressed.size());
  REQUIRE(deflate(&stream, Z_FINISH) == Z_STREAM_END);
  compressed.resize(stream.total_out);
  REQUIRE(deflateEnd(&stream) == Z_OK);
  return compressed;
}

ac6::Sha256Digest digest_file(const std::filesystem::path& path) {
  ac6::Sha256Digest digest{};
  REQUIRE(ac6::sha256_file(path, digest));
  return digest;
}

struct Fixture final {
  explicit Fixture(const std::filesystem::path& root, bool raw = false)
      : source(root / "source") {
    REQUIRE(std::filesystem::create_directories(source));
    payload.resize(8192);
    for (std::size_t index = 0; index < payload.size(); ++index) {
      payload[index] = static_cast<std::uint8_t>((index * 17u + 3u) & 0xffu);
    }
    payload[0] = 'F';
    payload[1] = 'H';
    payload[2] = 'M';
    payload[3] = ' ';
    std::vector<std::uint8_t> stored = raw ? payload : deflate_raw(payload);
    ac6::retail_mode1_xor(stored, 0);
    const std::vector<std::uint8_t> xex{'X', 'E', 'X', '2', 1, 2, 3, 4};
    const std::vector<std::uint8_t> data01{'P', 'A', 'C', '1'};
    std::vector<std::uint8_t> table(24, 0);
    put_be32(table, 0, 1);
    put_be32(table, 4, 2);
    put_be32(table, 8, raw ? 0x00020000u : 0u);
    put_be32(table, 12, 0);
    put_be32(table, 16, static_cast<std::uint32_t>(stored.size()));
    put_be32(table, 20, static_cast<std::uint32_t>(payload.size()));
    write_file(source / "default.xex", xex);
    write_file(source / "DATA.TBL", table);
    write_file(source / "DATA00.PAC", stored);
    write_file(source / "DATA01.PAC", data01);
    refresh_policy();
  }

  void refresh_policy() {
    policy.data_table_entries = 1;
    policy.pack_count = 2;
    policy.identity.xex_size = std::filesystem::file_size(source / "default.xex");
    policy.identity.data_table_size = std::filesystem::file_size(source / "DATA.TBL");
    policy.identity.data00_size = std::filesystem::file_size(source / "DATA00.PAC");
    policy.identity.data01_size = std::filesystem::file_size(source / "DATA01.PAC");
    policy.identity.xex_sha256 = digest_file(source / "default.xex");
    policy.identity.data_table_sha256 = digest_file(source / "DATA.TBL");
    policy.identity.data00_sha256 = digest_file(source / "DATA00.PAC");
    policy.identity.data01_sha256 = digest_file(source / "DATA01.PAC");
  }

  std::filesystem::path source;
  std::vector<std::uint8_t> payload;
  ac6::RetailIdentityPolicy policy;
};

std::filesystem::path index_path(const std::filesystem::path& cache,
                                 const ac6::Sha256Digest& digest) {
  return cache / "indices" / (ac6::sha256_hex(digest) + ".ac6idx");
}

std::filesystem::path blob_path(const std::filesystem::path& cache,
                                const ac6::Sha256Digest& digest) {
  const std::string hex = ac6::sha256_hex(digest);
  return cache / "blobs" / "sha256" / hex.substr(0, 2) / hex;
}

void sha256_and_mode1_tables_are_exact() {
  const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
  REQUIRE(ac6::sha256_hex(ac6::sha256_bytes(abc)) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  std::array<std::uint8_t, 8> pad{};
  ac6::retail_mode1_xor(pad, 0);
  const std::array<std::uint8_t, 8> expected_pad{
      0x85, 0xa3, 0x08, 0xd3, 0x13, 0x19, 0x8a, 0x2e};
  REQUIRE(pad == expected_pad);
  ac6::retail_mode1_xor(pad, 256);
  const std::array<std::uint8_t, 8> zero_pad{};
  REQUIRE(pad == zero_pad);
  std::vector<std::uint8_t> all_pads;
  all_pads.reserve(256u * 8u);
  for (std::uint32_t index = 0; index < 256; ++index) {
    std::array<std::uint8_t, 8> value{};
    ac6::retail_mode1_xor(value, index);
    all_pads.insert(all_pads.end(), value.begin(), value.end());
  }
  REQUIRE(ac6::sha256_hex(ac6::sha256_bytes(all_pads)) ==
          "61fbdce73b2a88f54f78929bf539faf3977aa5a8914e0b14607afc967932a625");
}

void import_is_reproducible_and_store_reads_the_payload() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const auto first = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, root.path() / "cache-a", entries);
  const auto second = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, root.path() / "cache-b", entries);
  REQUIRE(first.passed());
  REQUIRE(second.passed());
  REQUIRE(first.imported_records == 1);
  REQUIRE(first.imported_bytes == fixture.payload.size());
  REQUIRE(first.index_sha256 == second.index_sha256);
  REQUIRE(read_file(index_path(root.path() / "cache-a", first.index_sha256)) ==
          read_file(index_path(root.path() / "cache-b", second.index_sha256)));

  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(store.open(root.path() / "cache-a"));
  REQUIRE(store.records().size() == 1);
  REQUIRE(store.find(0) != nullptr);
  REQUIRE(store.find(1) == nullptr);
  std::vector<std::uint8_t> payload;
  REQUIRE(store.read_payload(0, payload));
  REQUIRE(payload == fixture.payload);
}

void raw_mode1_payloads_are_descrambled_without_inflation() {
  TempRoot root;
  Fixture fixture(root.path(), true);
  const std::array<std::uint32_t, 1> entries{0};
  const std::filesystem::path cache = root.path() / "cache";
  REQUIRE(ac6::RetailContentImporter(fixture.policy)
              .run(fixture.source, cache, entries)
              .passed());
  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(store.open(cache));
  REQUIRE(store.records().front().codec ==
          ac6::RetailStorageCodec::Mode1PiXorRaw);
  std::vector<std::uint8_t> payload;
  REQUIRE(store.read_payload(0, payload));
  REQUIRE(payload == fixture.payload);
}

void bad_hash_cannot_replace_a_valid_current_index() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const std::filesystem::path cache = root.path() / "cache";
  const ac6::RetailContentImporter importer(fixture.policy);
  REQUIRE(importer.run(fixture.source, cache, entries).passed());
  const std::vector<std::uint8_t> current = read_file(cache / "current");
  const std::vector<std::uint8_t> wrong{'b', 'a', 'd'};
  write_file(fixture.source / "default.xex", wrong);
  const auto report = importer.run(fixture.source, cache, entries);
  REQUIRE(report.error == ac6::RetailContentError::SourceIdentityMismatch);
  REQUIRE(read_file(cache / "current") == current);
  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(store.open(cache));
}

void truncation_and_excessive_sizes_fail_before_publication() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  std::vector<std::uint8_t> pac = read_file(fixture.source / "DATA00.PAC");
  pac.pop_back();
  write_file(fixture.source / "DATA00.PAC", pac);
  fixture.refresh_policy();
  auto report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, root.path() / "truncated", entries);
  REQUIRE(report.error == ac6::RetailContentError::DataTableInvalid);
  REQUIRE(!std::filesystem::exists(root.path() / "truncated" / "current"));

  Fixture complete(root.path() / "second");
  ac6::RetailImportLimits limits;
  limits.maximum_expanded_size = complete.payload.size() - 1;
  report = ac6::RetailContentImporter(complete.policy, limits).run(
      complete.source, root.path() / "oversized", entries);
  REQUIRE(report.error == ac6::RetailContentError::SizeLimitExceeded);
  REQUIRE(!std::filesystem::exists(root.path() / "oversized" / "current"));
}

void duplicate_requests_and_incomplete_caches_are_rejected() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const std::array<std::uint32_t, 2> duplicate{0, 0};
  auto report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, root.path() / "duplicate", duplicate);
  REQUIRE(report.error == ac6::RetailContentError::EntryDuplicate);

  std::vector<std::uint8_t> duplicate_table(40, 0);
  const std::vector<std::uint8_t> original_table =
      read_file(fixture.source / "DATA.TBL");
  put_be32(duplicate_table, 0, 2);
  put_be32(duplicate_table, 4, 2);
  std::copy(original_table.begin() + 8, original_table.end(),
            duplicate_table.begin() + 8);
  std::copy(original_table.begin() + 8, original_table.end(),
            duplicate_table.begin() + 24);
  write_file(fixture.source / "DATA.TBL", duplicate_table);
  fixture.refresh_policy();
  fixture.policy.data_table_entries = 2;
  report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, root.path() / "duplicate-range", entries);
  REQUIRE(report.error == ac6::RetailContentError::EntryDuplicate);
  REQUIRE(!std::filesystem::exists(root.path() / "duplicate-range" / "current"));

  const std::filesystem::path cache = root.path() / "incomplete";
  Fixture complete(root.path() / "complete");
  report = ac6::RetailContentImporter(complete.policy).run(
      complete.source, cache, entries);
  REQUIRE(report.passed());
  ac6::RetailContentStore store(complete.policy);
  REQUIRE(store.open(cache));
  REQUIRE(std::filesystem::remove(
      blob_path(cache, store.records().front().payload_sha256)));
  REQUIRE(!store.open(cache));
  REQUIRE(store.error() == ac6::RetailContentError::CacheIncomplete);
}

void decode_failure_after_a_blob_never_publishes_an_index() {
  TempRoot root;
  Fixture fixture(root.path());
  std::vector<std::uint8_t> pac = read_file(fixture.source / "DATA00.PAC");
  const std::uint32_t first_size = static_cast<std::uint32_t>(pac.size());
  pac.push_back(0);
  std::vector<std::uint8_t> table(40, 0);
  put_be32(table, 0, 2);
  put_be32(table, 4, 2);
  put_be32(table, 8, 0);
  put_be32(table, 12, 0);
  put_be32(table, 16, first_size);
  put_be32(table, 20, static_cast<std::uint32_t>(fixture.payload.size()));
  put_be32(table, 24, 0);
  put_be32(table, 28, first_size);
  put_be32(table, 32, 1);
  put_be32(table, 36, 64);
  write_file(fixture.source / "DATA.TBL", table);
  write_file(fixture.source / "DATA00.PAC", pac);
  fixture.refresh_policy();
  fixture.policy.data_table_entries = 2;
  const std::array<std::uint32_t, 2> entries{0, 1};
  const std::filesystem::path cache = root.path() / "interrupted";
  const auto report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, cache, entries);
  REQUIRE(report.error == ac6::RetailContentError::DecodeFailed);
  REQUIRE(!std::filesystem::exists(cache / "current"));
  REQUIRE(std::filesystem::exists(
      blob_path(cache, ac6::sha256_bytes(fixture.payload))));
}

void inconsistent_index_metadata_is_rejected_after_digest_verification() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const std::filesystem::path cache = root.path() / "cache";
  const auto report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, cache, entries);
  REQUIRE(report.passed());
  std::vector<std::uint8_t> index =
      read_file(index_path(cache, report.index_sha256));
  constexpr std::size_t kRecordSize = 108;
  const std::size_t record = index.size() - kRecordSize;
  index[record + 9] = 2;  // group says deflate; codec now falsely says raw.
  const ac6::Sha256Digest digest = ac6::sha256_bytes(index);
  write_file(index_path(cache, digest), index);
  std::vector<std::uint8_t> current(48, 0);
  const std::array<std::uint8_t, 8> magic{'A', 'C', '6', 'R', 'C', 'U', 'R', 0};
  std::copy(magic.begin(), magic.end(), current.begin());
  put_be32(current, 8, 2);
  put_be32(current, 12, static_cast<std::uint32_t>(current.size()));
  std::copy(digest.begin(), digest.end(), current.begin() + 16);
  write_file(cache / "current", current);

  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(!store.open(cache));
  REQUIRE(store.error() == ac6::RetailContentError::CacheIncompatible);
}

void abandoned_staging_is_not_a_published_generation() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const std::filesystem::path cache = root.path() / "cache";
  REQUIRE(ac6::RetailContentImporter(fixture.policy)
              .run(fixture.source, cache, entries)
              .passed());
  const std::filesystem::path abandoned = cache / ".staging" / "interrupted";
  REQUIRE(std::filesystem::create_directories(abandoned));
  const std::vector<std::uint8_t> garbage{'b', 'a', 'd'};
  write_file(abandoned / "current", garbage);
  write_file(abandoned / "index.ac6idx", garbage);
  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(store.open(cache));
  std::vector<std::uint8_t> payload;
  REQUIRE(store.read_payload(0, payload));
  REQUIRE(payload == fixture.payload);
}

void incompatible_current_and_corrupt_index_are_distinct_failures() {
  TempRoot root;
  Fixture fixture(root.path());
  const std::array<std::uint32_t, 1> entries{0};
  const std::filesystem::path incompatible = root.path() / "incompatible";
  auto report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, incompatible, entries);
  REQUIRE(report.passed());
  std::vector<std::uint8_t> current = read_file(incompatible / "current");
  current[11] = 1;  // v1 caches require an explicit re-import.
  write_file(incompatible / "current", current);
  ac6::RetailContentStore store(fixture.policy);
  REQUIRE(!store.open(incompatible));
  REQUIRE(store.error() == ac6::RetailContentError::CacheIncompatible);
  current = read_file(incompatible / "current");
  current[11] = 3;
  write_file(incompatible / "current", current);
  REQUIRE(!store.open(incompatible));
  REQUIRE(store.error() == ac6::RetailContentError::CacheIncompatible);

  const std::filesystem::path corrupt = root.path() / "corrupt";
  report = ac6::RetailContentImporter(fixture.policy).run(
      fixture.source, corrupt, entries);
  REQUIRE(report.passed());
  std::vector<std::uint8_t> index = read_file(index_path(corrupt, report.index_sha256));
  index.back() ^= 0x80u;
  write_file(index_path(corrupt, report.index_sha256), index);
  REQUIRE(!store.open(corrupt));
  REQUIRE(store.error() == ac6::RetailContentError::CacheDigestMismatch);
}

void media_manifest_is_atomic_reproducible_and_fail_closed() {
  TempRoot root;
  ac6::RetailMediaPolicy policy;
  policy.required = true;
  constexpr std::array<const char*, ac6::kRetailMediaAssetCount> names{
      "media0.bin", "media1.bin", "media2.bin", "media3.bin", "media4.bin", "media5.bin"};
  for (std::size_t index = 0; index < names.size(); ++index) {
    const std::vector<std::uint8_t> bytes{
        static_cast<std::uint8_t>(index), 0x41, 0x43, 0x36,
        static_cast<std::uint8_t>(index + 1)};
    write_file(root.path() / names[index], bytes);
    policy.assets[index].filename = names[index];
    policy.assets[index].container = "test";
    policy.assets[index].size = bytes.size();
    policy.assets[index].sha256 = ac6::sha256_bytes(bytes);
  }
  const auto source = root.path();
  const auto cache_a = root.path() / "media-a";
  const auto cache_b = root.path() / "media-b";
  REQUIRE(std::filesystem::create_directories(cache_a / ".staging" / "one"));
  REQUIRE(std::filesystem::create_directories(cache_b / ".staging" / "two"));
  ac6::Sha256Digest digest_a{}, digest_b{};
  std::string detail;
  REQUIRE(ac6::import_retail_media(source, cache_a, cache_a / ".staging" / "one",
                                    policy, digest_a, detail));
  REQUIRE(ac6::import_retail_media(source, cache_b, cache_b / ".staging" / "two",
                                    policy, digest_b, detail));
  REQUIRE(digest_a == digest_b);
  ac6::RetailMediaStore store;
  REQUIRE(store.open(cache_a, policy));
  std::vector<std::uint8_t> range;
  REQUIRE(store.read_range(ac6::RetailMediaAsset::Movie, 1, 3, range));
  REQUIRE(range == std::vector<std::uint8_t>({0x41, 0x43, 0x36}));
  const std::string hex = ac6::sha256_hex(policy.assets[0].sha256);
  const auto blob = cache_a / "media/blobs/sha256" / hex.substr(0, 2) / hex;
  std::vector<std::uint8_t> corrupt = read_file(blob);
  corrupt[0] ^= 0xffu;
  write_file(blob, corrupt);
  REQUIRE(!store.open(cache_a, policy));
}

void optional_ffmpeg_media_decode_smoke() {
  const char* cache_name = std::getenv("AC6_MEDIA_CACHE");
  if (cache_name == nullptr || *cache_name == '\0') return;
  ac6::RetailContentStore store;
  REQUIRE(store.open(cache_name));
  ac6::RetailDecodedAudio decoded;
  std::string detail;
  REQUIRE(ac6::RetailMediaDecoder::decode_audio(
      store.media(), ac6::RetailMediaAsset::Bgm, decoded, detail));
  REQUIRE(decoded.sample_rate == 48000);
  REQUIRE(decoded.channels == 6);
  REQUIRE(!decoded.pcm.empty());
  REQUIRE(!decoded.decoder_version.empty());
  REQUIRE(decoded.pcm_sha256 != ac6::Sha256Digest{});
  std::fprintf(stdout, "media_decode=pass decoder=%s pcm_sha256=%s samples=%zu\n",
               decoded.decoder_version.c_str(),
               ac6::sha256_hex(decoded.pcm_sha256).c_str(), decoded.pcm.size());
}

}  // namespace

int main() {
  sha256_and_mode1_tables_are_exact();
  import_is_reproducible_and_store_reads_the_payload();
  raw_mode1_payloads_are_descrambled_without_inflation();
  bad_hash_cannot_replace_a_valid_current_index();
  truncation_and_excessive_sizes_fail_before_publication();
  duplicate_requests_and_incomplete_caches_are_rejected();
  decode_failure_after_a_blob_never_publishes_an_index();
  abandoned_staging_is_not_a_published_generation();
  incompatible_current_and_corrupt_index_are_distinct_failures();
  media_manifest_is_atomic_reproducible_and_fail_closed();
  optional_ffmpeg_media_decode_smoke();
  inconsistent_index_metadata_is_rejected_after_digest_verification();
  std::puts("retail_content: all checks passed");
  return 0;
}
