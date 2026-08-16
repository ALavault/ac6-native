#include "ac6demo_native/content_store.hpp"

#include "ac6demo_native/sha256.hpp"
#include "ac6demo_native/vfs.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <random>
#include <set>
#include <span>
#include <unordered_set>

namespace ac6demo_native {
namespace {

namespace fs = std::filesystem;

constexpr char kMarkerName[] = ".ac6-demo-native.marker";
constexpr char kMarkerContents[] = "AC6-DEMO-NATIVE-CONTENT-STORE/v1\n";
constexpr char kRetailXexSha256[] =
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";

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

bool validate_profile(const IdentityProfile& profile, std::string* error) {
    if (profile.target_id != "ac6-demo-xbox360-pal" ||
        profile.product != "ac6-demo-native" ||
        profile.platform != "xbox360-xenon" || profile.region != "PAL-demo") {
        return set_error(error, "identity target mismatch");
    }
    if (profile.files.size() != production_files().size()) {
        return set_error(error, "identity file count mismatch");
    }

    std::unordered_set<std::string> names;
    for (const auto& expected : profile.files) {
        if (!names.insert(expected.name).second || expected.name.find('/') != std::string::npos ||
            expected.name.find('\\') != std::string::npos || expected.name.empty() ||
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
    }
    for (const auto& known : production_files()) {
        if (names.find(known.name) == names.end()) {
            return set_error(error, "identity file name missing");
        }
    }
    return true;
}

std::string operation_id() {
    static std::atomic<std::uint64_t> sequence{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto value = sequence.fetch_add(1U, std::memory_order_relaxed);
    return std::to_string(static_cast<unsigned long long>(now)) + "-" +
           std::to_string(static_cast<unsigned long long>(value));
}

bool read_pointer(const fs::path& root, fs::path* generation, std::string* error) {
    const fs::path pointer = root / "current";
    std::error_code ec;
    const fs::file_status pointer_status = fs::symlink_status(pointer, ec);
    if (ec || !fs::is_regular_file(pointer_status)) {
        return set_error(error, "published pointer missing");
    }
    if (fs::file_size(pointer, ec) > 128U || ec) {
        return set_error(error, "published pointer invalid");
    }
    std::ifstream input(pointer, std::ios::binary);
    std::string name;
    std::getline(input, name);
    std::string trailing;
    if (!input || std::getline(input, trailing) || name.size() < 12U ||
        name.rfind("generation-", 0U) != 0U || name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos || name.find("..") != std::string::npos) {
        return set_error(error, "published pointer invalid");
    }
    const fs::path candidate = root / "generations" / name;
    if (has_symlink_component(candidate)) {
        return set_error(error, "published generation path invalid");
    }
    const fs::file_status status = fs::symlink_status(candidate, ec);
    if (ec || !fs::is_directory(status)) {
        return set_error(error, "published generation missing");
    }
    *generation = candidate;
    return true;
}

bool write_file(const fs::path& path, const std::string& value, std::string* error) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return set_error(error, "cannot create store metadata");
    }
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!output) {
        return set_error(error, "cannot write store metadata");
    }
    output.flush();
    return output.good() || set_error(error, "cannot flush store metadata");
}

bool copy_checked(const fs::path& source, const fs::path& destination,
                  const ExpectedFile& expected, std::string* error) {
    std::error_code ec;
    const fs::file_status source_status = fs::symlink_status(source, ec);
    if (ec || !fs::is_regular_file(source_status)) {
        return set_error(error, "source changed during import");
    }
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!input || !output) {
        return set_error(error, "cannot stage content");
    }
    std::array<char, 1024U * 1024U> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            output.write(buffer.data(), count);
        }
    }
    output.flush();
    if (!input.eof() || !output) {
        return set_error(error, "cannot stage content");
    }
    const auto staged_size = fs::file_size(destination, ec);
    if (ec || staged_size != expected.size) {
        return set_error(error, "staged size mismatch");
    }
    std::string hash_error;
    if (sha256_file(destination, &hash_error) != expected.sha256) {
        return set_error(error, "staged hash mismatch");
    }
    return true;
}

