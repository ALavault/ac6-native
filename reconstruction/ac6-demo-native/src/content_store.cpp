#include "ac6demo_native/content_store.hpp"

#include "ac6demo_native/sha256.hpp"
#include "posix_fd.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <set>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#else
constexpr int O_RDONLY = 0;
constexpr int O_RDWR = 0;
constexpr int O_CREAT = 0;
constexpr int O_EXCL = 0;
#endif

namespace ac6demo_native {
namespace {

namespace fs = std::filesystem;
using detail::UniqueFd;

constexpr char kMarkerName[] = ".ac6-demo-native.marker";
// The marker is deliberately self-describing and fixed. It is checked before
// any published content is opened by the VFS or verifier.
constexpr char kMarkerContents[] =
    "schema=ac6-demo-native-identity-profile/v1\n"
    "target_id=ac6-demo-xbox360-pal\n"
    "product=ac6-demo-native\n"
    "platform=xbox360-xenon\n"
    "region=PAL-demo\n"
    "supported=false\n"
    "vfs_namespace=game:/\n"
    "files=Default.xex,1454080,de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8;"
    "DATA.TBL,13784,0d9e11cf19881971e7d14c0077e9e719c1795e0316afab4b48b153351591eef8;"
    "DATA00.PAC,177340416,838356ade0f41fc7eee11684dda8e4d6c07eac7512a23ef1d148eb3144dbb162;"
    "DATA01.PAC,15138816,08ef13fe61caf0b072a4de6de577e965b4f1c8feb88d638ffc099dd4d63238d3;"
    "bgmpack.bin,66455552,1ff738c781eeffb510295fc539fc98aeb298040b4ea5965f8eb628bfe1365a8b;"
    "demopack_eng.bin,11427840,b2e7a883951a1a95484a98829beaf5381d4e68c5a7bf2a2a0847857365f1cfad;"
    "demopack_jpn.bin,11675648,b35049450612e8ca9b75e29b8318508b81a46c173fdd626de3de39c1df2ff306;"
    "voicepack_eng.bin,16988160,f482d54d8314c4eae8c3ab20cda40a6c539c96cd933dcfde3c41f03b33e121ba;"
    "voicepack_jpn.bin,21876736,e67c7add7e4dde4297ae87ec0ca30e6e91333d32511b6fd12e8f2d01e4c987b8\n";
constexpr char kRetailXexSha256[] =
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
constexpr char kImportLockName[] = ".ac6-demo-native.import.lock";
constexpr std::size_t kMaxAttempts = 16U;

const std::array<ExpectedFile, 9U>& production_files() {
    static const std::array<ExpectedFile, 9U> files = {{
        {"Default.xex", 1454080U,
         "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"},
        {"DATA.TBL", 13784U,
         "0d9e11cf19881971e7d14c0077e9e719c1795e0316afab4b48b153351591eef8"},
        {"DATA00.PAC", 177340416U,
         "838356ade0f41fc7eee11684dda8e4d6c07eac7512a23ef1d148eb3144dbb162"},
        {"DATA01.PAC", 15138816U,
         "08ef13fe61caf0b072a4de6de577e965b4f1c8feb88d638ffc099dd4d63238d3"},
        {"bgmpack.bin", 66455552U,
         "1ff738c781eeffb510295fc539fc98aeb298040b4ea5965f8eb628bfe1365a8b"},
        {"demopack_eng.bin", 11427840U,
         "b2e7a883951a1a95484a98829beaf5381d4e68c5a7bf2a2a0847857365f1cfad"},
        {"demopack_jpn.bin", 11675648U,
         "b35049450612e8ca9b75e29b8318508b81a46c173fdd626de3de39c1df2ff306"},
        {"voicepack_eng.bin", 16988160U,
         "f482d54d8314c4eae8c3ab20cda40a6c539c96cd933dcfde3c41f03b33e121ba"},
        {"voicepack_jpn.bin", 21876736U,
         "e67c7add7e4dde4297ae87ec0ca30e6e91333d32511b6fd12e8f2d01e4c987b8"},
    }};
    return files;
}

bool set_error(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool valid_sha256(const std::string& value) {
    if (value.size() != 64U) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char character) {
        return (character >= '0' && character <= '9') ||
               (character >= 'a' && character <= 'f') ||
               (character >= 'A' && character <= 'F');
    });
}

bool validate_profile(const IdentityProfile& profile, std::string* error,
                      bool fixture_profile) {
    if (profile.schema != "ac6-demo-native-identity-profile/v1" ||
        profile.target_id != "ac6-demo-xbox360-pal" ||
        profile.product != "ac6-demo-native" ||
        profile.platform != "xbox360-xenon" || profile.region != "PAL-demo" ||
        profile.vfs_namespace != "game:/" || profile.supported) {
        return set_error(error, "identity metadata mismatch");
    }
    if (profile.files.size() != production_files().size()) {
        return set_error(error, "identity file count mismatch");
    }

    std::unordered_set<std::string> names;
    for (const auto& expected : profile.files) {
        if (!names.insert(expected.name).second || expected.name.empty() ||
            expected.name.find('/') != std::string::npos ||
            expected.name.find('\\') != std::string::npos ||
            !valid_sha256(expected.sha256)) {
            return set_error(error, "invalid identity file entry");
        }
        const auto known = std::find_if(
            production_files().begin(), production_files().end(),
            [&expected](const ExpectedFile& candidate) {
                return candidate.name == expected.name;
            });
        if (known == production_files().end()) {
            return set_error(error, "identity file name mismatch");
        }
        if (!fixture_profile &&
            (expected.size != known->size || expected.sha256 != known->sha256)) {
            return set_error(error, "identity file digest mismatch");
        }
    }
    for (const auto& known : production_files()) {
        if (names.find(known.name) == names.end()) {
            return set_error(error, "identity file name missing");
        }
    }
    return true;
}

std::string operation_id(std::size_t attempt) {
    static std::atomic<std::uint64_t> sequence{0U};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto value = sequence.fetch_add(1U, std::memory_order_relaxed);
#if defined(__unix__) || defined(__APPLE__)
    const auto process = static_cast<unsigned long long>(::getpid());
#else
    const auto process = 0ULL;
#endif
    return std::to_string(static_cast<unsigned long long>(now)) + "-" +
           std::to_string(process) + "-" +
           std::to_string(static_cast<unsigned long long>(value)) + "-" +
           std::to_string(static_cast<unsigned long long>(attempt));
}

struct SourceFile {
    std::string name;
    UniqueFd fd;
    std::uint64_t size = 0U;
};

struct SourceSnapshot {
    UniqueFd directory;
    std::vector<SourceFile> files;
};

bool is_expected_name(const IdentityProfile& profile, std::string_view name) {
    return std::any_of(profile.files.begin(), profile.files.end(),
                       [name](const ExpectedFile& expected) {
                           return expected.name == name;
                       });
}

bool enumerate_source(int directory_fd, const IdentityProfile& profile,
                      std::string* error) {
#if !defined(__unix__) && !defined(__APPLE__)
    (void)directory_fd;
    (void)profile;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    const int duplicate = detail::reopen_directory(directory_fd, error);
    if (duplicate < 0) {
        return set_error(error, "cannot enumerate source directory");
    }
    DIR* directory = ::fdopendir(duplicate);
    if (directory == nullptr) {
        (void)::close(duplicate);
        return set_error(error, "cannot enumerate source directory");
    }
    std::unordered_set<std::string> actual;
    errno = 0;
    while (const dirent* entry = ::readdir(directory)) {
        const std::string name(entry->d_name);
        if (name == "." || name == "..") {
            continue;
        }
        if (!actual.insert(name).second || !is_expected_name(profile, name)) {
            (void)::closedir(directory);
            return set_error(error, "source file set invalid");
        }
    }
    const bool read_failed = errno != 0;
    (void)::closedir(directory);
    if (read_failed || actual.size() != profile.files.size()) {
        return set_error(error, "source file set incomplete");
    }
    return true;
#endif
}

bool open_source_snapshot(const fs::path& source, const IdentityProfile& profile,
                          SourceSnapshot* snapshot, std::string* error) {
    if (snapshot == nullptr) {
        return set_error(error, "source snapshot output invalid");
    }
    snapshot->directory.reset(detail::open_directory_path(source, false, error));
    if (!snapshot->directory) {
        return false;
    }
    if (!enumerate_source(snapshot->directory.get(), profile, error)) {
        return false;
    }

    snapshot->files.clear();
    snapshot->files.reserve(profile.files.size());
    for (const auto& expected : profile.files) {
        SourceFile source_file;
        source_file.name = expected.name;
        source_file.fd.reset(detail::open_regular_at(
            snapshot->directory.get(), expected.name.c_str(), O_RDONLY, 0U, error));
        if (!source_file.fd) {
            return set_error(error, "source file changed during import");
        }
        bool regular = false;
        if (!detail::stat_fd(source_file.fd.get(), &source_file.size, &regular, error) ||
            !regular || source_file.size != expected.size) {
            return set_error(error, "source size mismatch");
        }
        std::string hash_error;
        const std::string actual_hash =
            detail::sha256_fd(source_file.fd.get(), source_file.size, &hash_error);
        if (expected.name == "Default.xex" && actual_hash == kRetailXexSha256) {
            return set_error(error, "retail XEX identity rejected");
        }
        if (actual_hash.empty() || actual_hash != expected.sha256) {
            return set_error(error, "source hash mismatch");
        }
        snapshot->files.push_back(std::move(source_file));
    }
    return true;
}

bool copy_checked(const SourceFile& source, int staging_fd,
                  const ExpectedFile& expected, std::string* error) {
    UniqueFd destination(detail::open_regular_at(
        staging_fd, expected.name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600U, error));
    if (!destination) {
        return set_error(error, "cannot stage content");
    }
    std::array<std::byte, 1024U * 1024U> buffer{};
    std::uint64_t offset = 0U;
    while (offset < source.size) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(
            static_cast<std::uint64_t>(buffer.size()), source.size - offset));
        if (!detail::read_exact(source.fd.get(), buffer.data(), count, offset, error) ||
            !detail::write_all(destination.get(), buffer.data(), count, error)) {
            return set_error(error, "cannot stage content");
        }
        offset += static_cast<std::uint64_t>(count);
    }
    if (!detail::sync_fd(destination.get(), "content fsync failed", error)) {
        return false;
    }
    std::uint64_t staged_size = 0U;
    bool regular = false;
    if (!detail::stat_fd(destination.get(), &staged_size, &regular, error) || !regular ||
        staged_size != expected.size) {
        return set_error(error, "staged size mismatch");
    }
    std::string hash_error;
    if (detail::sha256_fd(destination.get(), staged_size, &hash_error) != expected.sha256) {
        return set_error(error, "staged hash mismatch");
    }
    return true;
}

