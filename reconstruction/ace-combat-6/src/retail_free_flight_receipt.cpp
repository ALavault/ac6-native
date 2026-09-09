#include "ac6/retail_free_flight_receipt.h"

#include "ac6/sha256.h"

#include <algorithm>
#include <bit>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace ac6::retail {
namespace {

constexpr std::uint32_t kWidth = 1280U;
constexpr std::uint32_t kHeight = 720U;
constexpr std::uint64_t kMinimumVisibleChangePixels = 1024U;
constexpr std::uint64_t kMinimumHudGreenPixels = 64U;
constexpr std::uint8_t kMinimumLuminanceRange = 32U;

constexpr std::array<std::string_view, 8> kControlNames{
    "neutral", "pitch", "roll", "yaw", "throttle", "brake", "recovery",
    "sustained-flight"};

std::string image_name(const std::size_t sample_index) {
  if (sample_index >= kRetailFreeFlightReceiptSampleTicks.size()) return {};
  std::ostringstream name;
  name << std::setfill('0') << std::setw(6)
       << kRetailFreeFlightReceiptSampleTicks[sample_index] << '-'
       << kControlNames[sample_index] << ".ppm";
  return name.str();
}

bool write_ppm(const std::filesystem::path& path,
               const std::span<const std::uint8_t> pixels) {
  if (pixels.size() != static_cast<std::size_t>(kWidth) * kHeight * 4U) {
    return false;
  }
  std::ofstream output(path, std::ios::binary);
  if (!output) return false;
  output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
  for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
    output.put(static_cast<char>(pixels[index]));
    output.put(static_cast<char>(pixels[index + 1U]));
    output.put(static_cast<char>(pixels[index + 2U]));
  }
  return static_cast<bool>(output);
}

std::uint64_t changed_pixels(
    const std::span<const std::uint8_t> previous,
    const std::span<const std::uint8_t> current) noexcept {
  if (previous.size() != current.size()) return 0U;
  std::uint64_t changed = 0U;
  for (std::size_t index = 0U; index < current.size(); index += 4U) {
    if (previous[index] != current[index] ||
        previous[index + 1U] != current[index + 1U] ||
        previous[index + 2U] != current[index + 2U]) {
      ++changed;
    }
  }
  return changed;
}

bool in_green_hud_region(const std::uint32_t x,
                         const std::uint32_t y) noexcept {
  const bool reticle = x >= 610U && x < 670U && y >= 328U && y < 392U;
  const bool telemetry = x >= 12U && x < 320U && y >= 604U && y < 708U;
  return reticle || telemetry;
}

std::uint64_t hud_green_pixels(
    const std::span<const std::uint8_t> pixels) noexcept {
  std::uint64_t count = 0U;
  for (std::uint32_t y = 0U; y < kHeight; ++y) {
    for (std::uint32_t x = 0U; x < kWidth; ++x) {
      if (!in_green_hud_region(x, y)) continue;
      const std::size_t index =
          (static_cast<std::size_t>(y) * kWidth + x) * 4U;
      const std::uint8_t red = pixels[index];
      const std::uint8_t green = pixels[index + 1U];
      const std::uint8_t blue = pixels[index + 2U];
      if (green >= 180U && static_cast<unsigned>(green) >= red + 50U &&
          static_cast<unsigned>(green) >= blue + 50U) {
        ++count;
      }
    }
  }
  return count;
}

std::array<std::uint8_t, 2> luminance_range(
    const std::span<const std::uint8_t> pixels) noexcept {
  std::uint8_t minimum = 255U;
  std::uint8_t maximum = 0U;
  for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
    const unsigned value =
        (54U * pixels[index] + 183U * pixels[index + 1U] +
         19U * pixels[index + 2U]) >>
        8U;
    const auto luminance = static_cast<std::uint8_t>(value);
    minimum = std::min(minimum, luminance);
    maximum = std::max(maximum, luminance);
  }
  return {minimum, maximum};
}

