#include "posix_fd.hpp"

#include "ac6demo_native/content_store.hpp"
#include "ac6demo_native/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#define AC6DEMO_NATIVE_POSIX_BACKEND 1
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#else
#define AC6DEMO_NATIVE_POSIX_BACKEND 0
#endif

namespace ac6demo_native::detail {
namespace {

bool set_error(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

constexpr std::size_t kPointerLimit = 128U;
constexpr std::size_t kMarkerLimit = 4096U;

#if AC6DEMO_NATIVE_POSIX_BACKEND

#if !defined(O_CLOEXEC) || !defined(O_NOFOLLOW) || !defined(O_DIRECTORY)
#define AC6DEMO_NATIVE_OPEN_FLAGS_AVAILABLE 0
#else
#define AC6DEMO_NATIVE_OPEN_FLAGS_AVAILABLE 1
#endif

int directory_flags() noexcept {
#if AC6DEMO_NATIVE_OPEN_FLAGS_AVAILABLE
    return O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY;
#else
    return -1;
#endif
}

int regular_flags(int flags) noexcept {
#if AC6DEMO_NATIVE_OPEN_FLAGS_AVAILABLE
    return flags | O_CLOEXEC | O_NOFOLLOW;
#else
    (void)flags;
    return -1;
#endif
}

bool posix_flags_available(std::string* error) {
#if AC6DEMO_NATIVE_OPEN_FLAGS_AVAILABLE
    (void)error;
    return true;
#else
    return set_error(error, "POSIX no-follow descriptor flags unavailable");
#endif
}

bool valid_component(const std::string& component) {
    return !component.empty() && component != "." && component != ".." &&
           component.find('/') == std::string::npos &&
           component.find('\\') == std::string::npos;
}

bool valid_name(std::string_view name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string_view::npos ||
        name.find('\\') != std::string_view::npos) {
        return false;
    }
    return true;
}

bool errno_error(std::string* error, const char* message) {
    (void)errno;
    return set_error(error, message);
}

bool stat_descriptor(int fd, struct stat* status, std::string* error) {
    if (fstat(fd, status) != 0) {
        return errno_error(error, "descriptor stat failed");
    }
    if (status->st_size < 0) {
        return set_error(error, "descriptor size invalid");
    }
    return true;
}

bool offset_fits(std::uint64_t offset, std::string* error) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
        return set_error(error, "descriptor offset invalid");
    }
    return true;
}

#endif

}  // namespace

UniqueFd::~UniqueFd() { reset(); }

void UniqueFd::reset(int fd) noexcept {
#if AC6DEMO_NATIVE_POSIX_BACKEND
    if (fd_ >= 0) {
        (void)::close(fd_);
    }
#endif
    fd_ = fd;
}

int open_directory_path(const std::filesystem::path& path, bool create,
                        std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)path;
    (void)create;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error) || path.empty()) {
        if (path.empty()) {
            set_error(error, "directory path invalid");
        }
        return -1;
    }

    UniqueFd current(::open(path.is_absolute() ? "/" : ".", directory_flags()));
    if (!current) {
        errno_error(error, "cannot anchor directory path");
        return -1;
    }

    for (const auto& component_value : path) {
        const std::string component = component_value.string();
        if (component == "/" || component.empty() || component == ".") {
            continue;
        }
        if (!valid_component(component)) {
            set_error(error, "directory path contains unsupported component");
            return -1;
        }

        int child = ::openat(current.get(), component.c_str(), directory_flags());
        if (child < 0 && create && errno == ENOENT) {
            if (::mkdirat(current.get(), component.c_str(), 0700) != 0 &&
                errno != EEXIST) {
                errno_error(error, "cannot create directory");
                return -1;
            }
            while (::fsync(current.get()) != 0) {
                if (errno == EINTR) {
                    continue;
                }
                errno_error(error, "parent directory fsync failed");
                return -1;
            }
            child = ::openat(current.get(), component.c_str(), directory_flags());
        }
        if (child < 0) {
            errno_error(error, "cannot open directory component");
            return -1;
        }
        current.reset(child);
    }
    return current.release();
#endif
}

int open_directory_at(int parent_fd, const char* name, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error) || name == nullptr || !valid_name(name)) {
        if (name == nullptr || !valid_name(name)) {
            set_error(error, "directory entry name invalid");
        }
        return -1;
    }
    const int fd = ::openat(parent_fd, name, directory_flags());
    if (fd < 0) {
        errno_error(error, "cannot open directory entry");
    }
    return fd;
#endif
}

