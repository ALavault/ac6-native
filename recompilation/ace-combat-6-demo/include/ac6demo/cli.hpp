#pragma once

#include "ac6demo/graphics.hpp"

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>

namespace ac6demo {

void print_cli_usage(std::ostream &output);
[[nodiscard]] GraphicsBackend parse_cli_backend(std::string_view value);
[[nodiscard]] std::uint64_t parse_cli_ticks(std::string_view value);
[[nodiscard]] std::string option_value(int &index, int argc, char **argv,
                                       std::string_view option);
[[nodiscard]] std::string read_binary_file(
    const std::filesystem::path &path);
[[nodiscard]] std::string read_xam_movie_file(
    const std::filesystem::path &path);
void publish_new_file(const std::filesystem::path &path,
                      std::string_view payload);

} // namespace ac6demo
