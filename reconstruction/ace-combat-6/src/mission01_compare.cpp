#include "ac6/mission01_compare.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace ac6 {
namespace {

std::uint64_t fnv64_file(const std::filesystem::path& path, std::uint64_t& size) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return 0;
  std::uint64_t hash = 1469598103934665603ull;
  size = 0;
  std::array<char, 8192> bytes{};
  while (input) {
    input.read(bytes.data(), bytes.size());
    const std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      hash ^= static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);
      hash *= 1099511628211ull;
    }
    size += static_cast<std::uint64_t>(count);
  }
  return input.eof() ? hash : 0;
}

bool parse_u64(std::string_view text, std::uint64_t& value, int base = 10) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool verify_reference_manifest(const std::filesystem::path& directory,
                               std::uint32_t& declared_width, std::uint32_t& declared_height) {
  std::ifstream input(directory / "reference.tsv");
  if (!input) return false;
  bool version = false, mission = false, ticks = false, width = false, height = false;
  const std::array<std::string, 4> required{
      "replay.ac6rply", "checkpoints.tsv", "oracle-color.ppm", "oracle-depth.f32"};
  std::array<bool, 4> files{};
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::vector<std::string_view> fields;
    std::string_view rest(line);
    while (true) {
      const std::size_t tab = rest.find('\t');
      fields.push_back(rest.substr(0, tab));
      if (tab == std::string_view::npos) break;
      rest.remove_prefix(tab + 1);
    }
    std::uint64_t value = 0;
    if (fields.size() == 2 && fields[0] == "version" && fields[1] == "1" && !version) version = true;
    else if (fields.size() == 2 && fields[0] == "mission_id" && fields[1] == "1" && !mission) mission = true;
    else if (fields.size() == 2 && fields[0] == "ticks" && fields[1] == "1800" && !ticks) ticks = true;
    else if (fields.size() == 2 && fields[0] == "width" && !width && parse_u64(fields[1], value) && value > 0 && value <= 8192) {
      declared_width = static_cast<std::uint32_t>(value); width = true;
    } else if (fields.size() == 2 && fields[0] == "height" && !height && parse_u64(fields[1], value) && value > 0 && value <= 8192) {
      declared_height = static_cast<std::uint32_t>(value); height = true;
    } else if (fields.size() == 4 && fields[0] == "file") {
      const auto found = std::find(required.begin(), required.end(), fields[1]);
      if (found == required.end()) return false;
      const std::size_t index = static_cast<std::size_t>(found - required.begin());
      std::uint64_t expected_size = 0, expected_hash = 0, actual_size = 0;
      if (files[index] || !parse_u64(fields[2], expected_size) || expected_size == 0 ||
          !parse_u64(fields[3], expected_hash, 16) || expected_hash == 0 ||
          fnv64_file(directory / required[index], actual_size) != expected_hash ||
          actual_size != expected_size) return false;
      files[index] = true;
    } else return false;
  }
  return version && mission && ticks && width && height &&
      std::all_of(files.begin(), files.end(), [](bool present) { return present; });
}

bool parse_ppm(const std::filesystem::path& path, std::uint32_t& width,
               std::uint32_t& height, std::vector<std::uint8_t>& rgba) {
  std::ifstream input(path, std::ios::binary);
  std::string magic;
  unsigned maximum{};
  if (!(input >> magic >> width >> height >> maximum) || magic != "P6" || maximum != 255u ||
      width == 0 || height == 0 || width > 8192 || height > 8192) return false;
  input.get();
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3u);
  input.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  if (!input || input.peek() != std::char_traits<char>::eof()) return false;
  rgba.resize(static_cast<std::size_t>(width) * height * 4u);
  for (std::size_t i = 0; i < rgb.size() / 3u; ++i) {
    rgba[i * 4u] = rgb[i * 3u];
    rgba[i * 4u + 1u] = rgb[i * 3u + 1u];
    rgba[i * 4u + 2u] = rgb[i * 3u + 2u];
    rgba[i * 4u + 3u] = 255;
  }
  return true;
}

