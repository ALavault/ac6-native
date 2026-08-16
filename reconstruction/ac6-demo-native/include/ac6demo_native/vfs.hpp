#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo_native {

class Vfs {
public:
    explicit Vfs(std::filesystem::path store_root);

    [[nodiscard]] static bool is_guest_path(std::string_view path) noexcept;
    [[nodiscard]] std::optional<std::vector<std::byte>> read(
        std::string_view guest_path, std::uint64_t offset,
        std::uint64_t length, std::string* error = nullptr) const;

    [[nodiscard]] static constexpr std::uint64_t max_read_length() noexcept {
        return 8U * 1024U * 1024U;
    }

private:
    [[nodiscard]] std::optional<std::filesystem::path> resolve(
        std::string_view guest_path, std::string* error) const;

    std::filesystem::path store_root_;
};

}  // namespace ac6demo_native
