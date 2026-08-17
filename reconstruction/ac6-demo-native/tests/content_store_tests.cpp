#include "ac6demo_native/content_store.hpp"
#include "ac6demo_native/sha256.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

struct Fixture {
    fs::path root;
    fs::path source;
    fs::path store;
    ac6demo_native::IdentityProfile profile;

    Fixture() = default;
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    Fixture(Fixture&& other) noexcept
        : root(std::move(other.root)),
          source(std::move(other.source)),
          store(std::move(other.store)),
          profile(std::move(other.profile)) {}
    Fixture& operator=(Fixture&& other) noexcept {
        if (this != &other) {
            std::error_code ec;
            fs::remove_all(root, ec);
            root = std::move(other.root);
            source = std::move(other.source);
            store = std::move(other.store);
            profile = std::move(other.profile);
        }
        return *this;
    }

    ~Fixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

Fixture make_fixture() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    Fixture fixture;
    fixture.root = fs::temp_directory_path() / ("ac6-demo-native-content-" +
                                                 std::to_string(stamp));
    fixture.source = fixture.root / "source";
    fixture.store = fixture.root / "store";
    fs::create_directories(fixture.source);
    fixture.profile = ac6demo_native::production_identity();
    constexpr std::array<const char*, 9U> fixture_hashes = {
        "24bfefcb6e74c711d738ab9c85f4b46c618811064680126aaab31f366e6d745e",
        "6b4582cfe087a7dc262faec27674c167e1ee3ac9ce6067b72bfc428d22b24db8",
        "9c8c0939a3a1012e3813962738531239b64c8ce7e3c40c3c915a208957da4fc3",
        "0f43adb318b645b1aaa444ffb2859e2d9834364c182da22c6715b5151b3acb4d",
        "d134a0f7aed318b5810a104e4198211b5ddb47416bed61f09cb1c8b64c2ca439",
        "263f1bdc8f65f7ba15495955ce4a2cbd532a4902f6efa3f4c45c10e4e406295f",
        "f7c32948afe52f85f9ec36064511443e5716347a4b6e45b98fa8e369a75b5ff1",
        "5088008fc1fbef91958e5b6815e9624f9b665ec041819bf2ac7193f454bc2e1d",
        "138383eea238817b70501b35db6a8c30833b076728e313707e8c6682f4398ae6",
    };
    for (std::size_t index = 0; index < fixture.profile.files.size(); ++index) {
        const std::string payload = "synthetic-fixture-" + std::to_string(index) +
                                     "\nread-only identity test\n";
        const auto& expected = fixture.profile.files[index];
        const fs::path path = fixture.source / expected.name;
        std::ofstream output(path, std::ios::binary);
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        output.close();
        fixture.profile.files[index].size = payload.size();
        fixture.profile.files[index].sha256 = fixture_hashes[index];
    }
    return fixture;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

bool rejected(Fixture& fixture) {
    ac6demo_native::ContentStore store(fixture.store);
    std::string error;
    return !ac6demo_native::testing::import_fixture(store, fixture.source, fixture.profile, &error) &&
           !error.empty();
}

void test_sha256_vectors() {
    const std::array<std::byte, 0U> empty{};
    require(ac6demo_native::sha256_bytes(empty) ==
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "SHA-256 empty vector");
    const std::string abc = "abc";
    require(ac6demo_native::sha256_bytes(std::as_bytes(std::span(abc))) ==
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "SHA-256 abc vector");
    std::vector<std::byte> million(1000000U, std::byte{'a'});
    require(ac6demo_native::sha256_bytes(million) ==
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "SHA-256 million-a vector");
    Fixture fixture = make_fixture();
    std::string error;
    require(ac6demo_native::sha256_file(fixture.root / "does-not-exist", &error).empty() &&
                !error.empty(),
            "SHA-256 I/O error is reported");
}

void test_positive_and_rollback() {
    Fixture fixture = make_fixture();
    ac6demo_native::ContentStore store(fixture.store);
    std::string error;
    require(!store.import_directory(fixture.source, &error),
            "production import does not accept synthetic fixture identity");
    const bool imported = ac6demo_native::testing::import_fixture(store, fixture.source,
                                                                    fixture.profile, &error);
    if (!imported) {
        std::cerr << "synthetic import error: " << error << '\n';
    }
    require(imported, "synthetic nine-file import");
    require(ac6demo_native::testing::verify_fixture(store, fixture.profile, &error),
            "synthetic nine-file verification through VFS");
    const fs::path pointer = fixture.store / "current";
    std::ifstream pointer_input(pointer, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(pointer_input)),
                             std::istreambuf_iterator<char>());