int reopen_directory(int directory_fd, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)directory_fd;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error)) {
        return -1;
    }
    const int fd = ::openat(directory_fd, ".", directory_flags());
    if (fd < 0) {
        return errno_error(error, "cannot reopen directory descriptor");
    }
    return fd;
#endif
}

int open_lock_at(int parent_fd, const char* name, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    return open_regular_at(parent_fd, name, O_RDWR | O_CREAT, 0600U, error);
#endif
}

bool lock_exclusive(int fd, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    while (::flock(fd, LOCK_EX) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return set_error(error, "interprocess import lock failed");
    }
    return true;
#endif
}

int create_directory_at(int parent_fd, const char* name, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error) || name == nullptr || !valid_name(name)) {
        if (name == nullptr || !valid_name(name)) {
            set_error(error, "directory entry name invalid");
        }
        return -1;
    }
    if (::mkdirat(parent_fd, name, 0700) != 0) {
        errno_error(error, "cannot create directory entry");
        return -1;
    }
    return open_directory_at(parent_fd, name, error);
#endif
}

int create_exclusive_directory_at(int parent_fd, const char* name,
                                  std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error) || name == nullptr || !valid_name(name)) {
        if (name == nullptr || !valid_name(name)) {
            set_error(error, "directory entry name invalid");
        }
        return -1;
    }
    if (::mkdirat(parent_fd, name, 0700) != 0) {
        errno_error(error, "cannot create exclusive directory entry");
        return -1;
    }
    return open_directory_at(parent_fd, name, error);
#endif
}

int open_regular_at(int parent_fd, const char* name, int flags, unsigned int mode,
                    std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    (void)flags;
    (void)mode;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (!posix_flags_available(error) || name == nullptr || !valid_name(name)) {
        if (name == nullptr || !valid_name(name)) {
            set_error(error, "file entry name invalid");
        }
        return -1;
    }
    const int fd = ::openat(parent_fd, name, regular_flags(flags),
                            static_cast<mode_t>(mode));
    if (fd < 0) {
        errno_error(error, "cannot open file entry");
    }
    return fd;
#endif
}

int open_regular_path(const std::filesystem::path& path, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)path;
    set_error(error, "POSIX descriptor backend unavailable");
    return -1;
#else
    if (path.empty() || path.filename().empty()) {
        set_error(error, "file path invalid");
        return -1;
    }
    const std::filesystem::path parent = path.parent_path().empty()
                                             ? std::filesystem::path(".")
                                             : path.parent_path();
    UniqueFd parent_fd(open_directory_path(parent, false, error));
    if (!parent_fd) {
        return -1;
    }
    const std::string name = path.filename().string();
    return open_regular_at(parent_fd.get(), name.c_str(), O_RDONLY, 0U, error);
#endif
}

bool stat_fd(int fd, std::uint64_t* size, bool* regular, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)size;
    (void)regular;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (size == nullptr || regular == nullptr) {
        return set_error(error, "descriptor stat output invalid");
    }
    struct stat status {};
    if (!stat_descriptor(fd, &status, error)) {
        return false;
    }
    *size = static_cast<std::uint64_t>(status.st_size);
    *regular = S_ISREG(status.st_mode) != 0;
    return true;
#endif
}

bool identity_fd(int fd, std::uint64_t* device, std::uint64_t* inode,
                 std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)device;
    (void)inode;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (device == nullptr || inode == nullptr) {
        return set_error(error, "descriptor identity output invalid");
    }
    struct stat status {};
    if (!stat_descriptor(fd, &status, error)) {
        return false;
    }
    *device = static_cast<std::uint64_t>(status.st_dev);
    *inode = static_cast<std::uint64_t>(status.st_ino);
    return true;
#endif
}

bool identity_at(int parent_fd, const char* name, std::uint64_t* device,
                 std::uint64_t* inode, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)parent_fd;
    (void)name;
    (void)device;
    (void)inode;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (name == nullptr || device == nullptr || inode == nullptr) {
        return set_error(error, "entry identity output invalid");
    }
    struct stat status {};
    if (::fstatat(parent_fd, name, &status, AT_SYMLINK_NOFOLLOW) != 0) {
        return errno_error(error, "entry identity unavailable");
    }
    *device = static_cast<std::uint64_t>(status.st_dev);
    *inode = static_cast<std::uint64_t>(status.st_ino);
    return true;
#endif
}

