#include "ac6/retail_projection_receipt.h"

#include "ac6/sha256.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using ac6::retail::RetailProjectionReceiptError;
using ac6::retail::RetailSessionReplay;

int check(bool condition, std::string_view message) {
  if (!condition)
    std::cerr << "FAIL " << message << '\n';
  return condition ? 0 : 1;
}

class TempRoot final {
public:
  TempRoot() {
    static std::atomic<unsigned> next{};
    path_ = std::filesystem::temp_directory_path() /
            ("ac6-projection-receipt-" + std::to_string(::getpid()) + "-" +
             std::to_string(next++));
    std::filesystem::create_directories(path_);
  }
  ~TempRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path &path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

void write_text(const std::filesystem::path &path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

RetailSessionReplay make_replay(std::int16_t last_yaw = 300) {
  RetailSessionReplay replay;
  replay.mission_id = 1;
  replay.difficulty = ac6::retail::RetailDifficulty::Normal;
  replay.loadout = {1, 1, true};
  replay.content_index_sha256.fill(0x5a);
  replay.random_seed = 0xAC60000000000001ull;
  replay.frames = {
      {-100, 200, -300, 127, 0x0100},
      {-100, 200, -300, 127, 0x0100},
      {100, -200, last_yaw, 255, 0x0200},
      {100, -200, last_yaw, 255, 0x0200},
  };
  replay.final_tick = replay.frames.size();
  replay.final_digest = replay.input_digest();
  return replay;
}

RetailSessionReplay make_identity_replay() {
  RetailSessionReplay replay = make_replay();
  replay.frames = {replay.frames[0], replay.frames[2]};
  replay.final_tick = replay.frames.size();
  replay.final_digest = replay.input_digest();
  return replay;
}

std::string repeat(char value, std::size_t count) {
  return std::string(count, value);
}

std::string receipt_for(const RetailSessionReplay &replay,
                        const std::filesystem::path &replay_path) {
  ac6::Sha256Digest replay_sha{};
  if (!ac6::sha256_file(replay_path, replay_sha))
    return {};
  const std::string cache = ac6::sha256_hex(replay.content_index_sha256);
  const std::string input = ac6::sha256_hex(replay.input_digest());
  const std::string final = ac6::sha256_hex(replay.final_digest);
  const std::string output = ac6::sha256_hex(replay_sha);
  std::ostringstream json;
  json
      << "{\"cache_index_sha256\":\"" << cache
      << "\",\"cadence\":{\"census\":{\"file_sha256\":\"" << repeat('a', 64)
      << "\",\"integrity_level\":\"integrity_only_runtime_census\","
         "\"interval_count\":1,\"method\":\"uniform_marker_interval_v1\","
         "\"payload_sha256\":\""
      << repeat('b', 64)
      << "\",\"record_count\":2,\"schema\":"
         "\"ac6.controller-cadence-census.v1\"},\"hold\":2,"
         "\"integrity_level\":\"integrity_only_runtime_census\","
         "\"marker_contract\":{\"address\":\"821CA908\","
         "\"code\":{\"length\":16,\"offset\":1878280,\"sha256\":\""
      << repeat('2', 64)
      << "\"},\"phase\":\"before_input\",\"role\":"
         "\"ac6_frame_input_stage\"},\"native_clock\":{"
         "\"clock_id\":\"ac6_native_fixed_step\",\"frequency\":{"
         "\"denominator\":1,\"numerator\":60},\"schema\":"
         "\"ac6.native-simulation-clock.v1\",\"tick_semantics\":"
         "\"one_simulation_step\"},\"native_hz\":60,"
         "\"resampling\":\"zero_order_hold\",\"source_hz\":30},"
         "\"kind\":\"native_projection_receipt\",\"mapping\":{"
         "\"buttons\":\"raw_xinput_buttons\",\"pitch\":\"thumb_ly\","
         "\"roll\":\"thumb_lx\",\"throttle\":\"right_trigger\","
         "\"yaw\":\"left_shoulder=-32768;right_shoulder=32767;otherwise="
         "thumb_rx;left_precedes_right\"},\"output\":{"
         "\"aircraft_id\":"
      << replay.loadout.aircraft_id << ",\"capability_data_valid\":"
      << (replay.loadout.capability_data_valid ? "true" : "false")
      << ",\"checkpoint_count\":" << replay.checkpoints.size()
      << ",\"difficulty\":" << static_cast<unsigned>(replay.difficulty)
      << ",\"difficulty_name\":\"Normal\",\"final_digest_sha256\":\"" << final
      << "\",\"final_tick\":" << replay.final_tick
      << ",\"format\":\"AC6RTPLY\",\"frame_count\":" << replay.frames.size()
      << ",\"input_digest_sha256\":\"" << input
      << "\",\"mission_id\":" << replay.mission_id << ",\"output_sha256\":\""
      << output << "\",\"random_seed\":" << replay.random_seed
      << ",\"source_marker_count\":2,\"version\":" << replay.version
      << ",\"weapon_id\":" << replay.loadout.weapon_id
      << "},\"schema\":\"ac6.native-controller-projection-receipt.v3\","
         "\"source\":{\"parent_payload_sha256\":\""
      << repeat('9', 64) << "\",\"parent_replay_sha256\":\"" << repeat('8', 64)
      << "\",\"parent_window\":{\"marker_count\":2,\"start_marker\":1},"
         "\"raw_payload_sha256\":\""
      << repeat('7', 64) << "\",\"raw_replay_sha256\":\"" << repeat('6', 64)
      << "\"},\"target\":{\"base_version\":\"v0.0.0.11\","
         "\"marker_address\":\"821CA908\",\"marker_code_length\":16,"
         "\"marker_code_offset\":1878280,\"marker_code_sha256\":\""
      << repeat('2', 64)
      << "\",\"media_id\":\"0379EFB3\",\"module\":\"default.xex\","
         "\"module_xxh3\":\"0123456789abcdef\",\"title_id\":\"4E4D07D1\","
         "\"xex_sha256\":"
         "\"acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde\","
         "\"xex_version\":\"v0.0.0.11\"}}\n";
  return json.str();
}

std::string receipt_v4_for(const RetailSessionReplay &replay,
                           const std::filesystem::path &replay_path,
                           std::uint64_t source_hz = 30u,
                           std::uint64_t hold = 2u) {
  ac6::Sha256Digest replay_sha{};
  if (!ac6::sha256_file(replay_path, replay_sha))
    return {};
  const std::string cache = ac6::sha256_hex(replay.content_index_sha256);
  const std::string input = ac6::sha256_hex(replay.input_digest());
  const std::string final = ac6::sha256_hex(replay.final_digest);
  const std::string output = ac6::sha256_hex(replay_sha);
  std::ostringstream json;
  json
      << "{\"cache_index_sha256\":\"" << cache
      << "\",\"cadence\":{\"census\":{\"file_sha256\":\"" << repeat('a', 64)
      << "\",\"integrity_level\":\"integrity_only_runtime_census\","
         "\"interval_count\":1,\"method\":\"uniform_marker_interval_v1\","
         "\"payload_sha256\":\""
      << repeat('b', 64)
      << "\",\"record_count\":2,\"schema\":"
         "\"ac6.controller-cadence-census.v2\"},\"hold\":"
      << hold
      << ","
         "\"integrity_level\":\"integrity_only_runtime_census\","
         "\"native_clock\":{\"clock_id\":\"ac6_native_fixed_step\","
         "\"frequency\":{\"denominator\":1,\"numerator\":60},\"schema\":"
         "\"ac6.native-simulation-clock.v1\",\"tick_semantics\":"
         "\"one_simulation_step\"},\"native_hz\":60,\"resampling\":\""
      << (hold == 1u ? "identity" : "zero_order_hold")
      << "\",\"source_hz\":" << source_hz
      << "},\"kind\":"
         "\"native_projection_receipt\",\"mapping\":{\"buttons\":"
         "\"raw_xinput_buttons\",\"pitch\":\"thumb_ly\",\"roll\":"
         "\"thumb_lx\",\"throttle\":\"right_trigger\",\"yaw\":"
         "\"left_shoulder=-32768;right_shoulder=32767;otherwise="
         "thumb_rx;left_precedes_right\"},\"native_target\":{"
         "\"base_version\":\"v0.0.0.11\",\"media_id\":\"0379EFB3\","
         "\"module\":\"default.xex\",\"target_id\":"
         "\"ac6-pal-default-xex\",\"title_id\":\"4E4D07D1\","
         "\"xex_sha256\":"
         "\"acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde\","
         "\"xex_version\":\"v0.0.0.11\"},\"output\":{\"aircraft_id\":"
      << replay.loadout.aircraft_id << ",\"capability_data_valid\":"
      << (replay.loadout.capability_data_valid ? "true" : "false")
      << ",\"checkpoint_count\":" << replay.checkpoints.size()
      << ",\"difficulty\":" << static_cast<unsigned>(replay.difficulty)
      << ",\"difficulty_name\":\"Normal\",\"final_digest_sha256\":\"" << final
      << "\",\"final_tick\":" << replay.final_tick
      << ",\"format\":\"AC6RTPLY\",\"frame_count\":" << replay.frames.size()
      << ",\"input_digest_sha256\":\"" << input
      << "\",\"mission_id\":" << replay.mission_id << ",\"output_sha256\":\""
      << output << "\",\"random_seed\":" << replay.random_seed
      << ",\"source_marker_count\":2,\"version\":" << replay.version
      << ",\"weapon_id\":" << replay.loadout.weapon_id
      << "},\"schema\":\"ac6.native-controller-projection-receipt.v4\","
         "\"source\":{\"oracle\":{\"marker_contract\":{\"address\":"
         "\"821CA940\",\"code\":{\"image_rva\":\"001CA940\",\"length\":"
         "328,\"sha256\":"
         "\"a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d\"}"
         ","
         "\"phase\":\"before_input\",\"role\":\"ac6_frame_input_stage\"},"
         "\"target\":{\"base_version\":\"v0.0.0.8\",\"entry_point\":"
         "\"821F5ED0\",\"media_id\":\"531C30BE\",\"module\":"
         "\"default.xex\",\"module_xxh3\":\"892639B654015428\","
         "\"region_mask\":\"0000FDFF\",\"target_id\":"
         "\"ac6-ntsc-uj-default-xex\",\"title_id\":\"4E4D07D1\","
         "\"xex_sha256\":"
         "\"6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc\","
         "\"xex_version\":\"v0.0.0.8\"}},\"parent_payload_sha256\":\""
      << repeat('9', 64) << "\",\"parent_replay_sha256\":\"" << repeat('8', 64)
      << "\",\"parent_window\":{\"marker_count\":2,\"start_marker\":1},"
         "\"raw_payload_sha256\":\""
      << repeat('7', 64) << "\",\"raw_replay_sha256\":\"" << repeat('6', 64)
      << "\",\"raw_schema\":\"ac6.controller-input-replay.v4\"}}\n";
  return json.str();
}

bool replace_once(std::string &text, std::string_view old_value,
                  std::string_view new_value) {
  const std::size_t offset = text.find(old_value);
  if (offset == std::string::npos)
    return false;
  text.replace(offset, old_value.size(), new_value);
  return true;
}

RetailProjectionReceiptError run(const std::filesystem::path &receipt_path,
                                 const std::filesystem::path &replay_path,
                                 const RetailSessionReplay &replay,
                                 const ac6::Sha256Digest &cache) {
  return ac6::retail::preflight_retail_projection_receipt(
             receipt_path, replay_path, replay, cache)
      .error;
}

int metadata_mutations(const std::filesystem::path &receipt_path,
                       const std::filesystem::path &replay_path,
                       const RetailSessionReplay &replay,
                       std::string_view canonical) {
  const std::array replacements{
      std::pair{std::string_view{"\"aircraft_id\":1"},
                std::string_view{"\"aircraft_id\":2"}},
      std::pair{std::string_view{"\"capability_data_valid\":true"},
                std::string_view{"\"capability_data_valid\":false"}},
      std::pair{std::string_view{"\"checkpoint_count\":0"},
                std::string_view{"\"checkpoint_count\":1"}},
      std::pair{std::string_view{"\"difficulty\":1"},
                std::string_view{"\"difficulty\":2"}},
      std::pair{std::string_view{"\"final_tick\":4"},
                std::string_view{"\"final_tick\":3"}},
      std::pair{std::string_view{"\"format\":\"AC6RTPLY\""},
                std::string_view{"\"format\":\"AC6RTPLX\""}},
      std::pair{std::string_view{"\"frame_count\":4"},
                std::string_view{"\"frame_count\":3"}},
      std::pair{std::string_view{"\"mission_id\":1"},
                std::string_view{"\"mission_id\":2"}},
      std::pair{std::string_view{"\"random_seed\":12420927772287827969"},
                std::string_view{"\"random_seed\":1"}},
      std::pair{std::string_view{"\"version\":3"},
                std::string_view{"\"version\":2"}},
      std::pair{std::string_view{"\"weapon_id\":1"},
                std::string_view{"\"weapon_id\":2"}},
  };
  int failures = 0;
  for (const auto &[old_value, new_value] : replacements) {
    std::string mutated(canonical);
    failures += check(replace_once(mutated, old_value, new_value),
                      "metadata mutation locates its field");
    write_text(receipt_path, mutated);
    failures += check(
        run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
            RetailProjectionReceiptError::ReplayMetadataMismatch,
        "mutated replay metadata fails closed");
  }
  return failures;
}

int contract_mutations(const std::filesystem::path &receipt_path,
                       const std::filesystem::path &replay_path,
                       const RetailSessionReplay &replay,
                       std::string_view canonical) {
  constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
      replacements{{
          {"ac6.native-controller-projection-receipt.v3",
           "ac6.native-controller-projection-receipt.v1"},
          {"ac6.native-controller-projection-receipt.v3",
           "ac6.native-controller-projection-receipt.v2"},
          {"ac6.controller-cadence-census.v1",
           "ac6.controller-cadence-census.v2"},
          {"uniform_marker_interval_v1", "uniform_marker_interval_v2"},
          {"integrity_only_runtime_census", "runner_attested"},
          {"before_input", "invalid_phase"},
          {"ac6_frame_input_stage", "unknown_marker_role"},
          {"\"address\":\"821CA908\"", "\"address\":\"8226D1C8\""},
      }};
  int failures = 0;
  for (const auto &[old_value, new_value] : replacements) {
    std::string mutated(canonical);
    failures += check(replace_once(mutated, old_value, new_value),
                      "v3 contract mutation locates its field");
    write_text(receipt_path, mutated);
    failures += check(
        run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
            RetailProjectionReceiptError::SchemaMismatch,
        "v3 contract mutation fails closed");
  }

  std::string top_level_relabel(canonical);
  failures += check(
      replace_once(top_level_relabel,
                   "\"hold\":2,\"integrity_level\":"
                   "\"integrity_only_runtime_census\",\"marker_contract\"",
                   "\"hold\":2,\"integrity_level\":\"runner_attested\","
                   "\"marker_contract\""),
      "top-level integrity relabel locates its field");
  write_text(receipt_path, top_level_relabel);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "top-level runner label without attestation is rejected");

  std::string boolean_native_clock(canonical);
  failures += check(replace_once(boolean_native_clock,
                                 "\"denominator\":1,\"numerator\":60",
                                 "\"denominator\":true,\"numerator\":60"),
                    "boolean native clock mutation locates its field");
  write_text(receipt_path, boolean_native_clock);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "boolean native clock integer is rejected");

