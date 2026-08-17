#include "ac6/execution_trace.h"

#include "ac6/sha256.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ac6 {
namespace {

std::string hash_payload(std::string_view payload) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(payload.data());
  return sha256_hex(sha256_bytes(std::span<const std::uint8_t>(bytes, payload.size())));
}

const char* backend_name(TraceGraphicsBackend backend) noexcept {
  switch (backend) {
    case TraceGraphicsBackend::Headless: return "headless";
    case TraceGraphicsBackend::CpuCompatibility: return "cpu_compatibility";
    case TraceGraphicsBackend::VulkanDirect: return "vulkan_direct";
  }
  return "invalid";
}

std::ostringstream json_stream() {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(std::numeric_limits<float>::max_digits10);
  return output;
}

std::string input_payload(InputFrame input) {
  std::ostringstream output = json_stream();
  output << "{\"pitch\":" << input.pitch
         << ",\"roll\":" << input.roll
         << ",\"yaw\":" << input.yaw
         << ",\"throttle\":" << static_cast<unsigned>(input.throttle)
         << ",\"buttons\":" << input.buttons << '}';
  return output.str();
}

std::string simulation_payload(const WorldFrame& frame) {
  std::ostringstream output = json_stream();
  output << "{\"tick\":" << frame.tick
         << ",\"mission_id\":" << frame.mission_id
         << ",\"mission_ready\":" << (frame.mission_ready ? "true" : "false")
         << ",\"player\":{\"entity\":" << frame.player_entity
         << ",\"position\":[" << frame.position_x << ',' << frame.position_y << ','
         << frame.position_z << "],\"attitude\":[" << frame.pitch << ',' << frame.roll
         << ',' << frame.yaw << "],\"speed\":" << frame.speed
         << "},\"camera\":{\"position\":[" << frame.camera_x << ',' << frame.camera_y
         << ',' << frame.camera_z << "],\"target\":[" << frame.camera_target_x << ','
         << frame.camera_target_y << ',' << frame.camera_target_z
         << "]},\"active_units\":" << frame.active_units << '}';
  return output.str();
}

std::string mission_payload(TraceMissionObjectives mission) {
  std::sort(mission.objectives.begin(), mission.objectives.end(),
            [](const ObjectiveRecord& left, const ObjectiveRecord& right) {
              return left.id < right.id;
            });
  std::ostringstream output = json_stream();
  output << "{\"state\":" << static_cast<unsigned>(mission.state)
         << ",\"sub_mission\":" << mission.sub_mission
         << ",\"step\":" << mission.step
         << ",\"script_ended\":" << (mission.script_ended ? "true" : "false")
         << ",\"objectives\":[";
  for (std::size_t index = 0; index < mission.objectives.size(); ++index) {
    const ObjectiveRecord& objective = mission.objectives[index];
    if (index != 0) output << ',';
    output << "{\"id\":" << objective.id
           << ",\"required\":" << (objective.required ? "true" : "false")
           << ",\"state\":" << static_cast<unsigned>(objective.state) << '}';
  }
  output << "]}";
  return output.str();
}

std::string graphics_payload(TraceGraphicsSubmission graphics) {
  std::ostringstream output = json_stream();
  output << "{\"backend\":\"" << backend_name(graphics.backend)
         << "\",\"draw_packets\":" << graphics.draw_packets
         << ",\"hud\":" << (graphics.hud ? "true" : "false") << '}';
  return output.str();
}

std::string media_payload(TraceMediaSubmission media) {
  std::ostringstream output = json_stream();
  output << "{\"qualified\":"
         << (media.qualified ? "true" : "false")
         << ",\"clock_tick\":" << media.clock_tick
         << ",\"temporal_events\":" << media.temporal_events
         << ",\"subtitle_events\":" << media.subtitle_events << '}';
  return output.str();
}

}  // namespace

ExecutionTraceJsonlWriter::~ExecutionTraceJsonlWriter() {
  (void)close();
}

bool ExecutionTraceJsonlWriter::open(const std::filesystem::path& path) {
  if (output_.is_open() || path.empty()) return false;
  output_.open(path, std::ios::out | std::ios::trunc);
  sequence_ = 0;
  previous_tick_ = 0;
  has_tick_ = false;
  failed_ = !output_.is_open();
  return !failed_;
}

bool ExecutionTraceJsonlWriter::emit(std::uint64_t tick, const char* domain,
                                     const std::string& payload) {
  if (!good()) return false;
  output_ << "{\"sequence\":" << sequence_ << ",\"tick\":" << tick
          << ",\"domain\":\"" << domain << "\",\"payload\":" << payload
          << "}\n";
  if (!output_) {
    failed_ = true;
    return false;
  }
  ++sequence_;
  return true;
}

bool ExecutionTraceJsonlWriter::append(std::uint64_t sample_tick,
                                       InputFrame input,
                                       const WorldFrame& simulation,
                                       TraceMissionObjectives mission,
                                       TraceGraphicsSubmission graphics,
                                       TraceMediaSubmission media) {
  if (!good() || input != simulation.input || sample_tick == 0 ||
      simulation.tick == 0 || (has_tick_ && sample_tick != previous_tick_ + 1u)) {
    return false;
  }
  const std::string input_json = input_payload(input);
  const std::string simulation_json = simulation_payload(simulation);
  const std::string mission_json = mission_payload(std::move(mission));
  const std::string graphics_json = graphics_payload(graphics);
  const std::string media_json = media_payload(media);
  std::ostringstream hashes = json_stream();
  hashes << "{\"input\":\"" << hash_payload(input_json)
         << "\",\"simulation\":\"" << hash_payload(simulation_json)
         << "\",\"objectives\":\"" << hash_payload(mission_json)
         << "\",\"graphics\":\"" << hash_payload(graphics_json)
         << "\",\"media\":\"" << hash_payload(media_json)
         << "\"}";
  if (!emit(sample_tick, "input", input_json) ||
      !emit(sample_tick, "simulation", simulation_json) ||
      !emit(sample_tick, "objectives", mission_json) ||
      !emit(sample_tick, "graphics", graphics_json) ||
      !emit(sample_tick, "media", media_json) ||
      !emit(sample_tick, "hashes", hashes.str())) {
    return false;
  }
  previous_tick_ = sample_tick;
  has_tick_ = true;
  output_.flush();
  if (!output_) failed_ = true;
  return !failed_;
}

bool ExecutionTraceJsonlWriter::close() {
  if (!output_.is_open()) return !failed_;
  output_.flush();
  if (!output_) failed_ = true;
  output_.close();
  return !failed_;
}

}  // namespace ac6
