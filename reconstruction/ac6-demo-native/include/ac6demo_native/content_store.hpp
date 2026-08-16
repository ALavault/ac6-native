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
    std::vector<ExpectedFile> files;
};

class ContentStore;

namespace testing {
bool import_fixture(ContentStore& store, const std::filesystem::path& source,
                    const IdentityProfile& profile, std::string* error = nullptr);
bool verify_fixture(const ContentStore& store, const IdentityProfile& profile,
                    std::string* error = nullptr);
}  // namespace testing

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
    friend bool testing::import_fixture(ContentStore&, const std::filesystem::path&,
                                        const IdentityProfile&, std::string*);
    friend bool testing::verify_fixture(const ContentStore&, const IdentityProfile&,
                                        std::string*);

    [[nodiscard]] bool import_with_profile(const std::filesystem::path& source,
                                           const IdentityProfile& profile,
                                           std::string* error) const;
    [[nodiscard]] bool verify_with_profile(const IdentityProfile& profile,
                                           std::string* error) const;

    std::filesystem::path root_;
};

}  // namespace ac6demo_native