    {
        std::ofstream tampered(fixture.source / "DATA.TBL", std::ios::binary | std::ios::trunc);
        tampered << "tampered";
    }
    require(!ac6demo_native::testing::import_fixture(store, fixture.source, fixture.profile, &error),
            "tamper rejected before publication");
    std::ifstream after_input(pointer, std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(after_input)),
                            std::istreambuf_iterator<char>());
    require(after == before, "failed import preserves current pointer");
    bool has_staging = false;
    for (const auto& entry : fs::directory_iterator(fixture.store)) {
        has_staging = has_staging || entry.path().filename().string().rfind(".staging-", 0U) == 0U;
    }
    require(!has_staging, "preflight failure creates no staging");
}

void test_rejections() {
    {
        Fixture fixture = make_fixture();
        fs::remove(fixture.source / "DATA.TBL");
        require(rejected(fixture), "missing file rejected");
    }
    {
        Fixture fixture = make_fixture();
        std::ofstream extra(fixture.source / "moviepack.bin", std::ios::binary);
        extra << "unexpected";
        require(rejected(fixture), "unexpected file rejected");
    }
    {
        Fixture fixture = make_fixture();
        fs::rename(fixture.source / "DATA.TBL", fixture.source / "data.tbl");
        require(rejected(fixture), "wrong case rejected");
    }
    {
        Fixture fixture = make_fixture();
        std::error_code ec;
        fs::remove(fixture.source / "DATA.TBL", ec);
        fs::create_symlink(fixture.source / "DATA00.PAC", fixture.source / "DATA.TBL", ec);
        require(!ec && rejected(fixture), "source symlink rejected");
    }
    {
        Fixture fixture = make_fixture();
        std::ofstream truncated(fixture.source / "DATA.TBL", std::ios::binary | std::ios::trunc);
        truncated << "short";
        require(rejected(fixture), "truncated file rejected");
    }
    {
        Fixture fixture = make_fixture();
        auto profile = fixture.profile;
        profile.files[0].sha256 = ac6demo_native::retail_xex_sha256();
        ac6demo_native::ContentStore store(fixture.store);
        require(!ac6demo_native::testing::import_fixture(store, fixture.source, profile),
                "retail identity cannot be injected as production corpus");
    }
    {
        Fixture fixture = make_fixture();
        auto profile = fixture.profile;
        profile.schema = "wrong-schema";
        ac6demo_native::ContentStore store(fixture.store);
        std::string error;
        require(!ac6demo_native::testing::import_fixture(store, fixture.source, profile, &error),
                "metadata schema cannot be overridden");
    }
    {
        Fixture fixture = make_fixture();
        auto profile = fixture.profile;
        profile.supported = true;
        ac6demo_native::ContentStore store(fixture.store);
        std::string error;
        require(!ac6demo_native::testing::import_fixture(store, fixture.source, profile, &error),
                "supported profile cannot be enabled");
    }
    {
        Fixture fixture = make_fixture();
        std::error_code ec;
        fs::remove(fixture.source / "DATA.TBL", ec);
        fs::create_symlink(fixture.source / "DATA00.PAC", fixture.source / "DATA.TBL", ec);
        require(!ec && rejected(fixture), "source symlink swap rejected");
    }
    {
        Fixture fixture = make_fixture();
        std::error_code ec;
        fs::create_directories(fixture.root / "other", ec);
        fs::remove_all(fixture.store, ec);
        fs::create_symlink(fixture.root / "other", fixture.store, ec);
        require(!ec && rejected(fixture), "store symlink rejected");
    }
}

