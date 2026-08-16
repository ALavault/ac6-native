#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace ac6demo_native {

class Sha256 {
public:
    Sha256() noexcept;

    void update(std::span<const std::byte> bytes) noexcept;
    void update(const void* data, std::size_t size) noexcept;
    [[nodiscard]] std::string final_hex();

private:
    void transform(const std::uint8_t* block) noexcept;

    std::uint32_t state_[8];
    std::uint64_t bit_count_;
    std::uint8_t buffer_[64];
    std::size_t buffered_;
    bool finalized_;
};

[[nodiscard]] std::string sha256_bytes(std::span<const std::byte> bytes);
[[nodiscard]] std::string sha256_file(const std::filesystem::path& path,
                                       std::string* error = nullptr);

}  // namespace ac6demo_native