bool validate_source(const fs::path& source, const IdentityProfile& profile,
                     std::string* error) {
    std::error_code ec;
    const fs::file_status source_status = fs::symlink_status(source, ec);
    if (ec || !fs::is_directory(source_status) || fs::is_symlink(source_status) ||
        has_symlink_component(source)) {
        return set_error(error, "source directory invalid");
    }

    std::unordered_set<std::string> actual;
    fs::directory_iterator entries(source, fs::directory_options::none, ec);
    if (ec) {
        return set_error(error, "cannot enumerate source");
    }
    for (const auto& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (!actual.insert(name).second) {
            return set_error(error, "duplicate source entry");
        }
        const fs::file_status status = entry.symlink_status(ec);
        if (ec || fs::is_symlink(status) || !fs::is_regular_file(status)) {
            return set_error(error, "source contains non-regular entry");
        }
        const auto expected = std::find_if(
            profile.files.begin(), profile.files.end(),
            [&name](const ExpectedFile& candidate) { return candidate.name == name; });
        if (expected == profile.files.end()) {
            return set_error(error, "source contains unexpected file");
        }
    }
    if (actual.size() != profile.files.size()) {
        return set_error(error, "source file set incomplete");
    }
    for (const auto& expected : profile.files) {
        const fs::path path = source / expected.name;
        const auto size = fs::file_size(path, ec);
        if (ec || size != expected.size) {
            return set_error(error, "source size mismatch");
        }
        std::string hash_error;
        const std::string actual_hash = sha256_file(path, &hash_error);
        if (actual_hash == kRetailXexSha256 && expected.name == "Default.xex") {
            return set_error(error, "retail XEX identity rejected");
        }
        if (actual_hash != expected.sha256) {
            return set_error(error, "source hash mismatch");
        }
    }
    return true;
}

bool check_generation_files(const fs::path& generation,
                            const IdentityProfile& profile, std::string* error) {
    std::set<std::string> expected_names;
    for (const auto& file : profile.files) {
        expected_names.insert(file.name);
    }
    expected_names.insert(kMarkerName);

    std::error_code ec;
    fs::directory_iterator entries(generation, fs::directory_options::none, ec);
    if (ec) {
        return set_error(error, "cannot enumerate published generation");
    }
    std::set<std::string> actual_names;
    for (const auto& entry : entries) {
        const std::string name = entry.path().filename().string();
        const fs::file_status status = entry.symlink_status(ec);
        if (ec || fs::is_symlink(status) || !fs::is_regular_file(status) ||
            !actual_names.insert(name).second || expected_names.find(name) == expected_names.end()) {
            return set_error(error, "published generation contains unexpected entry");
        }
    }
    if (actual_names != expected_names) {
        return set_error(error, "published generation file set incomplete");
    }
    std::ifstream marker(generation / kMarkerName, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(marker)),
                         std::istreambuf_iterator<char>());
    if (!marker || contents != kMarkerContents) {
        return set_error(error, "published marker invalid");
    }
    return true;
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
        value.files.assign(production_files().begin(), production_files().end());
        return value;
    }();
    return profile;
}

const char* store_marker_name() noexcept { return kMarkerName; }
const char* store_marker_contents() noexcept { return kMarkerContents; }
const char* retail_xex_sha256() noexcept { return kRetailXexSha256; }

ContentStore::ContentStore(fs::path root) : root_(fs::absolute(root).lexically_normal()) {}

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

bool ContentStore::import_directory(const fs::path& source, std::string* error) const {
    return import_with_profile(source, production_identity(), error);
}

