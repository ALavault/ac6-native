#include "ac6demo/cli.hpp"

#include "ac6xbox360/xam_input_movie.hpp"

#include <cerrno>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace ac6demo {

void print_cli_usage(std::ostream &output) {
  output << "ac6-demo-recomp import <directory> [--store <directory>]\n"
         << "ac6-demo-recomp play [--store <directory>] [--trace <file>] "
            "[--backend headless|vulkan] [--ticks N] "
            "[--xam-movie-record <file>]\n"
         << "ac6-demo-recomp replay <AC6RTPLY-v4> [--store <directory>] "
            "[--backend headless|vulkan] [--xam-movie-replay <file>]\n"
         << "ac6-demo-recomp probe --store <directory> "
            "--until frontend|mission|terminal --max-ticks N --trace <file> "
            "--report <file> --backend headless|vulkan "
            "[--input-at tick,buttons,lt,rt,lx,ly,rx,ry,connected]... "
            "[--xam-movie-record <file> | --xam-movie-replay <file>] "
            "[--atlas <file>]\n";
}

GraphicsBackend parse_cli_backend(std::string_view value) {
  if (value == "headless") return GraphicsBackend::Headless;
  if (value == "vulkan") return GraphicsBackend::Vulkan;
  throw std::invalid_argument("backend must be headless or vulkan");
}

std::uint64_t parse_cli_ticks(std::string_view value) {
  std::uint64_t result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                      result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
    throw std::invalid_argument("ticks must be an unsigned integer");
  }
  return result;
}

std::string option_value(int &index, int argc, char **argv,
                         std::string_view option) {
  if (index + 1 >= argc) {
    throw std::invalid_argument("missing value for " + std::string(option));
  }
  return argv[++index];
}

std::string read_binary_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot read replay output: " + path.string());
  }
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::string read_xam_movie_file(const std::filesystem::path &path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0U || size > ac6xbox360::kMaxXamInputMovieBytes ||
      !std::filesystem::is_regular_file(path, error) || error) {
    throw std::runtime_error(
        "XAM movie is absent, non-regular, empty or outside the byte bound");
  }
  const auto movie = read_binary_file(path);
  if (movie.size() != size) {
    throw std::runtime_error("XAM movie changed while being read");
  }
  return movie;
}

void publish_new_file(const std::filesystem::path &path,
                      std::string_view payload) {
  const auto parent = path.parent_path().empty()
                          ? std::filesystem::current_path()
                          : path.parent_path();
  if (path.filename().empty() || payload.empty() ||
      !std::filesystem::is_directory(parent)) {
    throw std::runtime_error("XAM movie output path or parent is invalid");
  }
  const int descriptor =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (descriptor < 0) {
    throw std::runtime_error("cannot create exclusive XAM movie output: " +
                             std::string(std::strerror(errno)));
  }
  std::size_t offset = 0U;
  try {
    while (offset < payload.size()) {
      const auto written =
          ::write(descriptor, payload.data() + offset, payload.size() - offset);
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) {
        throw std::runtime_error("cannot write complete XAM movie output");
      }
      offset += static_cast<std::size_t>(written);
    }
  } catch (...) {
    const int saved_errno = errno;
    static_cast<void>(::close(descriptor));
    static_cast<void>(::unlink(path.c_str()));
    errno = saved_errno;
    throw;
  }
  if (::close(descriptor) != 0) {
    static_cast<void>(::unlink(path.c_str()));
    throw std::runtime_error("cannot close XAM movie output");
  }
}

} // namespace ac6demo
