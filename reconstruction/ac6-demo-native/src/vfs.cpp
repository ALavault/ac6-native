#include "ac6demo_native/vfs.hpp"

#include "ac6demo_native/content_store.hpp"
#include "posix_fd.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#else
constexpr int O_RDONLY = 0;
#endif

namespace ac6demo_native {
namespace {

bool fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // namespace

Vfs::Vfs(std::filesystem::path store_root) : store_root_(std::move(store_root)) {}

bool Vfs::is_guest_path(std::string_view path) noexcept {
    if (path.size() <= 6U || path.substr(0U, 6U) != "game:/") {
        return false;
    }
    const std::string_view name = path.substr(6U);
    if (name.find('/') != std::string_view::npos ||
        name.find('\\') != std::string_view::npos ||
        name.find(':') != std::string_view::npos || name == "." || name == "..") {
        return false;
    }
    return std::any_of(production_identity().files.begin(), production_identity().files.end(),
                       [name](const ExpectedFile& expected) {
                           return name == expected.name;
                       });
}

std::optional<std::vector<std::byte>> Vfs::read(std::string_view guest_path,
                                                std::uint64_t offset,
                                                std::uint64_t length,
                                                std::string* error) const {
    if (!is_guest_path(guest_path)) {
        fail(error, "invalid guest path");
        return std::nullopt;
    }
    if (length > max_read_length() ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        fail(error, "guest read exceeds bound");
        return std::nullopt;
    }

    detail::UniqueFd root(detail::open_directory_path(store_root_, false, error));
    if (!root) {
        fail(error, "published store unavailable");
        return std::nullopt;
    }
    detail::UniqueFd generation;
    if (!detail::read_current_generation(root.get(), &generation, error)) {
        fail(error, "published store unavailable");
        return std::nullopt;
    }
    const std::string name(guest_path.substr(6U));
    detail::UniqueFd file(detail::open_regular_at(generation.get(), name.c_str(), O_RDONLY, 0U,
                                                  error));
    if (!file) {
        fail(error, "guest file unavailable");
        return std::nullopt;
    }
    std::uint64_t size = 0U;
    bool regular = false;
    if (!detail::stat_fd(file.get(), &size, &regular, error) || !regular ||
        offset > size || length > size - offset) {
        fail(error, "guest read range invalid");
        return std::nullopt;
    }

    std::vector<std::byte> result(static_cast<std::size_t>(length));
    if (!detail::read_exact(file.get(), result.data(), result.size(), offset, error)) {
        fail(error, "guest read failed");
        return std::nullopt;
    }
    return result;
}

}  // namespace ac6demo_native