  std::string evidence_digest(canonical);
  failures +=
      check(replace_once(evidence_digest,
                         "\"file_sha256\":\"" + repeat('a', 64) + "\"",
                         "\"file_sha256\":\"g" + repeat('a', 63) + "\""),
            "cadence census digest mutation locates its field");
  write_text(receipt_path, evidence_digest);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "invalid cadence census digest is rejected");

  std::string census_window_count(canonical);
  failures += check(replace_once(census_window_count, "\"interval_count\":1",
                                 "\"interval_count\":2") &&
                        replace_once(census_window_count, "\"record_count\":2",
                                     "\"record_count\":3"),
                    "cadence census count mutation locates its fields");
  write_text(receipt_path, census_window_count);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "internally consistent census count must match the parent window");

  std::string marker_digest(canonical);
  failures += check(
      replace_once(marker_digest,
                   "\"offset\":1878280,\"sha256\":\"" + repeat('2', 64) + "\"",
                   "\"offset\":1878280,\"sha256\":\"" + repeat('3', 64) + "\""),
      "marker code mutation locates its field");
  write_text(receipt_path, marker_digest);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "marker code mismatch with target is rejected");

  std::string marker_offset(canonical);
  failures +=
      check(replace_once(marker_offset, "\"length\":16,\"offset\":1878280",
                         "\"length\":16,\"offset\":1878281"),
            "marker offset mutation locates its field");
  write_text(receipt_path, marker_offset);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "marker offset mismatch with target is rejected");

  std::string window_overflow(canonical);
  failures += check(
      replace_once(window_overflow,
                   "\"parent_window\":{\"marker_count\":2,\"start_marker\":1}",
                   "\"parent_window\":{\"marker_count\":2,"
                   "\"start_marker\":500000}"),
      "parent window overflow mutation locates its field");
  write_text(receipt_path, window_overflow);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "parent start plus count overflow is rejected");

  std::string target_identity(canonical);
  failures += check(
      replace_once(
          target_identity,
          "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde",
          repeat('1', 64)),
      "PAL XEX mutation locates its field");
  write_text(receipt_path, target_identity);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "non-PAL target identity is rejected");
  return failures;
}