bool write_all(int fd, const void* data, std::size_t size, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)data;
    (void)size;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (size != 0U && data == nullptr) {
        return set_error(error, "descriptor write input invalid");
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (size != 0U) {
        const std::size_t chunk = std::min<std::size_t>(size, 1024U * 1024U);
        const ssize_t count = ::write(fd, bytes, chunk);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return errno_error(error, "descriptor write failed");
        }
        bytes += static_cast<std::size_t>(count);
        size -= static_cast<std::size_t>(count);
    }
    return true;
#endif
}

bool read_exact(int fd, void* data, std::size_t size, std::uint64_t offset,
                std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)data;
    (void)size;
    (void)offset;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (size != 0U && data == nullptr) {
        return set_error(error, "descriptor read output invalid");
    }
    if (!offset_fits(offset, error)) {
        return false;
    }
    auto* bytes = static_cast<unsigned char*>(data);
    std::uint64_t position = offset;
    while (size != 0U) {
        const std::size_t chunk = std::min<std::size_t>(size, 1024U * 1024U);
        const ssize_t count = ::pread(fd, bytes, chunk, static_cast<off_t>(position));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return errno_error(error, "descriptor read failed");
        }
        bytes += static_cast<std::size_t>(count);
        size -= static_cast<std::size_t>(count);
        position += static_cast<std::uint64_t>(count);
        if (size != 0U && position > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
            return set_error(error, "descriptor read offset invalid");
        }
    }
    return true;
#endif
}

bool read_bounded(int fd, std::size_t limit, std::string* value,
                  std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)limit;
    (void)value;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (value == nullptr) {
        return set_error(error, "bounded read output invalid");
    }
    std::uint64_t size = 0;
    bool regular = false;
    if (!stat_fd(fd, &size, &regular, error) || !regular || size > limit ||
        size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return set_error(error, "bounded descriptor read rejected");
    }
    value->assign(static_cast<std::size_t>(size), '\0');
    if (!read_exact(fd, value->data(), value->size(), 0U, error)) {
        value->clear();
        return false;
    }
    return true;
#endif
}

bool sync_fd(int fd, const char* what, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)what;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
#if defined(AC6DEMO_NATIVE_ENABLE_TESTING)
    static std::atomic_bool fsync_injected{false};
    if (std::getenv("AC6DEMO_NATIVE_TEST_FAIL_FSYNC") != nullptr ||
        (std::getenv("AC6DEMO_NATIVE_TEST_FAIL_FSYNC_ONCE") != nullptr &&
         !fsync_injected.exchange(true))) {
        return set_error(error, "injected fsync failure");
    }
#endif
    while (::fsync(fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return set_error(error, what == nullptr ? "descriptor fsync failed" : what);
    }
    return true;
#endif
}

bool sync_directory(int fd, const char* what, std::string* error) {
    return sync_fd(fd, what == nullptr ? "directory fsync failed" : what, error);
}

bool rename_noreplace(int old_parent_fd, const char* old_name, int new_parent_fd,
                      const char* new_name, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)old_parent_fd;
    (void)old_name;
    (void)new_parent_fd;
    (void)new_name;
    return set_error(error, "POSIX descriptor backend unavailable");
#elif defined(__linux__) && defined(SYS_renameat2)
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0U)
#endif
#if defined(AC6DEMO_NATIVE_ENABLE_TESTING)
    static std::atomic_bool rename_injected{false};
    if (std::getenv("AC6DEMO_NATIVE_TEST_FAIL_RENAME") != nullptr ||
        (std::getenv("AC6DEMO_NATIVE_TEST_FAIL_RENAME_ONCE") != nullptr &&
         !rename_injected.exchange(true))) {
        return set_error(error, "injected rename failure");
    }
#endif
    if (::syscall(SYS_renameat2, old_parent_fd, old_name, new_parent_fd, new_name,
                  static_cast<unsigned int>(RENAME_NOREPLACE)) != 0) {
        return errno_error(error, "cannot publish generation without replacement");
    }
    return true;
#else
    (void)old_parent_fd;
    (void)old_name;
    (void)new_parent_fd;
    (void)new_name;
    return set_error(error, "atomic no-replace rename unavailable");
#endif
}

bool rename_replace(int old_parent_fd, const char* old_name, int new_parent_fd,
                    const char* new_name, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)old_parent_fd;
    (void)old_name;
    (void)new_parent_fd;
    (void)new_name;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
#if defined(AC6DEMO_NATIVE_ENABLE_TESTING)
    static std::atomic_bool pointer_rename_injected{false};
    if (std::getenv("AC6DEMO_NATIVE_TEST_FAIL_POINTER_RENAME_ONCE") != nullptr &&
        !pointer_rename_injected.exchange(true)) {
        return set_error(error, "injected pointer rename failure");
    }