bool stage_marker(int staging_fd, std::string* error) {
    UniqueFd marker(detail::open_regular_at(
        staging_fd, kMarkerName, O_RDWR | O_CREAT | O_EXCL, 0600U, error));
    if (!marker) {
        return set_error(error, "cannot create store marker");
    }
    if (!detail::write_all(marker.get(), kMarkerContents,
                           sizeof(kMarkerContents) - 1U, error) ||
        !detail::sync_fd(marker.get(), "marker fsync failed", error)) {
        return false;
    }
    std::uint64_t size = 0U;
    bool regular = false;
    if (!detail::stat_fd(marker.get(), &size, &regular, error) || !regular ||
        size != sizeof(kMarkerContents) - 1U) {
        return set_error(error, "store marker size invalid");
    }
    return true;
}

bool check_generation_files(int generation_fd, const IdentityProfile& profile,
                            std::string* error) {
    if (!detail::validate_store_marker(generation_fd, error)) {
        return false;
    }
#if !defined(__unix__) && !defined(__APPLE__)
    (void)profile;
    return set_error(error, "POSIX descriptor backend unavailable");
#else
    std::set<std::string> expected_names;
    for (const auto& file : profile.files) {
        expected_names.insert(file.name);
    }
    expected_names.insert(kMarkerName);
    const int duplicate = detail::reopen_directory(generation_fd, error);
    if (duplicate < 0) {
        return set_error(error, "cannot enumerate published generation");
    }
    DIR* directory = ::fdopendir(duplicate);
    if (directory == nullptr) {
        (void)::close(duplicate);
        return set_error(error, "cannot enumerate published generation");
    }
    std::set<std::string> actual_names;
    errno = 0;
    while (const dirent* entry = ::readdir(directory)) {
        const std::string name(entry->d_name);
        if (name == "." || name == "..") {
            continue;
        }
        if (!actual_names.insert(name).second || expected_names.find(name) == expected_names.end()) {
            (void)::closedir(directory);
            return set_error(error, "published generation contains unexpected entry");
        }
    }
    const bool read_failed = errno != 0;
    (void)::closedir(directory);
    if (read_failed || actual_names != expected_names) {
        return set_error(error, "published generation file set incomplete");
    }
    for (const auto& expected : profile.files) {
        UniqueFd file(detail::open_regular_at(generation_fd, expected.name.c_str(),
                                              O_RDONLY, 0U, error));
        if (!file) {
            return set_error(error, "published generation file unavailable");
        }
        std::uint64_t size = 0U;
        bool regular = false;
        if (!detail::stat_fd(file.get(), &size, &regular, error) || !regular ||
            size != expected.size) {
            return set_error(error, "published generation size mismatch");
        }
    }
    return true;
#endif
}

