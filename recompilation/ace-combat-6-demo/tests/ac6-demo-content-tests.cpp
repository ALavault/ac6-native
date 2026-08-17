#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6demo/content.hpp"
#include "ac6demo/mission_bins.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <iostream>

int main() {
  assert(ac6demo::qualified_files().size() == 9U);
  assert(ac6demo::qualified_files()[0].sha256 == ac6demo::kQualifiedXexSha256);

  static_assert(sizeof(ac6demo::DurableBinAbi) == 0x10);
  static_assert(sizeof(ac6demo::ObjBinAbi) == 0x20);
  static_assert(sizeof(ac6demo::UnitBinAbi) == 0x08);
  static_assert(sizeof(ac6demo::UnitTblBinAbi) == 0x0c);
  static_assert(offsetof(ac6demo::UnitTblBinAbi, units) == 0x04);
  static_assert(offsetof(ac6demo::UnitTblBinAbi, objects) == 0x08);
  static_assert(offsetof(ac6demo::ObjBinAbi, durable) == 0x0c);
  ac6demo::DurableBinView empty_view;
  assert(!empty_view.valid());

  const std::array<std::byte, 8> decoded{
      std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13},
      std::byte{0x14}, std::byte{0x15}, std::byte{0x16}, std::byte{0x17}};
  ac6demo::DurableBinAbi wrapper;
  wrapper.payload = 0x1004;
  const auto resolved = ac6demo::resolve_durable_payload(
      wrapper, 0x1000, std::span<const std::byte>(decoded));
  assert(resolved.valid());
  assert(*resolved.data == std::byte{0x14});

  wrapper.payload = 0;
  assert(!ac6demo::resolve_durable_payload(
                  wrapper, 0x1000, std::span<const std::byte>(decoded))
              .valid());
  wrapper.payload = 0x0fff;
  assert(!ac6demo::resolve_durable_payload(
                  wrapper, 0x1000, std::span<const std::byte>(decoded))
              .valid());
  wrapper.payload = 0x1008;
  assert(!ac6demo::resolve_durable_payload(
                  wrapper, 0x1000, std::span<const std::byte>(decoded))
              .valid());

  const auto root = std::filesystem::temp_directory_path() / "ac6-demo-content-test";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  std::string failure;
  assert(!ac6demo::DemoStore::verify(root, &failure));
  assert(!failure.empty());
  assert(!ac6demo::DemoStore::import_directory(root, root / "store", &failure));
  std::filesystem::remove_all(root, error);

  std::cout << "ac6-demo-content-tests: ok\n";
  return 0;
}