void test_concurrent_imports_and_orphan_recovery() {
    Fixture fixture = make_fixture();
    fs::create_directories(fixture.store / ".staging-orphan");
    {
        std::ofstream orphan(fixture.store / ".staging-orphan" / "left-by-crash");
        orphan << "orphan";
    }
    ac6demo_native::ContentStore first(fixture.store);
    ac6demo_native::ContentStore second(fixture.store);
    std::string first_error;
    std::string second_error;
    bool first_ok = false;
    bool second_ok = false;
    std::thread first_thread([&] {
        first_ok = ac6demo_native::testing::import_fixture(
            first, fixture.source, fixture.profile, &first_error);
    });
    std::thread second_thread([&] {
        second_ok = ac6demo_native::testing::import_fixture(
            second, fixture.source, fixture.profile, &second_error);
    });
    first_thread.join();
    second_thread.join();
    if (!first_ok || !second_ok) {
        std::cerr << "concurrent import errors: " << first_error << " / " << second_error
                  << '\n';
    }
    require(first_ok && second_ok, "concurrent imports are valid");
    std::string error;
    require(ac6demo_native::testing::verify_fixture(first, fixture.profile, &error),
            "concurrent publication verifies");
    require(fs::exists(fixture.store / ".staging-orphan" / "left-by-crash"),
            "orphan staging is not deleted by another process");
}

void test_multiprocess_import_and_failure_rollback() {
#if defined(__unix__) || defined(__APPLE__)
    Fixture fixture = make_fixture();
    int result_pipe[2] = {-1, -1};
    require(::pipe(result_pipe) == 0, "multiprocess result pipe created");
    const pid_t child = ::fork();
    require(child >= 0, "multiprocess child created");
    if (child == 0) {
        (void)::close(result_pipe[0]);
        ac6demo_native::ContentStore child_store(fixture.store);
        std::string child_error;
        const bool ok = ac6demo_native::testing::import_fixture(
            child_store, fixture.source, fixture.profile, &child_error);
        const unsigned char result = ok ? 1U : 0U;
        (void)::write(result_pipe[1], &result, sizeof(result));
        (void)::close(result_pipe[1]);
        ::_exit(ok ? 0 : 1);
    }
    (void)::close(result_pipe[1]);
    ac6demo_native::ContentStore parent_store(fixture.store);
    std::string parent_error;
    const bool parent_ok = ac6demo_native::testing::import_fixture(
        parent_store, fixture.source, fixture.profile, &parent_error);
    unsigned char child_result = 0U;
    require(::read(result_pipe[0], &child_result, sizeof(child_result)) ==
                static_cast<ssize_t>(sizeof(child_result)),
            "multiprocess result received");
    (void)::close(result_pipe[0]);
    int child_status = 0;
    require(::waitpid(child, &child_status, 0) == child, "multiprocess child joined");
    require(parent_ok && child_result == 1U && WIFEXITED(child_status) &&
                WEXITSTATUS(child_status) == 0,
            "multiprocess imports are valid");
    std::string error;
    require(ac6demo_native::testing::verify_fixture(parent_store, fixture.profile, &error),
            "multiprocess publication verifies");

    const fs::path pointer = fixture.store / "current";
    std::ifstream before_input(pointer, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_input)),
                             std::istreambuf_iterator<char>());
    setenv("AC6DEMO_NATIVE_TEST_FAIL_RENAME_ONCE", "1", 1);
    require(!ac6demo_native::testing::import_fixture(parent_store, fixture.source,
                                                      fixture.profile, &error),
            "injected staging rename failure rejected");
    unsetenv("AC6DEMO_NATIVE_TEST_FAIL_RENAME_ONCE");
    setenv("AC6DEMO_NATIVE_TEST_FAIL_FSYNC_ONCE", "1", 1);
    require(!ac6demo_native::testing::import_fixture(parent_store, fixture.source,
                                                      fixture.profile, &error),
            "injected fsync failure rejected");
    unsetenv("AC6DEMO_NATIVE_TEST_FAIL_FSYNC_ONCE");
    setenv("AC6DEMO_NATIVE_TEST_FAIL_POINTER_RENAME_ONCE", "1", 1);
    require(!ac6demo_native::testing::import_fixture(parent_store, fixture.source,
                                                      fixture.profile, &error),
            "injected pointer rename failure rejected");
    unsetenv("AC6DEMO_NATIVE_TEST_FAIL_POINTER_RENAME_ONCE");
    std::ifstream after_input(pointer, std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(after_input)),
                            std::istreambuf_iterator<char>());
    require(after == before, "failure injections preserve old current");
