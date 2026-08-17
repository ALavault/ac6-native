#include "ac6demo_native/replay.hpp"

#include "ac6demo_native/content_store.hpp"
#include "ac6demo_native/sha256.hpp"

#include <charconv>
#include <limits>
#include <regex>

namespace ac6demo_native {
namespace {

constexpr std::string_view kHeaderPrefix =
    "{\"magic\":\"AC6RTPLY\",\"version\":4,\"xex_sha256\":\"";
constexpr std::string_view kHeaderSuffix =
    "\",\"backend\":\"headless\",\"domains\":[\"input\",\"simulation\","
    "\"objectives\",\"graphics\",\"media\",\"hashes\"]}";

bool fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

std::string_view qualified_xex() noexcept {
    for (const auto& file : production_identity().files) {
        if (file.name == "Default.xex") {
            return file.sha256;
        }
    }
    return {};
}

template <typename T>
bool parse_integer(const std::ssub_match& match, T* output) {
    if (output == nullptr || !match.matched) {
        return false;
    }
    const std::string text = match.str();
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }
    *output = value;
    return true;
}

std::string event_line(std::uint64_t sequence, const PlatformObservation& observation) {
    const auto& input = observation.input;
    return "{\"type\":\"event\",\"sequence\":" + std::to_string(sequence) +
           ",\"tick\":" + std::to_string(observation.tick) +
           ",\"domain\":\"input\",\"payload\":{\"buttons\":" +
           std::to_string(input.buttons) + ",\"left_trigger\":" +
           std::to_string(input.left_trigger) + ",\"right_trigger\":" +
           std::to_string(input.right_trigger) + ",\"lx\":" +
           std::to_string(input.left_x) + ",\"ly\":" +
           std::to_string(input.left_y) + ",\"rx\":" +
           std::to_string(input.right_x) + ",\"ry\":" +
           std::to_string(input.right_y) + ",\"connected\":" +
           (input.connected ? "true" : "false") + ",\"present\":" +
           std::to_string(observation.present_count) +
           ",\"simulation_time_ns\":" +
           std::to_string(observation.simulation_time_ns) + "}}";
}

bool replay_observations(std::span<const PlatformObservation> observations,
                         std::string* error) {
    PlatformRuntime runtime;
    for (const auto& expected : observations) {
        const auto before = runtime.observe();
        if (!runtime.step(expected.input, error)) {
            return false;
        }
        if (expected.present_count == before.present_count + 1U) {
            if (!runtime.notify_present(error)) {
                return false;
            }
        } else if (expected.present_count != before.present_count) {
            return fail(error, "replay PRESENT order invalid");
        }
        if (!(runtime.observe() == expected)) {
            return fail(error, "replay observation divergence");
        }
    }
    return true;
}

bool parse_event(std::string_view line, PlatformObservation* output,
                 std::uint64_t expected_sequence, std::string* error) {
    static const std::regex pattern(
        R"(^\{"type":"event","sequence":([0-9]+),"tick":([0-9]+),"domain":"input","payload":\{"buttons":([0-9]+),"left_trigger":([0-9]+),"right_trigger":([0-9]+),"lx":(-?[0-9]+),"ly":(-?[0-9]+),"rx":(-?[0-9]+),"ry":(-?[0-9]+),"connected":(true|false),"present":([0-9]+),"simulation_time_ns":([0-9]+)\}\}$)");
    std::smatch match;
    const std::string owned(line);
    std::uint64_t sequence = 0U;
    std::uint64_t buttons = 0U;
    std::uint64_t left_trigger = 0U;
    std::uint64_t right_trigger = 0U;
    std::int64_t left_x = 0;
    std::int64_t left_y = 0;
    std::int64_t right_x = 0;
    std::int64_t right_y = 0;
    if (output == nullptr || line.size() > 512U ||
        !std::regex_match(owned, match, pattern) ||
        !parse_integer(match[1], &sequence) || sequence != expected_sequence ||
        !parse_integer(match[2], &output->tick) ||
        !parse_integer(match[3], &buttons) || buttons > 0xffffU ||
        !parse_integer(match[4], &left_trigger) || left_trigger > 0xffU ||
        !parse_integer(match[5], &right_trigger) || right_trigger > 0xffU ||
        !parse_integer(match[6], &left_x) ||
        left_x < std::numeric_limits<std::int16_t>::min() ||
        left_x > std::numeric_limits<std::int16_t>::max() ||
        !parse_integer(match[7], &left_y) ||
        left_y < std::numeric_limits<std::int16_t>::min() ||
        left_y > std::numeric_limits<std::int16_t>::max() ||
        !parse_integer(match[8], &right_x) ||
        right_x < std::numeric_limits<std::int16_t>::min() ||
        right_x > std::numeric_limits<std::int16_t>::max() ||
        !parse_integer(match[9], &right_y) ||
        right_y < std::numeric_limits<std::int16_t>::min() ||
        right_y > std::numeric_limits<std::int16_t>::max() ||
        !parse_integer(match[11], &output->present_count) ||
        !parse_integer(match[12], &output->simulation_time_ns)) {
        return fail(error, "replay event invalid");
    }
    output->input = {static_cast<std::uint16_t>(buttons),
                     static_cast<std::uint8_t>(left_trigger),
                     static_cast<std::uint8_t>(right_trigger),
                     static_cast<std::int16_t>(left_x),
                     static_cast<std::int16_t>(left_y),
                     static_cast<std::int16_t>(right_x),
                     static_cast<std::int16_t>(right_y), match[10].str() == "true"};
    return true;
}

}  // namespace

