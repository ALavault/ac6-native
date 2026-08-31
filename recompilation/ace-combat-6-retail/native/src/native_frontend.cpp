#include "ac6/native_frontend.h"

#include <cstdlib>
#include <string>

namespace ac6::native {

std::optional<MediaInput> parse_media_argument(
    const std::filesystem::path& argument) {
  std::error_code error;
  if (std::filesystem::is_directory(argument, error) && !error) {
    return MediaInput{argument, MediaKind::kAssetsDirectory};
  }
  if (!std::filesystem::is_regular_file(argument, error) || error) {
    return std::nullopt;
  }
  std::string extension = argument.extension().string();
  for (char& character : extension) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  if (extension != ".iso") return std::nullopt;
  return MediaInput{argument, MediaKind::kIso};
}

std::filesystem::path xdg_data_directory(std::string_view application) {
  if (application.empty() || application == "." || application == ".." ||
      application.find('/') != std::string_view::npos ||
      application.find('\\') != std::string_view::npos) {
    application = "ac6recomp";
  }
  const char* xdg = std::getenv("XDG_DATA_HOME");
  std::filesystem::path base;
  if (xdg != nullptr && *xdg != '\0') {
    base = xdg;
  } else {
    const char* home = std::getenv("HOME");
    base = (home != nullptr && *home != '\0')
               ? std::filesystem::path(home) / ".local/share"
               : std::filesystem::current_path();
  }
  return (base / std::string(application)).lexically_normal();
}

}  // namespace ac6::native