#endif
}

void test_pointer_corruption_and_rollback() {
    Fixture fixture = make_fixture();
    ac6demo_native::ContentStore store(fixture.store);
    std::string error;
    require(ac6demo_native::testing::import_fixture(store, fixture.source, fixture.profile, &error),
            "initial publication for pointer tests");
    const fs::path pointer = fixture.store / "current";
    std::ifstream before_input(pointer, std::ios::binary);
    const std::string before((std::istreambuf_iterator<char>(before_input)),
                             std::istreambuf_iterator<char>());
    {
        std::ofstream corrupt(pointer, std::ios::binary | std::ios::trunc);
        corrupt << "not-a-generation\n";
    }
    require(!ac6demo_native::testing::import_fixture(store, fixture.source, fixture.profile,
                                                       &error),
            "corrupt pointer rejects import");
    std::ifstream after_input(pointer, std::ios::binary);
    const std::string after((std::istreambuf_iterator<char>(after_input)),
                            std::istreambuf_iterator<char>());
    require(after != before && after == "not-a-generation\n",
            "corrupt pointer remains untouched");
    require(!store.verify(&error), "corrupt pointer rejects production verify");

    Fixture symlink_fixture = make_fixture();
    const fs::path symlink_generation = symlink_fixture.store / "generations" /
                                        "foreign-generation";
    fs::create_directories(symlink_generation);
    const fs::path symlink_pointer = symlink_fixture.store / "current";
    fs::remove(symlink_pointer);
    fs::create_symlink(symlink_generation, symlink_pointer);
    ac6demo_native::ContentStore symlink_store(symlink_fixture.store);
    require(!ac6demo_native::testing::import_fixture(
                symlink_store, symlink_fixture.source, symlink_fixture.profile, &error),
            "current symlink swap rejects import");
    require(fs::is_symlink(symlink_pointer), "foreign current symlink is preserved");
}

void test_separation() {
    const auto root = ac6demo_native::ContentStore::default_root().generic_string();
    require(root.find("ac6-demo-native") != std::string::npos,
            "default store has dedicated XDG product name");
    require(root.find("ac6-native") == std::string::npos &&
                root.find("ac6-demo-recomp") == std::string::npos,
            "default store is separate from other products");
    require(ac6demo_native::production_identity().target_id == "ac6-demo-xbox360-pal",
            "compiled target identity");
}

}  // namespace

int main() {
    test_sha256_vectors();
    test_positive_and_rollback();
    test_rejections();
    test_concurrent_imports_and_orphan_recovery();
    test_multiprocess_import_and_failure_rollback();
    test_pointer_corruption_and_rollback();
    test_separation();
    std::cout << "content store tests passed\n";
    return 0;
}