bool check_generation_content(int generation_fd, const IdentityProfile& profile,
                              std::string* error) {
    if (!check_generation_files(generation_fd, profile, error)) {
        return false;
    }
    for (const auto& expected : profile.files) {
        UniqueFd file(detail::open_regular_at(generation_fd, expected.name.c_str(),
                                              O_RDONLY, 0U, error));
        if (!file) {
            return set_error(error, "published content unavailable");
        }
        std::uint64_t size = 0U;
        bool regular = false;
        if (!detail::stat_fd(file.get(), &size, &regular, error) || !regular ||
            size != expected.size) {
            return set_error(error, "published content size mismatch");
        }
        std::string hash_error;
        if (detail::sha256_fd(file.get(), size, &hash_error) != expected.sha256) {
            return set_error(error, "published content hash mismatch");
        }
    }
    return true;
}

bool revalidate_current(int root_fd, int expected_generation_fd,
                        const std::string& expected_pointer,
                        const IdentityProfile& profile, std::string* error) {
    std::string pointer;
    bool present = false;
    if (!detail::read_current_pointer(root_fd, &pointer, &present, error) || !present ||
        pointer != expected_pointer) {
        return set_error(error, "published pointer changed during commit");
    }
    UniqueFd generation;
    if (!detail::read_current_generation(root_fd, &generation, error)) {
        return false;
    }
    std::uint64_t expected_device = 0U;
    std::uint64_t expected_inode = 0U;
    std::uint64_t actual_device = 0U;
    std::uint64_t actual_inode = 0U;
    if (!detail::identity_fd(expected_generation_fd, &expected_device, &expected_inode,
                             error) ||
        !detail::identity_fd(generation.get(), &actual_device, &actual_inode, error) ||
        expected_device != actual_device || expected_inode != actual_inode) {
        return set_error(error, "published generation identity changed");
    }
    if (!check_generation_content(generation.get(), profile, error)) {
        return false;
    }
    std::string confirmed_pointer;
    bool confirmed_present = false;
    UniqueFd confirmed_generation;
    std::uint64_t confirmed_device = 0U;
    std::uint64_t confirmed_inode = 0U;
    if (!detail::read_current_pointer(root_fd, &confirmed_pointer, &confirmed_present, error) ||
        !confirmed_present || confirmed_pointer != expected_pointer ||
        !detail::read_current_generation(root_fd, &confirmed_generation, error) ||
        !detail::identity_fd(confirmed_generation.get(), &confirmed_device, &confirmed_inode,
                             error) ||
        confirmed_device != expected_device || confirmed_inode != expected_inode) {
        return set_error(error, "published generation changed during validation");
    }
    return true;
}