bool parse_checkpoints(const std::filesystem::path& path,
                       std::vector<Mission01Checkpoint>& checkpoints) {
  std::ifstream input(path);
  if (!input) return false;
  std::string line;
  std::vector<Mission01Checkpoint> loaded;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::replace(line.begin(), line.end(), '\t', ' ');
    Mission01Checkpoint checkpoint;
    std::istringstream fields(line);
    WorldFrame& f = checkpoint.frame;
    if (!(fields >> f.tick >> f.position_x >> f.position_y >> f.position_z >> f.pitch >> f.roll >>
          f.yaw >> f.camera_x >> f.camera_y >> f.camera_z >> f.camera_target_x >>
          f.camera_target_y >> f.camera_target_z) || !fields.eof() || f.tick == 0 ||
        !std::isfinite(f.position_x) || !std::isfinite(f.position_y) ||
        !std::isfinite(f.position_z) || !std::isfinite(f.pitch) || !std::isfinite(f.roll) ||
        !std::isfinite(f.yaw) || !std::isfinite(f.camera_x) || !std::isfinite(f.camera_y) ||
        !std::isfinite(f.camera_z) || !std::isfinite(f.camera_target_x) ||
        !std::isfinite(f.camera_target_y) || !std::isfinite(f.camera_target_z) ||
        (!loaded.empty() && loaded.back().frame.tick >= f.tick)) return false;
    loaded.push_back(checkpoint);
  }
  if (loaded.size() != 5u || loaded.back().frame.tick != 1800u) return false;
  checkpoints = std::move(loaded);
  return true;
}

float max3(float a, float b, float c) { return std::max(a, std::max(b, c)); }

float position_error(const WorldFrame& a, const WorldFrame& b) {
  const float dx = a.position_x - b.position_x;
  const float dy = a.position_y - b.position_y;
  const float dz = a.position_z - b.position_z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float orientation_error(const WorldFrame& a, const WorldFrame& b) {
  constexpr float radians_to_degrees = 57.2957795131f;
  return max3(std::abs(a.pitch - b.pitch), std::abs(a.roll - b.roll),
              std::abs(a.yaw - b.yaw)) * radians_to_degrees;
}

float camera_error(const WorldFrame& a, const WorldFrame& b) {
  return std::max({std::abs(a.camera_x - b.camera_x), std::abs(a.camera_y - b.camera_y),
                   std::abs(a.camera_z - b.camera_z),
                   std::abs(a.camera_target_x - b.camera_target_x),
                   std::abs(a.camera_target_y - b.camera_target_y),
                   std::abs(a.camera_target_z - b.camera_target_z)});
}

double luminance(const std::vector<std::uint8_t>& rgba, std::size_t pixel) {
  return 0.2126 * rgba[pixel * 4u] + 0.7152 * rgba[pixel * 4u + 1u] +
         0.0722 * rgba[pixel * 4u + 2u];
}

float global_ssim(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  const std::size_t count = a.size() / 4u;
  if (count == 0 || a.size() != b.size()) return 0.0f;
  double mean_a = 0.0, mean_b = 0.0;
  for (std::size_t i = 0; i < count; ++i) { mean_a += luminance(a, i); mean_b += luminance(b, i); }
  const double sample_count = static_cast<double>(count);
  mean_a /= sample_count;
  mean_b /= sample_count;
  double variance_a = 0.0, variance_b = 0.0, covariance = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double da = luminance(a, i) - mean_a;
    const double db = luminance(b, i) - mean_b;
    variance_a += da * da; variance_b += db * db; covariance += da * db;
  }
  const double denominator = count > 1 ? static_cast<double>(count - 1) : 1.0;
  variance_a /= denominator; variance_b /= denominator; covariance /= denominator;
  constexpr double c1 = 6.5025;
  constexpr double c2 = 58.5225;
  return static_cast<float>(((2.0 * mean_a * mean_b + c1) * (2.0 * covariance + c2)) /
      ((mean_a * mean_a + mean_b * mean_b + c1) * (variance_a + variance_b + c2)));
}