int v4_contract_tests(const std::filesystem::path &receipt_path,
                      const std::filesystem::path &replay_path,
                      const std::filesystem::path &identity_path,
                      const RetailSessionReplay &replay) {
  int failures = 0;
  const std::string canonical = receipt_v4_for(replay, replay_path);
  write_text(receipt_path, canonical);
  const auto accepted = ac6::retail::preflight_retail_projection_receipt(
      receipt_path, replay_path, replay, replay.content_index_sha256);
  failures +=
      check(accepted.passed(), "canonical NTSC-U/J to PAL receipt v4 passes");
  failures += check(accepted.native_output_verified &&
                        !accepted.source_lineage_verified,
                    "v4 keeps source lineage unverified");
  failures += check(
      accepted.detail.find("provisional NTSC-U/J oracle to PAL") !=
              std::string::npos &&
          accepted.detail.find("not parity evidence") != std::string::npos,
      "v4 success keeps provisional qualification explicit");

  constexpr std::array<std::pair<std::string_view, std::string_view>, 14>
      mutations{{
          {"ac6.native-controller-projection-receipt.v4",
           "ac6.native-controller-projection-receipt.v3"},
          {"ac6.controller-input-replay.v4", "ac6.controller-input-replay.v3"},
          {"ac6.controller-cadence-census.v2",
           "ac6.controller-cadence-census.v1"},
          {"ac6-ntsc-uj-default-xex", "ac6-pal-default-xex"},
          {"\"base_version\":\"v0.0.0.8\"", "\"base_version\":\"v0.0.0.11\""},
          {"531C30BE", "0379EFB3"},
          {"6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc",
           "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"},
          {"892639B654015428", "892639b654015428"},
          {"821F5ED0", "821CA940"},
          {"0000FDFF", "0000FFFF"},
          {"821CA940", "821CA908"},
          {"\"image_rva\":\"001CA940\"", "\"image_rva\":\"001CA908\""},
          {"\"length\":328", "\"length\":16"},
          {"a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d",
           "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"},
      }};
  for (const auto &[old_value, new_value] : mutations) {
    std::string mutated(canonical);
    failures += check(replace_once(mutated, old_value, new_value),
                      "v4 contract mutation locates its field");
    write_text(receipt_path, mutated);
    failures += check(
        run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
            RetailProjectionReceiptError::SchemaMismatch,
        "v4 contract mutation fails closed");
  }

  std::string native_to_source_swap(canonical);
  failures += check(replace_once(native_to_source_swap, "ac6-pal-default-xex",
                                 "ac6-ntsc-uj-default-xex"),
                    "v4 native target swap locates its field");
  write_text(receipt_path, native_to_source_swap);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "v4 refuses the source identity as native target");

  std::string hybrid_target(canonical);
  failures += check(replace_once(hybrid_target, "\"media_id\":\"0379EFB3\"",
                                 "\"media_id\":\"531C30BE\""),
                    "v4 hybrid native target locates its field");
  write_text(receipt_path, hybrid_target);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "v4 rejects a PAL/NTSC hybrid native tuple");

  std::string extra_cadence_marker(canonical);
  failures += check(
      replace_once(extra_cadence_marker,
                   "\"integrity_level\":\"integrity_only_runtime_census\","
                   "\"native_clock\"",
                   "\"integrity_level\":\"integrity_only_runtime_census\","
                   "\"marker_contract\":{},\"native_clock\""),
      "v4 cadence marker injection locates its field");
  write_text(receipt_path, extra_cadence_marker);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "v4 rejects the legacy cadence marker field");

  std::string output_version(canonical);
  failures +=
      check(replace_once(output_version, "\"version\":3", "\"version\":4"),
            "v4 output replay version locates its field");
  write_text(receipt_path, output_version);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayMetadataMismatch,
      "v4 receipt cannot relabel its AC6RTPLY output as version 4");

  std::string extra_root_target(canonical);
  failures += check(
      replace_once(extra_root_target,
                   "\"raw_schema\":\"ac6.controller-input-replay.v4\"}}\n",
                   "\"raw_schema\":\"ac6.controller-input-replay.v4\"},"
                   "\"target\":{}}\n"),
      "v4 legacy root target injection locates its field");
  write_text(receipt_path, extra_root_target);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::SchemaMismatch,
      "v4 rejects a legacy root target alongside native_target");

  const RetailSessionReplay identity = make_identity_replay();
  failures += check(identity.write_file(identity_path),
                    "v4 identity-cadence replay writes");
  write_text(receipt_path, receipt_v4_for(identity, identity_path, 60u, 1u));
  const auto identity_result = ac6::retail::preflight_retail_projection_receipt(
      receipt_path, identity_path, identity, identity.content_index_sha256);
  failures += check(identity_result.passed() &&
                        identity_result.native_output_verified &&
                        !identity_result.source_lineage_verified,
                    "v4 accepts the exact 60-to-60 identity projection");
  return failures;
}

} // namespace