bool ContentStore::import_with_profile(const fs::path& source,
                                       const IdentityProfile& profile,
                                       std::string* error) const {
    try {
        if (!validate_profile(profile, error) || !validate_source(source, profile, error)) {
            return false;
        }
        if (has_symlink_component(root_)) {
            return set_error(error, "store path invalid");
        }

        std::error_code ec;
        const fs::file_status generations_status =
            fs::symlink_status(root_ / "generations", ec);
        if (!ec && fs::is_symlink(generations_status)) {
            return set_error(error, "store path invalid");
        }
        ec.clear();
        const fs::file_status pointer_status = fs::symlink_status(root_ / "current", ec);
        if (!ec && fs::is_symlink(pointer_status)) {
            return set_error(error, "store path invalid");
        }
        ec.clear();
        fs::create_directories(root_ / "generations", ec);
        if (ec) {
            return set_error(error, "cannot create store");
        }

        const std::string id = operation_id();
        const fs::path staging = root_ / (".staging-" + id);
        const fs::path generation = root_ / "generations" / ("generation-" + id);
        const fs::path pointer_temp = root_ / (".current-" + id + ".tmp");
        bool generation_published = false;
        const auto cleanup = [&] {
            std::error_code cleanup_ec;
            fs::remove(pointer_temp, cleanup_ec);
            if (!generation_published) {
                fs::remove_all(staging, cleanup_ec);
            }
        };

        fs::create_directory(staging, ec);
        if (ec) {
            cleanup();
            return set_error(error, "cannot create staging area");
        }
        for (const auto& expected : profile.files) {
            if (!copy_checked(source / expected.name, staging / expected.name, expected, error)) {
                cleanup();
                return false;
            }
        }
        if (!write_file(staging / kMarkerName, kMarkerContents, error)) {
            cleanup();
            return false;
        }
        if (!check_generation_files(staging, profile, error)) {
            cleanup();
            return false;
        }
        fs::rename(staging, generation, ec);
        if (ec) {
            cleanup();
            return set_error(error, "cannot publish generation");
        }
        generation_published = true;
        if (!write_file(pointer_temp, generation.filename().string() + "\n", error)) {
            cleanup();
            fs::remove_all(generation, ec);
            return false;
        }
        fs::rename(pointer_temp, root_ / "current", ec);
        if (ec) {
            cleanup();
            fs::remove_all(generation, ec);
            return set_error(error, "cannot publish pointer");
        }
        return true;
    } catch (const fs::filesystem_error&) {
        return set_error(error, "filesystem operation failed");
    }
}

bool ContentStore::verify(std::string* error) const {
    return verify_with_profile(production_identity(), error);
}

bool ContentStore::verify_with_profile(const IdentityProfile& profile,
                                       std::string* error) const {
    try {
        if (!validate_profile(profile, error) || has_symlink_component(root_)) {
            return false;
        }
        fs::path generation;
        if (!read_pointer(root_, &generation, error) ||
            !check_generation_files(generation, profile, error)) {
            return false;
        }

        Vfs vfs(root_);
        for (const auto& expected : profile.files) {
            Sha256 hasher;
            std::uint64_t offset = 0;
            while (offset < expected.size) {
                const std::uint64_t remaining = expected.size - offset;
                const std::uint64_t length = std::min<std::uint64_t>(
                    remaining, Vfs::max_read_length());
                std::string read_error;
                const auto bytes = vfs.read("game:/" + expected.name, offset, length,
                                            &read_error);
                if (!bytes.has_value()) {
                    return set_error(error, "VFS content read failed");
                }
                hasher.update(std::span<const std::byte>(bytes->data(), bytes->size()));
                if (bytes->size() != length) {
                    return set_error(error, "VFS content length mismatch");
                }
                offset += length;
            }
            if (hasher.final_hex() != expected.sha256) {
                return set_error(error, "published content hash mismatch");
            }
        }
        return true;
    } catch (const fs::filesystem_error&) {
        return set_error(error, "filesystem operation failed");
    }
}

const fs::path& ContentStore::root() const noexcept { return root_; }

namespace testing {

bool import_fixture(ContentStore& store, const fs::path& source,
                    const IdentityProfile& profile, std::string* error) {
    return store.import_with_profile(source, profile, error);
}

bool verify_fixture(const ContentStore& store, const IdentityProfile& profile,
                    std::string* error) {
    return store.verify_with_profile(profile, error);
}

}  // namespace testing

}  // namespace ac6demo_native