std::uint32_t float_bits(const float value) noexcept {
  return std::bit_cast<std::uint32_t>(value);
}

const char* json_bool(const bool value) noexcept {
  return value ? "true" : "false";
}

}  // namespace

bool retail_free_flight_receipt_sample_tick(const std::uint32_t tick) noexcept {
  return std::find(kRetailFreeFlightReceiptSampleTicks.begin(),
                   kRetailFreeFlightReceiptSampleTicks.end(),
                   tick) != kRetailFreeFlightReceiptSampleTicks.end();
}

RetailFreeFlightReceipt::RetailFreeFlightReceipt(
    std::filesystem::path output_directory)
    : output_directory_(std::move(output_directory)),
      manifest_path_(output_directory_ / "receipt.json") {}

bool RetailFreeFlightReceipt::fail(std::string detail) {
  failure_detail_ = std::move(detail);
  eligible_ = false;
  return false;
}

bool RetailFreeFlightReceipt::begin(
    const Sha256Digest& cache_index_sha256,
    const RetailMission01VulkanSceneReport& scene_report) {
  if (begun_ || finalized_ || output_directory_.empty()) {
    return fail("invalid_state");
  }
  if (scene_report.content_index_sha256 != cache_index_sha256 ||
      !scene_report.store_backed || !scene_report.free_flight_world_complete ||
      scene_report.runtime_draw_instances != 4226U ||
      scene_report.terrain_draw_instances != 65536U ||
      scene_report.water_sampled_cells != 65536U ||
      scene_report.water_visible_cells == 0U ||
      scene_report.water_draw_instances == 0U ||
      scene_report.player_aircraft_vertices != 4435U ||
      scene_report.player_aircraft_source_indices != 6468U ||
      scene_report.player_aircraft_draw_instances != 1U ||
      scene_report.player_aircraft_texture_identifier != 0x10002215U) {
    return fail("scene_contract");
  }

  std::error_code error;
  const bool exists = std::filesystem::exists(output_directory_, error);
  if (error) return fail("output_stat");
  if (exists) {
    if (!std::filesystem::is_directory(output_directory_, error) || error) {
      return fail("output_not_directory");
    }
    const std::filesystem::directory_iterator begin(output_directory_, error);
    if (error || begin != std::filesystem::directory_iterator{}) {
      return fail("output_not_empty");
    }
  } else if (!std::filesystem::create_directories(output_directory_, error) ||
             error) {
    return fail("output_create");
  }

  cache_index_sha256_ = cache_index_sha256;
  scene_report_ = scene_report;
  samples_.reserve(kRetailFreeFlightReceiptSampleTicks.size());
  previous_rgba8_.reserve(static_cast<std::size_t>(kWidth) * kHeight * 4U);
  failure_detail_.clear();
  begun_ = true;
  return true;
}

