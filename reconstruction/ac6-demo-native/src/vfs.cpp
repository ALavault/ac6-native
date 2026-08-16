#include "ac6demo_native/vfs.hpp"

#include "ac6demo_native/content_store.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <string_view>

namespace ac6demo_native {
namespace {

namespace fs = std::filesystem;

bool fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool has_symlink_component(const fs::path& path) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec).lexically_normal();
    if (ec) {
        return true;
    }
    fs::path current;
    for (const auto& component : absolute) {
        current /= component;
        const fs::file_status status = fs::symlink_status(current, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) {
                ec.clear();
                continue;
            }
            return true;
        }
        if (fs::is_symlink(status)) {
            return true;
        }
    }
    return false;
}

bool read_generation(const fs::path& root, fs::path* generation) {
    if (has_symlink_component(root)) {
        return false;
    }
    std::error_code ec;
    const fs::file_status pointer_status = fs::symlink_status(root / "current", ec);
    if (ec || !fs::is_regular_file(pointer_status)) {
        return false;
    }
    if (fs::file_size(root / "current", ec) > 128U || ec) {
        return false;
    }
    std::ifstream input(root / "current", std::ios::binary);
    std::string name;
    std::getline(input, name);
    std::string trailing;
    if (!input || std::getline(input, trailing) || name.rfind("generation-", 0U) != 0U ||
        name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
        name.find("..") != std::string::npos) {
        return false;
    }
    const fs::path candidate = root / "generations" / name;
    if (has_symlink_component(candidate)) {
        return false;
    }
    const fs::file_status generation_status = fs::symlink_status(candidate, ec);
    if (ec || !fs::is_directory(generation_status)) {
        return false;
    }
    const fs::file_status marker_status = fs::symlink_status(
        candidate / store_marker_name(), ec);
    if (ec || !fs::is_regular_file(marker_status)) {
        return false;
    }
    std::ifstream marker(candidate / store_marker_name(), std::ios::binary);
    const std::string marker_contents((std::istreambuf_iterator<char>(marker)),
                                       std::istreambuf_iterator<char>());
    if (!marker || marker_contents != store_marker_contents()) {
        return false;
    }
    *generation = candidate;
    return true;
}

}  // namespace

Vfs::Vfs(fs::path store_root) : store_root_(fs::absolute(store_root).lexically_normal()) {}

bool Vfs::is_guest_path(std::string_view path) noexcept {
    if (path.size() <= 6U || path.substr(0U, 6U) != "game:/") {
        return false;
    }
    const std::string_view name = path.substr(6U);
    if (name.find('/') != std::string_view::npos ||
        name.find('\\') != std::string_view::npos || name.find(':') != std::string_view::npos ||
        name == "." || name == "..") {
        return false;
    }
    return std::any_of(production_identity().files.begin(), production_identity().files.end(),
                       [name](const ExpectedFile& expected) {
                           return name == expected.name;
                       });
}

std::optional<fs::path> Vfs::resolve(std::string_view guest_path,
                                     std::string* error) const {
    if (!is_guest_path(guest_path)) {
        fail(error, "invalid guest path");
        return std::nullopt;
    }
    fs::path generation;
    if (!read_generation(store_root_, &generation)) {
        fail(error, "published store unavailable");
        return std::nullopt;
    }
    const std::string name(guest_path.substr(6U));
    const fs::path file = generation / name;
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(file, ec);
    if (ec || fs::is_symlink(status) || !fs::is_regular_file(status) ||
        has_symlink_component(file)) {
        fail(error, "guest file unavailable");
        return std::nullopt;
    }
    return file;
}

std::optional<std::vector<std::byte>> Vfs::read(std::string_view guest_path,
                                                std::uint64_t offset,
                                                std::uint64_t length,
                                                std::string* error) const {
    if (length > max_read_length()) {
        fail(error, "guest read exceeds bound");
        return std::nullopt;
    }
    const auto file = resolve(guest_path, error);
    if (!file.has_value()) {
        return std::nullopt;
    }
    std::error_code ec;
    const std::uint64_t size = fs::file_size(*file, ec);
    if (ec || offset > size || length > size - offset ||
        offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        fail(error, "guest read range invalid");
        return std::nullopt;
    }
    std::vector<std::byte> result(static_cast<std::size_t>(length));
    std::ifstream input(*file, std::ios::binary);
    if (!input) {
        fail(error, "guest file unavailable");
        return std::nullopt;
    }
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) {
        fail(error, "guest read seek failed");
        return std::nullopt;
    }
    if (length != 0U) {
        input.read(reinterpret_cast<char*>(result.data()),
                   static_cast<std::streamsize>(length));
        if (input.gcount() != static_cast<std::streamsize>(length)) {
            fail(error, "guest read failed");
            return std::nullopt;
        }
    }
    return result;
}

}  // namespace ac6demo_native