bool covered(const std::vector<std::uint8_t>& image, std::size_t pixel) {
  return image[pixel * 4u] != 0 || image[pixel * 4u + 1u] != 0 || image[pixel * 4u + 2u] != 0;
}

float coverage_iou(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  std::uint64_t intersection = 0, union_count = 0;
  for (std::size_t i = 0; i < a.size() / 4u; ++i) {
    const bool ca = covered(a, i), cb = covered(b, i);
    intersection += ca && cb; union_count += ca || cb;
  }
  return union_count == 0
      ? 1.0f
      : static_cast<float>(intersection) / static_cast<float>(union_count);
}

float depth_rmse(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || a.size() != b.size()) return std::numeric_limits<float>::infinity();
  double squared = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return std::numeric_limits<float>::infinity();
    const double delta = static_cast<double>(a[i]) - b[i];
    squared += delta * delta;
  }
  return static_cast<float>(
      std::sqrt(squared / static_cast<double>(a.size())));
}

bool write_diff(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
                const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (std::size_t i = 0; i < a.size() / 4u; ++i) {
    const std::array<unsigned char, 3> pixel{
      static_cast<unsigned char>(std::abs(int(a[i * 4u]) - int(b[i * 4u]))),
      static_cast<unsigned char>(std::abs(int(a[i * 4u + 1u]) - int(b[i * 4u + 1u]))),
      static_cast<unsigned char>(std::abs(int(a[i * 4u + 2u]) - int(b[i * 4u + 2u])))};
    output.write(reinterpret_cast<const char*>(pixel.data()), pixel.size());
  }
  return static_cast<bool>(output);
}

bool write_depth(const std::filesystem::path& path, const std::vector<float>& depth) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(reinterpret_cast<const char*>(depth.data()),
               static_cast<std::streamsize>(depth.size() * sizeof(float)));
  return static_cast<bool>(output);
}

}  // namespace

bool Mission01Reference::load(const std::filesystem::path& directory) {
  Mission01Reference loaded;
  std::uint32_t declared_width = 0, declared_height = 0;
  if (directory.empty() || !verify_reference_manifest(directory, declared_width, declared_height) ||
      !loaded.replay_.read_file(directory / "replay.ac6rply") ||
      loaded.replay_.frames().size() != 1800u ||
      !parse_checkpoints(directory / "checkpoints.tsv", loaded.checkpoints_) ||
      !parse_ppm(directory / "oracle-color.ppm", loaded.width_, loaded.height_, loaded.oracle_rgba_)) {
    return false;
  }
  if (loaded.width_ != declared_width || loaded.height_ != declared_height) return false;
  const auto depth_path = directory / "oracle-depth.f32";
  std::ifstream depth(depth_path, std::ios::binary | std::ios::ate);
  const std::size_t count = static_cast<std::size_t>(loaded.width_) * loaded.height_;
  if (!depth || depth.tellg() != static_cast<std::streamoff>(count * sizeof(float))) return false;
  depth.seekg(0);
  loaded.oracle_depth_.resize(count);
  depth.read(reinterpret_cast<char*>(loaded.oracle_depth_.data()),
             static_cast<std::streamsize>(count * sizeof(float)));
  if (!depth || std::any_of(loaded.oracle_depth_.begin(), loaded.oracle_depth_.end(),
                            [](float value) { return !std::isfinite(value) || value < 0.0f || value > 1.0f; })) {
    return false;
  }
  *this = std::move(loaded);
  return true;
}