bool RetailFreeFlightReceipt::observe(
    const std::uint32_t tick, const InputFrame input,
    const SimulationSnapshot& snapshot,
    const std::uint64_t flight_state_digest,
    const std::span<const std::uint8_t> rgba8) {
  const std::size_t index = samples_.size();
  if (!begun_ || finalized_ ||
      index >= kRetailFreeFlightReceiptSampleTicks.size() ||
      tick != kRetailFreeFlightReceiptSampleTicks[index] ||
      input != native_free_flight_qualification_input(tick)) {
    return fail("sample_order_or_input");
  }
  if (snapshot.tick != tick || snapshot.mission_id != 1U ||
      snapshot.mission_state != ScenarioState::Gameplay || !snapshot.valid() ||
      !snapshot.digest_matches() ||
      rgba8.size() != static_cast<std::size_t>(kWidth) * kHeight * 4U) {
    return fail("sample_state_or_pixels");
  }

  Sample sample;
  sample.tick = tick;
  sample.input = input;
  sample.snapshot_sha256 = snapshot.digest;
  sample.flight_state_digest = flight_state_digest;
  sample.rgba_sha256 = sha256_bytes(rgba8);
  sample.attitude = snapshot.player_attitude;
  sample.speed = snapshot.player_speed;
  sample.changed_pixels =
      index == 0U ? 0U : changed_pixels(previous_rgba8_, rgba8);
  sample.hud_green_pixels = hud_green_pixels(rgba8);
  const std::array<std::uint8_t, 2> range = luminance_range(rgba8);
  sample.luminance_min = range[0];
  sample.luminance_max = range[1];
  sample.visual_effect =
      index == 0U || sample.changed_pixels >= kMinimumVisibleChangePixels;
  sample.image_name = image_name(index);
  if (sample.image_name.empty() ||
      !write_ppm(output_directory_ / sample.image_name, rgba8)) {
    return fail("sample_write");
  }
  samples_.push_back(std::move(sample));
  Sample& observed = samples_.back();
  if (index == 0U || index == 6U) {
    observed.semantic_effect = true;
  } else if (index == 1U) {
    observed.semantic_effect =
        observed.attitude[0] != samples_[0U].attitude[0];
  } else if (index == 2U) {
    observed.semantic_effect =
        observed.attitude[1] != samples_[index - 1U].attitude[1];
  } else if (index == 3U) {
    observed.semantic_effect =
        observed.attitude[2] != samples_[index - 1U].attitude[2];
  } else if (index == 4U) {
    observed.semantic_effect = observed.speed > samples_[index - 1U].speed;
  } else if (index == 5U) {
    observed.semantic_effect = observed.speed < samples_[index - 1U].speed;
  } else {
    observed.semantic_effect =
        observed.flight_state_digest !=
            samples_[index - 1U].flight_state_digest &&
        observed.snapshot_sha256 != samples_[index - 1U].snapshot_sha256;
  }
  previous_rgba8_.assign(rgba8.begin(), rgba8.end());
  return true;
}

