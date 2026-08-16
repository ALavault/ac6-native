#include "ac6demo_native/content_store.hpp"
#include "ac6demo_native/sha256.hpp"
#include "ac6demo_native/vfs.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

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
    fixture.root = fs::temp_directory_path() / ("ac6-demo-native-vfs-" +
                                                 std::to_string(stamp));
    fixture.source = fixture.root / "source";
    fixture.store = fixture.root / "store";
    fs::create_directories(fixture.source);
    fixture.profile = ac6demo_native::production_identity();
    constexpr std::array<const char*, 9U> fixture_hashes = {
        "28866794d33e5f58db1c4a8a14a59f4021d594b9fb610de521b0e7fd07a5f5e2",
        "3c5180fe488bbeb3735fe966871a37db9a8ce3a92898e52d39263697977c9337",
        "46624b152325958dfe55bade2dadb20b30aa15c6f5906e05d712465fa7ff808d",
        "e35b4648e2379f379b99a0d5d78a78ab6f37d91365bb066f7dff2530ad7a712a",
        "dea2e94b63c693367ac987abcf0ec18b4eb296bd3c9b63bdd622441126168e97",
        "d23b72b1f0b801ddd207b17311a920f5870a1ba43b7dd088b33223f209aee074",
        "632c409a6af7da9f9afb16c5dad1622005c27b72753380ec5f1c24102f0fc397",
        "b7f30f7c122a7678bdd8069a5cb0395d47cb7325d7ae9e55e1a87c83d763f5b7",
        "029fa331595b13a1da6a630e9ed04bed1e8b9cd653e0d65a5afca255f1ef8f11",
    };
    for (std::size_t index = 0; index < fixture.profile.files.size(); ++index) {
        const std::string payload = "entry-" + std::to_string(index) + "-0123456789";
        const auto& expected = fixture.profile.files[index];
        const fs::path path = fixture.source / expected.name;
        std::ofstream output(path, std::ios::binary);
        output << payload;
        output.close();
        fixture.profile.files[index].size = payload.size();
        fixture.profile.files[index].sha256 = fixture_hashes[index];
    }
    ac6demo_native::ContentStore store(fixture.store);
    std::string error;
    if (!ac6demo_native::testing::import_fixture(store, fixture.source, fixture.profile, &error)) {
        std::cerr << "fixture setup failed: " << error << '\n';
        std::abort();
    }
    return fixture;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

fs::path generation_path(const fs::path& store) {
    std::ifstream pointer(store / "current", std::ios::binary);
    std::string name;
    std::getline(pointer, name);
    return store / "generations" / name;
}

void test_positive_offset_reads() {
    Fixture fixture = make_fixture();
    ac6demo_native::Vfs vfs(fixture.store);
    std::string error;
    const auto bytes = vfs.read("game:/Default.xex", 2U, 5U, &error);
    require(bytes.has_value(), "valid game path read");
    const std::string value(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    require(value == "try-0", "bounded offset read contents");
    const auto empty = vfs.read("game:/DATA.TBL", 0U, 0U, &error);
    require(empty.has_value() && empty->empty(), "zero-length bounded read");
}

void test_path_boundaries() {
    Fixture fixture = make_fixture();
    ac6demo_native::Vfs vfs(fixture.store);
    const std::array<std::string_view, 9U> invalid = {
        "file:/Default.xex", "host:/Default.xex", "/Default.xex",
        "game:/../Default.xex", "game:/..\\Default.xex", "game:/default.xex",
        "game:/unknown.bin", "game://Default.xex", "game:/Default.xex/extra",
    };
    for (const auto path : invalid) {
        std::string error;
        require(!vfs.read(path, 0U, 1U, &error).has_value(), "invalid VFS path rejected");
        require(error.find(fixture.store.string()) == std::string::npos,
                "VFS error does not leak host path");
    }
    std::string error;
    require(!vfs.read("game:/Default.xex", 100U, 1U, &error).has_value(),
            "offset beyond file rejected");
    require(!vfs.read("game:/Default.xex", 0U, ac6demo_native::Vfs::max_read_length() + 1U,
                       &error)
                 .has_value(),
            "oversized read rejected");
    require(!vfs.read("game:/Default.xex", std::numeric_limits<std::uint64_t>::max(), 0U,
                       &error)
                 .has_value(),
            "overflow offset rejected");
}

void test_marker_and_symlink_boundaries() {
    Fixture fixture = make_fixture();
    const fs::path generation = generation_path(fixture.store);
    ac6demo_native::Vfs vfs(fixture.store);
    std::string error;
    {
        std::ofstream marker(generation / ac6demo_native::store_marker_name(),
                             std::ios::binary | std::ios::trunc);
        marker << "tampered-marker\n";
    }
    require(!vfs.read("game:/Default.xex", 0U, 1U, &error).has_value(),
            "tampered marker rejects VFS");
    ac6demo_native::ContentStore store(fixture.store);
    require(!ac6demo_native::testing::verify_fixture(store, fixture.profile, &error),
            "tampered marker rejects verification");

    Fixture oversize_marker_fixture = make_fixture();
    const fs::path oversize_generation = generation_path(oversize_marker_fixture.store);
    {
        std::ofstream marker(oversize_generation / ac6demo_native::store_marker_name(),
                             std::ios::binary | std::ios::trunc);
        marker << std::string(5000U, 'x');
    }
    ac6demo_native::Vfs oversize_vfs(oversize_marker_fixture.store);
    require(!oversize_vfs.read("game:/Default.xex", 0U, 1U, &error).has_value(),
            "oversized marker rejects before guest allocation");

    Fixture missing_marker_fixture = make_fixture();
    const fs::path missing_marker_generation = generation_path(missing_marker_fixture.store);
    std::error_code ec;
    fs::remove(missing_marker_generation / ac6demo_native::store_marker_name(), ec);
    ac6demo_native::Vfs missing_marker_vfs(missing_marker_fixture.store);
    require(!missing_marker_vfs.read("game:/Default.xex", 0U, 1U, &error).has_value(),
            "missing marker rejects VFS");

    Fixture symlink_fixture = make_fixture();
    const fs::path symlink_generation = generation_path(symlink_fixture.store);
    fs::remove(symlink_generation / "Default.xex", ec);
    fs::create_symlink(symlink_generation / "DATA.TBL",
                       symlink_generation / "Default.xex", ec);
    require(!ec, "test symlink created");
    ac6demo_native::Vfs symlink_vfs(symlink_fixture.store);
    require(!symlink_vfs.read("game:/Default.xex", 0U, 1U, &error).has_value(),
            "published symlink rejected");

    Fixture symlink_store_fixture = make_fixture();
    const fs::path symlink_store = symlink_store_fixture.root / "store-link";
    fs::create_symlink(symlink_store_fixture.store, symlink_store, ec);
    require(!ec, "store symlink created");
    ac6demo_native::Vfs symlink_store_vfs(symlink_store);
    require(!symlink_store_vfs.read("game:/Default.xex", 0U, 1U, &error).has_value(),
            "VFS store symlink rejected");
}

}  // namespace

int main() {
    test_positive_offset_reads();
    test_path_boundaries();
    test_marker_and_symlink_boundaries();
    std::cout << "VFS boundary tests passed\n";
    return 0;
}
