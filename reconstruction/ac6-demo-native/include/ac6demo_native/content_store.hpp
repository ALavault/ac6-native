#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ac6demo_native {

struct ExpectedFile {
    std::string name;
    std::uint64_t size;
    std::string sha256;
};

struct IdentityProfile {
    std::string schema;
    std::string target_id;
    std::string product;
    std::string platform;
    std::string region;
    std::string vfs_namespace;
    bool supported = false;
    std::vector<ExpectedFile> files;
};

class ContentStore;

#if defined(AC6DEMO_NATIVE_ENABLE_TESTING)
namespace testing {
bool import_fixture(ContentStore& store, const std::filesystem::path& source,
                    const IdentityProfile& profile, std::string* error = nullptr);
bool verify_fixture(const ContentStore& store, const IdentityProfile& profile,
                    std::string* error = nullptr);
}  // namespace testing
#endif

[[nodiscard]] const IdentityProfile& production_identity() noexcept;
[[nodiscard]] const char* store_marker_name() noexcept;
[[nodiscard]] const char* store_marker_contents() noexcept;
[[nodiscard]] const char* retail_xex_sha256() noexcept;

class ContentStore {
public:
    explicit ContentStore(std::filesystem::path root);

    [[nodiscard]] static std::filesystem::path default_root();
    [[nodiscard]] static std::filesystem::path default_config_root();

    // These two operations use only the compiled production identity.
    [[nodiscard]] bool import_directory(const std::filesystem::path& source,
                                        std::string* error = nullptr) const;
    [[nodiscard]] bool verify(std::string* error = nullptr) const;

    [[nodiscard]] const std::filesystem::path& root() const noexcept;

private:
    std::filesystem::path root_;
};

}  // namespace ac6demo_native