bool current_pointer_is_valid(int root_fd, const IdentityProfile& profile,
                              std::string* error) {
    std::string pointer;
    bool present = false;
    if (!detail::read_current_pointer(root_fd, &pointer, &present, error)) {
        return false;
    }
    if (!present) {
        return true;
    }
    UniqueFd generation;
    return detail::read_current_generation(root_fd, &generation, error) &&
           revalidate_current(root_fd, generation.get(), pointer, profile, error);
}

bool sync_root_parent(const fs::path& root_path, std::string* error) {
    const fs::path parent = root_path.parent_path().empty()
                                ? fs::path(".")
                                : root_path.parent_path();
    UniqueFd parent_fd(detail::open_directory_path(parent, false, error));
    return parent_fd && detail::sync_directory(parent_fd.get(), "root parent fsync failed",
                                               error);
}

struct StagedGeneration {
    std::string name;
    std::string pointer_temp_name;
    UniqueFd directory;
};

enum class StageResult { ready, retry, failure };

StageResult stage_generation(int root_fd, int generations_fd,
                             const SourceSnapshot& source_snapshot,
                             const IdentityProfile& profile, std::size_t attempt,
                             StagedGeneration* result, std::string* error) {
    const std::string id = operation_id(attempt);
    const std::string staging_name = ".staging-" + id;
    result->name = "generation-" + id;
    result->pointer_temp_name = ".current-" + id + ".tmp";
    std::string create_error;
    UniqueFd staging(detail::create_exclusive_directory_at(
        root_fd, staging_name.c_str(), &create_error));
#if defined(__unix__) || defined(__APPLE__)
    if (!staging && errno == EEXIST) {
        return StageResult::retry;
    }
#endif
    std::uint64_t staging_device = 0U;
    std::uint64_t staging_inode = 0U;
    if (!staging ||
        !detail::identity_fd(staging.get(), &staging_device, &staging_inode, error) ||
        !detail::sync_directory(root_fd, "staging parent fsync failed", error)) {
        set_error(error, "cannot create durable staging area");
        return StageResult::failure;
    }
    for (std::size_t index = 0; index < profile.files.size(); ++index) {
        if (!copy_checked(source_snapshot.files[index], staging.get(), profile.files[index],
                          error)) {
            return StageResult::failure;
        }
    }
    if (!stage_marker(staging.get(), error) ||
        !check_generation_files(staging.get(), profile, error) ||
        !detail::sync_directory(staging.get(), "staging fsync failed", error)) {
        return StageResult::failure;
    }
    std::string rename_error;
    if (!detail::rename_noreplace(root_fd, staging_name.c_str(), generations_fd,
                                  result->name.c_str(), &rename_error)) {
#if defined(__unix__) || defined(__APPLE__)
        if (errno == EEXIST) {
            return StageResult::retry;
        }
#endif
        if (error != nullptr) {
            *error = rename_error;
        }
        return StageResult::failure;
    }
    std::uint64_t named_device = 0U;
    std::uint64_t named_inode = 0U;
    if (!detail::identity_at(generations_fd, result->name.c_str(), &named_device,
                             &named_inode, error) ||
        named_device != staging_device || named_inode != staging_inode ||
        !detail::sync_directory(staging.get(), "generation fsync failed", error) ||
        !check_generation_content(staging.get(), profile, error) ||
        !detail::identity_at(generations_fd, result->name.c_str(), &named_device,
                             &named_inode, error) ||
        named_device != staging_device || named_inode != staging_inode ||
        !detail::sync_fd(staging.get(), "generation fsync failed", error) ||
        !detail::sync_directory(generations_fd, "generations fsync failed", error) ||
        !detail::sync_directory(root_fd, "store fsync failed", error)) {
        set_error(error, "staged generation validation failed");
        return StageResult::failure;
    }
    result->directory = std::move(staging);
    return StageResult::ready;
}

