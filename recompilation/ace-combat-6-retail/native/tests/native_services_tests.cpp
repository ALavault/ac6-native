#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_services.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

void offline_network_never_opens_a_socket() {
  ac6::native::OfflineNetwork network;
  const std::vector<std::uint8_t> data{1u, 2u};
  assert(network.connect("example.invalid") == ac6::native::ServiceError::kOffline);
  assert(network.send(data) == ac6::native::ServiceError::kOffline);
  assert(network.receive(std::span<std::uint8_t>{}) ==
         ac6::native::ServiceError::kOffline);
}

void kernel_boundaries_are_deterministic() {
  ac6::native::HandleTable handles;
  const std::uint32_t first = handles.allocate();
  assert(first != 0u && handles.contains(first));
  assert(!handles.close(0u));
  assert(handles.close(first));
  assert(!handles.contains(first));

  ac6::native::AutoResetEvent event;
  assert(!event.consume());
  event.signal();
  assert(event.consume());
  assert(!event.consume());
  event.signal();
  event.signal();
  assert(event.consume());
  assert(!event.consume());

  ac6::native::XenonTimebase timebase;
  timebase.set_tick(3u);
  assert(timebase.now() == 2'500'000u);
  timebase.advance();
  assert(timebase.tick() == 4u);
  assert(timebase.now() == 3'333'333u);
}

void vfs_and_save_paths_are_confined_and_atomic() {
  const auto root = std::filesystem::temp_directory_path() / "ac6-native-services-test";
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  std::filesystem::create_directories(root, ignored);
  const auto file = root / "read.txt";
  std::filesystem::copy_file(
      std::filesystem::path("/etc/hostname"), file,
      std::filesystem::copy_options::overwrite_existing, ignored);
  ac6::native::ConfinedVfs vfs(root);
  std::filesystem::path resolved;
  assert(vfs.resolve("read.txt", resolved) == ac6::native::ServiceError::kNone);
  assert(vfs.resolve("READ.TXT", resolved) == ac6::native::ServiceError::kNone);
  assert(vfs.resolve("../read.txt", resolved) ==
         ac6::native::ServiceError::kInvalidPath);
  assert(vfs.resolve("/etc/hostname", resolved) ==
         ac6::native::ServiceError::kInvalidPath);
  std::vector<std::uint8_t> bytes;
  assert(vfs.read("read.txt", bytes) == ac6::native::ServiceError::kNone);
  ac6::native::AtomicSaveStore saves(root / "saves");
  const std::vector<std::uint8_t> save{0xACu, 0x06u, 0x01u};
  assert(saves.write("slot0.bin", save) == ac6::native::ServiceError::kNone);
  assert(saves.read("slot0.bin", bytes) == ac6::native::ServiceError::kNone);
  assert(bytes == save);
  assert(saves.write("../escape", save) == ac6::native::ServiceError::kInvalidPath);
  assert(!std::filesystem::exists(root / "saves/.slot0.bin.tmp.1"));
  std::filesystem::remove_all(root, ignored);
}

void replay_is_poll_exact_and_xma_has_two_slots() {
  const std::vector<ac6::native::ReplaySample> samples{
      {10u, 0u, 1u, 2, 3, 4, 5}, {11u, 0u, 2u, 3, 4, 5, 6}};
  ac6::native::StrictInputReplay replay(samples);
  ac6::native::ReplaySample sample;
  assert(replay.poll(9u, 0u, sample) == ac6::native::ServiceError::kPollMismatch);
  assert(replay.poll(10u, 0u, sample) == ac6::native::ServiceError::kNone);
  assert(sample.buttons == 1u);
  assert(replay.finalize() == ac6::native::ServiceError::kReplayIncomplete);
  assert(replay.poll(11u, 0u, sample) == ac6::native::ServiceError::kNone);
  assert(replay.finalize() == ac6::native::ServiceError::kNone);

  ac6::native::XmaDoubleBuffer ring(4u);
  const std::vector<std::uint8_t> packet{1u, 2u, 3u};
  assert(ring.push(packet) == ac6::native::ServiceError::kNone);
  assert(ring.push(packet) == ac6::native::ServiceError::kNone);
  assert(ring.push(packet) == ac6::native::ServiceError::kRingFull);
  std::vector<std::uint8_t> popped;
  assert(ring.pop(popped) == ac6::native::ServiceError::kNone);
  assert(popped == packet);
  assert(ring.pop(popped) == ac6::native::ServiceError::kNone);
  assert(ring.pop(popped) == ac6::native::ServiceError::kRingEmpty);
}

}  // namespace

int main() {
  offline_network_never_opens_a_socket();
  kernel_boundaries_are_deterministic();
  vfs_and_save_paths_are_confined_and_atomic();
  replay_is_poll_exact_and_xma_has_two_slots();
  return 0;
}
