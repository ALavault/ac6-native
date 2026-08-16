#include <rex/graphics/pipeline/shader/shader.h>
#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/string/buffer.h>

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <span>
#include <string>
#include <vector>

namespace {

struct Options {
  rex::graphics::xenos::ShaderType stage;
  std::filesystem::path input;
  std::filesystem::path spirv;
  std::filesystem::path disassembly;
  std::string expected_sha256;
  std::uint32_t register_count = 0;
  bool register_count_auto = false;
  bool analysis_only = false;
};

Options parse_options(int argc, char **argv) {
  if (argc != 13 && argc != 14) {
    throw std::runtime_error(
        "usage: rexglue-shader-cli --stage vertex|pixel --input FILE "
        "--spirv FILE --disassembly FILE --expected-sha256 SHA256 "
        "--register-count auto|N [--analysis-only]");
  }
  Options options{};
  bool have_stage = false;
  for (int index = 1; index < argc;) {
    const std::string key(argv[index]);
    if (key == "--analysis-only") {
      options.analysis_only = true;
      ++index;
      continue;
    }
    if (index + 1 >= argc) {
      throw std::runtime_error("option requires a value: " + key);
    }
    const std::string value(argv[index + 1]);
    if (key == "--stage") {
      if (value == "vertex") {
        options.stage = rex::graphics::xenos::ShaderType::kVertex;
      } else if (value == "pixel") {
        options.stage = rex::graphics::xenos::ShaderType::kPixel;
      } else {
        throw std::runtime_error("stage must be vertex or pixel");
      }
      have_stage = true;
    } else if (key == "--input") {
      options.input = value;
    } else if (key == "--spirv") {
      options.spirv = value;
    } else if (key == "--disassembly") {
      options.disassembly = value;
    } else if (key == "--expected-sha256") {
      if (value.size() != 64 || !std::ranges::all_of(value, [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
          })) {
        throw std::runtime_error("expected SHA-256 must be 64 lowercase hex digits");
      }
      options.expected_sha256 = value;
    } else if (key == "--register-count") {
      if (value == "auto") {
        options.register_count_auto = true;
      } else {
        std::size_t parsed = 0;
        const unsigned long count = std::stoul(value, &parsed, 10);
        if (parsed != value.size() || count == 0 || count > 64) {
          throw std::runtime_error("register count must be auto or in [1,64]");
        }
        options.register_count = static_cast<std::uint32_t>(count);
      }
    } else {
      throw std::runtime_error("unknown option: " + key);
    }
    index += 2;
  }
  if (!have_stage || options.input.empty() || options.spirv.empty() ||
      options.disassembly.empty() || options.expected_sha256.empty() ||
      (!options.register_count_auto && options.register_count == 0)) {
    throw std::runtime_error("all options are required");
  }
  const char *temporary_root = std::getenv("TMPDIR");
  if (!temporary_root || !*temporary_root) {
    throw std::runtime_error("TMPDIR must be set");
  }
  const auto root = std::filesystem::weakly_canonical(temporary_root);
  for (const auto &path : {options.input, options.spirv, options.disassembly}) {
    const auto parent = std::filesystem::weakly_canonical(path.parent_path());
    const auto mismatch = std::mismatch(root.begin(), root.end(), parent.begin(), parent.end());
    if (mismatch.first != root.end()) {
      throw std::runtime_error("all shader artefacts must stay under TMPDIR");
    }
  }
  if (options.spirv == options.disassembly || options.input == options.spirv ||
      options.input == options.disassembly) {
    throw std::runtime_error("input and output paths must not collide");
  }
  return options;
}

std::vector<std::uint32_t> read_microcode(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) {
    throw std::runtime_error("cannot open microcode");
  }
  const auto size = stream.tellg();
  if (size <= 0 || size > 4096 || size % 12 != 0) {
    throw std::runtime_error("microcode must be 1..4096 bytes in 3-dword blocks");
  }
  stream.seekg(0);
  std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / 4U);
  if (!stream.read(reinterpret_cast<char *>(words.data()), size)) {
    throw std::runtime_error("short microcode read");
  }
  return words;
}

