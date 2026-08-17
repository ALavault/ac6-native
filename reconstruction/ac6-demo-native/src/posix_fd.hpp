#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace ac6demo_native::detail {

class UniqueFd {
public:
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    ~UniqueFd();

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int release() noexcept {
        const int value = fd_;
        fd_ = -1;
        return value;
    }
    void reset(int fd = -1) noexcept;

private:
    int fd_ = -1;
};

// All filesystem access in the content boundary goes through these descriptor
// helpers. On non-POSIX hosts they fail closed with an explicit error.
[[nodiscard]] int open_directory_path(const std::filesystem::path& path,
                                      bool create, std::string* error);
[[nodiscard]] int open_directory_at(int parent_fd, const char* name,
                                    std::string* error);
[[nodiscard]] int reopen_directory(int directory_fd, std::string* error);
[[nodiscard]] int open_lock_at(int parent_fd, const char* name,
                               std::string* error);
[[nodiscard]] bool lock_exclusive(int fd, std::string* error);
[[nodiscard]] int create_directory_at(int parent_fd, const char* name,
                                      std::string* error);
[[nodiscard]] int create_exclusive_directory_at(int parent_fd, const char* name,
                                                std::string* error);
[[nodiscard]] int open_regular_at(int parent_fd, const char* name, int flags,
                                  unsigned int mode, std::string* error);
[[nodiscard]] int open_regular_path(const std::filesystem::path& path,
                                    std::string* error);

[[nodiscard]] bool stat_fd(int fd, std::uint64_t* size, bool* regular,
                           std::string* error);
[[nodiscard]] bool identity_fd(int fd, std::uint64_t* device,
                               std::uint64_t* inode, std::string* error);
[[nodiscard]] bool identity_at(int parent_fd, const char* name,
                               std::uint64_t* device, std::uint64_t* inode,
                               std::string* error);
[[nodiscard]] bool write_all(int fd, const void* data, std::size_t size,
                             std::string* error);
[[nodiscard]] bool read_exact(int fd, void* data, std::size_t size,
                              std::uint64_t offset, std::string* error);
[[nodiscard]] bool read_bounded(int fd, std::size_t limit, std::string* value,
                                std::string* error);
[[nodiscard]] bool sync_fd(int fd, const char* what, std::string* error);
[[nodiscard]] bool sync_directory(int fd, const char* what, std::string* error);
[[nodiscard]] bool rename_noreplace(int old_parent_fd, const char* old_name,
                                    int new_parent_fd, const char* new_name,
                                    std::string* error);
[[nodiscard]] bool rename_exchange(int first_parent_fd, const char* first_name,
                                   int second_parent_fd, const char* second_name,
                                   std::string* error);

[[nodiscard]] bool read_current_generation(int root_fd, UniqueFd* generation,
                                           std::string* error);
[[nodiscard]] bool read_current_pointer(int root_fd, std::string* contents,
                                        bool* present, std::string* error);
[[nodiscard]] bool validate_store_marker(int generation_fd,
                                         std::string* error);
[[nodiscard]] std::string sha256_fd(int fd, std::uint64_t size,
                                    std::string* error);

}  // namespace ac6demo_native::detail
