#include <catch2/catch_test_macros.hpp>
#include <native/filesystem.h>
#include <native/platform.h>

#if !REX_PLATFORM_WIN32 && !REX_PLATFORM_ANDROID

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include <unistd.h>

TEST_CASE("POSIX file handles preserve combined read and write access", "[filesystem][posix]") {
  const auto path = std::filesystem::temp_directory_path() /
                    ("rex_file_access_test_" + std::to_string(getpid()));
  struct RemoveFile {
    std::filesystem::path path;
    ~RemoveFile() { std::filesystem::remove(path); }
  } remove_file{path};

  REQUIRE(rex::filesystem::CreateEmptyFile(path));
  auto file = rex::filesystem::FileHandle::OpenExisting(
      path, rex::filesystem::FileAccess::kGenericRead |
                rex::filesystem::FileAccess::kGenericWrite);
  REQUIRE(file != nullptr);

  constexpr std::array<uint8_t, 8> expected = {0x41, 0x43, 0x36, 0x00,
                                                0x52, 0x45, 0x58, 0x00};
  size_t bytes_written = 0;
  REQUIRE(file->Write(0, expected.data(), expected.size(), &bytes_written));
  CHECK(bytes_written == expected.size());

  std::array<uint8_t, expected.size()> actual{};
  size_t bytes_read = 0;
  REQUIRE(file->Read(0, actual.data(), actual.size(), &bytes_read));
  CHECK(bytes_read == actual.size());
  CHECK(actual == expected);
}

#endif