bool publish_generation(int root_fd, const StagedGeneration& generation,
                        const IdentityProfile& profile, const std::string& old_pointer,
                        bool had_old_pointer, std::string* error) {
    UniqueFd pointer_temp(detail::open_regular_at(
        root_fd, generation.pointer_temp_name.c_str(), O_WRONLY | O_CREAT | O_EXCL,
        0600U, error));
    const std::string pointer_contents = generation.name + "\n";
    if (!pointer_temp ||
        !detail::write_all(pointer_temp.get(), pointer_contents.data(),
                           pointer_contents.size(), error) ||
        !detail::sync_fd(pointer_temp.get(), "pointer fsync failed", error) ||
        !current_pointer_is_valid(root_fd, profile, error)) {
        return false;
    }

    UniqueFd old_current;
    std::uint64_t old_device = 0U;
    std::uint64_t old_inode = 0U;
    if (had_old_pointer) {
        old_current.reset(detail::open_regular_at(root_fd, "current", O_RDONLY, 0U, error));
        std::string confirmed_old_pointer;
        bool confirmed_old_present = false;
        std::uint64_t old_path_device = 0U;
        std::uint64_t old_path_inode = 0U;
        if (!old_current ||
            !detail::identity_fd(old_current.get(), &old_device, &old_inode, error) ||
            !detail::read_current_pointer(root_fd, &confirmed_old_pointer,
                                          &confirmed_old_present, error) ||
            !confirmed_old_present || confirmed_old_pointer != old_pointer ||
            !detail::identity_at(root_fd, "current", &old_path_device, &old_path_inode,
                                 error) ||
            old_path_device != old_device || old_path_inode != old_inode) {
            return set_error(error, "current changed before compare-and-swap");
        }
    }

    std::uint64_t pointer_device = 0U;
    std::uint64_t pointer_inode = 0U;
    std::uint64_t pointer_path_device = 0U;
    std::uint64_t pointer_path_inode = 0U;
    if (!detail::identity_fd(pointer_temp.get(), &pointer_device, &pointer_inode, error) ||
        !detail::identity_at(root_fd, generation.pointer_temp_name.c_str(),
                             &pointer_path_device, &pointer_path_inode, error) ||
        pointer_device != pointer_path_device || pointer_inode != pointer_path_inode) {
        return set_error(error, "pointer temporary identity changed");
    }
#if defined(__unix__) || defined(__APPLE__)
    const bool exchanged = had_old_pointer
        ? detail::rename_exchange(root_fd, generation.pointer_temp_name.c_str(), root_fd,
                                  "current", error)
        : detail::rename_noreplace(root_fd, generation.pointer_temp_name.c_str(), root_fd,
                                   "current", error);
    if (!exchanged) {
        return false;
    }
#else
    return set_error(error, "POSIX descriptor backend unavailable");
#endif

    std::uint64_t current_device = 0U;
    std::uint64_t current_inode = 0U;
    bool valid = detail::identity_at(root_fd, "current", &current_device, &current_inode,
                                     nullptr) &&
                 current_device == pointer_device && current_inode == pointer_inode;
    if (had_old_pointer) {
        std::uint64_t swapped_device = 0U;
        std::uint64_t swapped_inode = 0U;
        valid = valid &&
            detail::identity_at(root_fd, generation.pointer_temp_name.c_str(),
                                &swapped_device, &swapped_inode, nullptr) &&
            swapped_device == old_device && swapped_inode == old_inode;
    }
    valid = valid &&
        detail::sync_directory(root_fd, "publication root fsync failed", error) &&
        revalidate_current(root_fd, generation.directory.get(), pointer_contents, profile,
                           error);
    if (valid) {
        return true;
    }
    if (had_old_pointer) {
        std::uint64_t current_now_device = 0U;
        std::uint64_t current_now_inode = 0U;
        std::uint64_t swapped_now_device = 0U;
        std::uint64_t swapped_now_inode = 0U;
        const bool exchange_back_safe =
            detail::identity_at(root_fd, "current", &current_now_device,
                                &current_now_inode, nullptr) &&
            detail::identity_at(root_fd, generation.pointer_temp_name.c_str(),
                                &swapped_now_device, &swapped_now_inode, nullptr) &&
            current_now_device == pointer_device && current_now_inode == pointer_inode &&
            swapped_now_device == old_device && swapped_now_inode == old_inode;
        if (exchange_back_safe &&
            detail::rename_exchange(root_fd, "current", root_fd,
                                    generation.pointer_temp_name.c_str(), error) &&
            detail::sync_directory(root_fd, "rollback root fsync failed", error) &&
            detail::identity_at(root_fd, "current", &current_now_device,
                                &current_now_inode, error) &&
            current_now_device == old_device && current_now_inode == old_inode) {
            return false;
        }
    }
    return set_error(error, had_old_pointer ? "publication rollback failed"
                                            : "initial publication validation failed");
}

}  // namespace

