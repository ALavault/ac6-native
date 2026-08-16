#pragma once

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ac6demo {

struct QualifiedFile final {
  std::string_view name;
  std::uintmax_t size;
  std::string_view sha256;
};

[[nodiscard]] inline const auto& qualified_files() {
  static const std::array files = {
      QualifiedFile{"Default.xex", 1454080U,
                    "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"},
      QualifiedFile{"DATA.TBL", 13784U,
                    "0d9e11cf19881971e7d14c0077e9e719c1795e0316afab4b48b153351591eef8"},
      QualifiedFile{"DATA00.PAC", 177340416U,
                    "838356ade0f41fc7eee11684dda8e4d6c07eac7512a23ef1d148eb3144dbb162"},
      QualifiedFile{"DATA01.PAC", 15138816U,
                    "08ef13fe61caf0b072a4de6de577e965b4f1c8feb88d638ffc099dd4d63238d3"},
      QualifiedFile{"bgmpack.bin", 66455552U,
                    "1ff738c781eeffb510295fc539fc98aeb298040b4ea5965f8eb628bfe1365a8b"},
      QualifiedFile{"demopack_eng.bin", 11427840U,
                    "b2e7a883951a1a95484a98829beaf5381d4e68c5a7bf2a2a0847857365f1cfad"},
      QualifiedFile{"demopack_jpn.bin", 11675648U,
                    "b35049450612e8ca9b75e29b8318508b81a46c173fdd626de3de39c1df2ff306"},
      QualifiedFile{"voicepack_eng.bin", 16988160U,
                    "f482d54d8314c4eae8c3ab20cda40a6c539c96cd933dcfde3c41f03b33e121ba"},
      QualifiedFile{"voicepack_jpn.bin", 21876736U,
                    "e67c7add7e4dde4297ae87ec0ca30e6e91333d32511b6fd12e8f2d01e4c987b8"}};
  return files;
}

inline constexpr std::string_view kQualifiedXexSha256 =
    "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8";

[[nodiscard]] bool verify_qualified_file(const std::filesystem::path& path,
                                         const QualifiedFile& expected,
                                         std::string* failure = nullptr);

class DemoStore final {
 public:
  static bool import_directory(const std::filesystem::path& source,
                               const std::filesystem::path& destination,
                               std::string* failure = nullptr);
  static bool verify(const std::filesystem::path& store,
                     std::string* failure = nullptr);
  [[nodiscard]] static std::filesystem::path default_path();
};

class VfsMount final {
 public:
  explicit VfsMount(std::filesystem::path store);

  [[nodiscard]] const std::filesystem::path& store() const noexcept { return store_; }
  [[nodiscard]] std::optional<std::filesystem::path> resolve_if_qualified(
      std::string_view xbox_path) const;
  [[nodiscard]] std::filesystem::path resolve(std::string_view xbox_path) const;

 private:
  std::filesystem::path store_;
};

}  // namespace ac6demo