std::string write_replay_journal(std::span<const PlatformObservation> observations,
                                 std::string* error) {
    if (observations.size() > max_replay_records ||
        !replay_observations(observations, error)) {
        fail(error, "replay observations invalid");
        return {};
    }
    std::string body = std::string(kHeaderPrefix) + std::string(qualified_xex()) +
                       std::string(kHeaderSuffix) + "\n";
    for (std::size_t index = 0; index < observations.size(); ++index) {
        body += event_line(index + 1U, observations[index]) + "\n";
        if (body.size() > max_replay_bytes) {
            fail(error, "replay journal exceeds byte budget");
            return {};
        }
    }
    const std::string digest = sha256_bytes(std::as_bytes(std::span(body)));
    const std::string trailer = "{\"type\":\"hashes\",\"event_count\":" +
        std::to_string(observations.size()) + ",\"sha256\":\"" + digest + "\"}\n";
    if (body.size() > max_replay_bytes - trailer.size()) {
        fail(error, "replay journal exceeds byte budget");
        return {};
    }
    return body + trailer;
}

std::optional<std::vector<PlatformObservation>> replay_journal(
    std::string_view journal, std::string* error) {
    if (journal.empty() || journal.size() > max_replay_bytes || journal.back() != '\n') {
        fail(error, "replay journal size or terminator invalid");
        return std::nullopt;
    }
    const std::string expected_header = std::string(kHeaderPrefix) +
        std::string(qualified_xex()) + std::string(kHeaderSuffix);
    const std::size_t header_end = journal.find('\n');
    if (header_end == std::string_view::npos || journal.substr(0U, header_end) != expected_header) {
        fail(error, "replay header identity invalid");
        return std::nullopt;
    }
    std::vector<PlatformObservation> observations;
    std::size_t offset = header_end + 1U;
    std::size_t trailer_start = std::string_view::npos;
    while (offset < journal.size()) {
        const std::size_t end = journal.find('\n', offset);
        if (end == std::string_view::npos) {
            break;
        }
        const std::string_view line = journal.substr(offset, end - offset);
        if (line.starts_with("{\"type\":\"hashes\",")) {
            trailer_start = offset;
            break;
        }
        PlatformObservation observation;
        if (observations.size() == max_replay_records ||
            !parse_event(line, &observation, observations.size() + 1U, error)) {
            return std::nullopt;
        }
        observations.push_back(observation);
        offset = end + 1U;
    }
    if (trailer_start == std::string_view::npos) {
        fail(error, "replay integrity trailer missing");
        return std::nullopt;
    }
    const std::string expected_digest = sha256_bytes(
        std::as_bytes(std::span(journal.data(), trailer_start)));
    const std::string expected_trailer = "{\"type\":\"hashes\",\"event_count\":" +
        std::to_string(observations.size()) + ",\"sha256\":\"" + expected_digest + "\"}\n";
    if (journal.substr(trailer_start) != expected_trailer ||
        !replay_observations(observations, error)) {
        fail(error, "replay integrity or observation validation failed");
        return std::nullopt;
    }
    return observations;
}

}  // namespace ac6demo_native