Mission01ComparisonResult Mission01Comparator::compare(
    const Mission01Reference& reference, const std::vector<WorldFrame>& observed,
    const NativeRenderTarget& target, bool deterministic,
    const std::filesystem::path& output_directory) const {
  Mission01ComparisonResult result;
  result.deterministic = deterministic;
  if (observed.size() != reference.checkpoints().size()) result.failure = "checkpoint_count";
  for (std::size_t i = 0; i < std::min(observed.size(), reference.checkpoints().size()); ++i) {
    const WorldFrame& expected = reference.checkpoints()[i].frame;
    const WorldFrame& actual = observed[i];
    if (actual.tick != expected.tick) { result.failure = "checkpoint_tick"; break; }
    result.maximum_position_error = std::max(result.maximum_position_error, position_error(actual, expected));
    result.maximum_orientation_error = std::max(result.maximum_orientation_error, orientation_error(actual, expected));
    result.maximum_camera_error = std::max(result.maximum_camera_error, camera_error(actual, expected));
    if (i > 0) {
      const WorldFrame& previous_expected = reference.checkpoints()[i - 1].frame;
      const WorldFrame& previous_actual = observed[i - 1];
      const float seconds = static_cast<float>(expected.tick - previous_expected.tick) / 60.0f;
      const float dx = (actual.position_x - previous_actual.position_x) -
                       (expected.position_x - previous_expected.position_x);
      const float dy = (actual.position_y - previous_actual.position_y) -
                       (expected.position_y - previous_expected.position_y);
      const float dz = (actual.position_z - previous_actual.position_z) -
                       (expected.position_z - previous_expected.position_z);
      result.maximum_velocity_error = std::max(result.maximum_velocity_error,
          std::sqrt(dx * dx + dy * dy + dz * dz) / seconds);
    }
  }
  result.simulation_pass = result.failure.empty() &&
      result.maximum_position_error <= thresholds_.position_m &&
      result.maximum_orientation_error <= thresholds_.orientation_deg &&
      result.maximum_camera_error <= thresholds_.camera_absolute &&
      result.maximum_velocity_error <= thresholds_.velocity_mps;
  if (!result.simulation_pass && result.failure.empty()) result.failure = "simulation_threshold";

  std::vector<std::uint8_t> native_rgba;
  std::vector<float> native_depth;
  if (target.width() != reference.width() || target.height() != reference.height() ||
      !target.copy_rgba8(native_rgba) || !target.copy_depth(native_depth)) {
    if (result.failure.empty()) result.failure = "render_dimensions";
    return result;
  }
  result.color_ssim = global_ssim(native_rgba, reference.oracle_rgba());
  result.coverage_iou = coverage_iou(native_rgba, reference.oracle_rgba());
  result.depth_rmse = depth_rmse(native_depth, reference.oracle_depth());
  result.render_pass = result.color_ssim >= thresholds_.minimum_ssim &&
      result.coverage_iou >= thresholds_.minimum_coverage_iou &&
      result.depth_rmse <= thresholds_.maximum_depth_rmse;
  if (!result.render_pass && result.failure.empty()) result.failure = "render_threshold";
  if (!deterministic && result.failure.empty()) result.failure = "nondeterministic_replay";

  std::error_code error;
  std::filesystem::create_directories(output_directory, error);
  if (error || !target.write_ppm(output_directory / "native-color.ppm") ||
      !write_depth(output_directory / "native-depth.f32", native_depth) ||
      !write_diff(output_directory / "color-diff.ppm", target.width(), target.height(),
                  native_rgba, reference.oracle_rgba())) {
    result.render_pass = false;
    result.failure = "artifact_write";
    return result;
  }
  std::ofstream report(output_directory / "comparison.json", std::ios::trunc);
  if (!report) { result.render_pass = false; result.failure = "artifact_write"; return result; }
  report << std::boolalpha << std::setprecision(9)
         << "{\n  \"passed\": " << result.passed()
         << ",\n  \"deterministic\": " << result.deterministic
         << ",\n  \"simulation_pass\": " << result.simulation_pass
         << ",\n  \"render_pass\": " << result.render_pass
         << ",\n  \"maximum_position_error_m\": " << result.maximum_position_error
         << ",\n  \"maximum_orientation_error_deg\": " << result.maximum_orientation_error
         << ",\n  \"maximum_camera_error\": " << result.maximum_camera_error
         << ",\n  \"maximum_velocity_error_mps\": " << result.maximum_velocity_error
         << ",\n  \"color_ssim\": " << result.color_ssim
         << ",\n  \"coverage_iou\": " << result.coverage_iou
         << ",\n  \"depth_rmse\": " << result.depth_rmse
         << ",\n  \"failure\": \"" << result.failure << "\"\n}\n";
  return result;
}

}  // namespace ac6
