#include "ac6/product_runtime.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using Loader = std::function<bool(const std::filesystem::path&)>;
using PreservesState = std::function<bool()>;

bool write_text(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary);
  if (!output) return false;
  output << text;
  return static_cast<bool>(output);
}

std::uint64_t fnv64(const std::string& bytes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

bool expect_transaction(const std::filesystem::path& manifest,
                        const std::string& valid,
                        const std::string& invalid_line,
                        const Loader& load,
                        const PreservesState& preserves) {
  const auto fail = [&](const char* phase) {
    std::fprintf(stderr, "manifest transaction failed: %s (%s)\n", manifest.filename().c_str(), phase);
    return false;
  };
  if (!write_text(manifest, valid)) return fail("write-valid");
  if (!load(manifest)) return fail("load-valid");
  if (!preserves()) return fail("preserve-valid");
  if (!write_text(manifest, valid + invalid_line)) return fail("write-invalid");
  if (load(manifest)) return fail("accepted-invalid");
  if (!preserves()) return fail("lost-after-invalid");
  if (!write_text(manifest, valid + valid)) return fail("write-duplicate");
  if (load(manifest)) return fail("accepted-duplicate");
  if (!preserves()) return fail("lost-after-duplicate");
  return true;
}

bool test_manifest_families(const std::filesystem::path& root) {
  const auto path = [&root](const char* name) { return root / name; };

  ac6::MissionRenderDatabase renders;
  if (!expect_transaction(path("render.tsv"), "1\t9\n", "bad\n",
                          [&renders](const auto& file) { return renders.load_manifest(file); },
                          [&renders] { return renders.find(1) != nullptr; })) return false;

  ac6::MissionDrawableDatabase drawables;
  if (!expect_transaction(path("drawable.tsv"),
                          "1\tstable\tmesh\t9\t1\tbuffer\t1\t3\thash\n", "1\tbad\n",
                          [&drawables](const auto& file) { return drawables.load_manifest(file); },
                          [&drawables] { return drawables.find(1, "stable") != nullptr; })) return false;

  ac6::MissionTransformDatabase transforms;
  if (!expect_transaction(path("transform.tsv"), "1\tstable\t0\t0\t0\t1\t1\t1\n",
                          "1\tstable\t0\t0\t0\t1\t1\t0\n",
                          [&transforms](const auto& file) { return transforms.load_manifest(file); },
                          [&transforms] { return transforms.find(1, "stable") != nullptr; })) return false;

  ac6::MissionMaterialDatabase materials;
  if (!expect_transaction(path("material.tsv"),
                          "1\tstable\tshader\t1\t1\topaque\t0xFF112233\n",
                          "1\tstable\tshader\t1\t1\tbad\t0xFF112233\n",
                          [&materials](const auto& file) { return materials.load_manifest(file); },
                          [&materials] { return materials.find(1, "stable") != nullptr; })) return false;

  ac6::ShaderPermutationDatabase shaders;
  if (!expect_transaction(path("shader.tsv"), "shader\tpos\t1\t1\trgba8\n",
                          "shader\tpos\tbad\t1\trgba8\n",
                          [&shaders](const auto& file) { return shaders.load_manifest(file); },
                          [&shaders] { return shaders.find("shader") != nullptr; })) return false;

  ac6::MissionRenderTargetDatabase targets;
  if (!expect_transaction(path("target.tsv"), "1\tmain_color\t32\t32\t1\trgba8\tnone\t0\n",
                          "1\tmain_color\t32\t32\t3\trgba8\tnone\t0\n",
                          [&targets](const auto& file) { return targets.load_manifest(file); },
                          [&targets] { return targets.find(1, "main_color") != nullptr; })) return false;

  ac6::MissionRenderPassDatabase passes;
  if (!expect_transaction(path("pass.tsv"),
                          "1\tworld\t1\tmain_color\tnone\t0x00000000\t1\n",
                          "1\tworld\t1\tbad\tnone\t0x00000000\t1\n",
                          [&passes](const auto& file) { return passes.load_manifest(file); },
                          [&passes] { return passes.find(1, "world") != nullptr; })) return false;

  ac6::MissionRenderResolveDatabase resolves;
  if (!expect_transaction(path("resolve.tsv"), "1\tworld\tmain_color\tpresent\tcopy\n",
                          "1\tworld\tmain_color\tother\tcopy\n",
                          [&resolves](const auto& file) { return resolves.load_manifest(file); },
                          [&resolves] { return resolves.find(1, "world") != nullptr; })) return false;

  std::string image = "P6\n1 1\n255\n";
  image.push_back(static_cast<char>(0xFF));
  image.push_back('\0');
  image.push_back('\0');
  const auto image_path = path("texture.ppm");
  if (!write_text(image_path, image)) return false;
  const std::string texture_line = "1\tstable\ttex\tnearest\twrap\t0x" +
      [&] { const auto value = fnv64(image); std::stringstream stream; stream << std::hex << value; return stream.str(); }() +
      "\ttexture.ppm\t" + std::to_string(image.size()) + "\n";
  ac6::MissionTextureDatabase textures;
  if (!expect_transaction(path("texture.tsv"), texture_line,
                          "1\tstable\ttex\tbad\twrap\t0x1\n",
                          [&textures](const auto& file) { return textures.load_manifest(file); },
                          [&textures] {
                            std::uint32_t rgba = 0;
                            return textures.sample(1, "stable", 0.5f, 0.5f, rgba) && rgba == 0xFFFF0000u;
                          })) return false;

  const std::string bytes = "abc";
  if (!write_text(path("buffer.bin"), bytes)) return false;
  const std::string buffer_line = "buffer\tbuffer.bin\t3\t" + std::to_string(fnv64(bytes)) + "\n";
  ac6::QualifiedBufferDatabase buffers;
  if (!expect_transaction(path("buffer.tsv"), buffer_line, "buffer\tbuffer.bin\tbad\t1\n",
                          [&buffers](const auto& file) { return buffers.load_manifest(file); },
                          [&buffers] { return buffers.find("buffer") != nullptr; })) return false;
  return true;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
      ("ac6-manifest-transaction-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(root, error);
  if (error) return 1;
  const bool passed = test_manifest_families(root);
  std::filesystem::remove_all(root, error);
  return passed && !error ? 0 : 1;
}
