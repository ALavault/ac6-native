#include "ac6/mission01_compare.h"

#include <chrono>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {
void require(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", line, expression);
    std::abort();
  }
}
#define REQUIRE(value) require((value), #value, __LINE__)

std::pair<std::uint64_t, std::uint64_t> file_identity(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(static_cast<bool>(input));
  std::uint64_t hash = 1469598103934665603ull;
  std::uint64_t size = 0;
  char byte{};
  while (input.get(byte)) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
    ++size;
  }
  REQUIRE(input.eof());
  return {size, hash};
}
}  // namespace

int main() {
  const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path root = std::filesystem::temp_directory_path() /
      ("ac6-mission01-compare-" + std::to_string(suffix));
  const std::filesystem::path reference_dir = root / "reference";
  const std::filesystem::path output_dir = root / "output";
  REQUIRE(std::filesystem::create_directories(reference_dir));

  ac6::ReplayLog replay;
  for (std::size_t i = 0; i < 1800; ++i) replay.append({});
  REQUIRE(replay.write_file(reference_dir / "replay.ac6rply"));
  {
    std::ofstream checkpoints(reference_dir / "checkpoints.tsv");
    REQUIRE(static_cast<bool>(checkpoints));
    checkpoints << "# tick position xyz orientation camera camera_target\n";
    for (const std::uint64_t tick : {1u, 60u, 300u, 900u, 1800u}) {
      checkpoints << tick << "\t0\t0\t0\t0\t0\t0\t-12\t3\t12\t0\t0\t0\n";
    }
  }
  ac6::NativeRenderTarget target;
  REQUIRE(target.resize(4, 2));
  REQUIRE(target.clear(0xFF204060u, 0.5f));
  REQUIRE(target.write_ppm(reference_dir / "oracle-color.ppm"));
  std::vector<float> depth;
  REQUIRE(target.copy_depth(depth));
  {
    std::ofstream output(reference_dir / "oracle-depth.f32", std::ios::binary);
    REQUIRE(static_cast<bool>(output));
    output.write(reinterpret_cast<const char*>(depth.data()),
                 static_cast<std::streamsize>(depth.size() * sizeof(float)));
    REQUIRE(static_cast<bool>(output));
  }
  {
    std::ofstream manifest(reference_dir / "reference.tsv");
    REQUIRE(static_cast<bool>(manifest));
    manifest << "version\t1\nmission_id\t1\nticks\t1800\nwidth\t4\nheight\t2\n";
    for (const char* name : {"replay.ac6rply", "checkpoints.tsv", "oracle-color.ppm",
                             "oracle-depth.f32"}) {
      const auto [size, hash] = file_identity(reference_dir / name);
      manifest << "file\t" << name << '\t' << size << '\t' << std::hex << hash << std::dec << '\n';
    }
  }

  ac6::Mission01Reference reference;
  REQUIRE(reference.load(reference_dir));
  REQUIRE(reference.replay().frames().size() == 1800);
  REQUIRE(reference.checkpoints().size() == 5);
  std::vector<ac6::WorldFrame> observed;
  for (const ac6::Mission01Checkpoint& checkpoint : reference.checkpoints()) {
    observed.push_back(checkpoint.frame);
  }
  const ac6::Mission01ComparisonResult pass =
      ac6::Mission01Comparator{}.compare(reference, observed, target, true, output_dir);
  REQUIRE(pass.passed());
  REQUIRE(pass.color_ssim >= 0.999f);
  REQUIRE(pass.coverage_iou == 1.0f);
  REQUIRE(pass.depth_rmse == 0.0f);
  REQUIRE(pass.first_divergence_domain.empty());
  REQUIRE(std::filesystem::is_regular_file(output_dir / "native-color.ppm"));
  REQUIRE(std::filesystem::is_regular_file(output_dir / "color-diff.ppm"));
  REQUIRE(std::filesystem::is_regular_file(output_dir / "native-depth.f32"));
  REQUIRE(std::filesystem::is_regular_file(output_dir / "comparison.json"));

  observed.back().position_x = 2.0f;
  const ac6::Mission01ComparisonResult fail =
      ac6::Mission01Comparator{}.compare(reference, observed, target, true, output_dir / "failed");
  REQUIRE(!fail.passed());
  REQUIRE(!fail.simulation_pass);
  REQUIRE(fail.failure == "simulation_threshold");
  REQUIRE(fail.first_divergence_checkpoint == 4);
  REQUIRE(fail.first_divergence_tick == 1800);
  REQUIRE(fail.first_divergence_domain == "position");

  observed = {};
  for (const ac6::Mission01Checkpoint& checkpoint : reference.checkpoints()) {
    observed.push_back(checkpoint.frame);
  }
  observed[1].camera_x = 1.0f;
  observed.back().position_x = 2.0f;
  const ac6::Mission01ComparisonResult first_fail =
      ac6::Mission01Comparator{}.compare(reference, observed, target, true,
                                         output_dir / "first-failed");
  REQUIRE(!first_fail.passed());
  REQUIRE(first_fail.first_divergence_checkpoint == 1);
  REQUIRE(first_fail.first_divergence_tick == 60);
  REQUIRE(first_fail.first_divergence_domain == "camera");

  std::filesystem::remove(reference_dir / "oracle-depth.f32");
  ac6::Mission01Reference incomplete;
  REQUIRE(!incomplete.load(reference_dir));

  if (const char* real_ndxr = std::getenv("AC6_REAL_NDXR")) {
    const std::filesystem::path path(real_ndxr);
    const auto [size, hash] = file_identity(path);
    ac6::QualifiedBufferDatabase real_buffers;
    REQUIRE(real_buffers.add({"real-010", path, size, hash, false}));
    REQUIRE(real_buffers.verify("real-010"));
    ac6::NativeGeometryDatabase real_geometry;
    const bool f16 = path.filename().string().find("m16") != std::string::npos;
    const ac6::MissionDrawable real_drawable{
        1, "real-010", f16 ? "aircraft" : "terrain", f16 ? 9u : 119u,
        f16 ? 12u : 21u, "real-010", f16 ? 4435u : 1300u,
        f16 ? 6468u : 1626u, "retail-010"};
    REQUIRE(real_geometry.load_verified(real_drawable, real_buffers));
    REQUIRE(real_geometry.find("real-010") != nullptr);
    REQUIRE(real_geometry.find("real-010")->source_format == "NDXR_BE");
    REQUIRE(real_geometry.decoded("real-010") != nullptr);
    REQUIRE(real_geometry.decoded("real-010")->vertices.size() == (f16 ? 4435u : 1300u));
    REQUIRE(!real_geometry.decoded("real-010")->indices.empty());
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  REQUIRE(!error);
  return 0;
}