#endif
    if (::renameat(old_parent_fd, old_name, new_parent_fd, new_name) != 0) {
        return errno_error(error, "pointer rename failed");
    }
    return true;
#endif
}

bool read_current_generation(int root_fd, UniqueFd* generation, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)root_fd;
    (void)generation;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (generation == nullptr) {
        return set_error(error, "generation output invalid");
    }
    UniqueFd pointer(open_regular_at(root_fd, "current", O_RDONLY, 0U, error));
    if (!pointer) {
        return set_error(error, "published pointer missing");
    }
    std::string pointer_contents;
    if (!read_bounded(pointer.get(), kPointerLimit, &pointer_contents, error)) {
        return set_error(error, "published pointer invalid");
    }
    if (pointer_contents.empty() || pointer_contents.back() != '\n' ||
        pointer_contents.find('\n') != pointer_contents.size() - 1U) {
        return set_error(error, "published pointer invalid");
    }
    const std::string name = pointer_contents.substr(0U, pointer_contents.size() - 1U);
    constexpr std::string_view prefix = "generation-";
    if (name.size() <= prefix.size() || name.compare(0U, prefix.size(), prefix) != 0) {
        return set_error(error, "published pointer invalid");
    }
    for (const char character : std::string_view(name).substr(prefix.size())) {
        const auto unsigned_character = static_cast<unsigned char>(character);
        if ((std::isalnum(unsigned_character) == 0) && character != '-' &&
            character != '_') {
            return set_error(error, "published pointer invalid");
        }
    }

    UniqueFd generations(open_directory_at(root_fd, "generations", error));
    if (!generations) {
        return set_error(error, "published generations unavailable");
    }
    UniqueFd opened(open_directory_at(generations.get(), name.c_str(), error));
    if (!opened) {
        return set_error(error, "published generation missing");
    }
    if (!validate_store_marker(opened.get(), error)) {
        return false;
    }
    *generation = std::move(opened);
    return true;
#endif
}

bool read_current_pointer(int root_fd, std::string* contents, bool* present,
                          std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)root_fd;
    (void)contents;
    (void)present;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    if (contents == nullptr || present == nullptr) {
        return set_error(error, "pointer output invalid");
    }
    *present = false;
    contents->clear();
    UniqueFd pointer(open_regular_at(root_fd, "current", O_RDONLY, 0U, nullptr));
    if (!pointer) {
        if (errno == ENOENT) {
            return true;
        }
        return set_error(error, "published pointer invalid");
    }
    *present = true;
    if (!read_bounded(pointer.get(), kPointerLimit, contents, error)) {
        return set_error(error, "published pointer invalid");
    }
    return true;
#endif
}

bool validate_store_marker(int generation_fd, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)generation_fd;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    UniqueFd marker(open_regular_at(generation_fd, store_marker_name(), O_RDONLY, 0U,
                                    error));
    if (!marker) {
        return set_error(error, "published marker missing");
    }
    std::string contents;
    if (!read_bounded(marker.get(), kMarkerLimit, &contents, error) ||
        contents != store_marker_contents()) {
        return set_error(error, "published marker invalid");
    }
    return true;
#endif
}

std::string sha256_fd(int fd, std::uint64_t size, std::string* error) {
#if !AC6DEMO_NATIVE_POSIX_BACKEND
    (void)fd;
    (void)size;
    set_error(error, "POSIX descriptor backend unavailable");
    return {};
#else
    std::uint64_t actual_size = 0;
    bool regular = false;
    if (!stat_fd(fd, &actual_size, &regular, error) || !regular || actual_size != size) {
        set_error(error, "descriptor size changed");
        return {};
    }
    Sha256 hasher;
    std::array<std::byte, 1024U * 1024U> buffer{};
    std::uint64_t offset = 0;
    while (offset < size) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(
            static_cast<std::uint64_t>(buffer.size()), size - offset));
        if (!read_exact(fd, buffer.data(), count, offset, error)) {
            return {};
        }
        hasher.update(std::span<const std::byte>(buffer.data(), count));
        offset += static_cast<std::uint64_t>(count);
    }
    std::uint64_t final_size = 0;
    bool final_regular = false;
    if (!stat_fd(fd, &final_size, &final_regular, error) || !final_regular ||
        final_size != size) {
        set_error(error, "descriptor changed during hash");
        return {};
    }
    return hasher.final_hex();
#endif
}

}  // namespace ac6demo_native::detail