const IdentityProfile& production_identity() noexcept {
    static const IdentityProfile profile = [] {
        IdentityProfile value;
        value.schema = "ac6-demo-native-identity-profile/v1";
        value.target_id = "ac6-demo-xbox360-pal";
        value.product = "ac6-demo-native";
        value.platform = "xbox360-xenon";
        value.region = "PAL-demo";
        value.vfs_namespace = "game:/";
        value.supported = false;
        value.files.assign(production_files().begin(), production_files().end());
        return value;
    }();
    return profile;
}

const char* store_marker_name() noexcept { return kMarkerName; }
const char* store_marker_contents() noexcept { return kMarkerContents; }
const char* retail_xex_sha256() noexcept { return kRetailXexSha256; }

ContentStore::ContentStore(fs::path root) : root_(std::move(root)) {}

fs::path ContentStore::default_root() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg != nullptr && *xdg != '\0') {
        return fs::path(xdg) / "ac6-demo-native";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return fs::path(home) / ".local" / "share" / "ac6-demo-native";
    }
    return fs::path(".") / ".ac6-demo-native";
}

fs::path ContentStore::default_config_root() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && *xdg != '\0') {
        return fs::path(xdg) / "ac6-demo-native";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return fs::path(home) / ".config" / "ac6-demo-native";
    }
    return fs::path(".") / ".ac6-demo-native-config";
}