bool RetailFreeFlightReceipt::finalize(
    const std::uint64_t final_tick,
    const Sha256Digest& replay_input_sha256) {
  if (!begun_ || finalized_ ||
      samples_.size() != kRetailFreeFlightReceiptSampleTicks.size() ||
      final_tick != kNativeFreeFlightQualificationTicks) {
    return fail("final_tick_or_samples");
  }

  eligible_ = true;
  for (std::size_t index = 0U; index < samples_.size(); ++index) {
    const Sample& sample = samples_[index];
    const unsigned range =
        static_cast<unsigned>(sample.luminance_max) - sample.luminance_min;
    if (!sample.visual_effect ||
        sample.hud_green_pixels < kMinimumHudGreenPixels ||
        range < kMinimumLuminanceRange ||
        (index != 0U && !sample.semantic_effect)) {
      eligible_ = false;
    }
  }
  scene_report_.jv_eligible = eligible_ && scene_report_.complete_render_scene;
  eligible_ = scene_report_.jv_eligible;

  std::vector<std::uint8_t> evidence_bytes;
  evidence_bytes.reserve(samples_.size() * 72U + replay_input_sha256.size());
  evidence_bytes.insert(evidence_bytes.end(), replay_input_sha256.begin(),
                        replay_input_sha256.end());
  for (const Sample& sample : samples_) {
    evidence_bytes.insert(evidence_bytes.end(), sample.snapshot_sha256.begin(),
                          sample.snapshot_sha256.end());
    evidence_bytes.insert(evidence_bytes.end(), sample.rgba_sha256.begin(),
                          sample.rgba_sha256.end());
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
      evidence_bytes.push_back(static_cast<std::uint8_t>(
          sample.flight_state_digest >> shift));
    }
  }
  const Sha256Digest evidence_sha256 = sha256_bytes(evidence_bytes);

  std::ostringstream json;
  json << "{\"schema\":\"ac6.native-m01-free-flight-receipt.v2\""
       << ",\"target\":\"ntsc-uj\",\"ticks\":3600,\"fixed_hz\":60"
       << ",\"duration_seconds\":60,\"renderer\":\"vulkan\""
       << ",\"diagnostic_camera\":false,\"interactive_cpu_raster\":false"
       << ",\"target_marker\":false,\"cache_index_sha256\":\""
       << sha256_hex(cache_index_sha256_) << "\",\"replay_input_sha256\":\""
       << sha256_hex(replay_input_sha256) << "\",\"evidence_sha256\":\""
       << sha256_hex(evidence_sha256) << "\",\"scene\":{"
       << "\"city_draw_instances\":" << scene_report_.runtime_draw_instances
       << ",\"terrain_draw_instances\":"
       << scene_report_.terrain_draw_instances
       << ",\"water_sampled_cells\":" << scene_report_.water_sampled_cells
       << ",\"water_visible_cells\":" << scene_report_.water_visible_cells
       << ",\"water_draw_instances\":" << scene_report_.water_draw_instances
       << ",\"player_aircraft_vertices\":"
       << scene_report_.player_aircraft_vertices
       << ",\"player_aircraft_source_indices\":"
       << scene_report_.player_aircraft_source_indices
       << ",\"player_aircraft_draw_instances\":"
       << scene_report_.player_aircraft_draw_instances
       << ",\"player_aircraft_texture_identifier\":"
       << scene_report_.player_aircraft_texture_identifier
       << ",\"free_flight_world_complete\":"
       << json_bool(scene_report_.free_flight_world_complete)
       << ",\"complete_render_scene\":"
       << json_bool(scene_report_.complete_render_scene)
       << ",\"jv_eligible\":" << json_bool(scene_report_.jv_eligible) << "}"
       << ",\"samples\":[";
  for (std::size_t index = 0U; index < samples_.size(); ++index) {
    const Sample& sample = samples_[index];
    if (index != 0U) json << ',';
    json << "{\"tick\":" << sample.tick << ",\"control\":\""
         << kControlNames[index] << "\",\"input\":{\"pitch\":"
         << sample.input.pitch << ",\"roll\":" << sample.input.roll
         << ",\"yaw\":" << sample.input.yaw
         << ",\"throttle\":" << static_cast<unsigned>(sample.input.throttle)
         << ",\"buttons\":" << sample.input.buttons << "}"
         << ",\"snapshot_sha256\":\""
         << sha256_hex(sample.snapshot_sha256)
         << "\",\"flight_state_digest\":" << sample.flight_state_digest
         << ",\"rgba_sha256\":\"" << sha256_hex(sample.rgba_sha256)
         << "\",\"pitch_f32_bits\":" << float_bits(sample.attitude[0])
         << ",\"roll_f32_bits\":" << float_bits(sample.attitude[1])
         << ",\"yaw_f32_bits\":" << float_bits(sample.attitude[2])
         << ",\"speed_f32_bits\":" << float_bits(sample.speed)
         << ",\"changed_pixels\":" << sample.changed_pixels
         << ",\"hud_green_pixels\":" << sample.hud_green_pixels
         << ",\"luminance_min\":"
         << static_cast<unsigned>(sample.luminance_min)
         << ",\"luminance_max\":"
         << static_cast<unsigned>(sample.luminance_max)
         << ",\"semantic_effect\":" << json_bool(sample.semantic_effect)
         << ",\"visual_effect\":" << json_bool(sample.visual_effect)
         << ",\"image\":\"" << sample.image_name << "\"}";
  }
  json << "],\"five_controls_visible\":" << json_bool(eligible_)
       << ",\"eligible\":" << json_bool(eligible_) << "}\n";

  std::ofstream output(manifest_path_, std::ios::binary);
  const std::string bytes = json.str();
  if (!output || !output.write(bytes.data(),
                               static_cast<std::streamsize>(bytes.size()))) {
    return fail("manifest_write");
  }
  output.close();
  if (!output || !sha256_file(manifest_path_, manifest_sha256_)) {
    return fail("manifest_seal");
  }
  finalized_ = true;
  if (!eligible_) failure_detail_ = "qualification";
  return true;
}

}  // namespace ac6::retail