int main(int argc, char **argv) {
  if (argc == 4) {
    RetailSessionReplay replay;
    ac6::Sha256Digest cache{};
    if (!replay.read_file(argv[1]) || !ac6::parse_sha256(argv[3], cache))
      return 2;
    const auto result = ac6::retail::preflight_retail_projection_receipt(
        argv[2], argv[1], replay, cache);
    if (!result.passed()) {
      std::cerr << "FAIL exact Python receipt: "
                << ac6::retail::retail_projection_receipt_error_name(
                       result.error)
                << " " << result.detail << '\n';
      return 1;
    }
    return 0;
  }
  if (argc != 1)
    return 2;
  TempRoot root;
  const auto replay_path = root.path() / "mission01.ac6rply";
  const auto receipt_path = root.path() / "mission01.receipt.json";
  const auto other_path = root.path() / "other.ac6rply";
  const RetailSessionReplay replay = make_replay();
  int failures = 0;
  failures += check(replay.valid() && replay.write_file(replay_path),
                    "fixture replay writes as valid v3");
  const std::string canonical = receipt_for(replay, replay_path);
  write_text(receipt_path, canonical);

  const auto accepted = ac6::retail::preflight_retail_projection_receipt(
      receipt_path, replay_path, replay, replay.content_index_sha256);
  failures += check(accepted.passed(), "canonical matching receipt passes");
  failures += check(accepted.native_output_verified &&
                        !accepted.source_lineage_verified,
                    "result exposes the verified and unverified boundaries");
  failures +=
      check(accepted.detail.find("lineage unverified") != std::string::npos,
            "success keeps raw/parent lineage boundary explicit");
  failures += contract_mutations(receipt_path, replay_path, replay, canonical);
  failures += metadata_mutations(receipt_path, replay_path, replay, canonical);

  failures += v4_contract_tests(receipt_path, replay_path,
                                root.path() / "identity.ac6rply", replay);

  std::string digest_mutation = canonical;
  const std::string digest = ac6::sha256_hex(replay.input_digest());
  failures +=
      check(replace_once(digest_mutation,
                         "\"input_digest_sha256\":\"" + digest + "\"",
                         "\"input_digest_sha256\":\"" + repeat('0', 64) + "\""),
            "input digest mutation locates field");
  write_text(receipt_path, digest_mutation);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayMetadataMismatch,
      "input digest mutation is rejected");
  std::string final_digest_mutation = canonical;
  failures +=
      check(replace_once(final_digest_mutation,
                         "\"final_digest_sha256\":\"" + digest + "\"",
                         "\"final_digest_sha256\":\"" + repeat('0', 64) + "\""),
            "final digest mutation locates field");
  write_text(receipt_path, final_digest_mutation);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayMetadataMismatch,
      "final digest mutation is rejected");
  ac6::Sha256Digest replay_sha{};
  failures += check(ac6::sha256_file(replay_path, replay_sha),
                    "replay digest is available for mutation");
  std::string output_digest_mutation = canonical;
  failures += check(
      replace_once(output_digest_mutation,
                   "\"output_sha256\":\"" + ac6::sha256_hex(replay_sha) + "\"",
                   "\"output_sha256\":\"" + repeat('0', 64) + "\""),
      "exact replay digest mutation locates field");
  write_text(receipt_path, output_digest_mutation);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayMetadataMismatch,
      "exact replay digest mutation is rejected");

  std::string cache_mutation = canonical;
  failures += check(
      replace_once(cache_mutation,
                   "\"cache_index_sha256\":\"" +
                       ac6::sha256_hex(replay.content_index_sha256) + "\"",
                   "\"cache_index_sha256\":\"" + repeat('4', 64) + "\""),
      "cache mutation locates field");
  write_text(receipt_path, cache_mutation);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::CacheIdentityMismatch,
      "receipt cache swap is rejected");
  ac6::Sha256Digest other_cache = replay.content_index_sha256;
  other_cache[0] ^= 0xffu;
  write_text(receipt_path, canonical);
  failures += check(run(receipt_path, replay_path, replay, other_cache) ==
                        RetailProjectionReceiptError::CacheIdentityMismatch,
                    "opened cache swap is rejected");

  const RetailSessionReplay other = make_replay(301);
  failures += check(other.write_file(other_path), "second replay writes");
  write_text(receipt_path, receipt_for(other, other_path));
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayMetadataMismatch,
      "receipt from another replay is rejected");

  write_text(receipt_path, canonical);
  {
    std::fstream changed(replay_path,
                         std::ios::binary | std::ios::in | std::ios::out);
    changed.seekg(-1, std::ios::end);
    char byte = 0;
    changed.read(&byte, 1);
    byte ^= 1;
    changed.seekp(-1, std::ios::end);
    changed.write(&byte, 1);
  }
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayIdentityMismatch,
      "exact replay byte mutation is rejected");
  failures += check(replay.write_file(replay_path), "fixture replay restores");

  write_text(receipt_path, " " + canonical);
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::JsonNonCanonical,
      "JSON whitespace is rejected as noncanonical");
  write_text(receipt_path, canonical.substr(0, canonical.size() - 1u));
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::JsonNonCanonical,
      "missing canonical newline is rejected");

  write_text(receipt_path,
             std::string(17u, '[') + "0" + std::string(17u, ']') + "\n");
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::JsonBound,
      "JSON nesting bound is enforced");
  write_text(receipt_path, repeat('x', 65537u));
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReceiptByteBound,
      "receipt byte bound is enforced before parsing");

  const auto fifo_path = root.path() / "receipt.fifo";
  failures += check(::mkfifo(fifo_path.c_str(), 0600) == 0,
                    "receipt FIFO fixture is created");
  failures +=
      check(run(fifo_path, replay_path, replay, replay.content_index_sha256) ==
                RetailProjectionReceiptError::ReceiptUnreadable,
            "non-regular receipt is rejected without blocking");

  write_text(receipt_path, canonical);
  {
    std::ofstream oversized(replay_path, std::ios::binary | std::ios::trunc);
    oversized.seekp(10 * 1024 * 1024);
    oversized.put('\0');
  }
  failures += check(
      run(receipt_path, replay_path, replay, replay.content_index_sha256) ==
          RetailProjectionReceiptError::ReplayByteBound,
      "replay byte bound is enforced before hashing");

  return failures == 0 ? 0 : 1;
}