bool import_impl(const fs::path& root_path, const fs::path& source,
                 const IdentityProfile& profile, std::string* error,
                 bool fixture_profile) {
    if (!validate_profile(profile, error, fixture_profile)) {
        return false;
    }

    SourceSnapshot source_snapshot;
    if (!open_source_snapshot(source, profile, &source_snapshot, error)) {
        return false;
    }
    UniqueFd root(detail::open_directory_path(root_path, true, error));
    if (!root) {
        return false;
    }
    if (!sync_root_parent(root_path, error)) {
        return false;
    }
    UniqueFd import_lock(detail::open_lock_at(root.get(), kImportLockName, error));
    if (!import_lock || !detail::lock_exclusive(import_lock.get(), error)) {
        return false;
    }
    UniqueFd generations(detail::open_directory_at(root.get(), "generations", nullptr));
#if defined(__unix__) || defined(__APPLE__)
    if (!generations && errno == ENOENT) {
        std::string directory_error;
        generations.reset(detail::create_directory_at(root.get(), "generations",
                                                       &directory_error));
        if (!generations && errno == EEXIST) {
            generations.reset(detail::open_directory_at(root.get(), "generations",
                                                        &directory_error));
        }
    }
#endif
    if (!generations) {
        return set_error(error, "cannot create generations directory");
    }
    if (!detail::sync_directory(generations.get(), "generations fsync failed", error) ||
        !detail::sync_directory(root.get(), "store fsync failed", error)) {
        return false;
    }
    std::string old_pointer;
    bool had_old_pointer = false;
    if (!detail::read_current_pointer(root.get(), &old_pointer, &had_old_pointer, error)) {
        return false;
    }
    if (!current_pointer_is_valid(root.get(), profile, error)) {
        return false;
    }

    for (std::size_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
        StagedGeneration generation;
        const StageResult staged = stage_generation(
            root.get(), generations.get(), source_snapshot, profile, attempt, &generation,
            error);
        if (staged == StageResult::retry) {
            continue;
        }
        if (staged == StageResult::failure) {
            return false;
        }
        return publish_generation(root.get(), generation, profile, old_pointer,
                                  had_old_pointer, error);
    }
    return set_error(error, "generation collision retry limit exceeded");
}

bool verify_impl(const fs::path& root_path, const IdentityProfile& profile,
                 std::string* error, bool fixture_profile) {
    if (!validate_profile(profile, error, fixture_profile)) {
        return false;
    }
    UniqueFd root(detail::open_directory_path(root_path, false, error));
    if (!root) {
        return false;
    }
    std::string pointer;
    bool present = false;
    UniqueFd generation;
    if (!detail::read_current_pointer(root.get(), &pointer, &present, error) || !present ||
        !detail::read_current_generation(root.get(), &generation, error) ||
        !revalidate_current(root.get(), generation.get(), pointer, profile, error)) {
        return false;
    }
    return true;
}

bool ContentStore::import_directory(const fs::path& source, std::string* error) const {
    return import_impl(root_, source, production_identity(), error, false);
}

bool ContentStore::verify(std::string* error) const {
    return verify_impl(root_, production_identity(), error, false);
}

const fs::path& ContentStore::root() const noexcept { return root_; }

#if defined(AC6DEMO_NATIVE_ENABLE_TESTING)
namespace testing {

bool import_fixture(ContentStore& store, const fs::path& source,
                    const IdentityProfile& profile, std::string* error) {
    return import_impl(store.root(), source, profile, error, true);
}

bool verify_fixture(const ContentStore& store, const IdentityProfile& profile,
                    std::string* error) {
    return verify_impl(store.root(), profile, error, true);
}

}  // namespace testing
#endif

}  // namespace ac6demo_native
