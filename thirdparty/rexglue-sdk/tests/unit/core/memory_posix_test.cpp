#include <catch2/catch_test_macros.hpp>
#include <native/memory/utils.h>
#include <native/platform.h>

#if !REX_PLATFORM_WIN32 && !REX_PLATFORM_ANDROID

#include <cerrno>
#include <cstdint>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

TEST_CASE("POSIX file mappings are anonymous after creation", "[memory][posix]") {
  const std::string name = "rex_mapping_unlink_test_" + std::to_string(getpid());
  const std::string shm_name = "/" + name;
  const size_t length = rex::memory::page_size();

  const auto handle = rex::memory::CreateFileMappingHandle(
      name, length, rex::memory::PageAccess::kReadWrite, true);
  REQUIRE(handle != rex::memory::kFileMappingHandleInvalid);

  errno = 0;
  const int reopened = shm_open(shm_name.c_str(), O_RDONLY, 0);
  CHECK(reopened == -1);
  CHECK(errno == ENOENT);
  if (reopened >= 0) {
    close(reopened);
  }

  auto* view = static_cast<uint32_t*>(rex::memory::MapFileView(
      handle, nullptr, length, rex::memory::PageAccess::kReadWrite, 0));
  REQUIRE(view != nullptr);
  view[0] = 0xAC600001;
  CHECK(view[0] == 0xAC600001);

  CHECK(rex::memory::UnmapFileView(handle, view, length));
  rex::memory::CloseFileMappingHandle(handle, name);
}

#endif
