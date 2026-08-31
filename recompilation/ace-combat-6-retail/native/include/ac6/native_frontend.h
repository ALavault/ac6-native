#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace ac6::native {

enum class MediaKind { kIso, kAssetsDirectory };

struct MediaInput final {
  std::filesystem::path path;
  MediaKind kind{};
};

// Accept exactly one existing ISO file or an existing assets directory.  The
// media remains at its supplied path; this helper never copies proprietary
// bytes into the product tree.
[[nodiscard]] std::optional<MediaInput> parse_media_argument(
    const std::filesystem::path& argument);

// Mutable user data root.  XDG_DATA_HOME wins; HOME is only a fallback and is
// never used for retail media discovery.
[[nodiscard]] std::filesystem::path xdg_data_directory(
    std::string_view application = "ac6recomp");

}  // namespace ac6::native
