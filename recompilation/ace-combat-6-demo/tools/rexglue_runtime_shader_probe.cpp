#include "ac6demo/rexglue_runtime_shader.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool under(const std::filesystem::path &root,
           const std::filesystem::path &path) {
  const auto relative = std::filesystem::weakly_canonical(path).lexically_relative(
      std::filesystem::weakly_canonical(root));
  return !relative.empty() && *relative.begin() != "..";
}

ac6demo::XenosShaderLoadCommand read_shader(const std::filesystem::path &path,
                                             std::string stage) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    throw std::runtime_error("cannot open runtime shader input");
  }
  const auto byte_count = stream.tellg();
  if (byte_count <= 0 || byte_count > 4096 || byte_count % 12 != 0) {
    throw std::runtime_error("runtime shader input size is invalid");
  }
  stream.seekg(0);
  std::vector<unsigned char> bytes(static_cast<std::size_t>(byte_count));
  if (!stream.read(reinterpret_cast<char *>(bytes.data()), byte_count)) {
    throw std::runtime_error("runtime shader input is truncated");
  }
  ac6demo::XenosShaderLoadCommand shader;
  shader.stage = stage == "vertex" ? ac6demo::XenosShaderStage::Vertex
                                    : ac6demo::XenosShaderStage::Pixel;
  shader.size_dwords = static_cast<std::uint16_t>(bytes.size() / 4U);
  shader.guest_big_endian_dwords.reserve(shader.size_dwords);
  for (std::size_t offset = 0; offset < bytes.size(); offset += 4U) {
    shader.guest_big_endian_dwords.push_back(
        (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
        (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
        static_cast<std::uint32_t>(bytes[offset + 3U]));
  }
  return shader;
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc != 6) {
      throw std::runtime_error(
          "usage: probe vertex|pixel REGISTER_COUNT INPUT SHA256 OUTPUT");
    }
    const std::string stage(argv[1]);
    if (stage != "vertex" && stage != "pixel") {
      throw std::runtime_error("runtime shader stage is invalid");
    }
    const auto register_count = static_cast<std::uint32_t>(std::stoul(argv[2]));
    const char *temporary = std::getenv("TMPDIR");
    if (temporary == nullptr || !under(temporary, argv[3]) ||
        !under(temporary, argv[5])) {
      throw std::runtime_error("runtime shader files must stay under TMPDIR");
    }
    auto shader = read_shader(argv[3], stage);
    shader.guest_big_endian_sha256 = argv[4];
    const auto result =
        ac6demo::translate_reached_shader_spirv(shader, register_count);
    std::ofstream output(argv[5], std::ios::binary | std::ios::trunc);
    if (!output.write(reinterpret_cast<const char *>(result.words.data()),
                      static_cast<std::streamsize>(result.words.size() * 4U))) {
      throw std::runtime_error("cannot write temporary runtime SPIR-V");
    }
    std::cout << "microcode=" << result.microcode_sha256
              << " spirv=" << result.spirv_sha256
              << " bytes=" << result.words.size() * 4U << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "fail-closed: " << error.what() << '\n';
    return 2;
  }
}