std::string sha256(std::span<const std::uint32_t> words) {
  std::array<unsigned char, 32> digest{};
  unsigned int digest_size = 0;
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (!context || EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context, words.data(), words.size_bytes()) != 1 ||
      EVP_DigestFinal_ex(context, digest.data(), &digest_size) != 1 ||
      digest_size != digest.size()) {
    EVP_MD_CTX_free(context);
    throw std::runtime_error("SHA-256 failed");
  }
  EVP_MD_CTX_free(context);
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    output << std::setw(2) << static_cast<unsigned int>(byte);
  }
  return output.str();
}

void write_file(const std::filesystem::path &path, const void *data,
                std::size_t size) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream || !stream.write(static_cast<const char *>(data),
                               static_cast<std::streamsize>(size))) {
    throw std::runtime_error("failed to write output");
  }
}

} // namespace

int main(int argc, char **argv) {
  try {
    const Options options = parse_options(argc, argv);
    const auto words = read_microcode(options.input);
    const std::string digest = sha256(words);
    if (digest != options.expected_sha256) {
      throw std::runtime_error("shader SHA-256 identity mismatch");
    }
    rex::graphics::Shader shader(options.stage, 0, words.data(), words.size(),
                                 std::endian::big);
    rex::string::StringBuffer disassembly_buffer;
    shader.AnalyzeUcode(disassembly_buffer);

    const std::uint32_t static_register_bound =
        shader.register_static_address_bound();
    const bool dynamic_register_addressing =
        shader.uses_register_dynamic_addressing();
    const std::uint32_t point_size_edge_flag_kill_vertex_mask =
        shader.writes_point_size_edge_flag_kill_vertex();
    const auto &disassembly = shader.ucode_disassembly();
    write_file(options.disassembly, disassembly.data(), disassembly.size());
    if (options.analysis_only) {
      std::cout << "stage="
                << (options.stage == rex::graphics::xenos::ShaderType::kVertex
                        ? "vertex"
                        : "pixel")
                << " dwords=" << words.size() << " sha256=" << digest
                << " static_register_bound=" << static_register_bound
                << " dynamic_register_addressing="
                << (dynamic_register_addressing ? "true" : "false")
                << " point_size_edge_flag_kill_vertex_mask="
                << point_size_edge_flag_kill_vertex_mask
                << " translation=not_requested\n";
      return 0;
    }
    if (options.register_count_auto && dynamic_register_addressing) {
      std::cout << "stage="
                << (options.stage == rex::graphics::xenos::ShaderType::kVertex
                        ? "vertex"
                        : "pixel")
                << " dwords=" << words.size() << " sha256=" << digest
                << " static_register_bound=" << static_register_bound
                << " dynamic_register_addressing=true translation=blocked\n";
      return 4;
    }
    const std::uint32_t modification_register_count =
        options.register_count_auto ? 0U : options.register_count;

    const rex::graphics::SpirvShaderTranslator::Features features(false);
    rex::graphics::SpirvShaderTranslator translator(features, false, false,
                                                     false, 1, 1);
    const std::uint64_t modification =
        options.stage == rex::graphics::xenos::ShaderType::kVertex
            ? translator.GetDefaultVertexShaderModification(
                  modification_register_count)
            : translator.GetDefaultPixelShaderModification(
                  modification_register_count);
    auto *translation = shader.GetOrCreateTranslation(modification);
    if (!translator.TranslateAnalyzedShader(*translation) ||
        !translation->is_valid() || translation->translated_binary().empty()) {
      std::cerr << "translation failed with " << translation->errors().size()
                << " error(s)\n";
      return 3;
    }
    const auto &spirv = translation->translated_binary();
    write_file(options.spirv, spirv.data(), spirv.size());
    std::cout << "stage="
              << (options.stage == rex::graphics::xenos::ShaderType::kVertex
                      ? "vertex"
                      : "pixel")
              << " dwords=" << words.size() << " spirv_bytes=" << spirv.size()
              << " sha256=" << digest
              << " static_register_bound=" << static_register_bound
              << " dynamic_register_addressing="
              << (dynamic_register_addressing ? "true" : "false")
              << " point_size_edge_flag_kill_vertex_mask="
              << point_size_edge_flag_kill_vertex_mask
              << " modification_register_count=" << modification_register_count
              << " translation=pass\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "fail-closed: " << error.what() << "\n";
    return 2;
  }
}
