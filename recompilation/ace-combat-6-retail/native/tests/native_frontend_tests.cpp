#include "ac6/native_frontend.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
  const auto root = std::filesystem::temp_directory_path() / "ac6-native-frontend-test";
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  std::filesystem::create_directories(root / "assets", ignored);
  std::ofstream(root / "game.ISO").put('x');
  std::ofstream(root / "game.bin").put('x');
  const auto iso = ac6::native::parse_media_argument(root / "game.ISO");
  assert(iso.has_value() && iso->kind == ac6::native::MediaKind::kIso);
  const auto assets = ac6::native::parse_media_argument(root / "assets");
  assert(assets.has_value() &&
         assets->kind == ac6::native::MediaKind::kAssetsDirectory);
  assert(!ac6::native::parse_media_argument(root / "game.bin").has_value());
  assert(!ac6::native::parse_media_argument(root / "missing.iso").has_value());
  const auto data = ac6::native::xdg_data_directory();
  assert(data.filename() == "ac6recomp");
  assert(ac6::native::xdg_data_directory("../escape").filename() == "ac6recomp");
  std::filesystem::remove_all(root, ignored);
  return 0;
}
