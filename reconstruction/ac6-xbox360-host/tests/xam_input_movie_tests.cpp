#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6xbox360/xam_input_movie.hpp"

#include <openssl/sha.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

ac6xbox360::XamInputMovieIdentity identity() {
  return ac6xbox360::XamInputMovieIdentity{
      "ac6-demo-xbox360-pal",
      "Default.xex",
      "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8",
      0x82000000U,
      "ghidra-projects/ace-combat-6-demo",
      "PowerPC:BE:64:Xenon"};
}

ac6xbox360::XamInputState state() {
  ac6xbox360::XamInputState value{};
  for (std::size_t index = 0U; index < value.size(); ++index) {
    value[index] = static_cast<std::byte>(index);
  }
  return value;
}

std::string sha256(std::string_view value) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(),
         digest.data());
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result(digest.size() * 2U, '0');
  for (std::size_t index = 0U; index < digest.size(); ++index) {
    result[index * 2U] = kHex[digest[index] >> 4U];
    result[index * 2U + 1U] = kHex[digest[index] & 0x0FU];
  }
  return result;
}

std::string reseal_payload(std::string movie) {
  const auto footer = movie.rfind("{\"kind\":\"footer\"");
  assert(footer != std::string::npos);
  const std::string body = movie.substr(0U, footer);
  const std::string footer_base = "{\"kind\":\"footer\",\"event_count\":2}\n";
  const auto payload_key = movie.find("\"payload_sha256\":\"", footer);
  assert(payload_key != std::string::npos);
  const auto digest_offset =
      payload_key + std::string_view{"\"payload_sha256\":\""}.size();
  movie.replace(digest_offset, 64U, sha256(body + footer_base));
  return movie;
}

std::string record_movie() {
  ac6xbox360::XamInputMovie recorder;
  std::string error;
  assert(recorder.begin_record(identity(), error));
  assert(recorder.record(
      ac6xbox360::XamInputObservation{0U, 100U, 2U, 0x822F616CU, 0U, 1U, false,
                                      0U, true, state()},
      error));
  assert(recorder.record(
      ac6xbox360::XamInputObservation{
          1U, 101U, 3U, 0x822F60A8U, 1U, 0U, false, 1167U, false, {}},
      error));
  std::string movie;
  assert(recorder.finalize(movie, error));
  return movie;
}

} // namespace

int main() {
  using namespace ac6xbox360;
  const std::string movie = record_movie();
  assert(movie.find("\"schema\":\"ac6.xam-input-movie.v1\"") !=
         std::string::npos);
  assert(movie.find("\"guest_tick\":100,\"guest_thread\":2") !=
         std::string::npos);
  assert(movie.find("000102030405060708090a0b0c0d0e0f") != std::string::npos);
  assert(movie.find("\"state16\":null") == std::string::npos);
  assert(movie.find("\"state16\":\"00000000000000000000000000000000\"") !=
         std::string::npos);

  std::string error;
  XamInputMovie replay;
  assert(replay.begin_replay(movie, identity(), error));
  XamInputReplayValue value;
  assert(replay.replay({0x822F616CU, 0U, 1U, false}, value, error));
  assert(value.result == 0U && value.has_state && value.state == state());
  assert(replay.replay({0x822F60A8U, 1U, 0U, false}, value, error));
  assert(value.result == 1167U && !value.has_state);
  std::string none;
  assert(replay.finalize(none, error));
  assert(none.empty());

  for (const auto guards : {XamInputReplayGuards{0x822F6170U, 0U, 1U, false},
                            XamInputReplayGuards{0x822F616CU, 1U, 1U, false},
                            XamInputReplayGuards{0x822F616CU, 0U, 0U, false},
                            XamInputReplayGuards{0x822F616CU, 0U, 1U, true}}) {
    XamInputMovie strict;
    assert(strict.begin_replay(movie, identity(), error));
    assert(!strict.replay(guards, value, error));
    assert(error.find("strict replay mismatch") != std::string::npos);
  }

  for (std::string XamInputMovieIdentity::*field :
       {&XamInputMovieIdentity::target_id,
        &XamInputMovieIdentity::module,
        &XamInputMovieIdentity::ghidra_project,
        &XamInputMovieIdentity::ghidra_language}) {
    auto wrong = identity();
    wrong.*field += "-wrong";
    XamInputMovie rejected;
    assert(!rejected.begin_replay(movie, wrong, error));
    assert(error.find("identity mismatch") != std::string::npos);
  }

  auto wrong_xex = identity();
  wrong_xex.xex_sha256[0] = '0';
  XamInputMovie wrong_xex_replay;
  assert(!wrong_xex_replay.begin_replay(movie, wrong_xex, error));
  assert(error.find("identity mismatch") != std::string::npos);

  auto wrong_base = identity();
  wrong_base.base_address += 4U;
  XamInputMovie wrong_base_replay;
  assert(!wrong_base_replay.begin_replay(movie, wrong_base, error));
  assert(error.find("identity mismatch") != std::string::npos);

  std::string tampered = movie;
  const auto tick = tampered.find("\"guest_tick\":100");
  assert(tick != std::string::npos);
  tampered[tick + std::string_view{"\"guest_tick\":"}.size()] = '9';
  XamInputMovie tamper_replay;
  assert(!tamper_replay.begin_replay(tampered, identity(), error));
  assert(error == "payload digest mismatch");

  std::string wrong_type = movie;
  const auto type = wrong_type.find("XamInputGetState");
  assert(type != std::string::npos);
  wrong_type.replace(type, std::string_view{"XamInputGetState"}.size(),
                     "XamInputGetStatu");
  wrong_type = reseal_payload(std::move(wrong_type));
  XamInputMovie type_replay;
  assert(!type_replay.begin_replay(wrong_type, identity(), error));
  assert(error.find("event type") != std::string::npos);

  std::string wrong_ordinal = movie;
  const auto ordinal = wrong_ordinal.find("\"ordinal\":0");
  assert(ordinal != std::string::npos);
  wrong_ordinal[ordinal + std::string_view{"\"ordinal\":"}.size()] = '1';
  wrong_ordinal = reseal_payload(std::move(wrong_ordinal));
  XamInputMovie ordinal_replay;
  assert(!ordinal_replay.begin_replay(wrong_ordinal, identity(), error));
  assert(error == "event ordinal mismatch");

  XamInputMovie short_replay;
  assert(short_replay.begin_replay(movie, identity(), error));
  assert(short_replay.replay({0x822F616CU, 0U, 1U, false}, value, error));
  assert(!short_replay.finalize(none, error));
  assert(error.find("trace-length mismatch") != std::string::npos);

  XamInputMovie long_replay;
  assert(long_replay.begin_replay(movie, identity(), error));
  assert(long_replay.replay({0x822F616CU, 0U, 1U, false}, value, error));
  assert(long_replay.replay({0x822F60A8U, 1U, 0U, false}, value, error));
  assert(!long_replay.replay({0x822F616CU, 0U, 1U, false}, value, error));
  assert(error.find("trace-length mismatch") != std::string::npos);

  std::cout << "ac6-xbox360-host-xam-input-movie-tests: ok\n";
  return 0;
}
